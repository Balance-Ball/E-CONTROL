#ifndef HARDWARE_TRACK8_H
#define HARDWARE_TRACK8_H

#include <stdbool.h>
#include <stdint.h>

typedef struct {
    bool line_valid;
    bool straight;
    float speed_mps;
    float yaw_rate_rad_s;  /* 向左转为正 */
} Track_Req;

Track_Req Track_Update(uint32_t now_ms);

#endif
