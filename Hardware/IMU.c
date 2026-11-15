#include "IMU.h"
#include "ti_msp_dl_config.h"

static uint8_t imu_rx_buf[11];
static volatile JY_DATA imu_data;
static volatile bool data_ready = false;

static bool JY_Parse(uint8_t *buf, JY_DATA *out_data);
static void JY_StartReceive(void);

void JY_Init(void)
{
    NVIC_DisableIRQ(DMA_INT_IRQn);
    DL_DMA_disableChannel(DMA, DMA_CH0_CHAN_ID);
    data_ready = false;
    DL_DMA_clearInterruptStatus(DMA, DL_DMA_INTERRUPT_CHANNEL0);
    JY_StartReceive();
    NVIC_EnableIRQ(DMA_INT_IRQn);
}

bool Get_IMU_Data(JY_DATA *out_data)
{
    if (out_data == NULL) return false;

    uint32_t saved_mask = __get_PRIMASK();
    __disable_irq();

    bool ready = data_ready;
    if(ready)
    {
        *out_data = imu_data;
        data_ready = false;
    }

    __set_PRIMASK(saved_mask);
    return ready;
}

void JY_IRQHandler(void)
{
    uint32_t pending = DL_DMA_getPendingInterrupt(DMA);

    if(pending == DL_DMA_EVENT_IIDX_DMACH0)
    {
        DL_DMA_clearInterruptStatus(DMA, DL_DMA_INTERRUPT_CHANNEL0);

        JY_DATA parsed;
        if(JY_Parse(imu_rx_buf,&parsed))
        {
        imu_data = parsed;
        data_ready = true;
        }

    JY_StartReceive();

    }
}


//私有函数

static void JY_StartReceive(void)
{
    DL_DMA_setSrcAddr(DMA, DMA_CH0_CHAN_ID, (uint32_t)&IMU_INST->RXDATA);
    //设置目的地址及长度
    DL_DMA_setDestAddr(DMA, DMA_CH0_CHAN_ID, (uint32_t)imu_rx_buf);
    DL_DMA_setTransferSize(DMA, DMA_CH0_CHAN_ID, 11); 
    //使能DMA
    DL_DMA_enableChannel(DMA, DMA_CH0_CHAN_ID);
}

static bool JY_Parse(uint8_t *buf,JY_DATA *out_data)
{
    //核对包头
    if(buf[0] != 0x55 || buf[1] != 0x53) return false;

    //核对校验和
    uint8_t sum = 0;
    for(int i = 0;i < 10;i++) sum+=buf[i];
    if(sum != buf[10]) return false;

    //拼接字节
    int16_t raw_roll  = (int16_t)((buf[3] << 8) | buf[2]);
    int16_t raw_pitch = (int16_t)((buf[5] << 8) | buf[4]);
    int16_t raw_yaw   = (int16_t)((buf[7] << 8) | buf[6]);
    int16_t raw_temp  = (int16_t)((buf[9] << 8) | buf[8]);

     //转换物理单位
    out_data->roll  = raw_roll * 0.005493164f;
    out_data->pitch = raw_pitch * 0.005493164f;
    out_data->yaw   = raw_yaw * 0.005493164f;
    out_data->temp  = (raw_temp / 340.0f) + 36.53f;    

    return true;
}