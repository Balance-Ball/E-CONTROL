/*
 * Copyright (c) 2023, Texas Instruments Incorporated
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * *  Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 *
 * *  Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * *  Neither the name of Texas Instruments Incorporated nor the names of
 *    its contributors may be used to endorse or promote products derived
 *    from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO,
 * THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS;
 * OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
 * WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR
 * OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE,
 * EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "ti_msp_dl_config.h"
#include <stdio.h>
#include "Software/Debug.h"
#include "Hardware/emm42.h"
#include "Hardware/IMU.h"
#include "Hardware/OLED.h"
#include "Hardware/vision.h"
#include "Software/bsp_timer.h"
#include "Hardware/KEY.h"
#include "Software/LQR.h"

#define IMU_READ_MS 100
uint32_t last_read_time = 0;
// JY_DATA current_IMU;

void DMA_IRQHandler(void);



int main(void)
{
    
    /*
        初始化
    */
    SYSCFG_DL_init();
    systick_init();
    vision_init();
    OLED_Init();
    OLED_Write(1, 1, "[Motor]");
    OLED_Write(2,1,"[IMU]");
    KEY_Init();

    // //陀螺仪初始化
    // JY_Init();

    //电机初始化&使能
    Debug_MSG("init motor for first time\r\n");
    emm42_init(EMM42_MOTOR_ALL);
    for(int i = 1;;i++)
    {
        if(emm42_enable(EMM42_MOTOR_ALL))
        {
            OLED_Write(1,8,"Inited");
            Debug_MSG("motor inited\r\n");
            break;
        }
        OLED_Write(1,8,"Retrying");
        char buf[64];
        snprintf(buf,sizeof(buf), "Retrying Motor for the %d time\r\n", i);
        Debug_MSG(buf);
    }
    emm42_enable(EMM42_MOTOR_ALL);

    
    

    while (1) {
        // //IMU每一百毫秒读取一次（10HZ）数据
        // uint32_t current_time = get_current_ms();

        // if(current_time - last_read_time >= IMU_READ_MS)
        // {
        //     last_read_time = current_time;

        //     if(Get_IMU_Data(&current_IMU))
        //     {
        //         /*
        //         IMU处理函数
        //         */
        //     }            
        // }
        
        vision_data current_data;
        if(Get_Vision_data(&current_data))
        {
            uint32_t cur_time = get_current_ms();
            int16_t dx = current_data.dx;
            int16_t dy = current_data.dy;

            LQR(dx, dy, cur_time);
        }

    }
}


void DMA_IRQHandler(void)
{
    JY_IRQHandler();
}
