#include "ti_msp_dl_config.h"
#include "emm42.h"
#include <stddef.h>
#include <string.h>

#ifndef EMM42_INST
#error "Configure a UART named EMM42 in SysConfig first"
#endif
#if EMM42_BAUD_RATE != 115200
#error "Set the EMM42 UART baud rate to 115200 in SysConfig"
#endif

#define EMM42_POLL_US        (10U)
#define EMM42_WAIT_STEPS     (EMM42_TIMEOUT_MS * 1000U / EMM42_POLL_US)
#define EMM42_CHECKSUM       (0x6BU)
#define EMM42_UART_ERRORS    (DL_UART_ERROR_OVERRUN | DL_UART_ERROR_BREAK | \
                              DL_UART_ERROR_PARITY | DL_UART_ERROR_FRAMING)

#if EMM42_FORWARD_DIRECTION > 1U
#error "EMM42_FORWARD_DIRECTION must be 0 (CW) or 1 (CCW)"
#endif
#if EMM42_MAX_SPEED_RPM > 65535U || EMM42_DEFAULT_ACCELERATION > 255U
#error "Invalid EMM42 speed or acceleration configuration"
#endif
#if EMM42_TIMEOUT_MS < 1U || EMM42_TIMEOUT_MS > 1000U
#error "EMM42_TIMEOUT_MS must be in 1..1000"
#endif

static bool s_uart_initialized = false;
static bool s_initialized[EMM42_MOTOR_COUNT] = {false, false, false};
static uint8_t s_acceleration[EMM42_MOTOR_COUNT] = {
    EMM42_DEFAULT_ACCELERATION, EMM42_DEFAULT_ACCELERATION,
    EMM42_DEFAULT_ACCELERATION
};
static emm42_error_t s_last_error[EMM42_MOTOR_COUNT] = {
    EMM42_ERROR_NOT_INITIALIZED, EMM42_ERROR_NOT_INITIALIZED,
    EMM42_ERROR_NOT_INITIALIZED
};

/* 返回选中电机的地址范围；ALL 只在软件中展开，不使用广播应答。 */
static bool emm42_select(uint8_t motor, uint8_t *first, uint8_t *last)
{
    uint8_t i;
    if (motor > EMM42_MOTOR_COUNT) {
        for (i = 0U; i < EMM42_MOTOR_COUNT; ++i) {
            s_last_error[i] = EMM42_ERROR_INVALID_ARGUMENT;
        }
        return false;
    }
    *first = motor == EMM42_MOTOR_ALL ? EMM42_MOTOR_1 : motor;
    *last = motor == EMM42_MOTOR_ALL ? EMM42_MOTOR_3 : motor;
    return true;
}

static void emm42_poll_delay(void)
{
    delay_cycles((CPUCLK_FREQ / 1000000U) * EMM42_POLL_US);
}

static bool emm42_fail(uint8_t motor, emm42_error_t error)
{
    s_last_error[motor - 1U] = error;
    return false;
}

/* 所有等待都有预算，UART 故障或持续噪声也不会无限卡住。 */
static bool emm42_wait_tx_idle(uint8_t motor)
{
    uint32_t remaining = EMM42_WAIT_STEPS;

    while (DL_UART_Main_isBusy(EMM42_INST)) {
        if (remaining == 0U) {
            return emm42_fail(motor, EMM42_ERROR_TIMEOUT);
        }
        --remaining;
        emm42_poll_delay();
    }
    return true;
}

static bool emm42_discard_rx(uint8_t motor)
{
    uint32_t remaining = EMM42_WAIT_STEPS;

    while (!DL_UART_Main_isRXFIFOEmpty(EMM42_INST)) {
        if (remaining == 0U) {
            return emm42_fail(motor, EMM42_ERROR_UART);
        }
        --remaining;
        (void)DL_UART_Main_receiveData(EMM42_INST);
        emm42_poll_delay();
    }
    return true;
}

/* payload=NULL 表示动作 ACK；否则读取固定长度数据（状态 1 字节、位置 5 字节）。 */
static bool emm42_wait_response(uint8_t motor, uint8_t command,
                                uint8_t *payload, uint8_t payload_length)
{
    uint8_t window[8];
    uint8_t received = 0U;
    uint32_t remaining = EMM42_WAIT_STEPS;
    bool invalid_response = false;

    while (remaining > 0U) {
        --remaining;
        if (!DL_UART_Main_isRXFIFOEmpty(EMM42_INST)) {
            /* RXDATA 必须只读一次，读取数据和错误标志都会弹出 FIFO。 */
            uint32_t raw = EMM42_INST->RXDATA;
            if ((raw & EMM42_UART_ERRORS) != 0U) {
                return emm42_fail(motor, EMM42_ERROR_UART);
            }

            window[received++] = (uint8_t)(raw & UART_RXDATA_DATA_MASK);
            /* 按帧头和长度解析，避免把位置数据中的 00 EE 6B 当作错误帧。 */
            while (received >= 2U) {
                if (window[0] == motor &&
                    (window[1] == command || window[1] == 0x00U)) {
                    uint8_t expected = window[1] == command ?
                                       (uint8_t)(payload_length + 3U) : 4U;
                    if (received < expected) {
                        break;
                    }
                    if (window[expected - 1U] == EMM42_CHECKSUM) {
                        if (window[1] == 0x00U && window[2] == 0xEEU) {
                            return emm42_fail(motor, EMM42_ERROR_BAD_COMMAND);
                        }
                        if (window[1] == command) {
                            if (payload == NULL && window[2] == 0xE2U) {
                                return emm42_fail(motor, EMM42_ERROR_REJECTED);
                            }
                            if ((payload == NULL && window[2] == 0x02U) ||
                                (payload != NULL &&
                                 (payload_length != 5U || window[2] <= 1U))) {
                                if (payload != NULL) {
                                    memcpy(payload, &window[2], payload_length);
                                }
                                s_last_error[motor - 1U] = EMM42_ERROR_NONE;
                                return true;
                            }
                        }
                    }
                    invalid_response = true;
                }
                /* 丢掉一个字节重新寻找帧头，跳过噪声及损坏的应答。 */
                --received;
                memmove(window, &window[1], received);
            }
        }
        emm42_poll_delay();
    }

    return emm42_fail(motor, invalid_response ? EMM42_ERROR_RESPONSE :
                                          EMM42_ERROR_TIMEOUT);
}

static bool emm42_exchange(const uint8_t *frame, uint8_t length,
                           uint8_t *payload, uint8_t payload_length)
{
    uint32_t remaining = EMM42_WAIT_STEPS;
    uint8_t i;
    uint8_t motor = frame[0];

    if (!s_initialized[motor - 1U]) {
        return emm42_fail(motor, EMM42_ERROR_NOT_INITIALIZED);
    }
    /* 在发新命令前清理已有数据，避免把缓冲区中的旧应答当成本次成功。 */
    if (!emm42_wait_tx_idle(motor) || !emm42_discard_rx(motor)) {
        return false;
    }

    for (i = 0U; i < length; ++i) {
        while (DL_UART_Main_isTXFIFOFull(EMM42_INST)) {
            if (remaining == 0U) {
                return emm42_fail(motor, EMM42_ERROR_TIMEOUT);
            }
            --remaining;
            emm42_poll_delay();
        }
        DL_UART_Main_transmitData(EMM42_INST, frame[i]);
    }

    if (!emm42_wait_tx_idle(motor)) {
        return false;
    }
    return emm42_wait_response(motor, frame[1], payload, payload_length);
}

/* acceleration 非空时，为每台电机填入独立的加速度设置。 */
static bool emm42_send_selected(uint8_t motor, uint8_t *frame, uint8_t length,
                                uint8_t *acceleration)
{
    uint8_t first, last, address;
    bool success = true;

    if (!emm42_select(motor, &first, &last)) {
        return false;
    }
    for (address = first; address <= last; ++address) {
        frame[0] = address;
        if (acceleration != NULL) {
            *acceleration = s_acceleration[address - 1U];
        }
        /* 不短路：即使一台失败，也尝试处理其他选中的电机。 */
        if (!emm42_exchange(frame, length, NULL, 1U)) {
            success = false;
        }
    }
    return success;
}

void emm42_init(uint8_t motor)
{
    uint8_t first, last, address;
    if (!emm42_select(motor, &first, &last)) {
        return;
    }

    if (!s_uart_initialized) {
        /* SYSCFG_DL_init() 已完成 UART 时钟、引脚及 115200/8N1 配置。 */
        NVIC_DisableIRQ(EMM42_INST_INT_IRQN);
        DL_UART_Main_disableInterrupt(EMM42_INST, 0xFFFFFFFFU);
        DL_UART_Main_disable(EMM42_INST);
        DL_UART_Main_enableFIFOs(EMM42_INST);
        DL_UART_Main_enable(EMM42_INST);
        NVIC_ClearPendingIRQ(EMM42_INST_INT_IRQN);
        s_uart_initialized = true;
    }

    for (address = first; address <= last; ++address) {
        s_acceleration[address - 1U] = EMM42_DEFAULT_ACCELERATION;
        s_last_error[address - 1U] = EMM42_ERROR_NONE;
        s_initialized[address - 1U] = true;
    }
}

static bool emm42_speed(uint8_t motor, uint8_t direction, uint16_t speed_rpm)
{
    uint8_t frame[8] = {
        0U, 0xF6U, direction,
        (uint8_t)(speed_rpm >> 8), (uint8_t)speed_rpm,
        0U, 0x00U, EMM42_CHECKSUM
    };
    uint8_t first, last, address;

    if (!emm42_select(motor, &first, &last)) {
        return false;
    }
    if (speed_rpm > EMM42_MAX_SPEED_RPM) {
        for (address = first; address <= last; ++address) {
            (void)emm42_fail(address, EMM42_ERROR_INVALID_ARGUMENT);
        }
        return false;
    }
    return emm42_send_selected(motor, frame, (uint8_t)sizeof(frame), &frame[5]);
}

bool forward(uint8_t motor, uint16_t speed_rpm)
{
    return emm42_speed(motor, EMM42_FORWARD_DIRECTION, speed_rpm);
}

bool backward(uint8_t motor, uint16_t speed_rpm)
{
    return emm42_speed(motor, EMM42_FORWARD_DIRECTION ^ 1U, speed_rpm);
}

bool emm42_move_relative(uint8_t motor, emm42_direction_t direction,
                         uint16_t speed_rpm, uint8_t acceleration,
                         uint32_t pulses)
{
    uint8_t first, last, address;
    uint8_t frame[13] = {
        0U, 0xFDU, (uint8_t)direction,
        (uint8_t)(speed_rpm >> 8), (uint8_t)speed_rpm, acceleration,
        (uint8_t)(pulses >> 24), (uint8_t)(pulses >> 16),
        (uint8_t)(pulses >> 8), (uint8_t)pulses,
        0x00U, 0x00U, EMM42_CHECKSUM
    };
    if (!emm42_select(motor, &first, &last)) {
        return false;
    }
    if ((direction != EMM42_DIRECTION_CW && direction != EMM42_DIRECTION_CCW) ||
        speed_rpm == 0U || speed_rpm > EMM42_MAX_SPEED_RPM) {
        for (address = first; address <= last; ++address) {
            (void)emm42_fail(address, EMM42_ERROR_INVALID_ARGUMENT);
        }
        return false;
    }
    return emm42_send_selected(motor, frame, (uint8_t)sizeof(frame), NULL);
}

/* 读取只接受单台电机，避免 ALL 的多个结果写入同一输出指针。 */
static bool emm42_read(uint8_t motor, uint8_t command, void *output,
                        uint8_t *payload, uint8_t payload_length)
{
    uint8_t first, last;
    uint8_t frame[3] = {motor, command, EMM42_CHECKSUM};
    if (!emm42_select(motor, &first, &last)) {
        return false;
    }
    if (motor == EMM42_MOTOR_ALL || output == NULL) {
        uint8_t address;
        for (address = first; address <= last; ++address) {
            (void)emm42_fail(address, EMM42_ERROR_INVALID_ARGUMENT);
        }
        return false;
    }
    return emm42_exchange(frame, (uint8_t)sizeof(frame), payload, payload_length);
}

static bool emm42_read_signed_position(uint8_t motor, uint8_t command,
                                       int64_t *position)
{
    uint8_t payload[5];
    uint32_t magnitude;
    if (!emm42_read(motor, command, position, payload, (uint8_t)sizeof(payload))) {
        return false;
    }
    magnitude = ((uint32_t)payload[1] << 24) | ((uint32_t)payload[2] << 16) |
                ((uint32_t)payload[3] << 8) | (uint32_t)payload[4];
    /* 协议为独立符号 + uint32 幅值，int64 避免超过 INT32_MAX 后溢出。 */
    *position = payload[0] == 0U ? (int64_t)magnitude : -(int64_t)magnitude;
    return true;
}

bool emm42_read_position(uint8_t motor, int64_t *position)
{
    return emm42_read_signed_position(motor, 0x36U, position);
}

bool emm42_read_position_error(uint8_t motor, int64_t *error)
{
    return emm42_read_signed_position(motor, 0x37U, error);
}

float emm42_position_to_degrees(int64_t position)
{
    return (float)position * (360.0f / 65536.0f);
}

bool emm42_read_status(uint8_t motor, emm42_status_t *status)
{
    uint8_t payload;
    if (!emm42_read(motor, 0x3AU, status, &payload, 1U)) {
        return false;
    }
    status->enabled = (payload & 0x01U) != 0U;
    status->reached = (payload & 0x02U) != 0U;
    status->stalled = (payload & 0x04U) != 0U;
    status->stall_protected = (payload & 0x08U) != 0U;
    return true;
}

bool emm42_read_enabled(uint8_t motor, bool *enabled)
{
    emm42_status_t status;
    if (enabled == NULL) {
        uint8_t payload;
        return emm42_read(motor, 0x3AU, NULL, &payload, 1U);
    }
    if (!emm42_read_status(motor, &status)) {
        return false;
    }
    *enabled = status.enabled;
    return true;
}

bool emm42_read_stall(uint8_t motor, bool *stalled)
{
    emm42_status_t status;
    if (stalled == NULL) {
        uint8_t payload;
        return emm42_read(motor, 0x3AU, NULL, &payload, 1U);
    }
    if (!emm42_read_status(motor, &status)) {
        return false;
    }
    *stalled = status.stalled;
    return true;
}

static bool emm42_set_enabled(uint8_t motor, bool enable)
{
    uint8_t frame[6] = {
        0U, 0xF3U, 0xABU,
        enable ? 0x01U : 0x00U, 0x00U, EMM42_CHECKSUM
    };
    return emm42_send_selected(motor, frame, (uint8_t)sizeof(frame), NULL);
}

bool emm42_enable(uint8_t motor)
{
    return emm42_set_enabled(motor, true);
}

bool emm42_disable(uint8_t motor)
{
    return emm42_set_enabled(motor, false);
}

bool emm42_stop(uint8_t motor)
{
    uint8_t frame[5] = {
        0U, 0xFEU, 0x98U, 0x00U, EMM42_CHECKSUM
    };
    return emm42_send_selected(motor, frame, (uint8_t)sizeof(frame), NULL);
}

void emm42_set_acceleration(uint8_t motor, uint8_t acceleration)
{
    uint8_t first, last, address;
    if (!emm42_select(motor, &first, &last)) {
        return;
    }
    for (address = first; address <= last; ++address) {
        s_acceleration[address - 1U] = acceleration;
        s_last_error[address - 1U] = EMM42_ERROR_NONE;
    }
}

emm42_error_t emm42_get_last_error(uint8_t motor)
{
    uint8_t address;
    if (motor > EMM42_MOTOR_COUNT) {
        return EMM42_ERROR_INVALID_ARGUMENT;
    }
    if (motor != EMM42_MOTOR_ALL) {
        return s_last_error[motor - 1U];
    }
    for (address = EMM42_MOTOR_1; address <= EMM42_MOTOR_3; ++address) {
        if (s_last_error[address - 1U] != EMM42_ERROR_NONE) {
            return s_last_error[address - 1U];
        }
    }
    return EMM42_ERROR_NONE;
}
