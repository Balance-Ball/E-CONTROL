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


// #define CAR
// 编译整车开关

#include "ti_msp_dl_config.h"

#include "Hardware/IMU.h"
#include "Hardware/KEY.h"
#include "Hardware/OLED.h"
#include "Hardware/Track8.h"
#include "Hardware/vision.h"
#include "Software/Debug.h"
#include "Software/MotorScheduler.h"
#include "Software/bsp_timer.h"

static void initialize(void)
{
    SYSCFG_DL_init();
    systick_init();
    vision_init();
    JY_Init();
    OLED_Init();
    OLED_Write(1, 1, "[Motor]");
    OLED_Write(2, 1, "[IMU]");
    KEY_Init();

    bool sent = Motor_Initial();
    OLED_Write(1, 8, sent ? "Inited" : "TX fail");
    Debug_MSG(sent ? "Motor startup commands sent\r\n" : "Motor startup TX failed\r\n");
}

int main(void)
{
    initialize();

    while (1) {

        const uint32_t now_ms = get_current_ms();
        
        #if defined (CAR)
        const IMU_Req imu = IMU_Update(now_ms);
        const Track_Req track = Track_Update(now_ms);
        
        #else
        const IMU_Req imu = {0};
        const Track_Req track = {0};

        #endif
        
        const LQR_Req ball = LQR_Update(now_ms);
        Motor_Update(now_ms, &imu, &track, &ball);
    }
}

void DMA_IRQHandler(void)
{
    JY_IRQHandler();
}
