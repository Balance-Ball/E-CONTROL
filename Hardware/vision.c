#include "ti_msp_dl_config.h"
#include "vision.h"
#include <stddef.h>

static uint8_t rx_buf[frame_size];
static uint8_t rx_cnt = 0;

static volatile vision_data latest_data;
static volatile bool data_ready = false;

static int16_t READ_INT16_LE(const uint8_t *p);
static void Vision_Receive(uint8_t byte);

void vision_init(void)
{
    NVIC_DisableIRQ(Vision_INST_INT_IRQN);
    DL_UART_Main_disableInterrupt(Vision_INST,0xFFFFFFFFU);
    DL_UART_Main_disable(Vision_INST);

    DL_UART_Main_enableFIFOs(Vision_INST);
    DL_UART_Main_setRXFIFOThreshold(Vision_INST, DL_UART_MAIN_RX_FIFO_LEVEL_ONE_ENTRY);

    while (!DL_UART_Main_isRXFIFOEmpty(Vision_INST)) {
        (void)DL_UART_Main_receiveData(Vision_INST);
    }

    rx_cnt = 0;
    data_ready = false;

    DL_UART_Main_clearInterruptStatus(Vision_INST,0xFFFFFFFFU);
    NVIC_ClearPendingIRQ(Vision_INST_INT_IRQN);

    DL_UART_Main_enableInterrupt(Vision_INST,DL_UART_MAIN_INTERRUPT_RX);

    DL_UART_Main_enable(Vision_INST);
    NVIC_EnableIRQ(Vision_INST_INT_IRQN);

}

void Vision_INST_IRQHandler(void)
{
    DL_UART_Main_clearInterruptStatus(Vision_INST,DL_UART_MAIN_INTERRUPT_RX);

    while(!DL_UART_Main_isRXFIFOEmpty(Vision_INST))
    {
        uint32_t raw = Vision_INST->RXDATA;

        if (raw & (DL_UART_ERROR_OVERRUN |
                   DL_UART_ERROR_BREAK   |
                   DL_UART_ERROR_PARITY  |
                   DL_UART_ERROR_FRAMING))
        {
            // 出现硬件接收错误，重新寻找完整帧
            rx_cnt = 0;
            continue;
        }

        Vision_Receive((uint8_t)(raw & UART_RXDATA_DATA_MASK));

    }
}

bool Get_Vision_data(vision_data *out)
{
    if(out == NULL) return false;

    uint32_t saved_mask = __get_PRIMASK();
    __disable_irq();

    bool ready = data_ready;
    if(ready)
    {
        *out = latest_data;
        data_ready = false;
    }

    __set_PRIMASK(saved_mask);
    return ready;
}




static int16_t READ_INT16_LE(const uint8_t *p)
{
    return (int16_t)((uint16_t)p[0] | ((uint16_t)p[1] << 8));
}

static void Vision_Receive(uint8_t byte)
{
    rx_buf[rx_cnt++] = byte;
    
    if(rx_cnt < frame_size) return;

    uint8_t sum = 0;
    for(uint8_t i = 0 ; i < frame_size - 1U ; i++) sum += rx_buf[i];

    if(sum == rx_buf[7] &&
    rx_buf[0] == 0xAA   &&
    rx_buf[1] == 0x55   &&
    rx_buf[2] <= 1
    )
    {
        vision_data parsed = {0};

        parsed.head[0] = rx_buf[0];
        parsed.head[1] = rx_buf[1];
        parsed.found = (rx_buf[2] == 1);
        parsed.sum = rx_buf[7];

        if(parsed.found)
        {
            parsed.dx = READ_INT16_LE(&rx_buf[3]);
            parsed.dy = READ_INT16_LE(&rx_buf[5]);
        }

        latest_data = parsed;
        data_ready = true;

        rx_cnt = 0;
    }
    else 
    {
        for(uint8_t i = 0;i < frame_size - 1;i++) rx_buf[i] = rx_buf[i+1];
        rx_cnt = frame_size - 1;
    }
}
