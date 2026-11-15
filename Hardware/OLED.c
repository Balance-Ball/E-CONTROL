#include "ti_msp_dl_config.h"
#include "OLED.h"

#include <stdarg.h>
#include <stddef.h>
#include <stdio.h>

#if OLED_I2C_ADDRESS != 0x3CU && OLED_I2C_ADDRESS != 0x3DU
#error "OLED_I2C_ADDRESS must be the 7-bit address 0x3C or 0x3D"
#endif

#define OLED_PAGES          (OLED_HEIGHT / 8U)
#define OLED_HALF_BIT_US    (5U)    /* GPIO/API 开销使实际速率低于约100kHz */
#define OLED_SCL_TIMEOUT_US (1000U)

static uint8_t s_buffer[OLED_PAGES][OLED_WIDTH];
static uint8_t s_dirty_min[OLED_PAGES];
static uint8_t s_dirty_max[OLED_PAGES];
static bool s_initialized = false;
static bool s_auto_refresh = true;
static OLED_Font s_font = OLED_FONT_8X16;
static OLED_Error s_error = OLED_ERROR_NOT_INITIALIZED;

/* 5x7 ASCII 点阵；每字符5列，bit0为顶行，0x20..0x7E。 */
static const uint8_t s_font5x7[][5] = {
    {0x00U, 0x00U, 0x00U, 0x00U, 0x00U}, /* 32 */
    {0x00U, 0x00U, 0x5FU, 0x00U, 0x00U}, /* 33 */
    {0x00U, 0x07U, 0x00U, 0x07U, 0x00U}, /* 34 */
    {0x14U, 0x7FU, 0x14U, 0x7FU, 0x14U}, /* 35 */
    {0x24U, 0x2AU, 0x7FU, 0x2AU, 0x12U}, /* 36 */
    {0x23U, 0x13U, 0x08U, 0x64U, 0x62U}, /* 37 */
    {0x36U, 0x49U, 0x55U, 0x22U, 0x50U}, /* 38 */
    {0x00U, 0x05U, 0x03U, 0x00U, 0x00U}, /* 39 */
    {0x00U, 0x1CU, 0x22U, 0x41U, 0x00U}, /* 40 */
    {0x00U, 0x41U, 0x22U, 0x1CU, 0x00U}, /* 41 */
    {0x14U, 0x08U, 0x3EU, 0x08U, 0x14U}, /* 42 */
    {0x08U, 0x08U, 0x3EU, 0x08U, 0x08U}, /* 43 */
    {0x00U, 0x50U, 0x30U, 0x00U, 0x00U}, /* 44 */
    {0x08U, 0x08U, 0x08U, 0x08U, 0x08U}, /* 45 */
    {0x00U, 0x60U, 0x60U, 0x00U, 0x00U}, /* 46 */
    {0x20U, 0x10U, 0x08U, 0x04U, 0x02U}, /* 47 */
    {0x3EU, 0x51U, 0x49U, 0x45U, 0x3EU}, /* 48 */
    {0x00U, 0x42U, 0x7FU, 0x40U, 0x00U}, /* 49 */
    {0x42U, 0x61U, 0x51U, 0x49U, 0x46U}, /* 50 */
    {0x21U, 0x41U, 0x45U, 0x4BU, 0x31U}, /* 51 */
    {0x18U, 0x14U, 0x12U, 0x7FU, 0x10U}, /* 52 */
    {0x27U, 0x45U, 0x45U, 0x45U, 0x39U}, /* 53 */
    {0x3CU, 0x4AU, 0x49U, 0x49U, 0x30U}, /* 54 */
    {0x01U, 0x71U, 0x09U, 0x05U, 0x03U}, /* 55 */
    {0x36U, 0x49U, 0x49U, 0x49U, 0x36U}, /* 56 */
    {0x06U, 0x49U, 0x49U, 0x29U, 0x1EU}, /* 57 */
    {0x00U, 0x36U, 0x36U, 0x00U, 0x00U}, /* 58 */
    {0x00U, 0x56U, 0x36U, 0x00U, 0x00U}, /* 59 */
    {0x08U, 0x14U, 0x22U, 0x41U, 0x00U}, /* 60 */
    {0x14U, 0x14U, 0x14U, 0x14U, 0x14U}, /* 61 */
    {0x00U, 0x41U, 0x22U, 0x14U, 0x08U}, /* 62 */
    {0x02U, 0x01U, 0x51U, 0x09U, 0x06U}, /* 63 */
    {0x32U, 0x49U, 0x79U, 0x41U, 0x3EU}, /* 64 */
    {0x7EU, 0x11U, 0x11U, 0x11U, 0x7EU}, /* 65 */
    {0x7FU, 0x49U, 0x49U, 0x49U, 0x36U}, /* 66 */
    {0x3EU, 0x41U, 0x41U, 0x41U, 0x22U}, /* 67 */
    {0x7FU, 0x41U, 0x41U, 0x22U, 0x1CU}, /* 68 */
    {0x7FU, 0x49U, 0x49U, 0x49U, 0x41U}, /* 69 */
    {0x7FU, 0x09U, 0x09U, 0x09U, 0x01U}, /* 70 */
    {0x3EU, 0x41U, 0x49U, 0x49U, 0x7AU}, /* 71 */
    {0x7FU, 0x08U, 0x08U, 0x08U, 0x7FU}, /* 72 */
    {0x00U, 0x41U, 0x7FU, 0x41U, 0x00U}, /* 73 */
    {0x20U, 0x40U, 0x41U, 0x3FU, 0x01U}, /* 74 */
    {0x7FU, 0x08U, 0x14U, 0x22U, 0x41U}, /* 75 */
    {0x7FU, 0x40U, 0x40U, 0x40U, 0x40U}, /* 76 */
    {0x7FU, 0x02U, 0x0CU, 0x02U, 0x7FU}, /* 77 */
    {0x7FU, 0x04U, 0x08U, 0x10U, 0x7FU}, /* 78 */
    {0x3EU, 0x41U, 0x41U, 0x41U, 0x3EU}, /* 79 */
    {0x7FU, 0x09U, 0x09U, 0x09U, 0x06U}, /* 80 */
    {0x3EU, 0x41U, 0x51U, 0x21U, 0x5EU}, /* 81 */
    {0x7FU, 0x09U, 0x19U, 0x29U, 0x46U}, /* 82 */
    {0x46U, 0x49U, 0x49U, 0x49U, 0x31U}, /* 83 */
    {0x01U, 0x01U, 0x7FU, 0x01U, 0x01U}, /* 84 */
    {0x3FU, 0x40U, 0x40U, 0x40U, 0x3FU}, /* 85 */
    {0x1FU, 0x20U, 0x40U, 0x20U, 0x1FU}, /* 86 */
    {0x3FU, 0x40U, 0x38U, 0x40U, 0x3FU}, /* 87 */
    {0x63U, 0x14U, 0x08U, 0x14U, 0x63U}, /* 88 */
    {0x07U, 0x08U, 0x70U, 0x08U, 0x07U}, /* 89 */
    {0x61U, 0x51U, 0x49U, 0x45U, 0x43U}, /* 90 */
    {0x00U, 0x7FU, 0x41U, 0x41U, 0x00U}, /* 91 */
    {0x02U, 0x04U, 0x08U, 0x10U, 0x20U}, /* 92 */
    {0x00U, 0x41U, 0x41U, 0x7FU, 0x00U}, /* 93 */
    {0x04U, 0x02U, 0x01U, 0x02U, 0x04U}, /* 94 */
    {0x40U, 0x40U, 0x40U, 0x40U, 0x40U}, /* 95 */
    {0x00U, 0x01U, 0x02U, 0x04U, 0x00U}, /* 96 */
    {0x20U, 0x54U, 0x54U, 0x54U, 0x78U}, /* 97 */
    {0x7FU, 0x48U, 0x44U, 0x44U, 0x38U}, /* 98 */
    {0x38U, 0x44U, 0x44U, 0x44U, 0x20U}, /* 99 */
    {0x38U, 0x44U, 0x44U, 0x48U, 0x7FU}, /* 100 */
    {0x38U, 0x54U, 0x54U, 0x54U, 0x18U}, /* 101 */
    {0x08U, 0x7EU, 0x09U, 0x01U, 0x02U}, /* 102 */
    {0x0CU, 0x52U, 0x52U, 0x52U, 0x3EU}, /* 103 */
    {0x7FU, 0x08U, 0x04U, 0x04U, 0x78U}, /* 104 */
    {0x00U, 0x44U, 0x7DU, 0x40U, 0x00U}, /* 105 */
    {0x20U, 0x40U, 0x44U, 0x3DU, 0x00U}, /* 106 */
    {0x7FU, 0x10U, 0x28U, 0x44U, 0x00U}, /* 107 */
    {0x00U, 0x41U, 0x7FU, 0x40U, 0x00U}, /* 108 */
    {0x7CU, 0x04U, 0x18U, 0x04U, 0x78U}, /* 109 */
    {0x7CU, 0x08U, 0x04U, 0x04U, 0x78U}, /* 110 */
    {0x38U, 0x44U, 0x44U, 0x44U, 0x38U}, /* 111 */
    {0x7CU, 0x14U, 0x14U, 0x14U, 0x08U}, /* 112 */
    {0x08U, 0x14U, 0x14U, 0x18U, 0x7CU}, /* 113 */
    {0x7CU, 0x08U, 0x04U, 0x04U, 0x08U}, /* 114 */
    {0x48U, 0x54U, 0x54U, 0x54U, 0x20U}, /* 115 */
    {0x04U, 0x3FU, 0x44U, 0x40U, 0x20U}, /* 116 */
    {0x3CU, 0x40U, 0x40U, 0x20U, 0x7CU}, /* 117 */
    {0x1CU, 0x20U, 0x40U, 0x20U, 0x1CU}, /* 118 */
    {0x3CU, 0x40U, 0x30U, 0x40U, 0x3CU}, /* 119 */
    {0x44U, 0x28U, 0x10U, 0x28U, 0x44U}, /* 120 */
    {0x0CU, 0x50U, 0x50U, 0x50U, 0x3CU}, /* 121 */
    {0x44U, 0x64U, 0x54U, 0x4CU, 0x44U}, /* 122 */
    {0x00U, 0x08U, 0x36U, 0x41U, 0x00U}, /* 123 */
    {0x00U, 0x00U, 0x7FU, 0x00U, 0x00U}, /* 124 */
    {0x00U, 0x41U, 0x36U, 0x08U, 0x00U}, /* 125 */
    {0x08U, 0x04U, 0x08U, 0x10U, 0x08U}, /* 126 */
};
_Static_assert(sizeof(s_font5x7) / sizeof(s_font5x7[0]) == 95U,
               "The printable ASCII font must contain 95 characters");

static bool oled_fail(OLED_Error error)
{
    s_error = error;
    return false;
}

static bool oled_ready(void)
{
    return s_initialized || oled_fail(OLED_ERROR_NOT_INITIALIZED);
}

static void oled_delay_us(uint32_t microseconds)
{
    while (microseconds-- > 0U) {
        delay_cycles(CPUCLK_FREQ / 1000000U);
    }
}

static void oled_low(GPIO_Regs *port, uint32_t pin)
{
    DL_GPIO_clearPins(port, pin);
    DL_GPIO_enableOutput(port, pin);
}

static void oled_release(GPIO_Regs *port, uint32_t pin)
{
    DL_GPIO_disableOutput(port, pin);
}

static bool oled_scl_high(void)
{
    uint32_t remaining = OLED_SCL_TIMEOUT_US;
    oled_release(OLED_SCL_PORT, OLED_SCL_PIN);
    while (DL_GPIO_readPins(OLED_SCL_PORT, OLED_SCL_PIN) == 0U) {
        if (remaining-- == 0U) {
            return oled_fail(OLED_ERROR_BUS);
        }
        oled_delay_us(1U);
    }
    return true;
}

static void oled_scl_low(void)
{
    oled_low(OLED_SCL_PORT, OLED_SCL_PIN);
}

static bool oled_stop(void)
{
    oled_scl_low();
    oled_low(OLED_SDA_PORT, OLED_SDA_PIN);
    oled_delay_us(OLED_HALF_BIT_US);
    if (!oled_scl_high()) {
        oled_release(OLED_SDA_PORT, OLED_SDA_PIN);
        return false;
    }
    oled_delay_us(OLED_HALF_BIT_US);
    oled_release(OLED_SDA_PORT, OLED_SDA_PIN);
    oled_delay_us(OLED_HALF_BIT_US);
    if (DL_GPIO_readPins(OLED_SDA_PORT, OLED_SDA_PIN) == 0U) {
        return oled_fail(OLED_ERROR_BUS);
    }
    return true;
}

static bool oled_start(void)
{
    oled_release(OLED_SDA_PORT, OLED_SDA_PIN);
    if (!oled_scl_high()) {
        return false;
    }
    oled_delay_us(OLED_HALF_BIT_US);
    if (DL_GPIO_readPins(OLED_SDA_PORT, OLED_SDA_PIN) == 0U) {
        return oled_fail(OLED_ERROR_BUS);
    }
    oled_low(OLED_SDA_PORT, OLED_SDA_PIN);
    oled_delay_us(OLED_HALF_BIT_US);
    oled_scl_low();
    return true;
}

static bool oled_write_byte(uint8_t value)
{
    uint8_t bit;
    for (bit = 0U; bit < 8U; ++bit) {
        if ((value & 0x80U) != 0U) {
            oled_release(OLED_SDA_PORT, OLED_SDA_PIN);
        } else {
            oled_low(OLED_SDA_PORT, OLED_SDA_PIN);
        }
        oled_delay_us(OLED_HALF_BIT_US);
        if (!oled_scl_high()) {
            oled_scl_low();
            return false;
        }
        oled_delay_us(OLED_HALF_BIT_US);
        oled_scl_low();
        value = (uint8_t)(value << 1);
    }

    oled_release(OLED_SDA_PORT, OLED_SDA_PIN);
    oled_delay_us(OLED_HALF_BIT_US);
    if (!oled_scl_high()) {
        oled_scl_low();
        return false;
    }
    oled_delay_us(OLED_HALF_BIT_US);
    bool acknowledged = DL_GPIO_readPins(OLED_SDA_PORT, OLED_SDA_PIN) == 0U;
    oled_scl_low();
    return acknowledged || oled_fail(OLED_ERROR_NACK);
}

static bool oled_transfer(uint8_t control, const uint8_t *data, uint16_t count)
{
    uint16_t i;
    if (!oled_start()) {
        oled_release(OLED_SCL_PORT, OLED_SCL_PIN);
        oled_release(OLED_SDA_PORT, OLED_SDA_PIN);
        return false;
    }
    if (!oled_write_byte((uint8_t)(OLED_I2C_ADDRESS << 1)) ||
        !oled_write_byte(control)) {
        goto failed;
    }
    for (i = 0U; i < count; ++i) {
        if (!oled_write_byte(data[i])) {
            goto failed;
        }
    }
    if (!oled_stop()) {
        return false;
    }
    s_error = OLED_ERROR_NONE;
    return true;

failed: {
        /* 清理总线但保留导致失败的最初原因。 */
        OLED_Error saved = s_error;
        (void)oled_stop();
        s_error = saved;
        return false;
    }
}

static void oled_mark_all(void)
{
    uint8_t page;
    for (page = 0U; page < OLED_PAGES; ++page) {
        s_dirty_min[page] = 0U;
        s_dirty_max[page] = OLED_WIDTH - 1U;
    }
}

static void oled_store(uint8_t page, uint8_t x, uint8_t value)
{
    if (s_buffer[page][x] != value) {
        s_buffer[page][x] = value;
        if (x < s_dirty_min[page]) {
            s_dirty_min[page] = x;
        }
        if (x > s_dirty_max[page]) {
            s_dirty_max[page] = x;
        }
    }
}

static void oled_pixel(uint8_t x, uint8_t y, bool on)
{
    uint8_t page = y / 8U;
    uint8_t mask = (uint8_t)(1U << (y % 8U));
    uint8_t value = s_buffer[page][x];
    value = on ? (uint8_t)(value | mask) : (uint8_t)(value & (uint8_t)~mask);
    oled_store(page, x, value);
}

static bool oled_finish_edit(void)
{
    if (s_auto_refresh) {
        return OLED_Refresh();
    }
    s_error = OLED_ERROR_NONE;
    return true;
}

static uint8_t oled_font_width(void)
{
    return s_font == OLED_FONT_8X16 ? 8U : 6U;
}

static uint8_t oled_font_height(void)
{
    return s_font == OLED_FONT_8X16 ? 16U : 8U;
}

bool OLED_Init(void)
{
    static const uint8_t setup[] = {
        0xAEU,             /* display off during initialization */
        0xD5U, 0x80U,      /* clock divider / oscillator */
        0xA8U, 0x3FU,      /* multiplex 64 rows */
        0xD3U, 0x00U,      /* display offset */
        0x40U,             /* display start line */
        0x8DU, 0x14U,      /* internal charge pump */
        0x20U, 0x02U,      /* page addressing mode */
        0xA1U, 0xC8U,      /* default segment / COM orientation */
        0xDAU, 0x12U,      /* 128x64 COM pin configuration */
        0x81U, 0x7FU,      /* contrast */
        0xD9U, 0xF1U,      /* pre-charge for internal charge pump */
        0xDBU, 0x20U,      /* VCOMH: documented 0.77*VCC */
        0xA4U, 0xA6U,      /* RAM display, normal colors */
        0x2EU              /* stop hardware scrolling */
    };
    static const uint8_t display_on = 0xAFU;
    uint8_t page, x;

    s_initialized = false;
    s_auto_refresh = true;
    s_font = OLED_FONT_8X16;
    DL_GPIO_enablePower(OLED_SCL_PORT);
    DL_GPIO_enablePower(OLED_SDA_PORT);
    delay_cycles(POWER_STARTUP_DELAY);
    oled_release(OLED_SCL_PORT, OLED_SCL_PIN);
    oled_release(OLED_SDA_PORT, OLED_SDA_PIN);
    DL_GPIO_initDigitalInput(OLED_SCL_IOMUX);
    DL_GPIO_initDigitalInput(OLED_SDA_IOMUX);
    oled_delay_us(100000U); /* allow module power-on reset to settle */

    if (!oled_scl_high()) {
        return false;
    }
    /* 单主机总线恢复：从机停在半个字节时，最多发送9个时钟后发STOP。 */
    if (DL_GPIO_readPins(OLED_SDA_PORT, OLED_SDA_PIN) == 0U) {
        for (x = 0U; x < 9U; ++x) {
            oled_scl_low();
            oled_delay_us(OLED_HALF_BIT_US);
            if (!oled_scl_high()) {
                return false;
            }
            oled_delay_us(OLED_HALF_BIT_US);
        }
        if (!oled_stop()) {
            return false;
        }
    }
    if (!oled_transfer(0x00U, setup, (uint16_t)sizeof(setup))) {
        return false;
    }

    for (page = 0U; page < OLED_PAGES; ++page) {
        for (x = 0U; x < OLED_WIDTH; ++x) {
            s_buffer[page][x] = 0U;
        }
    }
    oled_mark_all();
    s_initialized = true;
    if (!OLED_Refresh() || !oled_transfer(0x00U, &display_on, 1U)) {
        s_initialized = false;
        return false;
    }
    return true;
}

bool OLED_Refresh(void)
{
    uint8_t page;
    if (!oled_ready()) {
        return false;
    }
    for (page = 0U; page < OLED_PAGES; ++page) {
        uint8_t start = s_dirty_min[page];
        if (start < OLED_WIDTH) {
            uint8_t position[] = {
                (uint8_t)(0xB0U | page),
                (uint8_t)(start & 0x0FU),
                (uint8_t)(0x10U | (start >> 4))
            };
            uint16_t count = (uint16_t)s_dirty_max[page] - start + 1U;
            if (!oled_transfer(0x00U, position, (uint16_t)sizeof(position)) ||
                !oled_transfer(0x40U, &s_buffer[page][start], count)) {
                return false; /* 此页仍标记为脏，可重试 */
            }
            s_dirty_min[page] = OLED_WIDTH;
            s_dirty_max[page] = 0U;
        }
    }
    s_error = OLED_ERROR_NONE;
    return true;
}

bool OLED_Clear(void)
{
    uint8_t page, x;
    if (!oled_ready()) {
        return false;
    }
    for (page = 0U; page < OLED_PAGES; ++page) {
        for (x = 0U; x < OLED_WIDTH; ++x) {
            oled_store(page, x, 0U);
        }
    }
    return oled_finish_edit();
}

uint8_t OLED_GetRows(void)
{
    return (uint8_t)(OLED_HEIGHT / oled_font_height());
}

uint8_t OLED_GetColumns(void)
{
    return (uint8_t)(OLED_WIDTH / oled_font_width());
}

bool OLED_SetFont(OLED_Font font)
{
    if (font != OLED_FONT_8X16 && font != OLED_FONT_6X8) {
        return oled_fail(OLED_ERROR_ARGUMENT);
    }
    s_font = font;
    s_error = OLED_ERROR_NONE;
    return true;
}

void OLED_SetAutoRefresh(bool enabled)
{
    s_auto_refresh = enabled;
}

/* 写入前完整验证，避免超长文本/非法字符只画出半段。 */
static bool oled_validate_text(uint8_t row, uint8_t column, const char *text)
{
    uint16_t r = row, c = column;
    uint8_t rows = OLED_GetRows(), columns = OLED_GetColumns();
    if (text == NULL || row < 1U || row > rows ||
        column < 1U || column > columns) {
        return oled_fail(OLED_ERROR_ARGUMENT);
    }
    while (*text != '\0') {
        uint8_t ch = (uint8_t)*text++;
        if (ch == '\r') {
            c = 1U;
        } else if (ch == '\n') {
            ++r;
            c = 1U;
            if (r > rows) {
                return oled_fail(OLED_ERROR_TEXT);
            }
        } else {
            if (ch < 32U || ch > 126U) {
                return oled_fail(OLED_ERROR_TEXT);
            }
            if (c > columns) {
                c = 1U;
                ++r;
            }
            if (r > rows) {
                return oled_fail(OLED_ERROR_TEXT);
            }
            ++c;
        }
    }
    return true;
}

static void oled_draw_character(uint8_t row, uint8_t column, uint8_t ch)
{
    uint8_t width = oled_font_width(), height = oled_font_height();
    uint8_t origin_x = (uint8_t)((column - 1U) * width);
    uint8_t origin_y = (uint8_t)((row - 1U) * height);
    uint8_t x, y;
    for (x = 0U; x < width; ++x) {
        for (y = 0U; y < height; ++y) {
            bool on = false;
            if (s_font == OLED_FONT_8X16) {
                /* 1列左边距，5列字形，2列右边距；纵向2倍，底部2行留白。 */
                if (x >= 1U && x <= 5U && y < 14U) {
                    on = (s_font5x7[ch - 32U][x - 1U] &
                          (uint8_t)(1U << (y / 2U))) != 0U;
                }
            } else if (x < 5U && y < 7U) {
                on = (s_font5x7[ch - 32U][x] &
                      (uint8_t)(1U << y)) != 0U;
            }
            oled_pixel((uint8_t)(origin_x + x), (uint8_t)(origin_y + y), on);
        }
    }
}

bool OLED_Write(uint8_t row, uint8_t column, const char *text)
{
    if (!oled_ready() || !oled_validate_text(row, column, text)) {
        return false;
    }
    while (*text != '\0') {
        uint8_t ch = (uint8_t)*text++;
        if (ch == '\r') {
            column = 1U;
        } else if (ch == '\n') {
            ++row;
            column = 1U;
        } else {
            if (column > OLED_GetColumns()) {
                column = 1U;
                ++row;
            }
            oled_draw_character(row, column, ch);
            ++column;
        }
    }
    return oled_finish_edit();
}

bool OLED_Printf(uint8_t row, uint8_t column, const char *format, ...)
{
    char text[128];
    int count;
    va_list args;
    if (!oled_ready()) {
        return false;
    }
    if (format == NULL) {
        return oled_fail(OLED_ERROR_ARGUMENT);
    }
    va_start(args, format);
    count = vsnprintf(text, sizeof(text), format, args);
    va_end(args);
    if (count < 0 || (size_t)count >= sizeof(text)) {
        return oled_fail(OLED_ERROR_FORMAT);
    }
    return OLED_Write(row, column, text);
}

bool OLED_ClearArea(uint8_t row, uint8_t column, uint8_t characters)
{
    uint16_t last = (uint16_t)column + characters;
    uint8_t i;
    if (!oled_ready()) {
        return false;
    }
    if (row < 1U || row > OLED_GetRows() || column < 1U ||
        column > OLED_GetColumns() || characters == 0U ||
        last > (uint16_t)OLED_GetColumns() + 1U) {
        return oled_fail(OLED_ERROR_ARGUMENT);
    }
    for (i = 0U; i < characters; ++i) {
        oled_draw_character(row, (uint8_t)(column + i), (uint8_t)' ');
    }
    return oled_finish_edit();
}

bool OLED_ClearLine(uint8_t row)
{
    uint8_t x, y;
    uint8_t top = (uint8_t)((row - 1U) * oled_font_height());
    if (!oled_ready()) {
        return false;
    }
    if (row < 1U || row > OLED_GetRows()) {
        return oled_fail(OLED_ERROR_ARGUMENT);
    }
    for (x = 0U; x < OLED_WIDTH; ++x) {
        for (y = 0U; y < oled_font_height(); ++y) {
            oled_pixel(x, (uint8_t)(top + y), false);
        }
    }
    return oled_finish_edit();
}

static bool oled_command(uint8_t value)
{
    return oled_ready() && oled_transfer(0x00U, &value, 1U);
}

bool OLED_SetContrast(uint8_t contrast)
{
    uint8_t command[] = {0x81U, contrast};
    return oled_ready() &&
           oled_transfer(0x00U, command, (uint16_t)sizeof(command));
}

bool OLED_SetInvert(bool invert)
{
    return oled_command(invert ? 0xA7U : 0xA6U);
}

bool OLED_SetDisplay(bool enabled)
{
    return oled_command(enabled ? 0xAFU : 0xAEU);
}

bool OLED_SetRotation(bool rotate180)
{
    uint8_t command[] = {
        rotate180 ? 0xA0U : 0xA1U,
        rotate180 ? 0xC0U : 0xC8U
    };
    return oled_ready() &&
           oled_transfer(0x00U, command, (uint16_t)sizeof(command));
}

bool OLED_DrawPixel(uint8_t x, uint8_t y, bool on)
{
    if (!oled_ready()) {
        return false;
    }
    if (x >= OLED_WIDTH || y >= OLED_HEIGHT) {
        return oled_fail(OLED_ERROR_ARGUMENT);
    }
    oled_pixel(x, y, on);
    s_error = OLED_ERROR_NONE;
    return true;
}

bool OLED_DrawLine(uint8_t x0, uint8_t y0, uint8_t x1, uint8_t y1, bool on)
{
    int16_t x = x0, y = y0;
    int16_t dx = x0 < x1 ? (int16_t)x1 - x0 : (int16_t)x0 - x1;
    int16_t dy = y0 < y1 ? (int16_t)y0 - y1 : (int16_t)y1 - y0;
    int16_t sx = x0 < x1 ? 1 : -1;
    int16_t sy = y0 < y1 ? 1 : -1;
    int16_t error = dx + dy;
    if (!oled_ready()) {
        return false;
    }
    if (x0 >= OLED_WIDTH || x1 >= OLED_WIDTH ||
        y0 >= OLED_HEIGHT || y1 >= OLED_HEIGHT) {
        return oled_fail(OLED_ERROR_ARGUMENT);
    }
    for (;;) {
        int16_t twice;
        oled_pixel((uint8_t)x, (uint8_t)y, on);
        if (x == x1 && y == y1) {
            break;
        }
        twice = 2 * error;
        if (twice >= dy) {
            error += dy;
            x += sx;
        }
        if (twice <= dx) {
            error += dx;
            y += sy;
        }
    }
    s_error = OLED_ERROR_NONE;
    return true;
}

bool OLED_DrawRect(uint8_t x, uint8_t y, uint8_t width, uint8_t height,
                   bool filled, bool on)
{
    uint16_t right = (uint16_t)x + width;
    uint16_t bottom = (uint16_t)y + height;
    uint16_t px, py;
    if (!oled_ready()) {
        return false;
    }
    if (width == 0U || height == 0U ||
        right > OLED_WIDTH || bottom > OLED_HEIGHT) {
        return oled_fail(OLED_ERROR_ARGUMENT);
    }
    for (px = x; px < right; ++px) {
        for (py = y; py < bottom; ++py) {
            if (filled || px == x || px + 1U == right ||
                py == y || py + 1U == bottom) {
                oled_pixel((uint8_t)px, (uint8_t)py, on);
            }
        }
    }
    s_error = OLED_ERROR_NONE;
    return true;
}

OLED_Error OLED_GetLastError(void)
{
    return s_error;
}