#ifndef HARDWARE_VISION_H
#define HARDWARE_VISION_H

#include <stdint.h>
#include <stdbool.h>

#define frame_size 8U


typedef struct
{
    uint8_t head[2];
    bool found;
    int16_t dx;
    int16_t dy;
    uint8_t sum;
    uint32_t sample_ms;
} vision_data;

void vision_init(void);
bool Get_Vision_data(vision_data *out);



#endif
