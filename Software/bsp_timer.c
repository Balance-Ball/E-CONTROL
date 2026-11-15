#include "bsp_timer.h"
#include "ti/driverlib/m0p/dl_systick.h"

static volatile uint32_t sys_tick_ms = 0U;

void systick_init(void)
{
    DL_SYSTICK_init(CPUCLK_FREQ / 1000U);
    sys_tick_ms = 0U;
    DL_SYSTICK_enableInterrupt();
    DL_SYSTICK_enable();

}

void SysTick_Handler(void)
{
    ++sys_tick_ms;
}

uint32_t get_current_ms(void)
{
    return sys_tick_ms;
}

