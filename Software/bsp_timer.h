#ifndef BSP_TIMER_H
#define BSP_TIMER_H

#include "ti_msp_dl_config.h"

uint32_t get_current_ms(void);
void systick_init(void);
void SysTick_Handler(void);

#endif 