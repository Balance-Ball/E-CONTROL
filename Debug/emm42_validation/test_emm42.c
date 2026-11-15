#include <assert.h>
#include <stdio.h>
#include <string.h>
#include "ti_msp_dl_config.h"
#include "emm42.h"
MockUART mock_uart;
static unsigned init_count, frame_count, tx_size, rx_size, rx_pos;
static uint8_t frames[64][13], lengths[64];
static uint32_t rx[32];
static uint32_t position_magnitude[3];
static uint8_t position_sign[3], status_flags[3];
/* 0 success, 1 timeout, 2 rejected, 3 wrong address, 4 bad checksum,
 * 5 UART error, 6 bad command, 7 foreign response followed by success,
 * 8 invalid position sign, 9 truncated reply, 10 wrong command,
 * 11 corrupted reply followed by valid reply. */
static int response[3];
void delay_cycles(uint32_t cycles) { (void)cycles; }
void NVIC_DisableIRQ(int irq) { (void)irq; }
void NVIC_ClearPendingIRQ(int irq) { (void)irq; }
void DL_UART_Main_disableInterrupt(MockUART *u, uint32_t m) { (void)u; (void)m; }
void DL_UART_Main_disable(MockUART *u) { (void)u; ++init_count; }
void DL_UART_Main_enableFIFOs(MockUART *u) { (void)u; }
void DL_UART_Main_enable(MockUART *u) { (void)u; }
bool DL_UART_Main_isBusy(MockUART *u) { (void)u; return false; }
bool DL_UART_Main_isTXFIFOFull(MockUART *u) { (void)u; return false; }
bool DL_UART_Main_isRXFIFOEmpty(MockUART *u) {
    if (rx_pos == rx_size) return true;
    u->RXDATA = rx[rx_pos++];
    return false;
}
uint8_t DL_UART_Main_receiveData(MockUART *u) { return (uint8_t)u->RXDATA; }
void DL_UART_Main_transmitData(MockUART *u, uint8_t b) {
    (void)u;
    assert(frame_count < 64 && tx_size < 13);
    uint8_t *f = frames[frame_count];
    f[tx_size++] = b;
    if (tx_size < 2) return;
    bool read_position = f[1] == 0x36 || f[1] == 0x37;
    bool read_data = read_position || f[1] == 0x3A;
    unsigned n = f[1] == 0xFD ? 13 : f[1] == 0xF6 ? 8 :
                 f[1] == 0xF3 ? 6 : read_data ? 3 : 5;
    if (tx_size != n) return;
    assert(f[0] >= 1 && f[0] <= 3 && f[n-1] == 0x6B);
    int mode = response[f[0]-1];
    rx_pos = rx_size = 0;
    if (mode != 1) {
        if (mode == 7) {
            rx[rx_size++] = 99; rx[rx_size++] = f[1];
            rx[rx_size++] = 2; rx[rx_size++] = 0x6B;
        }
        if (mode == 11) {
            rx[rx_size++] = f[0]; rx[rx_size++] = f[1];
            rx[rx_size++] = 0; rx[rx_size++] = 0; // Bad status checksum.
        }
        rx[rx_size++] = mode == 3 ? 99 : f[0];
        rx[rx_size++] = mode == 6 ? 0 : mode == 10 ? 0x35 : f[1];
        if (mode == 6) {
            rx[rx_size++] = 0xEE;
        } else if (read_position) {
            uint32_t value = position_magnitude[f[0]-1];
            rx[rx_size++] = mode == 8 ? 2 : position_sign[f[0]-1];
            rx[rx_size++] = value >> 24;
            rx[rx_size++] = (value >> 16) & 255U;
            rx[rx_size++] = (value >> 8) & 255U;
            rx[rx_size++] = value & 255U;
        } else {
            rx[rx_size++] = read_data ? status_flags[f[0]-1] : mode == 2 ? 0xE2 : 2;
        }
        rx[rx_size++] = mode == 4 ? 0 : 0x6B;
        if (mode == 5) rx[0] |= DL_UART_ERROR_FRAMING;
        if (mode == 9) --rx_size;
    }
    lengths[frame_count++] = (uint8_t)n;
    tx_size = 0;
}
static void reset_io(void) {
    frame_count = tx_size = rx_size = rx_pos = 0;
    memset(frames, 0, sizeof(frames));
    memset(response, 0, sizeof(response));
    memset(position_magnitude, 0, sizeof(position_magnitude));
    memset(position_sign, 0, sizeof(position_sign));
    memset(status_flags, 0, sizeof(status_flags));
}
static void expect_all(uint8_t command) {
    assert(frame_count == 3);
    for (unsigned i=0; i<3; ++i) {
        assert(frames[i][0] == i+1 && frames[i][1] == command);
    }
}
int main(void) {
    int64_t position = 123;
    assert(!emm42_read_position(2, &position) && position == 123);
    assert(!emm42_enable(2));
    assert(frame_count == 0 && emm42_get_last_error(2) == EMM42_ERROR_NOT_INITIALIZED);
    emm42_init(1);
    assert(init_count == 1 && emm42_get_last_error(1) == EMM42_ERROR_NONE);
    assert(!emm42_enable(EMM42_MOTOR_ALL));
    assert(frame_count == 1 && emm42_get_last_error(2) == EMM42_ERROR_NOT_INITIALIZED);
    emm42_init(EMM42_MOTOR_ALL);
    assert(init_count == 1);
    for (uint8_t m=1; m<=3; ++m) {
        reset_io();
        assert(emm42_enable(m));
        assert(frame_count == 1 && frames[0][0] == m && frames[0][3] == 1);
        assert(emm42_disable(m));
        assert(frames[1][0] == m && frames[1][3] == 0);
        assert(emm42_stop(m));
        assert(frames[2][0] == m && frames[2][1] == 0xFE && lengths[2] == 5);
        emm42_set_acceleration(m, (uint8_t)(10*m));
        assert(forward(m, 1000));
        assert(frames[3][0] == m && frames[3][2] == 0);
        assert(frames[3][3] == 3 && frames[3][4] == 0xE8 && frames[3][5] == 10*m);
        assert(backward(m, 0));
        assert(frames[4][2] == 1 && frames[4][3] == 0 && frames[4][4] == 0);
    }
    reset_io(); assert(forward(EMM42_MOTOR_ALL, 3000)); expect_all(0xF6);
    for (unsigned i=0; i<3; ++i) assert(frames[i][5] == 10*(i+1));
    reset_io(); assert(backward(EMM42_MOTOR_ALL, 100)); expect_all(0xF6);
    reset_io(); assert(emm42_enable(EMM42_MOTOR_ALL)); expect_all(0xF3);
    reset_io(); assert(emm42_disable(EMM42_MOTOR_ALL)); expect_all(0xF3);
    reset_io(); assert(emm42_stop(EMM42_MOTOR_ALL)); expect_all(0xFE);
    emm42_set_acceleration(EMM42_MOTOR_ALL, 55);
    emm42_init(2);
    reset_io(); assert(forward(EMM42_MOTOR_ALL, 100));
    assert(frames[0][5] == 55 && frames[1][5] == 0 && frames[2][5] == 55);
    assert(init_count == 1);
    reset_io(); response[0]=1; response[1]=2;
    assert(!emm42_stop(EMM42_MOTOR_ALL)); expect_all(0xFE);
    assert(emm42_get_last_error(1) == EMM42_ERROR_TIMEOUT);
    assert(emm42_get_last_error(2) == EMM42_ERROR_REJECTED);
    assert(emm42_get_last_error(3) == EMM42_ERROR_NONE);
    assert(emm42_get_last_error(EMM42_MOTOR_ALL) == EMM42_ERROR_TIMEOUT);
    const emm42_error_t expected[] = {EMM42_ERROR_NONE, EMM42_ERROR_TIMEOUT,
        EMM42_ERROR_REJECTED, EMM42_ERROR_TIMEOUT, EMM42_ERROR_RESPONSE,
        EMM42_ERROR_UART, EMM42_ERROR_BAD_COMMAND, EMM42_ERROR_NONE};
    for (int mode=0; mode<=7; ++mode) {
        reset_io(); response[1]=mode;
        bool ok=emm42_enable(2);
        assert(ok == (expected[mode] == EMM42_ERROR_NONE));
        assert(emm42_get_last_error(2) == expected[mode]);
    }
    reset_io(); assert(!forward(EMM42_MOTOR_ALL, 3001));
    assert(frame_count == 0);
    for (uint8_t m=1; m<=3; ++m) assert(emm42_get_last_error(m) == EMM42_ERROR_INVALID_ARGUMENT);
    for (unsigned m=4; m<=255; ++m) {
        emm42_init((uint8_t)m);
        emm42_set_acceleration((uint8_t)m, 4);
        assert(!forward((uint8_t)m, 100) && !backward((uint8_t)m, 100));
        assert(!emm42_enable((uint8_t)m) && !emm42_disable((uint8_t)m) && !emm42_stop((uint8_t)m));
        assert(emm42_get_last_error((uint8_t)m) == EMM42_ERROR_INVALID_ARGUMENT);
    }
    assert(frame_count == 0);
    reset_io(); assert(emm42_enable(EMM42_MOTOR_ALL));
    assert(emm42_get_last_error(EMM42_MOTOR_ALL) == EMM42_ERROR_NONE);
    assert(emm42_get_last_error(255) == EMM42_ERROR_INVALID_ARGUMENT);
    assert(emm42_get_last_error(EMM42_MOTOR_ALL) == EMM42_ERROR_NONE);
    // Exact relative-position frame, including high bytes, relative and sync flags.
    reset_io();
    assert(emm42_move_relative(2, EMM42_DIRECTION_CCW, 1500, 10, 0x12347D00U));
    const uint8_t relative[] = {2, 0xFD, 1, 5, 0xDC, 10, 0x12, 0x34, 0x7D, 0, 0, 0, 0x6B};
    assert(lengths[0] == sizeof(relative) && memcmp(frames[0], relative, sizeof(relative)) == 0);
    reset_io();
    assert(emm42_move_relative(EMM42_MOTOR_ALL, EMM42_DIRECTION_CW, 3000, 255, UINT32_MAX));
    expect_all(0xFD);
    for (unsigned i=0; i<3; ++i) {
        assert(frames[i][5] == 255 && frames[i][6] == 255 && frames[i][9] == 255);
        assert(frames[i][10] == 0 && frames[i][11] == 0);
    }
    reset_io(); response[0] = 1;
    assert(!emm42_move_relative(EMM42_MOTOR_ALL, EMM42_DIRECTION_CW, 100, 0, 800));
    expect_all(0xFD);
    reset_io();
    assert(!emm42_move_relative(1, (emm42_direction_t)256, 100, 0, 10));
    assert(!emm42_move_relative(1, EMM42_DIRECTION_CW, 0, 0, 10));
    assert(!emm42_move_relative(1, EMM42_DIRECTION_CW, 3001, 0, 10));
    assert(!emm42_move_relative(4, EMM42_DIRECTION_CW, 100, 0, 10));
    assert(frame_count == 0);

    for (uint8_t m=1; m<=3; ++m) {
        for (unsigned sign=0; sign<2; ++sign) {
            const uint32_t values[] = {0, 65536, 8, 0x80000000U, UINT32_MAX, 0x0200EE6BU};
            for (unsigned i=0; i<sizeof(values)/sizeof(values[0]); ++i) {
                reset_io(); position_sign[m-1] = (uint8_t)sign;
                position_magnitude[m-1] = values[i];
                assert(emm42_read_position(m, &position));
                assert(position == (sign ? -(int64_t)values[i] : (int64_t)values[i]));
                assert(lengths[0] == 3 && frames[0][0] == m && frames[0][1] == 0x36);
                assert(emm42_read_position_error(m, &position));
                assert(position == (sign ? -(int64_t)values[i] : (int64_t)values[i]));
                assert(frames[1][1] == 0x37);
            }
        }
        for (unsigned flags=0; flags<16; ++flags) {
            reset_io(); status_flags[m-1] = (uint8_t)flags;
            emm42_status_t status;
            bool enabled = true, stalled = true;
            assert(emm42_read_status(m, &status));
            assert(status.enabled == ((flags & 1) != 0));
            assert(status.reached == ((flags & 2) != 0));
            assert(status.stalled == ((flags & 4) != 0));
            assert(status.stall_protected == ((flags & 8) != 0));
            assert(emm42_read_enabled(m, &enabled) && enabled == status.enabled);
            assert(emm42_read_stall(m, &stalled) && stalled == status.stalled);
            for (unsigned i=0; i<3; ++i) assert(frames[i][1] == 0x3A && lengths[i] == 3);
        }
    }
    assert(emm42_position_to_degrees(65536) == 360.0f);
    assert(emm42_position_to_degrees(-16384) == -90.0f);
    // Record a horizontal reference and subtract it without changing motor counters.
    reset_io(); position_magnitude[0] = 65536;
    int64_t horizontal;
    assert(emm42_read_position(1, &horizontal));
    position_magnitude[0] += 16384;
    assert(emm42_read_position(1, &position));
    assert(emm42_position_to_degrees(position - horizontal) == 90.0f);

    const int modes[] = {1, 3, 4, 5, 6, 7, 8, 9, 10};
    const emm42_error_t read_errors[] = {EMM42_ERROR_TIMEOUT, EMM42_ERROR_TIMEOUT,
        EMM42_ERROR_RESPONSE, EMM42_ERROR_UART, EMM42_ERROR_BAD_COMMAND,
        EMM42_ERROR_NONE, EMM42_ERROR_RESPONSE, EMM42_ERROR_TIMEOUT, EMM42_ERROR_TIMEOUT};
    for (unsigned i=0; i<sizeof(modes)/sizeof(modes[0]); ++i) {
        reset_io(); response[1] = modes[i]; position = 123;
        position_magnitude[1] = 65536;
        bool ok = emm42_read_position(2, &position);
        assert(ok == (read_errors[i] == EMM42_ERROR_NONE));
        assert(emm42_get_last_error(2) == read_errors[i]);
        assert(position == (ok ? 65536 : 123));
        position = 123;
        assert(emm42_read_position_error(2, &position) == ok);
        assert(position == (ok ? 65536 : 123));
    }
    for (int mode=1; mode<=11; ++mode) {
        if (mode == 2 || mode == 8) continue; // Not applicable to status data.
        reset_io(); response[1] = mode; status_flags[1] = 0;
        emm42_status_t status = {true, true, true, true};
        bool enabled = true, stalled = true;
        bool ok = emm42_read_status(2, &status);
        assert(ok == (mode == 7 || mode == 11));
        assert(status.enabled == !ok && status.reached == !ok);
        assert(status.stalled == !ok && status.stall_protected == !ok);
        assert(emm42_read_enabled(2, &enabled) == ok && enabled == !ok);
        assert(emm42_read_stall(2, &stalled) == ok && stalled == !ok);
    }
    reset_io();
    for (unsigned m=0; m<=255; ++m) {
        if (m>=1 && m<=3) continue;
        emm42_status_t status = {false, false, false, false};
        bool value = true;
        position = 123;
        assert(!emm42_read_position((uint8_t)m, &position) && position == 123);
        assert(!emm42_read_position_error((uint8_t)m, &position) && position == 123);
        assert(!emm42_read_status((uint8_t)m, &status));
        assert(!emm42_read_enabled((uint8_t)m, &value) && value);
        assert(!emm42_read_stall((uint8_t)m, &value) && value);
    }
    assert(!emm42_read_position(1, NULL) && !emm42_read_position_error(1, NULL));
    assert(!emm42_read_status(1, NULL) && !emm42_read_enabled(1, NULL) && !emm42_read_stall(1, NULL));
    assert(frame_count == 0);
    puts("PASS: existing controls, relative motion, signed position/error, all status bits, horizontal reference, invalid arguments and response failures");
}
