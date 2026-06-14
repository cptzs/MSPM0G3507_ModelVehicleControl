# Changelog

本文件记录固件版本变更历史。`VERSION` 文件只保留当前版本号，便于脚本、构建和人工快速读取。

## v1.0.8 - 2026-06-14

- 移除 `launch.json` 中的 `IAR: Download and Run` 伪调试入口，避免 VS Code 进入需要手动停止的调试会话。
- 保留 `tasks.json` 中的同名默认 build task，下载运行改为通过 `Ctrl+Shift+B` 或 `Tasks: Run Task` 执行。

## v1.0.7 - 2026-06-14

- 修正 `IAR: Download and Run` 的 VS Code 启动方式：下载由 task 执行，Run and Debug 入口只等待 task 完成后自动退出。
- `tools/iar_download_run.ps1` 改用 C-SPY `--download_only`，避免下载后停留在调试会话中。
- 注：Run and Debug 入口仍会创建 VS Code 调试会话，已在 `v1.0.8` 移除该入口。

## v1.0.6 - 2026-06-14

- 将 `IAR: Download and Run` 加入 VS Code `launch.json`，可从 Run and Debug 下拉框直接执行。
- 将同名 task 设置为默认 build task，支持 `Tasks: Run Build Task` / `Ctrl+Shift+B` 触发。
- 注：`launch.json` 入口会触发调试 UI，已在 `v1.0.8` 移除。

## v1.0.5 - 2026-06-14

- 新增 IAR 命令行下载运行脚本 `tools/iar_download_run.ps1`，构建后调用 `cspybat` 下载并保持目标板复位运行。
- 新增 VS Code task `IAR: Download and Run`，可直接触发下载运行流程而不进入交互调试界面。

## v1.0.4 - 2026-06-14

- 按当前代码和 IAR 工程同步 README 与技术文档，移除旧调度表、旧 UI 页面数、旧命名和旧固件信息文件路径表述。

## v1.0.3 - 2026-06-14

- 将固件信息模块文件重命名为 `user_firmware_info.c/.h`，统一工程文件命名风格。

## v1.0.2 - 2026-06-14

- 将 `firmware_info.c/.h` 移动到 `User_OS` 目录，并同步更新 IAR 工程源文件路径。

## v1.0.1 - 2026-06-14

- 修复 SysInfo 页面 LOAD 显示重复百分号的问题。
- 修正 SysInfo 页面 RAM 占用统计口径，避免固定显示 100%。
- 将 CPU 利用率统计改为基于 `__WFI()` 空闲时间计算，显示整体非睡眠时间占比。

## v1.0.0 - 2026-06-14

- 引入固件版本号，初始版本为 `v1.0.0`。
- 新增 Service 菜单下 SysInfo 页面，显示 CPU 型号、CPU 主频、CPU 利用率、RAM、FLASH 和固件版本。
- 将版本号同步到 `main.c` 文件头注释、`user_firmware_info.h` 和仓库根目录 `VERSION`。
