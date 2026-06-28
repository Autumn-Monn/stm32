# CLAUDE.md

本文件为 Claude Code（claude.ai/code）在处理本代码仓库中的代码时提供操作指引。

## 项目概述

STM32F103C8T6 智能盆栽养护系统固件，使用 STM32CubeMX + HAL 库 + CMake (Ninja) + arm-none-eabi-gcc 工具链开发。项目按 `demo/STM32渐进式开发规划（HAL-only）.md` 中定义的阶段化开发流程推进，已完成阶段 1~3 + 4A（GPIO 点灯、按键轮询、EXTI 中断、蜂鸣器输出）。

## 回答语言偏好

回复时除专业技术名词，使用中文回答。

## 构建命令

```bash
# 配置（首次或修改 CMakeLists.txt 后）
cmake --preset Debug

# 编译
cmake --build build/Debug

# 输出产物：build/Debug/stm32-code.elf / .hex / .bin
```

工具链文件：`cmake/gcc-arm-none-eabi.cmake`（需要 `arm-none-eabi-gcc` 在 PATH 中）。

## 代码组织约定（关键）

工程严格分两层，业务代码不要放进 `Core/`：

- **`Core/`**：CubeMX 自动生成区。仅允许在 `main.c` 的 `USER CODE BEGIN/END` 块内编辑。CubeMX 重新生成时会保留这些块，但覆盖其余内容。
- **`User/Inc/`、`User/Src/`**：所有业务模块。新增源文件**必须**加入 `CMakeLists.txt` 的 `target_sources`（约第 46~52 行），否则不会被编译。
- **`main.c` 的边界**：`main()` 中只做 `xxx_init()` 与主循环里的 `xxx_task()` / `xxx_demo()` 调用；业务实现全部下沉到 `User/Src/*.c`。

CubeMX 工程文件为 `stm32-code.ioc`。修改引脚/外设后需用 CubeMX 重新生成代码，生成的 HAL 初始化代码位于 `Core/`，CMake 集成位于 `cmake/stm32cubemx/`（不要手改）。

## 模块约定

每个 `User/Src/<mod>.c` 模块遵循固定模式（参考 `beep.c`、`exti_demo.c`）：

- 提供 `<mod>_init()`，必要时还有 `<mod>_task()` 或 `<mod>_demo()` 在主循环调用。
- **非阻塞计时**：用 `HAL_GetTick()` + 时间戳比较实现周期性动作，**不允许在主循环里使用 `HAL_Delay()`**（会阻塞其他模块协作）。`beep_stage4a_demo()` 是标准模板。
- **中断回调转发**：`main.c` 的 `USER CODE BEGIN 4` 中提供 `HAL_GPIO_EXTI_Callback`，应在其中按 `GPIO_Pin` 分发到对应模块的回调函数（如 `exti_demo_gpio_exti_callback`）。中断里只做置标志位 + 软件去抖时间戳记录，业务处理留到主循环 `_task()`。
- **GPIO 名称**：使用 CubeMX 在 `main.h` 中导出的宏（如 `BEEP_GPIO_Port`/`BEEP_Pin`、`KEY_1_GPIO_Port`/`KEY_1_Pin`），不要硬编码端口/引脚号。

## 引脚分配基线

权威来源：`requirements/02-STM32F103C8T6引脚分配.md`。改动硬件分配必须先更新该文档与 `.ioc`，再同步代码。关键映射：

| 功能                            | 引脚              | 备注                                   |
| ------------------------------- | ----------------- | -------------------------------------- |
| LED 绿 / 红 / 蓝                | PA7 / PB0 / PB1   | 高电平有效（板载 LED PC13 低电平有效）；颜色语义遵循设计要求：**绿=正常、红=缺水、蓝=低温**（超温属报警，复用红灯闪烁+蜂鸣器） |
| ESP8266 RST                     | PB10              | GPIO 输出，复位控制                    |
| 按键 K1~K4                      | PB12~PB15         | 上拉输入；K1 配有 EXTI                 |
| 蜂鸣器                          | PA3               | 高电平触发                             |
| 继电器 IN1（水泵）/ IN2（风扇） | PA1 / PA2         | GPIO 输出                              |
| ADC（土壤湿度）                 | PA0               | ADC1_IN0                               |
| DS18B20                         | PA4               | 软件单总线                             |
| USART1（ESP8266）               | PA9 TX / PA10 RX  |                                        |
| I2C1（OLED + AT24C02）          | PB6 SCL / PB7 SDA |                                        |

## 渐进式开发流程

项目按 `demo/STM32渐进式开发规划（HAL-only）.md` 与 `requirements/03-渐进式开发任务清单.md` 分阶段推进。每个阶段完成后必须在 `requirements/实施记录/阶段XX-名称.md` 留一份记录（模板见 `requirements/实施记录/README.md`）。

**不要跨阶段开发**——例如在 ADC 还没打通前不要先写联动控制。进入下一阶段前应满足：当前阶段可独立编译、可下载、有可观察的现象、有实施记录归档。

## 提交信息风格

参考 `git log` 现有风格：`V1.0.x：完成阶段X，<简述>`。版本号与阶段进度对应递增。
