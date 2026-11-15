#include "ti_msp_dl_config.h"

#define Debug_MSG(x) _Generic((x), \
    char*      : __Debug_MSG_STR,\
    const char*: __Debug_MSG_STR, \
    uint8_t*   : __Debug_MSG_BArray \
)(x)

void __Debug_MSG_STR(const char *data);
void __Debug_MSG_BArray(uint8_t *data);
void FuckCora(void);

