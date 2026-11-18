#include "KEY.h"
#include <stdio.h>
#include "emm42.h"
#include "ti_msp_dl_config.h"


void KEY_Init(void)
{
    DL_GPIO_clearInterruptStatus(KEY_DEBUG_KEY_PORT, KEY_DEBUG_KEY_PIN);
    DL_GPIO_clearInterruptStatus(KEY_DEBUG_Track_PORT,KEY_DEBUG_Track_PIN );

    NVIC_ClearPendingIRQ(KEY_DEBUG_GPIOA_INT_IRQN);
    NVIC_ClearPendingIRQ(KEY_DEBUG_GPIOB_INT_IRQN);

    NVIC_EnableIRQ(KEY_DEBUG_GPIOA_INT_IRQN); 
    NVIC_EnableIRQ(KEY_DEBUG_GPIOB_INT_IRQN);
}

void GROUP1_IRQHandler(void)
{
    switch (DL_Interrupt_getPendingGroup(DL_INTERRUPT_GROUP_1)) {
        case KEY_DEBUG_GPIOB_INT_IIDX:
            delay_cycles(50);
            if(!DL_GPIO_readPins(KEY_DEBUG_KEY_PORT,KEY_DEBUG_KEY_PIN))
            {
                int64_t motor_position[3];
                if(emm42_read_position(1, &motor_position[0]) &&
                    emm42_read_position(2,&motor_position[1]) &&
                    emm42_read_position(3,&motor_position[2])
                    )
                {
                    for(int i = 0;i < 3;i++)
                    {
                        float angle;
                        angle = emm42_position_to_degrees(motor_position[i]);
                        char buf[64];
                        snprintf(buf, sizeof(buf), "Motor %d POS: %f ",i+1,angle);
                        Debug_MSG(buf);
                    }
                }
                else
                {
                    Debug_MSG("Motor POS Read False");
                }
            }
            break;

        case KEY_DEBUG_GPIOA_INT_IIDX:
            delay_cycles(50);
            while(DL_GPIO_readPins(KEY_DEBUG_Track_PORT,KEY_DEBUG_Track_PIN))
            {
                DL_GPIO_setPins(Tracker_T_KEY_PORT, Tracker_T_KEY_PIN);
                delay_cycles(50);
            }
            DL_GPIO_clearPins(Tracker_T_KEY_PORT,Tracker_T_KEY_PIN);
            break;

        default:
            break;
    }
    
    
}
