#include "LQR.h"
#include "Hardware/emm42.h"
#include <math.h>

static volatile uint32_t last_vision_time = 0;
static uint16_t last_dx = 0;
static uint16_t last_dy = 0;

//kp kv
static float Kx[2];
static float Ky[2];

static float vx , vy;
static float theta_x , theta_y; //圆盘目标角

/*
    像素 - m 换算 scale = m / 像素
*/
static float scale_x , scale_y;

/*
圆盘调平时电机倾角
*/
float default_angle[] = { 0.0 , 0.0 , 0.0 };

/*
ai:左右倾斜参
bi:上下倾斜参
*/
float a[] = { 0.0 , 0.0 , 0.0};
float b[] = { 0.0 , 0.0 , 0.0};

static void V_Speed(uint16_t dx,uint16_t dy,uint32_t cur_time);
static void api_motor();

void LQR(int16_t dx,int16_t dy,uint32_t cur_time)
{
    /*
    速度计算层
    */
    V_Speed(dx,dy,cur_time);

    /*
    @theta_x
    @theta_y
    角度计算层

        当数值大于零时：
            x 圆盘右侧高，左侧低
            y 圆盘下侧高，上侧低
    */
    theta_x = -(Kx[0] * dx + Kx[1] * vx);
    theta_y = -(Ky[0] * dy + Ky[1] * vy);

    /*
    电机实施层
    */
    api_motor();
}

static void api_motor()
{
    /*
        @pulse
        第一位 0/1 顺时针/逆时针 (EMM42_DIRECTION_CW，EMM42_DIRECTION_CCW)
        第二位正式脉冲数
    */
    uint8_t pulse[3][2];
    bool ready = false;
    for (int i = 0; i < 3;i++) 
    {
        //处理数据
        float tg_angle;
        int64_t pos;
        tg_angle = default_angle[i] + a[i] * theta_x + b[i] * theta_y;
        ready = emm42_read_position(i + 1, &pos);
        if(ready) return;
        float cur_angle = emm42_position_to_degrees(pos);
        float d_angle = tg_angle - (float)cur_angle;
        float raw_pulse = d_angle * 16.0 / 1.8;
        if(raw_pulse < 0)
        {
            raw_pulse = -raw_pulse;
            pulse[i][0] = EMM42_DIRECTION_CCW;
        }
        else pulse[i][0] = EMM42_DIRECTION_CW;
        pulse[i][1] = round((double)raw_pulse);

    }
    //电机执行
    emm42_move_relative(1,pulse[0][0],255U,30U,pulse[0][1]);
    emm42_move_relative(2,pulse[1][0],255U,30U,pulse[1][1]);
    emm42_move_relative(3,pulse[2][0],255U,30U,pulse[2][1]);

}



static void V_Speed(uint16_t dx,uint16_t dy,uint32_t cur_time)
{
    cur_time *= 0.001f;

    uint32_t dt_s = cur_time - last_vision_time; //傻逼delta_t会被标绿，给当成typedef了
    last_vision_time = cur_time;

    uint32_t delta_x = (uint32_t)(dx - last_dx);
    uint32_t delta_y = (uint32_t)(dy - last_dy);

    last_dx = dx;
    last_dy = dy;

    float raw_vx = (float)delta_x / dt_s / 100.0f * scale_x;
    float raw_vy = (float)delta_y / dt_s / 100.0f * scale_y;

    /*
    滤波
    alpha = delta_t /  (*滤波时间常数* + delta_t)

    *滤波时间常数* = 40.0f 即40ms

    alpha -> 本次新测量占得权重

    作用：使得新速度包含一点老速度，以免数据相差过大炸韶钢
    */
    float alpha = dt_s / (40.0f + dt_s);

    vx = (1.0f - alpha) * vx + alpha * raw_vx;
    vy = (1.0f - alpha) * vy + alpha * raw_vy;

}

