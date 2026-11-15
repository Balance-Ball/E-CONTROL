#include "Debug.h"

void __Debug_MSG_STR(const char *data)
{
    while(*data != '\0')
    {
        DL_UART_transmitDataBlocking(Debug_INST, *data);
        data++;
    }
}

void __Debug_MSG_BArray(uint8_t *data)
{
    while(*data != '\0')
    {
        DL_UART_transmitDataBlocking(Debug_INST, *data);
        data++;
    }
}

void FuckCora(void);

#define Debug_MSG(x) _Generic((x), \
    char*      : __Debug_MSG_STR, \
    const char*: __Debug_MSG_STR, \
    uint8_t*   : __Debug_MSG_BArray \
)(x)


