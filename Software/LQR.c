#include "LQR.h"

#include "ControlConfig.h"
#include "Hardware/vision.h"

static const float meters_per_pixel[2] = CONTROL_METERS_PER_PIXEL;
static const float position_gain[2] = CONTROL_POSITION_GAIN;
static const float velocity_gain[2] = CONTROL_VELOCITY_GAIN;

static bool ball_found;
static uint32_t sample_ms;
static float position_m[2];
static float velocity_mps[2];

static void update_ball_state(const vision_data *frame)
{
    uint32_t elapsed_ms = frame->sample_ms - sample_ms;
    float pixels[2] = {(float)frame->dx / 100.0f, (float)frame->dy / 100.0f};

    for (unsigned i = 0; i < 2U; ++i) {
        float position = pixels[i] * meters_per_pixel[i];
        if (!ball_found || elapsed_ms >= CONTROL_VISION_TIMEOUT_MS) {
            velocity_mps[i] = 0.0f;
        } else if (elapsed_ms != 0U) {
            float dt_s = (float)elapsed_ms * 0.001f;
            float measured_mps = (position - position_m[i]) / dt_s;
            float alpha = dt_s / (CONTROL_VELOCITY_TAU_S + dt_s);
            velocity_mps[i] += alpha * (measured_mps - velocity_mps[i]);
        }
        position_m[i] = position;
    }
    sample_ms = frame->sample_ms;
    ball_found = true;
}

static float ball_acceleration(unsigned axis)
{
    return -CONTROL_BALL_TILT_GAIN *
           (position_gain[axis] * position_m[axis] +
            velocity_gain[axis] * velocity_mps[axis]);
}

LQR_Req LQR_Update(uint32_t now_ms)
{
    vision_data frame;
    if (Get_Vision_data(&frame)) {
        if (frame.found) {
            update_ball_state(&frame);
        } else {
            ball_found = false;
        }
    }

    ball_found = ball_found &&
                 (int32_t)(now_ms - sample_ms) < (int32_t)CONTROL_VISION_TIMEOUT_MS;
    bool calibrated = meters_per_pixel[0] > 0.0f && meters_per_pixel[1] > 0.0f &&
                      position_gain[0] > 0.0f && position_gain[1] > 0.0f &&
                      velocity_gain[0] > 0.0f && velocity_gain[1] > 0.0f;
    if (!ball_found || !calibrated) {
        return (LQR_Req){0};
    }
    return (LQR_Req){true, ball_acceleration(0), ball_acceleration(1)};
}
