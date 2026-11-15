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

void JY_Init(void);
bool JY_Enable(void);
void JY_IRQHandler(void);
bool Get_IMU_Data(JY_DATA *out_data);

#endif