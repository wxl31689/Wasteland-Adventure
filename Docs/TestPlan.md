# 测试与验收要求

| 层级 | 执行方式 | 通过证据 |
| --- | --- | --- |
| 环境 | 实际调用 Git/LFS/UBT；MSVC 编译、链接并运行 Windows API 测试程序；UBT ValidatePlatforms | 真实版本、退出码、Win64 VALID、测试程序输出。 |
| 结构与配置 | `Tests/VerifyFoundation.ps1` | 文件和配置断言通过；此结果不能替代构建或运行。 |
| 构建 | `Tests/BuildEditor.ps1`，Development Editor / Win64 | UHT、编译、链接和最终 Result: Succeeded；脚本退出码 0；区分完整构建与增量/Up to date。 |
| 编辑器加载 | 打开正式 `.uproject` 和 Lvl_ThirdPerson | 正确地图、GameMode、角色资源加载，无阻止运行的错误。 |
| 实际游戏 | 在图形编辑器 PIE 中测试 W/A/S/D、鼠标相机、空格跳跃、Esc/Stop | 分项操作记录、实际画面/位置变化和相关日志；未执行不能标通过。 |
| Git/LFS | 检查工作树与索引、attributes、真实 LFS 指针及对象 SHA-256 | 资源进入 LFS；生成目录被排除；没有凭据；首次 commit 等待用户。 |
| 后续功能 | 每阶段增加针对业务边界的自动化和人工测试 | 可重复步骤、期望值、真实结果，不能仅写与实现相同的静态断言。 |
| 打包（阶段 10） | UAT Windows Development/Shipping，独立启动并离线验证 | 打包日志与退出码、独立进程实测；本阶段不声称打包通过。 |

记录规范：使用 Asia/Shanghai 日期和时间，命令、引擎/工具版本、构建目标、日志路径、操作者（Codex UI 或用户人工）、实际结果及未验证范围。日志保存在 Saved/Verification，不入 Git；文本报告保存在 Docs。

运行时检查 Output Log 中的 Error、Fatal、Ensure、蓝图编译失败、资源加载失败。区分影响游戏的错误和可选编辑器/在线服务警告，保留真实情况，不通过隐藏日志取得通过。

代码、资源、项目配置改变后重新构建并运行相关用例。文档修改仍按 AGENTS 执行实际增量构建。PIE 不等于已完成独立 Windows 包验收。
