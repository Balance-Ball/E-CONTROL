# Balance Ball — 三电机圆盘小球平衡

基于 TI MSPM0G3507、MaixCAM Pro 视觉模块和三台 Emm42 V5.0 闭环步进电机的小球平衡项目。三台电机呈三角形排布，通过三组连杆改变圆盘倾角，使圆盘上的小球回到中心并保持稳定。

主循环汇总底盘 IMU、八路循迹和小球 LQR 的需求，由电机调度层统一执行。当前处于算法与执行机构联调阶段，球盘和底盘参数需实机标定。

## 1. 项目条件与目标

- 小球为近似均匀实心球，放在可倾斜圆盘上。
- 三台电机通过连杆共同控制圆盘姿态，地址分别为1、2、3。
- 相机从下方仰视，图像中心已对准圆盘中心。图像坐标为X向右、Y向下。
- 相机提供球心相对于图像中心的`dx/dy`，单位为**像素×100**。
- LQR 根据球的位置和估计速度输出期望滚动加速度，由调度层换算为三台电机的绝对位置。
- 底盘 IMU 用于直行航向修正。底盘倾斜补偿默认关闭，标定安装方向和水平零偏后可启用。
- 像素比例、连杆映射、水平零位和LQR增益尚未标定。三角形布局不代表已确定等边尺寸或电机转向。

## 2. 工程结构与当前能力

```text
Balance_Ball/
├── empty_mspm0g3507.c        # 初始化和主循环
├── empty_mspm0g3507.syscfg   # 时钟、外设、管脚配置
├── Hardware/
│   ├── emm42.c / emm42.h    # 电机协议、运动命令与反馈
│   ├── vision.c / vision.h  # 视觉串口与8字节协议解析
│   ├── KEY.c / KEY.h        # PB21按键初始化和调试中断
│   ├── OLED.c / OLED.h      # SSD1306 硬件 I²C 显示驱动
│   └── IMU.c / IMU.h        # 底盘 IMU 角度帧 DMA 接收
├── Software/
│   ├── LQR.c / LQR.h        # 球位置、速度估计及加速度需求
│   ├── MotorScheduler.c / .h # 五轴目标合成和发送
│   ├── ControlConfig.h     # 机构标定和控制参数
│   ├── bsp_timer.c / .h     # 1 ms SysTick计时
│   └── Debug.c / Debug.h    # UART0阻塞调试输出
├── targetConfigs/          # MSPM0G3507调试配置
└── Debug/                  # 构建产物、生成配置和验证文件
```

相机程序位于独立目录`D:/Projects/Ti/maxicam/redball.py`，不包含在本MCU工程目录内；迁移项目时需要另外携带。

| 模块 | 当前实现情况 |
| --- | --- |
| 视觉接收 | 校验、滑窗寻帧、接收时间戳，LQR 按时间戳清除超时需求 |
| 电机通信 | 使能、停止、速度模式、绝对及相对位置模式、位置/误差/状态查询已实现 |
| 显示与日志 | OLED文本/基本绘图、串口日志已实现，均含阻塞操作 |
| 按键 | 调试按键中断记录事件，主循环处理电机位置查询 |
| 计时 | 1 ms SysTick，调度周期初值 20 ms |
| LQR | 已接入主循环，实测增益和像素比例待配置 |
| IMU | DMA 接收，主循环解析角度，有效航向参与直行修正 |

## 3. 硬件接口

MCU为**MSPM0G3507，LQFP-64**，当前生成配置的主频为**32 MHz**。

| 用途 | 外设 / 管脚 | 当前配置 |
| --- | --- | --- |
| 调试串口 | UART0：TX PA10，RX PA11 | 115200 baud |
| 电机串口 | UART1：TX PB6，RX PB7 | TTL，115200，8N1 |
| 视觉串口 | UART2：TX PA21，RX PA22 | 115200，8N1 |
| IMU串口 | UART3：TX PB12，RX PB13 | 9600，8N1，DMA CH0接收 |
| KEY | PB21 / PINCM49 | 内部上拉、下降沿、3周期输入滤波 |
| BOARD_LED / OS_LED | PB22 / PB1 | GPIO输出 |
| OLED | I2C1，PB2=SCL，PB3=SDA | 100 kHz，SSD1306 128×64，7位地址 0x3C |
| 保留的LineFollower配置 | I2C0：PA1=SCL，PA0=SDA | 主循环未接入相应业务 |

电机侧为PB6 → 电机R、PB7 ← 电机T，并共地；RS232/RS485接口型号需要相应收发器。视觉模块TX接MCU PA22。

OLED 接线以 SysConfig 中的 I2C1 为准。SCL/SDA 上拉至 3.3 V，模块自带上拉时可使用模块上的电阻。PB21按下接地形成下降沿；3周期硬件滤波不能代替机械按键消抖。

## 4. 视觉数据协议

相机端使用MaixPy v4和OpenCV，通过HSV红色阈值、形态学和圆形特征筛选小球，选取面积最大的合格目标。图像为**320×240**，参考中心为`(160, 120)`。

串口每帧固定**8字节**：

| 偏移 | 字段 | 编码 |
| --- | --- | --- |
| 0–1 | 帧头 | `AA 55` |
| 2 | `found` | 0=未找到，1=找到 |
| 3–4 | `dx` | 有符号16位、小端，像素×100 |
| 5–6 | `dy` | 有符号16位、小端，像素×100 |
| 7 | 校验和 | 前7字节求和，保留低8位 |

例如，`dx=2000`表示球心在图像中心右侧20像素，`dy=-1500`表示在上方15像素。

```c
vision_data data;
if (Get_Vision_data(&data)) {
    if (data.found) {
        /* 收到找到小球的新帧，可更新位置估计。 */
    } else {
        /* 未找到小球，不能将零坐标当作球已回到中心。 */
    }
}
```

函数返回值表示是否有新协议帧，`data.found`才表示是否检测到球。接收模块只保留最新帧，主循环较慢时旧帧会被覆盖。

相机程序按处理后的每帧发送结果。实际串口帧率和视觉延迟需实测。

## 5. 控制链路与单位约定

```text
新视觉帧 → 检查found和时效 → 位置换算 → 速度估计与滤波
        → LQR期望滚动加速度 → 调度层换算并合成倾角 → 倾角/变化率限制
        → 机构映射 → 三电机绝对位置目标
```

### 5.1 位置与速度

可以先使用像素和像素/秒完成差分与滤波，再进入米制LQR计算。比例需要在圆盘中心附近、实际仰视成像条件下标定。

| 量 | 单位 |
| --- | --- |
| 协议`dx/dy` | 像素×100，`int16_t` |
| 像素位置、像素速度 | 像素、像素/秒 |
| `scale_x/scale_y` | 米/像素 |
| LQR的`x/y`、`vx/vy` | 米、米/秒 |
| 圆盘目标倾角 | 弧度 |
| 电机目标轴角度 | 度 |
| 电机反馈原始位置 | 65536单位/圈 |
| 电机位置命令 | 脉冲数，由步距角和细分决定 |

目标计算形式如下，变量名称仅用于说明公式：

```c
x_px = (float)dx / 100.0f;
y_px = (float)dy / 100.0f;
dt_s = (float)(sample_ms - previous_ms) * 0.001f;
vx_raw = (x_px - previous_x_px) / dt_s;
vy_raw = (y_px - previous_y_px) / dt_s;

alpha = dt_s / (0.04f + dt_s);  /* 40 ms时间常数，待验证 */
vx_px += alpha * (vx_raw - vx_px);
vy_px += alpha * (vy_raw - vy_px);

x  = x_px  * scale_x;
y  = y_px  * scale_y;
vx = vx_px * scale_x;
vy = vy_px * scale_y;
```

实际实现需处理第一帧、零时间差、丢球、长间隔和重新捕获。应先在毫秒整数上相减，再转浮点秒；不能把秒存回整数时间变量。建议记录完整帧接收时间，避免直接用主循环读取时刻估计采样间隔；接收时间仍不能消除相机处理延迟。

低通滤波平滑差分噪声，也增加响应滞后。40 ms是初始设计值，不是已调好的参数。

### 5.2 LQR目标倾角

每个方向使用“位置、速度”两个状态：

```c
theta_x = -(Kx[0] * x + Kx[1] * vx);
theta_y = -(Ky[0] * y + Ky[1] * vy);
```

位置项使球回中心，速度项提前制动。球经过中心但仍在运动时，输出通常不为零。

初版采用小角度、均匀实心球无滑动滚动近似：若正倾角定义为使球沿对应正轴加速，则`位置二阶导数 ≈ (5g/7) × 倾角`。该模型还假设圆盘能够较快跟随目标倾角；明显的执行滞后需要进一步建模。

K根据运动模型、控制周期和Q/R权重在电脑端离线计算，MCU使用固定增益做乘加。两轴模型和要求相同时，可先使用相同增益。增益不能在像素、米、度和弧度之间直接混用。

讨论过的**20 ms控制周期、3°总倾角限幅**仅为设计与仿真起点，调度层采用这两个联调初值，实际周期和倾角范围仍需在实机上验证。

### 5.3 三电机目标位置

在水平附近，可先标定局部线性映射：

```text
motor_target_deg[i] = default_angle[i]
                    + a[i] × theta_x
                    + b[i] × theta_y
```

`default_angle[i]`为水平时电机轴角度，单位度；`a/b`为倾角到轴角度的映射系数，若倾角用弧度，则系数单位为度/弧度。映射需根据连杆几何或实验标定，并确定平台中心高度等约束；较大角度可能需要非线性逆运动学。

在轴角行程内，将目标换算为以驱动器零位为基准的绝对脉冲：

```text
pulses = round(abs(motor_target_deg) × pulses_per_revolution / 360)
```

用目标符号和该轴方向标定生成方向字段。对于1.8°步距角、16细分，每圈3200脉冲。反馈角度使用独立的65536单位/圈。

RPM和加速度属于运动控制层。初期可固定参数进行小幅姿态测试；需要协调到达时间时，再根据各轴移动量和共同时间估算RPM，并考虑加减速、转速上限和通信延迟。当前驱动逐台发送，没有同步启动接口。

## 6. Emm42驱动约定

电机菜单需与驱动匹配：`P_Serial=UART_FUN`、`UartBaud=115200`、地址1～5、`Checksum=0x6B`、`Response=Receive`、`S_Vel_IS=Disable`。

| 接口 | 功能 |
| --- | --- |
| `emm42_init(motor)` | 初始化共享UART及软件状态，不发送运动命令 |
| `emm42_enable()` / `emm42_disable()` | 使能 / 关闭使能 |
| `forward()` / `backward()` | 速度模式，RPM单位 |
| `emm42_move_relative()` | 指定方向、RPM、加速度档位和相对脉冲数 |
| `emm42_move_absolute()` | 指定相对于驱动器零位的绝对脉冲目标 |
| `emm42_read_position()` | 读取实际位置，输出`int64_t`原始计数 |
| `emm42_position_to_degrees()` | 反馈计数换算为电机轴角度 |
| `emm42_read_position_error()` | 查询位置误差 |
| `emm42_read_status()` | 查询使能、到位、堵转和堵转保护 |
| `emm42_stop()` | 停止运动，保持使能 |
| `emm42_get_last_error()` | 查询最近一次驱动错误 |

```c
bool emm42_move_relative(uint8_t motor,
                         emm42_direction_t direction,
                         uint16_t speed_rpm,
                         uint8_t acceleration,
                         uint32_t pulses);
```

- `EMM42_MOTOR_ALL=0`在软件中展开为逐台操作，不是同步广播；读取只接受单台。
- 动作返回`true`代表 UART 已发送完整帧。显式读取接口等待有效应答。
- 相对命令每次产生新增运动，不能把同一绝对目标当作相对移动量反复下发。
- 位置模式速度范围为1到`EMM42_MAX_SPEED_RPM`（当前3000），脉冲为`uint32_t`；实际工作范围仍取决于机构。
- 位置命令直接使用传入的加速度档位；`emm42_set_acceleration()`只设置后续速度模式的档位。
- Emm V5文档中，1～255档位越大加速越快，0表示不使用曲线加减速；档位不是物理加速度，需要匹配实际固件。
- UART1采用阻塞轮询，每个等待阶段预算约30 ms，多阶段及多台电机会累加。应在主循环调用，不能与中断中的电机操作并发。

协议依据：[张大头Emm V5.0驱动说明书](https://blog.csdn.net/zhangdatou666/article/details/132644047)。

## 7. 主循环与电机调度

```c
while (1) {
    const uint32_t now_ms = get_current_ms();
    const IMU_Req imu = IMU_Update(now_ms);
    const Track_Req track = Track_Update(now_ms);
    const LQR_Req ball = LQR_Update(now_ms);
    Motor_Update(now_ms, &imu, &track, &ball);
}
```

- `Motor_Initial()` 发送一次使能、两轮零速及已标定的球盘水平位置。OLED 启动状态对应 UART 发送结果。
- 车轮上电静止。`Motor_SetTrackingEnabled(true)` 是后续按钮事件的接入点。开启后遇线行驶，丢线停轮。丢球时循迹继续。
- 电机诊断按键只在中断登记请求，由调度层在主循环读取并打印三轴位置。

## 8. 实机标定

在 `Software/ControlConfig.h` 集中填写轮径、轮距、减速比、左右轮方向，以及以驱动器绝对零位为基准的球盘水平角、轴行程、连杆映射和每圈脉冲数。球盘映射缺失时，调度层跳过球盘位置命令。标定像素比例和两轴 LQR 增益后，小球加速度需求生效。底盘运动前馈需填写车身前方、左方在图像坐标中的投影。

## 9. 开发环境与构建

| 项目 | 当前工程记录 |
| --- | --- |
| IDE | Code Composer Studio 71.0.1，Theia版本 |
| SDK | MSPM0 SDK 2.11.00.07 |
| 编译器 | TI Arm Clang 5.1.1.LTS |
| SysConfig | 当前生成规则使用1.28.1 |
| 架构 | Cortex-M0+，Thumb，软件浮点 |
| 调试器 | TI XDS110 |

推荐通过CCS构建：

1. 安装SDK和兼容工具链，通过CCS工程导入功能打开本目录已有工程。
2. 确认SDK、SysConfig及编译器路径；生成makefile包含本机绝对路径，换电脑后应由CCS重新生成。
3. 使用SysConfig打开`empty_mspm0g3507.syscfg`检查配置；不要手工修改生成的`Debug/syscfg/ti_msp_dl_config.c/.h`。
4. 执行Build并检查编译和链接诊断，目标输出为`Debug/Balance_Ball.out`及`.hex`。
5. 完成基本标定后，通过 XDS110 进行分阶段硬件调试。

`Debug/emm42_validation/` 中的电机驱动模拟用例覆盖动作发送、发送超时和反馈解析。

## 10. 下一步开发顺序

1. 实测视觉帧率和延迟，标定米/像素比例、三电机水平零位、方向、细分和连杆映射。
2. 做小幅姿态测试，测量到达时间，再确定 RPM、加速度和运动限制。
3. 根据实际周期与模型离线计算 LQR 增益，检查仿真输出后接入球盘控制。
4. 连接循迹启动按钮，核对轮向和 IMU 航向符号后调试底盘。

LQR计算参考：[MathWorks离散LQR及Q/R权重说明](https://www.mathworks.com/help/control/ref/lti.dlqr.html)。
