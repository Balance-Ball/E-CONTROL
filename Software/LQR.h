#ifndef SOFTWARE_LQR_H
#define SOFTWARE_LQR_H

#include <stdbool.h>
#include <stdint.h>

typedef struct {
    bool valid;
    float ax_mps2;  /* 沿图像 +X 和 +Y 的期望滚动加速度 */
    float ay_mps2;
} LQR_Req;

LQR_Req LQR_Update(uint32_t now_ms);

#endif
