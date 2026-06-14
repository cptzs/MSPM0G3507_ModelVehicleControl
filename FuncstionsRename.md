# Functions Rename

> 状态：已完成当前一轮全局命名整改。本文档记录命名规则、已整改项和仍保留的例外。
> 最后更新：2026-06-14。

## 命名规则

项目公开函数统一使用：

```text
USER_Module_Action(...)
```

约定：

- `USER_` 固定前缀。
- 普通英文模块名使用首字母大写驼峰，例如 `Encoder`、`Servo`、`State`。
- 常见硬件/协议缩写可保留全大写，例如 `ADC`、`UART`、`CAN`、`IMU`、`OLED`、`PID`、`OEMT`。
- 专名按行业常见写法保留，例如 `LiDAR`。
- 不保留兼容宏；调用点已递归同步修改。

## 已整改的函数命名

| 旧命名 | 当前命名 | 说明 |
|--------|----------|------|
| `USER_SYSTEM_Init` | `USER_System_Init` | 普通词 `System` 不使用全大写。 |
| `USER_SYSTICK_*` | `USER_SysTick_*` | 与 `SysTick` 官方命名保持一致。 |
| `USER_ENCODER_*` | `USER_Encoder_*` | 模块名改为首字母大写。 |
| `USER_SERVO_*` | `USER_Servo_*` | 模块名改为首字母大写。 |
| `USER_STATE_*` | `USER_State_*` | 与 `USER_State_Task` 统一。 |
| `USER_lidar_Init` / `USER_LIDAR_Task` | `USER_LiDAR_Init` / `USER_LiDAR_Task` | `LiDAR` 使用专名大小写。 |
| `USER_OEMT_AN_*` | `USER_OEMT_*` | OEMT 不再区分 `AN` / `DG` 二级前缀。 |
| `USER_LBB_*` | `USER_BoardIO_*` | 函数层面用 `BoardIO` 表达 LED、Button、Buzzer 组合。 |
| `USER_OLED_put*` | `USER_OLED_Put*` | 动作段首字母大写。 |
| `USER_UART_GetRecieved*` | `USER_UART_GetReceived*` | 修正拼写。 |
| `USER_UI_UT_*` | `USER_UI_UnitTest*` | 展开 `UnitTest`，提升可读性。 |
| `USER_RegisterSchedulerTasks` | `USER_Scheduler_RegisterTasks` | `main.c` 内部 static 函数也按模块段命名。 |

## 文件命名整改

| 旧文件 | 当前文件 | 说明 |
|--------|----------|------|
| `firmware_info.c/.h` | `User_OS/user_firmware_info.c/.h` | 固件信息属于 OS/系统信息模块，文件名增加 `user_` 前缀。 |

## 保留项

| 名称 | 保留原因 |
|------|----------|
| `userlib_lbb.c/h` | 文件名历史保留；公开函数已统一为 `USER_BoardIO_*`。 |
| `USER_LBB_ButtonEvent_t` / `USER_LBB_ButtonStats_t` | 类型名暂保留，避免扩大数据类型重命名范围。 |
| `USER_STATE_Estimate_t` 等状态估测类型 | 类型名暂保留，函数命名已统一为 `USER_State_*`。 |
| `USER_UI_UT_ActionResult_t` | 类型名暂保留，函数命名已统一为 `USER_UI_UnitTest*`。 |
| `ADC` / `UART` / `CAN` / `IMU` / `OLED` / `PID` / `OEMT` | 硬件或协议缩写，允许全大写。 |

## 当前结论

公开函数调用已统一到当前标准命名；后续新增函数应直接按 `USER_Module_Action()` 编写，不新增兼容别名或旧命名宏。
