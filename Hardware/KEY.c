#include "KEY.h"

#include "ti_msp_dl_config.h"

static volatile bool position_requested;

void KEY_Init(void)
{
    DL_GPIO_clearInterruptStatus(KEY_DEBUG_KEY_PORT, KEY_DEBUG_KEY_PIN);
    DL_GPIO_clearInterruptStatus(KEY_DEBUG_Track_PORT, KEY_DEBUG_Track_PIN);

    /* 调试按键电平直接传给循迹模块的 T_KEY，按住期间中断可正常返回。 */
    DL_GPIO_setUpperPinsPolarity(KEY_DEBUG_Track_PORT, DL_GPIO_PIN_18_EDGE_RISE_FALL);
    NVIC_ClearPendingIRQ(KEY_DEBUG_GPIOA_INT_IRQN);
    NVIC_ClearPendingIRQ(KEY_DEBUG_GPIOB_INT_IRQN);
    NVIC_EnableIRQ(KEY_DEBUG_GPIOA_INT_IRQN);
    NVIC_EnableIRQ(KEY_DEBUG_GPIOB_INT_IRQN);
}

bool KEY_TakePositionRequest(void)
{
    uint32_t saved_mask = __get_PRIMASK();
    __disable_irq();
    bool pending = position_requested;
    position_requested = false;
    __set_PRIMASK(saved_mask);
    return pending;
}

void GROUP1_IRQHandler(void)
{
    switch (DL_Interrupt_getPendingGroup(DL_INTERRUPT_GROUP_1)) {
        case KEY_DEBUG_GPIOB_INT_IIDX:
            DL_GPIO_clearInterruptStatus(KEY_DEBUG_KEY_PORT, KEY_DEBUG_KEY_PIN);
            position_requested = true;
            break;

        case KEY_DEBUG_GPIOA_INT_IIDX:
            DL_GPIO_clearInterruptStatus(KEY_DEBUG_Track_PORT, KEY_DEBUG_Track_PIN);
            if (DL_GPIO_readPins(KEY_DEBUG_Track_PORT, KEY_DEBUG_Track_PIN)) {
                DL_GPIO_setPins(Tracker_T_KEY_PORT, Tracker_T_KEY_PIN);
            } else {
                DL_GPIO_clearPins(Tracker_T_KEY_PORT, Tracker_T_KEY_PIN);
            }
            break;

        default:
            break;
    }
}
