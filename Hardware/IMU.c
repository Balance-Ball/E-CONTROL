#include "IMU.h"

#include "Software/ControlConfig.h"
#include "Software/bsp_timer.h"
#include "ti_msp_dl_config.h"
#include <string.h>

#define IMU_FRAME_SIZE 11U
#define IMU_QUEUE_SIZE 4U
#define IMU_RAW_TO_RAD (3.14159265f / 32768.0f)
#define IMU_RAD_TO_DEG (180.0f / 3.14159265f)

typedef struct {
    uint8_t bytes[IMU_FRAME_SIZE];
    uint32_t sample_ms;
} IMU_Block;

static uint8_t dma_buffer[IMU_FRAME_SIZE];
static IMU_Block queue[IMU_QUEUE_SIZE];
static volatile unsigned queue_read;
static volatile unsigned queue_write;
static volatile bool queue_overrun;

static uint8_t frame[IMU_FRAME_SIZE];
static unsigned frame_length;
static IMU_Req latest;
static uint32_t latest_ms;
static float temperature_c;

static void start_receive(void)
{
    DL_DMA_setSrcAddr(DMA, DMA_CH0_CHAN_ID, (uint32_t)(uintptr_t)&IMU_INST->RXDATA);
    DL_DMA_setDestAddr(DMA, DMA_CH0_CHAN_ID, (uint32_t)(uintptr_t)dma_buffer);
    DL_DMA_setTransferSize(DMA, DMA_CH0_CHAN_ID, IMU_FRAME_SIZE);
    DL_DMA_enableChannel(DMA, DMA_CH0_CHAN_ID);
}

void JY_Init(void)
{
    NVIC_DisableIRQ(DMA_INT_IRQn);
    DL_DMA_disableChannel(DMA, DMA_CH0_CHAN_ID);
    queue_read = queue_write = 0;
    queue_overrun = false;
    frame_length = 0;
    latest.valid = false;
    DL_DMA_clearInterruptStatus(DMA, DL_DMA_INTERRUPT_CHANNEL0);
    NVIC_ClearPendingIRQ(DMA_INT_IRQn);
    start_receive();
    NVIC_EnableIRQ(DMA_INT_IRQn);
}

void JY_IRQHandler(void)
{
    if (DL_DMA_getPendingInterrupt(DMA) != DL_DMA_EVENT_IIDX_DMACH0) {
        return;
    }
    DL_DMA_clearInterruptStatus(DMA, DL_DMA_INTERRUPT_CHANNEL0);

    unsigned write = queue_write;
    unsigned next = (write + 1U) % IMU_QUEUE_SIZE;
    if (next != queue_read) {
        memcpy(queue[write].bytes, dma_buffer, sizeof(dma_buffer));
        queue[write].sample_ms = get_current_ms();
        queue_write = next;
    } else {
        queue_overrun = true;
    }
    start_receive();
}

static bool take_block(IMU_Block *block)
{
    uint32_t saved_mask = __get_PRIMASK();
    __disable_irq();
    if (queue_overrun) {
        queue_read = queue_write;
        queue_overrun = false;
        frame_length = 0;
    }
    bool ready = queue_read != queue_write;
    if (ready) {
        *block = queue[queue_read];
        queue_read = (queue_read + 1U) % IMU_QUEUE_SIZE;
    }
    __set_PRIMASK(saved_mask);
    return ready;
}

static int16_t read_int16(const uint8_t *bytes)
{
    return (int16_t)((uint16_t)bytes[0] | ((uint16_t)bytes[1] << 8));
}

static bool parse_angle_frame(uint32_t sample_ms)
{
    if (frame[0] != 0x55U || frame[1] != 0x53U) {
        return false;
    }

    uint8_t sum = 0;
    for (unsigned i = 0; i < IMU_FRAME_SIZE - 1U; ++i) {
        sum += frame[i];
    }
    if (sum != frame[IMU_FRAME_SIZE - 1U]) {
        return false;
    }

    latest = (IMU_Req){
        .valid = true,
        .roll_rad = read_int16(&frame[2]) * IMU_RAW_TO_RAD,
        .pitch_rad = read_int16(&frame[4]) * IMU_RAW_TO_RAD,
        .yaw_rad = read_int16(&frame[6]) * IMU_RAW_TO_RAD * CONTROL_IMU_YAW_SIGN,
    };
    temperature_c = read_int16(&frame[8]) / 340.0f + 36.53f;
    latest_ms = sample_ms;
    return true;
}

static bool consume_blocks(void)
{
    bool updated = false;
    IMU_Block block;

    while (take_block(&block)) {
        for (unsigned i = 0; i < IMU_FRAME_SIZE; ++i) {
            frame[frame_length++] = block.bytes[i];
            if (frame_length != IMU_FRAME_SIZE) {
                continue;
            }
            if (parse_angle_frame(block.sample_ms)) {
                updated = true;
                frame_length = 0;
            } else {
                --frame_length;
                memmove(frame, &frame[1], frame_length);
            }
        }
    }
    return updated;
}

bool Get_IMU_Data(JY_DATA *out_data)
{
    if (!out_data || !consume_blocks()) {
        return false;
    }
    *out_data = (JY_DATA){
        .roll = latest.roll_rad * IMU_RAD_TO_DEG,
        .pitch = latest.pitch_rad * IMU_RAD_TO_DEG,
        .yaw = latest.yaw_rad * IMU_RAD_TO_DEG * CONTROL_IMU_YAW_SIGN,
        .temp = temperature_c,
    };
    return true;
}

IMU_Req IMU_Update(uint32_t now_ms)
{
    (void)consume_blocks();
    if ((int32_t)(now_ms - latest_ms) >= (int32_t)CONTROL_IMU_TIMEOUT_MS) {
        latest.valid = false;
    }
    return latest;
}
