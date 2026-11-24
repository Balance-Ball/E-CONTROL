#include "MotorScheduler.h"

#include "ControlConfig.h"
#include "Debug.h"
#include "Hardware/KEY.h"
#include "Hardware/emm42.h"
#include <math.h>
#include <stdio.h>

#define PI_RAD 3.14159265358979323846f
#define WHEEL_COUNT 2U
#define PLATE_AXIS_COUNT 3U

typedef struct {
    float neutral_deg;
    float min_deg;
    float max_deg;
    float x_deg_per_rad;
    float y_deg_per_rad;
    uint32_t pulses_per_rev;
    bool positive_is_ccw;
} PlateAxis;

typedef struct {
    bool valid;
    int64_t value;
} SentTarget;

static const PlateAxis plate_axes[PLATE_AXIS_COUNT] = CONTROL_PLATE_AXES;
static const bool wheel_reversed[WHEEL_COUNT] = {CONTROL_LEFT_REVERSED, CONTROL_RIGHT_REVERSED};
static const float forward_xy[2] = CONTROL_VEHICLE_FORWARD_XY;
static const float left_xy[2] = CONTROL_VEHICLE_LEFT_XY;
static const float roll_xy[2] = CONTROL_IMU_ROLL_XY;
static const float pitch_xy[2] = CONTROL_IMU_PITCH_XY;
static const float rpm_per_mps = 60.0f * CONTROL_WHEEL_GEAR_RATIO /
                                 (2.0f * PI_RAD * CONTROL_WHEEL_RADIUS_M);

static bool tracking_enabled;
static bool heading_locked;
static float reference_yaw_rad;
static bool plate_calibrated;
static bool cycle_started;
static uint32_t cycle_ms;
static float wheel_mps[WHEEL_COUNT];
static float plate_tilt_rad[2];
static SentTarget wheel_sent[WHEEL_COUNT];
static SentTarget plate_sent[PLATE_AXIS_COUNT];

static float clamp(float value, float min, float max)
{
    return fminf(fmaxf(value, min), max);
}

static void limit_vector(float vector[2], float limit)
{
    float magnitude = hypotf(vector[0], vector[1]);
    if (magnitude > limit) {
        float scale = limit / magnitude;
        vector[0] *= scale;
        vector[1] *= scale;
    }
}

static float heading_correction(const IMU_Req *imu, bool straight)
{
    if (!straight || !imu->valid) {
        heading_locked = false;
        return 0.0f;
    }
    if (!heading_locked) {
        reference_yaw_rad = imu->yaw_rad;
        heading_locked = true;
    }
    float error = remainderf(reference_yaw_rad - imu->yaw_rad, 2.0f * PI_RAD);
    return clamp(CONTROL_HEADING_GAIN * error,
                 -CONTROL_HEADING_MAX_RAD_S, CONTROL_HEADING_MAX_RAD_S);
}

static void wheel_targets(const IMU_Req *imu, const Track_Req *track,
                          float dt_s, float targets_mps[WHEEL_COUNT])
{
    if (!tracking_enabled || !track->line_valid) {
        heading_locked = false;
        targets_mps[0] = targets_mps[1] = 0.0f;
        return;
    }

    float yaw_rate = track->yaw_rate_rad_s + heading_correction(imu, track->straight);
    float turn_mps = yaw_rate * CONTROL_WHEEL_TRACK_M * 0.5f;
    float max_speed = CONTROL_WHEEL_MAX_RPM / rpm_per_mps;
    float left = track->speed_mps - turn_mps;
    float right = track->speed_mps + turn_mps;
    float peak = fmaxf(fabsf(left), fabsf(right));
    float scale = peak > max_speed ? max_speed / peak : 1.0f;
    float step = CONTROL_DRIVE_ACCEL_MPS2 * dt_s;
    targets_mps[0] = wheel_mps[0] + clamp(left * scale - wheel_mps[0], -step, step);
    targets_mps[1] = wheel_mps[1] + clamp(right * scale - wheel_mps[1], -step, step);
}

static void motion_feedforward(const float targets_mps[WHEEL_COUNT], float dt_s, float tilt[2])
{
    float forward_mps = (targets_mps[0] + targets_mps[1]) * 0.5f;
    float previous_mps = (wheel_mps[0] + wheel_mps[1]) * 0.5f;
    float forward_mps2 = (forward_mps - previous_mps) / dt_s;
    float yaw_rate = (targets_mps[1] - targets_mps[0]) / CONTROL_WHEEL_TRACK_M;
    float left_mps2 = forward_mps * yaw_rate;
    float feedforward[2];

    for (unsigned axis = 0; axis < 2U; ++axis) {
        feedforward[axis] = (forward_mps2 * forward_xy[axis] +
                             left_mps2 * left_xy[axis]) / CONTROL_GRAVITY_MPS2;
    }
    limit_vector(feedforward, CONTROL_MAX_FEEDFORWARD_RAD);
    tilt[0] += feedforward[0];
    tilt[1] += feedforward[1];
}

static void plate_tilt_target(const IMU_Req *imu, const LQR_Req *ball,
                              const float targets_mps[WHEEL_COUNT], float dt_s, float tilt[2])
{
    if (ball->valid) {
        tilt[0] = ball->ax_mps2 / CONTROL_BALL_TILT_GAIN;
        tilt[1] = ball->ay_mps2 / CONTROL_BALL_TILT_GAIN;
    }
    motion_feedforward(targets_mps, dt_s, tilt);
    if (CONTROL_IMU_TILT_ENABLED && imu->valid) {
        float roll = imu->roll_rad - CONTROL_IMU_LEVEL_ROLL_RAD;
        float pitch = imu->pitch_rad - CONTROL_IMU_LEVEL_PITCH_RAD;
        for (unsigned axis = 0; axis < 2U; ++axis) {
            tilt[axis] -= roll * roll_xy[axis] + pitch * pitch_xy[axis];
        }
    }

    limit_vector(tilt, CONTROL_MAX_TILT_RAD);
    float change[2] = {tilt[0] - plate_tilt_rad[0], tilt[1] - plate_tilt_rad[1]};
    limit_vector(change, CONTROL_MAX_TILT_RATE_RAD_S * dt_s);
    tilt[0] = plate_tilt_rad[0] + change[0];
    tilt[1] = plate_tilt_rad[1] + change[1];
}

static int64_t plate_axis_target(const PlateAxis *axis, const float tilt[2])
{
    float degrees = axis->neutral_deg + axis->x_deg_per_rad * tilt[0] +
                    axis->y_deg_per_rad * tilt[1];
    degrees = clamp(degrees, axis->min_deg, axis->max_deg);
    double scale = (double)axis->pulses_per_rev / 360.0;
    double pulses = round((double)degrees * scale);
    /* 取整后仍需落在机械行程内。 */
    pulses = fmin(fmax(pulses, ceil((double)axis->min_deg * scale)),
                  floor((double)axis->max_deg * scale));
    return (int64_t)pulses;
}

static bool send_plate_targets(const float tilt[2])
{
    bool all_sent = true;

    for (unsigned i = 0; i < PLATE_AXIS_COUNT; ++i) {
        const PlateAxis *axis = &plate_axes[i];
        int64_t target = plate_axis_target(axis, tilt);
        SentTarget *sent = &plate_sent[i];
        if (sent->valid && sent->value == target) {
            continue;
        }

        bool ccw = (target >= 0) == axis->positive_is_ccw;
        uint32_t pulses = (uint32_t)(target < 0 ? -target : target);
        sent->valid = emm42_move_absolute((uint8_t)(EMM42_MOTOR_1 + i),
                        ccw ? EMM42_DIRECTION_CCW : EMM42_DIRECTION_CW,
                        CONTROL_PLATE_RPM, CONTROL_PLATE_ACCELERATION, pulses);
        sent->value = target;
        all_sent &= sent->valid;
    }
    plate_tilt_rad[0] = tilt[0];
    plate_tilt_rad[1] = tilt[1];
    return all_sent;
}

static bool send_wheel_targets(const float targets_mps[WHEEL_COUNT])
{
    bool all_sent = true;

    for (unsigned i = 0; i < WHEEL_COUNT; ++i) {
        int32_t rpm = (int32_t)lroundf(targets_mps[i] * rpm_per_mps);
        if (wheel_reversed[i]) {
            rpm = -rpm;
        }
        SentTarget *sent = &wheel_sent[i];
        if (!sent->valid || sent->value != rpm) {
            uint8_t address = (uint8_t)(EMM42_MOTOR_LEFT + i);
            sent->valid = rpm < 0 ? backward(address, (uint16_t)-rpm)
                                  : forward(address, (uint16_t)rpm);
            sent->value = rpm;
        }
        if (sent->valid) {
            wheel_mps[i] = targets_mps[i];
        }
        all_sent &= sent->valid;
    }
    return all_sent;
}

static bool calibrated_plate(void)
{
    bool independent = false;
    for (unsigned i = 0; i < PLATE_AXIS_COUNT; ++i) {
        const PlateAxis *axis = &plate_axes[i];
        const PlateAxis *next = &plate_axes[(i + 1U) % PLATE_AXIS_COUNT];
        double scale = (double)axis->pulses_per_rev / 360.0;
        if (axis->pulses_per_rev == 0U || !isfinite(axis->x_deg_per_rad) ||
            !isfinite(axis->y_deg_per_rad) ||
            !(axis->min_deg < axis->neutral_deg && axis->neutral_deg < axis->max_deg) ||
            !(fmax(fabs(axis->min_deg), fabs(axis->max_deg)) * scale <= UINT32_MAX) ||
            ceil((double)axis->min_deg * scale) > floor((double)axis->max_deg * scale)) {
            return false;
        }
        independent |= axis->x_deg_per_rad * next->y_deg_per_rad !=
                       axis->y_deg_per_rad * next->x_deg_per_rad;
    }
    return independent;
}

static bool take_control_period(uint32_t now_ms, float *dt_s)
{
    uint32_t elapsed_ms = cycle_started ? now_ms - cycle_ms : CONTROL_PERIOD_MS;
    if (elapsed_ms < CONTROL_PERIOD_MS) {
        return false;
    }

    *dt_s = (float)elapsed_ms * 0.001f;
    cycle_ms = now_ms;
    cycle_started = true;
    return true;
}

static void report_requested_positions(void)
{
    if (!KEY_TakePositionRequest()) {
        return;
    }

    for (uint8_t address = EMM42_MOTOR_1; address <= EMM42_MOTOR_3; ++address) {
        int64_t position;
        char message[64];
        if (emm42_read_position(address, &position)) {
            snprintf(message, sizeof(message), "Motor %u POS: %f\r\n",
                     (unsigned)address, emm42_position_to_degrees(position));
        } else {
            snprintf(message, sizeof(message), "Motor %u POS Read Failed\r\n",
                     (unsigned)address);
        }
        Debug_MSG(message);
    }
}

bool Motor_Initial(void)
{
    const float zero[2] = {0.0f, 0.0f};

    tracking_enabled = heading_locked = cycle_started = false;
    plate_calibrated = calibrated_plate();
    for (unsigned i = 0; i < WHEEL_COUNT; ++i) {
        wheel_sent[i].valid = false;
        wheel_mps[i] = 0.0f;
    }
    for (unsigned i = 0; i < PLATE_AXIS_COUNT; ++i) {
        plate_sent[i].valid = false;
    }

    emm42_init(EMM42_MOTOR_ALL);
    bool enabled = emm42_enable(EMM42_MOTOR_ALL);
    bool wheels_sent = send_wheel_targets(zero);
    bool plate_initialized = !plate_calibrated || send_plate_targets(zero);
    return enabled && wheels_sent && plate_initialized;
}

void Motor_SetTrackingEnabled(bool enabled)
{
    tracking_enabled = enabled;
}

bool Motor_Update(uint32_t now_ms, const IMU_Req *imu,
                  const Track_Req *track, const LQR_Req *ball)
{
    float dt_s;
    if (!take_control_period(now_ms, &dt_s)) {
        return true;
    }

    float wheels[WHEEL_COUNT];
    float tilt[2] = {0};
    wheel_targets(imu, track, dt_s, wheels);
    if (plate_calibrated) {
        plate_tilt_target(imu, ball, wheels, dt_s, tilt);
    }

    bool wheels_sent = send_wheel_targets(wheels);
    bool plate_targets_sent = !plate_calibrated || send_plate_targets(tilt);
    report_requested_positions();
    return wheels_sent && plate_targets_sent;
}
