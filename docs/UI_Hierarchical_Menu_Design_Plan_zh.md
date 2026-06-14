# UI 分级菜单设计与实施计划

## 1. 目标

当前 UI 采用 12 个页面平铺切换，页面数量继续增加后，查找成本和按键切换次数都会明显上升。本次改造目标是：

1. 增加一级功能分类菜单。
2. 每个一级分类下显示对应的二级页面列表。
3. 保留现有页面的布局、动态刷新和业务逻辑。
4. 为 Auto Pilot、Setting 等未来功能预留稳定扩展位置。
5. 避免新增页面后继续修改大量导航判断代码。

推荐导航结构为：

```text
一级分类菜单 -> 二级页面列表 -> 现有页面内容
```

这里的“二级页面”包含页面列表和最终页面内容。页面列表负责选择，页面内容仍使用当前实现。

## 2. 页面信息架构

### 2.1 一级菜单

一级菜单固定为以下五项：

| 顺序 | 一级分类 | 用途 |
| --- | --- | --- |
| 1 | Run Route | 执行预设的固定路线 |
| 2 | Auto Pilot | 非固定路线的自主驾驶功能 |
| 3 | Sensors | 传感器观察、校准和调试 |
| 4 | Service | 工程维护、执行器手动控制、硬件测试和系统诊断 |
| 5 | Setting | PID、运动参数和系统参数设置 |

一级分类本身不因当前没有功能而隐藏，保证以后增加功能时菜单位置不变化。

### 2.2 二级页面归类

#### Run Route

| 二级项目 | 对应现有页面 | 状态 |
| --- | --- | --- |
| Fixed Route | Template Path | 已有 |

固定路线数量增加后，不建议为每条路线增加一个独立 UI 页面。应由 Fixed Route 页面内部使用路线图元表选择路线，继续复用现有路线确认、倒计时和运行状态界面。

#### Auto Pilot

Auto Pilot 的二级项目代表完整的自动驾驶算法模式，而不是单独的传感器功能或驾驶动作。当前算法尚未定型，先预留至少三个相互独立的算法槽：

| 二级项目 | 预期输入与用途 | 状态 |
| --- | --- | --- |
| Photo Pilot | 仅使用光电传感器循迹 | SOON |
| Vision Pilot | 主要使用相机完成道路识别和转向 | SOON |
| Fusion Pilot | 融合相机、激光雷达及其他传感器完成循迹和避障 | SOON |

这三个项目首先作为通用算法槽预留，内部建议使用稳定 ID，例如 `AUTO_PILOT_PHOTO`、`AUTO_PILOT_VISION` 和 `AUTO_PILOT_FUSION`。算法确定后可以修改显示名称和内部实现，但应保持菜单表结构与算法 ID 不变。后续增加算法时，只需向 Auto Pilot 菜单项表追加记录。

Smart Camera 和 LiDAR Sensors 调试页仍属于 Sensors。它们只负责观察设备数据，不应与使用这些设备的自动驾驶算法混为同一页面。

#### Sensors

| 二级项目 | 对应现有页面 |
| --- | --- |
| Encoder Data | Encoder Data |
| Photo Sensors | Photo Sensors |
| IMU Data | IMU Data |
| IMU Sum Data | IMU Sum Data |
| Smart Camera | Smart Camera |
| ADC Data | ADC Data |
| LiDAR Sensors | LiDAR Sensors |

该分类只放传感器数据观察、校准和输入状态诊断页面。一级菜单命名为 `Sensors`，避免暗示所有传感器页面都具备参数调整能力。

#### Service

| 二级项目 | 对应页面 | 归类理由 |
| --- | --- | --- |
| Hardware Test | `PAGE_UNITTEST` | 综合硬件测试入口 |
| Motor PID Monitor | `PAGE_MOTOR` | 只观察电机闭环状态，不直接控制电机 |
| Servo Manual | `PAGE_SERVO` | 监视舵机状态，并允许显式进入手动角度调节 |
| Thread Stats | `PAGE_THREADS` | 系统线程和调度诊断 |

Motor、Servo 不是传感器，因此放入 Service 比放入 Sensors 更清晰。这里的 Service 表示调试、维护、手动验证和系统诊断，不是用户运行路线时的常用功能。Motor 页面当前只显示模式、目标值、实际值、误差和 PID 输出，不具备控制功能，因此页面显示名应改为 `Motor PID Monitor`，避免名称误导。电机正反转、制动和滑行等动作继续集中在 Hardware Test，未来真正的电机 PID 参数编辑放入 Setting。

#### Setting

| 二级项目 | 说明 | 状态 |
| --- | --- | --- |
| Motor PID | 电机闭环参数 | SOON |
| Motion Params | 速度、加速度等运动参数 | SOON |
| Servo Params | 舵机中值、限位和映射参数 | SOON |
| System | UI、存储和系统级参数 | SOON |

## 3. OLED 界面方案

OLED 分辨率为 128x64，按当前 6x8 字体约可显示 21 字符、8 行。菜单文本应限制在这个范围内。

### 3.1 一级菜单

五个分类可以在同一屏完整显示：

```text
MAIN MENU       1/5
> Run Route
  Auto Pilot
  Sensors
  Service
  Setting

UP/DN   ENT:OPEN
```

设计规则：

- 第 0 行显示菜单标题和当前位置。
- 第 1 至 5 行显示五个一级分类。
- `>` 表示当前选择。
- 最后一行显示最必要的按键提示。
- 只有光标变化时才标记菜单重绘，不进行周期动态刷新。

### 3.2 二级页面列表

二级列表每屏最多显示 6 项，超过后滚动：

```text
SENSORS       2/7
  Encoder Data
> Photo Sensors
  IMU Data
  IMU Sum Data
  Smart Camera
  ADC Data       v
ESC:BACK ENT:OPEN
```

设计规则：

- 标题显示当前分类、选中序号和项目总数。
- 尾部 `v` 或 `^` 表示列表还有未显示项目。
- 返回二级列表时保留上次光标位置。
- 返回一级菜单时保留上次分类位置。
- `SOON` 项使用统一后缀，例如 `Vision Pilot [--]`。

### 3.3 未来功能占位

尚未实现的项目允许被选中，但按确认后只显示统一提示：

```text
VISION PILOT

   NOT IMPLEMENTED



ESC:BACK
```

占位页面由菜单模块统一绘制，不为每个未来功能创建空白页面源文件。

### 3.4 现有页面

进入现有页面后：

- 页面静态布局和动态数据布局保持不变；本计划明确改名的 `Motor PID Monitor`、`Servo Manual` 除外。
- 继续调用现有 `show_static`、`show_dynamic`、`on_enter`、`on_exit` 和 `on_key`。
- 页面退出后回到所属二级列表，不直接跳回一级菜单。

### 3.5 Servo Manual 页面

Servo 页面建议保留当前两路角度和脉宽显示，并增加明确的控制模式。进入页面时默认为 `MON` 监视状态，不立即取得舵机控制权：

```text
[09] Servo Manual
MODE MON    SEL SVO1
ANG      0        0
WDH   1522     1522
STEP      1deg

ENT:MANUAL
ESC:BACK
```

按 ENTER 后才进入 `MAN` 手动模式：

```text
[09] Servo Manual
MODE MAN    SEL>SVO1
ANG      0        0
WDH   1522     1522
STEP      1deg

L/R:-/+  UP/DN:SEL
ENT:MON  ESC:BACK
```

推荐按键语义：

| 按键 | MON 监视模式 | MAN 手动模式 |
| --- | --- | --- |
| ENTER | 请求进入手动模式 | 退出手动模式并释放控制权 |
| UP / DOWN | 无操作 | 选择 SVO1 / SVO2 |
| LEFT / RIGHT | 无操作 | 当前舵机角度减小 / 增加 1° |
| LEFT / RIGHT 长按重复 | 无操作 | 按固定 1° 步长连续调整 |
| ESC | 返回二级列表 | 释放手动控制权并返回二级列表 |

手动控制约束：

1. 页面打开时不能自动进入手动模式，避免查看数据时意外改变舵机输出。
2. 只有车辆和路线控制处于空闲状态时才允许进入 MAN；忙碌时显示 `BUSY`，不接管舵机。
3. UI 调用现有 `USER_SERVO_SetAngle()`，不直接写定时器比较寄存器。
4. 每次短按和每次长按重复事件都只变化固定 1°，便于精细调整。
5. 角度上下限使用舵机底层配置中的 `min_angle` / `max_angle`；当前 `USER_SERVO_SetAngle()` 已会自动限幅，因此 UI 不需要再按脉宽做二次限幅。
6. 角度限幅后自然映射到该舵机配置的最小/最大脉宽，不允许 UI 绕过角度接口设置超范围脉宽。
7. 退出 MAN 时释放 UI 控制权，但保持最后一次输出，避免自动回中造成机械突跳；后续控制算法取得控制权后再设置自己的目标值。

控制权建议使用独立状态表示，例如：

```c
typedef enum {
    SERVO_OWNER_NONE = 0,
    SERVO_OWNER_UI,
    SERVO_OWNER_ROUTE,
    SERVO_OWNER_AUTOPILOT
} ServoControlOwner;
```

手动页面只有成功取得 `SERVO_OWNER_UI` 后才能写输出。该机制可以避免未来路线线程、自动驾驶线程和 UI 同时设置舵机。

## 4. 按键规则

### 4.1 一级菜单

| 按键 | 行为 |
| --- | --- |
| UP / DOWN | 移动分类光标 |
| PREV / NEXT | 可作为 UP / DOWN 的别名 |
| ENTER | 进入所选分类的二级列表 |
| ESC | 无操作，或停留在一级菜单 |

### 4.2 二级页面列表

| 按键 | 行为 |
| --- | --- |
| UP / DOWN | 移动页面光标 |
| PREV / NEXT | 可作为 UP / DOWN 的别名 |
| ENTER | 打开页面或占位提示 |
| ESC | 返回一级菜单 |

### 4.3 页面内容

| 按键 | 行为 |
| --- | --- |
| PREV / NEXT | 仅切换当前分类内的相邻页面 |
| ENTER、方向键 | 继续交给当前页面处理 |
| ESC | 返回当前分类的二级列表 |

不再允许 PREV / NEXT 跨一级分类循环全部 12 个页面。

### 4.4 ESC 冲突处理

当前部分页面已经把 ESC 用于校准、参数调整或取消运行，不能直接在所有状态下强制返回。推荐引入统一的返回处理规则：

1. 页面空闲时，短按 ESC 返回二级列表。
2. Fixed Route 正在确认、倒计时或运行时，短按 ESC 优先取消当前操作；恢复空闲后再次按 ESC 才返回。
3. 其他页面原有的 ESC 功能迁移到 ENTER、LEFT/RIGHT 或页面内编辑模式，逐页确认后实施。
4. 页面描述符增加返回策略或 `on_back` 回调，不在核心代码中按页面编号写特殊判断。

该冲突是实施前必须完成的按键审计项，否则会出现某些页面无法返回或误触发校准的问题。

### 4.5 ESC 逐页影响清单

按当前代码扫描，真正占用 ESC 的页面只有 3 个：Photo Sensors、IMU Data 和 Fixed Route。其他页面即使注册了按键回调，也没有使用 ESC，可以直接由全局返回接管。

| 页面 | 当前 ESC 用途 | 是否冲突 | 建议处理 |
| --- | --- | --- | --- |
| Motor PID Monitor | 无按键回调 | 否 | ESC 直接返回二级列表 |
| Encoder Data | 未使用 ESC | 否 | ESC 直接返回二级列表 |
| Photo Sensors | 短按 ESC 降低当前 HIGH/LOW 阈值 50；长按 ESC 自动设置 LOW 阈值 | 是 | 阈值调节改为 LEFT/RIGHT 增减；自动校准改为长按 ENTER 循环执行 HIGH/LOW 或增加校准模式 |
| IMU Data | 短按 ESC 设置 yaw reference | 是 | yaw reference 改为长按 ENTER；原 ENTER 短按继续设置 angle reference |
| IMU Sum Data | 未使用 ESC | 否 | ESC 直接返回二级列表 |
| Smart Camera | 无按键回调 | 否 | ESC 直接返回二级列表 |
| ADC Data | 无按键回调 | 否 | ESC 直接返回二级列表 |
| LiDAR Sensors | 未使用 ESC | 否 | ESC 直接返回二级列表 |
| Motor PID Monitor / Servo Manual / Thread Stats / Hardware Test | 未使用 ESC | 否 | ESC 直接返回二级列表；Servo MAN 模式下 ESC 先释放 UI 控制权再返回 |
| Fixed Route | 任意 ESC 事件取消充电、倒计时或路线流程 | 是，但必须保留取消能力 | 忙碌状态下 ESC 仍然取消；空闲状态下 ESC 返回二级列表 |

Photo Sensors 的按键建议重新整理为：

| 按键 | 建议行为 |
| --- | --- |
| UP / DOWN | 选择 HIGH / LOW 阈值 |
| LEFT / RIGHT | 当前阈值 -50 / +50，支持长按连续调整 |
| ENTER 短按 | 对当前选中的 HIGH 或 LOW 执行自动设置 |
| ENTER 长按 | 同时自动设置 HIGH 和 LOW |
| ESC | 返回二级列表 |

IMU Data 的按键建议重新整理为：

| 按键 | 建议行为 |
| --- | --- |
| ENTER 短按 | 设置 angle reference，保持现有语义 |
| ENTER 长按 | 设置 yaw reference，替代原 ESC 短按 |
| ESC | 返回二级列表 |

Fixed Route 的返回策略需要做成页面状态相关：

```text
Idle                + ESC -> 返回二级列表
Charging/Countdown  + ESC -> 取消启动流程，停留在页面
Running             + ESC -> 取消/停止路线流程，停留在页面或回到 idle
```

因此核心层不能简单地在页面内容状态下无条件消费 ESC 返回菜单，而应先询问当前页面是否需要拦截返回。建议为页面描述符增加 `on_back` 或 `can_leave` 类回调。

## 5. 软件架构设计

### 5.1 保留现有页面注册表

现有页面描述符继续负责页面本身：

```c
typedef struct {
    DisplayPage_t page;
    const char *title;
    void (*show_static)(void);
    void (*show_dynamic)(void);
    void (*on_key)(...);
    void (*on_enter)(void);
    void (*on_exit)(void);
    uint8_t refresh_divider;
} UIPageDescriptor;
```

菜单层只保存“分类到页面”的映射，不复制页面回调。

### 5.2 新增菜单数据表

建议采用只读表驱动结构：

```c
typedef enum {
    UI_CATEGORY_RUN_ROUTE = 0,
    UI_CATEGORY_AUTO_PILOT,
    UI_CATEGORY_SENSORS,
    UI_CATEGORY_SERVICE,
    UI_CATEGORY_SETTING,
    UI_CATEGORY_COUNT
} UICategoryId;

typedef struct {
    const char *label;
    DisplayPage_t page;
    uint8_t flags;
} UIMenuItem;

typedef struct {
    UICategoryId id;
    const char *title;
    const UIMenuItem *items;
    uint8_t item_count;
} UIMenuCategory;
```

`flags` 至少支持：

- `UI_MENU_ITEM_ENABLED`
- `UI_MENU_ITEM_PLACEHOLDER`
- 后续可扩展 `HIDDEN`、`READ_ONLY` 等状态

增加页面或调整归类时，只修改菜单表，不修改导航状态机。

### 5.3 导航状态

建议增加三种显示模式：

```c
typedef enum {
    UI_VIEW_MAIN_MENU = 0,
    UI_VIEW_SUB_MENU,
    UI_VIEW_PAGE
} UIViewMode;
```

核心状态至少包括：

```text
view_mode
selected_category
selected_item[UI_CATEGORY_COUNT]
active_page
menu_dirty
```

每个分类独立保存二级光标位置，使用户返回分类后仍停留在上次访问页面。

### 5.4 页面生命周期

页面打开流程：

```text
二级列表 ENTER
-> 设置 active_page
-> 调用 on_enter
-> 绘制 show_static
-> 按 refresh_divider 调用 show_dynamic
```

页面退出流程：

```text
页面 ESC
-> 当前页面允许退出
-> 调用 on_exit
-> 切换到二级列表
-> 重绘菜单
```

菜单界面不执行页面动态刷新，避免后台页面继续写 OLED 缓冲区。

## 6. 文件组织建议

建议新增：

```text
User_UI/
  user_ui_menu.c
  user_ui_menu.h
```

职责划分：

| 文件 | 职责 |
| --- | --- |
| `user_ui_core.c` | 任务调度、输入事件、视图状态切换 |
| `user_ui_menu.c` | 菜单数据表、菜单绘制、光标和滚动 |
| `user_ui_pages.c` | 现有页面描述符注册 |
| `user_ui_page_*.c` | 页面自身显示和业务逻辑 |

不建议把所有菜单判断继续加入 `user_ui_core.c`，否则以后增加 Auto Pilot 和 Setting 页面时核心文件会再次膨胀。

## 7. 分阶段实施计划

### 阶段 1：菜单模型

- 新增分类枚举、菜单项结构和菜单数据表。
- 完成现有 12 个页面的归类映射。
- 增加三个视图模式和分类光标状态。
- 暂不修改现有页面实现。

验收：代码可以通过 IAR 编译，菜单表中的页面编号均能在页面注册表中找到。

### 阶段 2：一级和二级菜单

- 实现一级菜单静态绘制和光标移动。
- 实现二级列表、滚动指示和光标记忆。
- 上电默认显示一级菜单。
- 实现 Auto Pilot、Setting 的统一 SOON 占位。

验收：五个分类均可进入；二级列表无越界、无文字换行、无标题栏重叠。

### 阶段 3：接入现有页面

- 从二级列表进入现有页面。
- 复用现有页面生命周期和刷新周期。
- ESC 返回所属二级列表。
- PREV / NEXT 仅在同分类内切换页面。

验收：所有现有页面的布局和数据刷新与改造前一致。

### 阶段 4：按键冲突整理

- 审计所有页面对 ESC、ENTER 和长按事件的使用。
- 为运行中页面增加“取消优先、返回其次”的策略。
- 迁移传感器校准等与全局返回冲突的按键。
- 在页面描述符中声明特殊返回策略。
- 将 Motor 页面标题改为 `Motor PID Monitor`，保持只读监视。
- 为 Servo 页面增加 MON/MAN 状态、舵机选择和固定 1° 步长角度调节。
- 复用舵机驱动现有 `USER_SERVO_SetAngle()` 限幅能力，并增加控制权仲裁。

验收：任何页面都能明确返回，同时不会因为返回操作误启动、误校准或误修改参数。

### 阶段 5：整机验证与文档同步

- 使用 IAR 全量编译。
- 实机遍历所有分类和页面。
- 验证 OLED 100 FPS 刷新服务下菜单无闪烁。
- 更新 UI README、页面表和按键说明。

## 8. 验收清单

- [ ] 上电进入 MAIN MENU，不再直接进入任意二级页面。
- [ ] 一级菜单五项均完整显示且无越界。
- [ ] ENTER 可进入对应二级列表。
- [ ] 二级列表超过一屏时可正确滚动。
- [ ] ENTER 可进入已有页面，已有布局保持一致。
- [ ] ESC 可从页面返回二级列表。
- [ ] ESC 可从二级列表返回一级菜单。
- [ ] 返回后保留上次一级和二级光标。
- [ ] PREV / NEXT 不再跨分类切换页面。
- [ ] Fixed Route 运行中 ESC 先取消，空闲后 ESC 才返回。
- [ ] Motor PID Monitor 页面不提供可能误启动电机的控制按键。
- [ ] Servo 页面进入时默认为 MON，不改变当前舵机输出。
- [ ] Servo MAN 模式下可选择 SVO1 / SVO2，并以 1° 固定步长调节角度。
- [ ] Servo 角度始终被驱动限制在对应舵机配置范围内，并映射到合法脉宽。
- [ ] 路线或自动驾驶占用舵机时，Servo MAN 模式无法取得控制权。
- [ ] 退出 Servo 页面后 UI 控制权已释放，最后输出不会发生非预期跳变。
- [ ] SOON 项不会调用未实现代码或空函数指针。
- [ ] 菜单状态下不执行隐藏页面的动态刷新。
- [ ] 所有文本在 128x64 屏幕范围内，无自动换行污染下一行。
- [ ] IAR Debug/Release 配置均编译通过且无新增警告。

## 9. 建议结论

推荐采用“一级分类菜单 + 二级页面列表 + 原页面内容”的表驱动方案。它能够在不重写现有页面的情况下解决页面平铺问题，也为 Auto Pilot 和 Setting 预留稳定入口。

实施时最需要谨慎处理的是 ESC 的全局返回语义。应先建立统一返回机制，再逐页迁移冲突功能，不能简单地在 UI 核心层截获所有 ESC 事件。
