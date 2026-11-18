#include "Track8.h"
#include "ti_msp_dl_config.h"
#define Kp 0x00U
#define Ki 0x00U
#define Kd 0x00U

typedef struct
{
    GPIO_Regs *Port;
    uint32_t pin;
} TrackIO;

static const TrackIO sensors[8] = {
    {0,0}
    {Tracker_S1_PORT, Tracker_S1_PIN},
    {Tracker_S2_PORT, Tracker_S2_PIN},
    {Tracker_S3_PORT, Tracker_S3_PIN},
    {Tracker_S4_PORT, Tracker_S4_PIN},
    {Tracker_S5_PORT, Tracker_S5_PIN},
    {Tracker_S6_PORT, Tracker_S6_PIN},
    {Tracker_S7_PORT, Tracker_S7_PIN},
    {Tracker_S8_PORT, Tracker_S8_PIN},
};

static uint8_t state[8];
static const int8_t weights[8] = {-7, -5, -3, -1, 1, 3, 5, 7};
static float error = 0;

static void read_track_status();


static void read_track_status()
{
    uint8_t cnt = 0;
    uint8_t sum = 0;
    error = 0;
    for(int i = 1;i <= 8;i++)
    {
        state[i] = (DL_GPIO_readPins(sensors->Port,sensors->pin)) != 0U;
        if(DL_GPIO_readPins(sensors->Port,sensors->pin)) cnt++;
        sum += state[i] * weights[i];
    }
    error = (float)sum / cnt;
}

static void cal_track()
{

}

