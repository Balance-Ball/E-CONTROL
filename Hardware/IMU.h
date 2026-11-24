#ifndef HARDWARE_IMU_H
#define HARDWARE_IMU_H

#include <stdint.h>
#include <stdbool.h>


//数据结构体
typedef struct
{
    float roll;
    float pitch;
    float yaw;
    float temp;
} JY_DATA;

typedef struct
{
    bool valid;
    float yaw_rad;     /* 向左转为正 */
    float roll_rad;
    float pitch_rad;
} IMU_Req;

void JY_Init(void);
void JY_IRQHandler(void);
bool Get_IMU_Data(JY_DATA *out_data);
IMU_Req IMU_Update(uint32_t now_ms);

#endif
