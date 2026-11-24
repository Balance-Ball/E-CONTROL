#include "Track8.h"

#include "Software/ControlConfig.h"
#include "ti_msp_dl_config.h"
#include <math.h>

typedef struct {
    GPIO_Regs *port;
    uint32_t pin;
} TrackInput;

static const TrackInput sensors[8] = {
    {Tracker_S1_PORT, Tracker_S1_PIN},
    {Tracker_S2_PORT, Tracker_S2_PIN},
    {Tracker_S3_PORT, Tracker_S3_PIN},
    {Tracker_S4_PORT, Tracker_S4_PIN},
    {Tracker_S5_PORT, Tracker_S5_PIN},
    {Tracker_S6_PORT, Tracker_S6_PIN},
    {Tracker_S7_PORT, Tracker_S7_PIN},
    {Tracker_S8_PORT, Tracker_S8_PIN},
};

static Track_Req latest;
static bool sampled;
static uint32_t sample_ms;

static uint8_t read_line_mask(void)
{
    uint8_t mask = 0;
    for (unsigned i = 0; i < 8U; ++i) {
        if (DL_GPIO_readPins(sensors[i].port, sensors[i].pin) == 0U) {
            mask |= (uint8_t)(1U << i);
        }
    }
    return mask;
}

static Track_Req track_demand(uint8_t mask)
{
    static const int8_t weights[8] = {7, 5, 3, 1, -1, -3, -5, -7};
    unsigned hits = 0;
    unsigned segments = 0;
    int sum = 0;

    for (unsigned i = 0; i < 8U; ++i) {
        if ((mask & (1U << i)) != 0U) {
            ++hits;
            sum += weights[i];
            if (i == 0U || (mask & (1U << (i - 1U))) == 0U) {
                ++segments;
            }
        }
    }
    if (segments != 1U || hits == 8U) {
        return (Track_Req){0};
    }

    float error = (float)sum / (7.0f * (float)hits);
    return (Track_Req){
        .line_valid = true,
        .straight = fabsf(error) <= CONTROL_TRACK_STRAIGHT_ERROR,
        .speed_mps = CONTROL_CRUISE_MPS,
        .yaw_rate_rad_s = CONTROL_TRACK_YAW_GAIN * error,
    };
}

Track_Req Track_Update(uint32_t now_ms)
{
    if (!sampled || now_ms - sample_ms >= CONTROL_PERIOD_MS) {
        latest = track_demand(read_line_mask());
        sample_ms = now_ms;
        sampled = true;
    }
    return latest;
}
