# MSPM0G3507 Model Vehicle Control (Basic)

简要说明：这是基于 MSPM0G3507 的模型小车控制基础工程，包含驱动、设备抽象、应用层状态机与算法实现。

构建/调试提示：
- 使用 IAR Embedded Workbench（项目文件在仓库根目录）。
- 可使用 `make`/`makefile` 在支持的环境下构建。

仓库结构（部分）：
- `User_Application/` 应用层
- `User_Algorithm/` 算法库（PID 等）
- `User_Devices/` 设备驱动
- `User_Peripheral/` 外设封装

许可：MIT。作者：cptzs
