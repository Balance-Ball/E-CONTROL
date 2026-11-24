#ifndef SOFTWARE_CONTROL_CONFIG_H
#define SOFTWARE_CONTROL_CONFIG_H

/* 联调初值。轮径、轮距、减速比和方向按实际底盘修改。 */
#define CONTROL_PERIOD_MS               20U
#define CONTROL_VISION_TIMEOUT_MS       200U
#define CONTROL_IMU_TIMEOUT_MS          300U
#define CONTROL_CRUISE_MPS              0.12f
#define CONTROL_TRACK_YAW_GAIN          2.0f
#define CONTROL_TRACK_STRAIGHT_ERROR    0.15f
#define CONTROL_WHEEL_RADIUS_M          0.033f
#define CONTROL_WHEEL_TRACK_M           0.15f
#define CONTROL_WHEEL_GEAR_RATIO        1.0f
#define CONTROL_WHEEL_MAX_RPM           60
#define CONTROL_LEFT_REVERSED           false
#define CONTROL_RIGHT_REVERSED          false
#define CONTROL_DRIVE_ACCEL_MPS2        0.5f
#define CONTROL_HEADING_GAIN            1.5f
#define CONTROL_HEADING_MAX_RAD_S       0.6f

/* yaw 增大应表示向左转。roll/pitch 的零偏和投影随安装方向标定。 */
#define CONTROL_IMU_YAW_SIGN            1.0f
#define CONTROL_IMU_TILT_ENABLED        false
#define CONTROL_IMU_LEVEL_ROLL_RAD      0.0f
#define CONTROL_IMU_LEVEL_PITCH_RAD     0.0f
#define CONTROL_IMU_ROLL_XY             {0.0f, 0.0f}
#define CONTROL_IMU_PITCH_XY            {0.0f, 0.0f}

/* LQR 增益按 rad/m、rad/(m/s) 填写。比例或增益为零时不输出球加速度。 */
#define CONTROL_METERS_PER_PIXEL        {0.0f, 0.0f}
#define CONTROL_POSITION_GAIN           {0.0f, 0.0f}
#define CONTROL_VELOCITY_GAIN           {0.0f, 0.0f}
#define CONTROL_VELOCITY_TAU_S          0.04f
#define CONTROL_GRAVITY_MPS2            9.80665f
#define CONTROL_BALL_TILT_GAIN          (5.0f * CONTROL_GRAVITY_MPS2 / 7.0f)

/* 球盘标定按驱动器绝对坐标填写。角度正方向由每轴的方向配置决定。
 * 标定行格式为 {水平轴角, 最小轴角, 最大轴角, X系数, Y系数, 每圈脉冲数, 正方向为CCW}。
 * 三行依次对应电机地址 1、2、3，水平轴角取自水平时读取的 Motor POS（度）。
 * 车体前方及左方在图像坐标中的投影也要随相机安装方向标定。
 */
#define CONTROL_PLATE_AXES              { \
    {37.369995f, 0.0f, 0.0f, 0.0f, 0.0f, 0U, false}, \
    { 0.027466f, 0.0f, 0.0f, 0.0f, 0.0f, 0U, false}, \
    {-0.137329f, 0.0f, 0.0f, 0.0f, 0.0f, 0U, false}  \
}
#define CONTROL_VEHICLE_FORWARD_XY      {0.0f, 0.0f}
#define CONTROL_VEHICLE_LEFT_XY         {0.0f, 0.0f}
#define CONTROL_MAX_FEEDFORWARD_RAD     0.03f
#define CONTROL_MAX_TILT_RAD            0.05236f
#define CONTROL_MAX_TILT_RATE_RAD_S     0.35f
#define CONTROL_PLATE_RPM               60U
#define CONTROL_PLATE_ACCELERATION      0U

#endif
