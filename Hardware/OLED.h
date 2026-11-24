#ifndef HARDWARE_OLED_H_
#define HARDWARE_OLED_H_

#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/*
 * SSD1306 0.96-inch 128x64, 4-pin I2C module.
 * 硬件 I2C1 由 SysConfig 配置为 PB2=SCL、PB3=SDA，VCC=3.3V，GND 共地。
 * SDA/SCL 必须有到 3.3V 的上拉电阻（通常模块已带，未带则外接约 4.7k）。
 * 在 SYSCFG_DL_init() 后调用 OLED_Init()。传输通过 I2C1 轮询完成。
 * 4 针模块需自带上电复位电路；本驱动不控制单独的 RES# 引脚。
 *
 * 示例:
 *   #include "Hardware/OLED.h"
 *   SYSCFG_DL_init();
 *   if (OLED_Init()) {
 *       OLED_Write(1, 1, "Balance Ball");
 *       OLED_Printf(2, 1, "Speed: %u RPM", 100U);
 *   }
 *
 * 默认自动刷新；批量修改建议:
 *   OLED_SetAutoRefresh(false);
 *   OLED_ClearLine(2);
 *   OLED_Printf(2, 1, "Speed: %u", speed);
 *   OLED_Refresh();
 *
 * 1024 字节显存，刷新时只发送有变化的页/列，减少主循环阻塞。
 * 所有通信均为阻塞轮询，仅在主循环使用，不在控制中断/多个任务中并发调用。
 * ASCII 字库: 0x20..0x7E 共 95 字符，包含空格、大小写、数字和英文标点。
 * 支持 '\n' 换行、'\r' 回到行首；不支持中文/UTF-8。
 */
#define OLED_WIDTH          (128U)
#define OLED_HEIGHT         (64U)
#define OLED_I2C_ADDRESS    (0x3CU)  /* 7 位地址，可改 0x3D，不是 0x78 */

typedef enum {
    OLED_FONT_8X16 = 0,  /* 默认: 4 行 x 16 列，5x7 字形纵向放大并留白 */
    OLED_FONT_6X8        /* 小字体: 8 行 x 21 列 */
} OLED_Font;

typedef enum {
    OLED_ERROR_NONE = 0,
    OLED_ERROR_NOT_INITIALIZED,
    OLED_ERROR_ARGUMENT,
    OLED_ERROR_NACK,      /* 地址/数据未应答：检查接线、电源、地址 */
    OLED_ERROR_BUS,       /* 总线忙、仲裁丢失或传输超时 */
    OLED_ERROR_TEXT,      /* 非 ASCII 可打印字符，或文本超出屏幕 */
    OLED_ERROR_FORMAT     /* 格式化失败，或结果超过 127 字节 */
} OLED_Error;

bool OLED_Init(void);   /* 初始化、清屏、开启显示；失败返回 false */
bool OLED_Clear(void);  /* 清空显存，并按自动刷新设置决定是否立即显示 */

/*
 * OLED_Write(起始行, 起始列, "文本")，行/列均从 1 开始。
 * 默认 8x16 字体：row=1..4，column=1..16。
 * 切换 6x8 字体后：row=1..8，column=1..21。
 * 行满后自动折到下一行的第 1 列；文本越过底部或有非法字符返回 false，
 * 此时不修改显存。成功会覆盖对应字符格，但不会清除文本后面的旧字符。
 * I2C 发送失败时显存保留修改和待刷新标记，可稍后 OLED_Refresh() 重试。
 */
bool OLED_Write(uint8_t row, uint8_t column, const char *text);
bool OLED_Printf(uint8_t row, uint8_t column, const char *format, ...);
bool OLED_ClearLine(uint8_t row); /* 行范围与当前字体相同，清整行 */
bool OLED_ClearArea(uint8_t row, uint8_t column, uint8_t characters);
bool OLED_SetFont(OLED_Font font); /* 不清屏；换字体后建议 OLED_Clear() */
uint8_t OLED_GetRows(void);
uint8_t OLED_GetColumns(void);

void OLED_SetAutoRefresh(bool enabled); /* 设置开关本身不会触发刷新 */
bool OLED_Refresh(void);                /* 仅刷新有修改的范围 */
bool OLED_SetContrast(uint8_t contrast); /* 亮度 0..255 */
bool OLED_SetInvert(bool invert);       /* true=黑白反色 */
bool OLED_SetDisplay(bool enabled);     /* 显示开关，保留显存 */
bool OLED_SetRotation(bool rotate180);  /* false=默认方向，true=旋转180度 */

/* 以下绘图仅修改显存，需 OLED_Refresh()；不受自动刷新开关影响。
 * 像素坐标从 0 开始: x=0..127，y=0..63；on=true 点亮，false 擦除。
 */
bool OLED_DrawPixel(uint8_t x, uint8_t y, bool on);
bool OLED_DrawLine(uint8_t x0, uint8_t y0, uint8_t x1, uint8_t y1, bool on);
bool OLED_DrawRect(uint8_t x, uint8_t y, uint8_t width, uint8_t height,
                   bool filled, bool on);
OLED_Error OLED_GetLastError(void);

#ifdef __cplusplus
}
#endif
#endif /* HARDWARE_OLED_H_ */
