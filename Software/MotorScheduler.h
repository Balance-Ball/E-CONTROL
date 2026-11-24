#ifndef SOFTWARE_MOTOR_SCHEDULER_H
#define SOFTWARE_MOTOR_SCHEDULER_H

#include "Hardware/IMU.h"
#include "Hardware/Track8.h"
#include "LQR.h"

/* SYSCFG_DL_init 之后调用一次。返回启动命令是否全部发完。 */
bool Motor_Initial(void);

/* 由主循环的按钮处理调用。下一次调度时执行新的开关状态。 */
void Motor_SetTrackingEnabled(bool enabled);

/* true 表示本轮所有应发动作均已发送，包含无变化的周期。 */
bool Motor_Update(uint32_t now_ms, const IMU_Req *imu,
                  const Track_Req *track, const LQR_Req *ball);

#endif
