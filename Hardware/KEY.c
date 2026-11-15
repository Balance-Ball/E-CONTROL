#include "KEY.h"
#include <stdio.h>
#include "emm42.h"

void KEY_Init(void)
{
    DL_GPIO_clearInterruptStatus(KEY_DEBUG_PORT, KEY_DEBUG_KEY_PIN);

    NVIC_ClearPendingIRQ(KEY_DEBUG_INT_IRQN);
    NVIC_EnableIRQ(KEY_DEBUG_INT_IRQN); 
}

void GROUP1_IRQHandler(void)
{
    delay_cycles(50);
    if(!DL_GPIO_readPins(KEY_DEBUG_PORT,KEY_DEBUG_KEY_PIN))
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
}