#ifndef MOCK_TI_CONFIG_H
#define MOCK_TI_CONFIG_H
#include <stdint.h>
#include <stdbool.h>
typedef struct { uint32_t RXDATA; } MockUART;
extern MockUART mock_uart;
#define EMM42_INST (&mock_uart)
#define EMM42_BAUD_RATE 115200
#define EMM42_INST_INT_IRQN 1
#define CPUCLK_FREQ 32000000U
#define DL_UART_ERROR_OVERRUN (1U << 8)
#define DL_UART_ERROR_BREAK (1U << 9)
#define DL_UART_ERROR_PARITY (1U << 10)
#define DL_UART_ERROR_FRAMING (1U << 11)
#define UART_RXDATA_DATA_MASK 255U
void delay_cycles(uint32_t cycles);
void NVIC_DisableIRQ(int irq);
void NVIC_ClearPendingIRQ(int irq);
void DL_UART_Main_disableInterrupt(MockUART *uart, uint32_t mask);
void DL_UART_Main_disable(MockUART *uart);
void DL_UART_Main_enableFIFOs(MockUART *uart);
void DL_UART_Main_enable(MockUART *uart);
bool DL_UART_Main_isBusy(MockUART *uart);
bool DL_UART_Main_isRXFIFOEmpty(MockUART *uart);
uint8_t DL_UART_Main_receiveData(MockUART *uart);
bool DL_UART_Main_isTXFIFOFull(MockUART *uart);
void DL_UART_Main_transmitData(MockUART *uart, uint8_t byte);
#endif
