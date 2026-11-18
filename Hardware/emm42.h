#ifndef HARDWARE_EMM42_H_
#define HARDWARE_EMM42_H_

#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/*
 * Emm42 V5.0, MSPM0G3507 UART1, TTL 115200 / 8N1.
 * Manual: https://blog.csdn.net/zhangdatou666/article/details/132644047
 *
 * 接线: PB6 (UART1_TX) -> 电机 R，PB7 (UART1_RX) <- 电机 T，共地。
 * 本驱动适用于 TTL 接口；RS232/RS485 型号需要对应收发器。
 * 电机菜单: P_Serial=UART_FUN, UartBaud=115200, 五台 ID_Addr 分别为 1..5,
 *           Checksum=0x6B, Response=Receive, S_Vel_IS=Disable。
 * 1..3 为平衡球电机，4 为左轮，5 为右轮；地址需要在各驱动器菜单中设置。
 * 默认前进=CW，后退=CCW；实际机械方向反向时修改 EMM42_FORWARD_DIRECTION。
 *
 * 示例（main.c）:
 *   #include "ti_msp_dl_config.h"
 *   #include "Hardware/emm42.h"
 *
 *   SYSCFG_DL_init();
 *   emm42_init(EMM42_MOTOR_ALL);          // UART 初始化，无电机动作
 *   if (emm42_enable(EMM42_MOTOR_ALL) && forward(EMM42_MOTOR_LEFT, 100U)) {
 *       // 五台已确认使能，左轮已确认接收 100 RPM 前进命令。
 *   } else {
 *       emm42_error_t error = emm42_get_last_error(EMM42_MOTOR_ALL);
 *       (void)error;                     // 在调试器中检查失败原因
 *   }
 *   // forward(EMM42_MOTOR_RIGHT, 80U); // 右轮单独调速；方向沿用全局定义
 *   // emm42_stop(EMM42_MOTOR_BALANCE); // 仅停止平衡球电机 1..3
 *   // emm42_stop(EMM42_MOTOR_WHEELS);  // 仅停止左右轮 4、5
 *   // emm42_stop(EMM42_MOTOR_ALL);      // 全部立即停止，保持使能
 *   // emm42_disable(EMM42_MOTOR_ALL);   // 全部关闭使能，释放保持力矩
 *
 * 相对位置与反馈示例（在主循环中分阶段执行，不要每次循环重复发送运动）：
 *   // 平台处于水平时，保存该电机的基准；三台分别记录。
 *   int64_t horizontal;
 *   bool horizontal_valid = emm42_read_position(EMM42_MOTOR_1, &horizontal);
 *   // 1.8°/16细分示例：CW 转 800 脉冲，即 90°；100 RPM，加速度档位 10。
 *   bool accepted = emm42_move_relative(EMM42_MOTOR_1, EMM42_DIRECTION_CW,
 *                                       100U, 10U, 800U);
 *   // 后续周期查询；accepted 不代表运动完成。
 *   int64_t position, error;
 *   emm42_status_t status;
 *   if (horizontal_valid && emm42_read_position(EMM42_MOTOR_1, &position)) {
 *       float relative_deg = emm42_position_to_degrees(position - horizontal);
 *       (void)relative_deg;             // 相对水平基准的电机轴角度
 *   }
 *   if (emm42_read_position_error(EMM42_MOTOR_1, &error)) {
 *       float error_deg = emm42_position_to_degrees(error);
 *       (void)error_deg;
 *   }
 *   if (emm42_read_status(EMM42_MOTOR_1, &status)) {
 *       // 检查 status.enabled / reached / stalled / stall_protected。
 *   }
 *   (void)accepted;
 *
 * 初始化、运动、使能、停止及加速度设置支持单台 1..5 或 BALANCE/WHEELS/ALL 分组。
 * 读取接口只接受单台 1..5（含 LEFT/RIGHT 别名），不接受分组。
 * 分组按地址升序逐台发送相同参数并等待应答；某台失败仍继续处理组内其余电机。
 * 左右轮需要不同转速时，分别使用 EMM42_MOTOR_LEFT 和 EMM42_MOTOR_RIGHT。
 * bool 返回值仅在所有选中电机成功时为 true；这不是同步启动或广播命令。
 * 每台分别保存初始化状态、加速度和最后错误；全部操作的等待时间会累加。
 *
 * 动作成功=true 仅表示收到 02 应答，不保证到达目标速度或位置；读取成功表示获得有效数据。
 * 失败=false，可能是超时、电机拒绝、无效应答、UART 错误或参数错误。
 * 超时不能证明电机没有执行命令；本驱动不会自动重发。
 *
 * UART1 由本驱动独占，使用轮询收发，不使用中断、SysTick 或 DMA。
 * 仅在主循环调用，不要在中断中调用，也不要并发调用。
 * 默认等待预算约 30ms/阶段，实际耗时受轮询开销和中断影响。
 * 不适合直接放入高频平衡控制中断。
 */
#define EMM42_MOTOR_ALL             (0U)       /* 1..5，软件逐台展开 */
#define EMM42_MOTOR_1               (1U)
#define EMM42_MOTOR_2               (2U)
#define EMM42_MOTOR_3               (3U)
#define EMM42_MOTOR_4               (4U)
#define EMM42_MOTOR_5               (5U)
#define EMM42_MOTOR_LEFT            EMM42_MOTOR_4
#define EMM42_MOTOR_RIGHT           EMM42_MOTOR_5
#define EMM42_MOTOR_BALANCE         (0xFEU)    /* 平衡球电机 1..3 */
#define EMM42_MOTOR_WHEELS          (0xFFU)    /* 左右轮电机 4、5 */
#define EMM42_MOTOR_COUNT           (5U)
#define EMM42_MAX_SPEED_RPM         (3000U)    /* 按实际电机能力调整 */
#define EMM42_FORWARD_DIRECTION     (0U)       /* 0=CW, 1=CCW */
#define EMM42_DEFAULT_ACCELERATION  (0U)       /* 0=立即调速, 1..255=曲线 */
#define EMM42_TIMEOUT_MS            (30U)

/* UART 和引脚使用 SysConfig 的 EMM42 配置，当前为 UART1 / PB6、PB7。 */

typedef enum {
    EMM42_ERROR_NONE = 0,
    EMM42_ERROR_NOT_INITIALIZED,
    EMM42_ERROR_INVALID_ARGUMENT,
    EMM42_ERROR_TIMEOUT,
    EMM42_ERROR_REJECTED,       /* E2: 如未使能、堵转保护 */
    EMM42_ERROR_BAD_COMMAND,    /* 00 EE: 电机报告错误命令 */
    EMM42_ERROR_RESPONSE,
    EMM42_ERROR_UART
} emm42_error_t;

/* 协议物理方向，不受 EMM42_FORWARD_DIRECTION 的前进/后退定义影响。 */
typedef enum {
    EMM42_DIRECTION_CW = 0,
    EMM42_DIRECTION_CCW = 1
} emm42_direction_t;

typedef struct {
    bool enabled;
    bool reached;
    bool stalled;
    bool stall_protected;
} emm42_status_t;

/* 在 SYSCFG_DL_init() 之后调用；首次初始化共享 UART，重置选中电机的软件状态。
 * 不发送运动命令，不改变其他电机的初始化状态、加速度和错误。 */
void emm42_init(uint8_t motor);

/* 速度单位 RPM，范围 0..EMM42_MAX_SPEED_RPM；0 为速度模式减速至零。 */
bool forward(uint8_t motor, uint16_t speed_rpm);
bool backward(uint8_t motor, uint16_t speed_rpm);

/* 0xFD 相对运动：速度 1..MAX RPM，加速度 0..255，脉冲数 0..UINT32_MAX。
 * 1.8° 电机、16 细分时每圈 3200 脉冲；随实际步距角/细分变化。
 * acceleration 直接用于本次命令，不改变 set_acceleration 保存的速度模式设置。
 * 分组逐台立即启动，不是同步运动。true 为接收确认，请用 read_status 检查到位。 */
bool emm42_move_relative(uint8_t motor, emm42_direction_t direction,
                         uint16_t speed_rpm, uint8_t acceleration,
                         uint32_t pulses);

/* 以下读取仅接受单台编号 1..5 和非 NULL 输出指针；失败不修改输出。
 * 分组读取不发送命令，将所选组的最后错误设为 INVALID_ARGUMENT。
 * 位置/误差原始单位为 65536/圈（不是运动脉冲数），符号 + 32 位幅值
 * 用 int64_t 完整保存；换算成 float 角度时，大数值可能损失精度。
 * 0x36 读取实际位置；0x37 读取位置误差（此协议不是 0x39）。
 * 水平基准可由应用在平台水平时读取保存；断电保存需应用另行实现。 */
bool emm42_read_position(uint8_t motor, int64_t *position);
bool emm42_read_position_error(uint8_t motor, int64_t *error);
float emm42_position_to_degrees(int64_t position);

/* 0x3A 一次读取使能、到位、堵转、堵转保护；此协议没有单独的 0x3E 堵转查询。
 * 返回值代表通信成功；enabled/stalled 为 false 也可能是正常读取结果。
 * 各便捷接口均实际查询电机，不使用之前使能命令的软件缓存。 */
bool emm42_read_status(uint8_t motor, emm42_status_t *status);
bool emm42_read_enabled(uint8_t motor, bool *enabled);
bool emm42_read_stall(uint8_t motor, bool *stalled);

bool emm42_enable(uint8_t motor);
bool emm42_disable(uint8_t motor);
bool emm42_stop(uint8_t motor);

/* 设置后续速度命令的加速度档位，不向电机发送命令。 */
void emm42_set_acceleration(uint8_t motor, uint8_t acceleration);
/* 分组返回所选范围内按编号找到的首个非 NONE 错误，全部成功则为 NONE。
 * 无效选择不发送命令，将五台的最后错误设为 INVALID_ARGUMENT；
 * get_last_error(无效编号) 直接返回 INVALID_ARGUMENT，不改变已有错误。 */
emm42_error_t emm42_get_last_error(uint8_t motor);

#ifdef __cplusplus
}
#endif
#endif /* HARDWARE_EMM42_H_ */
