# Wasteland Adventure — 2.5 综合验收

验收日期：2026-11-02（北京时间）。结论：第二阶段 2.1—2.5 按本文范围验收通过。依据为新增测试的实际 C++ 编译、六项真实图形 PIE 回归及交互试玩；第三阶段未开始。首轮相机失败和当前限制保留在第七节。

## 一、当前任务

完成 2.5 角色控制、状态边界、动画、摄像机与碰撞综合验收。沿用 UE 5.8.4 官方 Third Person 地图、实际 BP_ThirdPersonCharacter 与 BP_ThirdPersonPlayerController。Left Ctrl 继续用于闪避。2.1—2.4 历史实现与验收分别见同目录对应报告。

## 二、实现功能

新增两个可重复运行的综合测试，分别在 30 / 60 FPS 上限下验证完整输入、物理与表现链路；新增统一入口同时执行全部六项角色测试。本次没有修改游戏行为、原有游戏 C++、Config 或 Content，也没有开发战斗、属性或其他 RPG 功能。

## 三、文件变更

| 文件 | 作用 |
| --- | --- |
| `Source/WastelandAdventure/Tests/MovementIntegrationPIETest.cpp` | 新增 Integration30FPS / Integration60FPS，测试真实输入、运行状态、碰撞、摄像机、动画及原地图边界。 |
| `Tests/RunPhase2Acceptance.ps1` | 新增统一启动与结果校验；同时核对真实编辑器退出码及六项 JSON 测试状态。 |
| `README.md` | 记录复现入口、第二阶段状态及限制。 |
| `Docs/DevelopmentPlan.md` | 更新第二阶段完成状态，后续阶段保留计划。 |
| `Docs/Phase2_5Acceptance.md` | 本报告。 |

开始验收时保存了 754 个 Content 文件与 97 个原有 Source / Config 文件的 SHA-256 基线，位于 `Saved/Verification/Phase2_5/Baseline/`。最终重新计算结果见 `Saved/Verification/Phase2_5/FinalVerification.json`。此前已有的配置、资源与未提交修改均保留；新增测试文件不属于原有 97 文件。

## 四、核心代码与复现

`FMovementIntegrationCommand` 在真实 PIE 世界中通过 PlayerController 的 `InputKey(FInputKeyEventArgs::CreateSimulated(...))` 发送按下/释放事件，读取 CharacterMovement、RootMotion 闪避状态、DefaultSlot 动画权重及实际 CameraBoom / FollowCamera。自动化 latent command 按世界帧采样；未新增游戏 Tick。

平地和墙体用临时 UBoxComponent 夹具隔离场景；测试起点通过碰撞检查定位，后续移动由真实输入和物理执行。斜坡与平台由射线在原地图碰撞中发现：斜坡法线约 `(0,-0.447,0.895)`，平台高差 200 cm。上坡、下坡与越边都发生在原地图碰撞上。结束后清理输入、临时对象并恢复帧率变量，不保存资产。

关闭 UE 编辑器，在工程根目录运行：

```powershell
& .\Tests\BuildEditor.ps1
& .\Tests\RunPhase2Acceptance.ps1
```

统一测试使用 UnrealEditor.exe、默认 D3D12 图形渲染与 `-NoLiveCoding`；不是 NullRHI、静态检查或仅确认进程存在。脚本只在编辑器退出码为 0 且六项指定测试全部 Success 时输出 `PHASE2_ACCEPTANCE_PASS`。

## 五、编译情况

| 检查 | 实际结果 | 本机证据 |
| --- | --- | --- |
| 官方工程文件生成 | 成功，退出码 0，64.41 秒 | `Saved/Verification/Phase2_5/GenerateProjectFiles.log` |
| 新增综合测试真实编译、链接 DLL/lib、元数据生成 | Succeeded，退出码 0，61.92 秒 | `Saved/Verification/EditorBuild-20261102-140047.log` |
| 文档更新后的最终构建、基础检查 | 独立记录实际退出码；最终 Up to date 不替代上述源代码编译 | `Saved/Verification/Phase2_5/FinalVerification.json` |

未安装或额外下载工具，未修改引擎。工程文件生成仍有 VS2022 与 .NET 10 C# 工具项目的兼容提示，本次游戏 C++ 实际构建成功。

## 六、运行验收

最终完整运行目录：`Saved/Verification/Phase2_5/Run-20261102-140945/`。原始 `Report/index.json` 记录六项 Success，每项 errors=0、warnings=0，succeeded=6、failed=0、notRun=0。编辑器退出码与统一验证脚本退出码均为 0。该轮测试开始至 Queue Empty 区间未发现关键 PIE / Blueprint / Script / World / Windows / AutomationController Error、Fatal、Assertion 或 Ensure；不据此宣称整个编辑器启动日志没有错误，见第七节。

| 测试 | 状态 | 实际覆盖 |
| --- | --- | --- |
| AnimationPIE | Success | Idle、500 走、750 跑、Jump/Fall/Land、真实 BlendSpace 样本、骨骼与闪避蒙太奇、恢复。 |
| CameraPIE | Success | 成对鼠标轴、俯仰 ±65°、移动平滑、停下收敛、碰撞缩臂/恢复、跳跃和闪避跟随。 |
| DodgePIE | Success | Left Ctrl、方向锁定、持续时间/距离/速度上限、冷却、空中拒绝、取消、墙体、失控清理和平台边缘。 |
| Integration30FPS | Success | 下表全部综合场景，实测仿真 30.000 / 墙钟 30.001 FPS。 |
| Integration60FPS | Success | 下表全部综合场景，实测仿真 59.844 / 墙钟 59.843 FPS。 |
| SprintPIE | Success | 实际 Enhanced Input、500/750 切换、释放/取消、跳跃、墙体及失控恢复。 |

| 综合场景 | 两种帧率下的结果 |
| --- | --- |
| 行走 → 奔跑 | W 达到实际 500 cm/s，Shift 后达到实际 750 cm/s。 |
| 奔跑 → 行走 | 释放 Shift 后上限恢复 500，落地后继续行走。 |
| 奔跑中跳跃 | 起跳前实际速度 750，离地后同时保持水平移动并上升，随后正常落地。 |
| 闪避后立即移动 | 闪避中按住 W，结束后立即恢复位移；随后仍可奔跑。 |
| 闪避撞墙 | 胶囊停在墙面外侧，无穿透、残余滑动或卡住的闪避动画；保留已消耗冷却。 |
| 空中闪避 | 跳跃时 Left Ctrl 被拒绝，不消耗新冷却。 |
| 快速 Shift | 各 12 个按下/释放周期，速度上限依次为 750/500，无叠加。 |
| 快速 Left Ctrl | 各 16 次按下，只接受 2 次；启动间隔分别为 1.06667 / 1.10139 秒，没有绕过 1 秒冷却或重启活动蒙太奇。 |
| 松键与输入清空 | 松开停止移动；FlushPressedKeys 同时清除奔跑与闪避，无残余速度/动画，已消耗冷却保留。 |
| 相机靠墙 | 接近过程中球形探针始终位于墙外；碰撞采样 49 / 97 次；离开并停稳后恢复 400 cm 臂长。 |
| 原地图斜坡 | W 上坡、松开停止、S 下坡；42 / 81 个采样均保持着地。 |
| 原地图平台边缘 | 越过 200 cm 高平台即取消闪避、淡出动作并进入下落，落地恢复；水平行程分别 240.001 / 210.001 cm，均未在空中继续走完整 450 cm 闪避。 |

额外验证：同帧跳跃/闪避只接受一个有效动作；所有观察到的位置与速度有限；未发生异常速度叠加或持续同时处于闪避与下落状态。平台越边取消在不同帧间检测，允许一帧内行程差异，不要求两种帧率的边缘行程完全一致。

真实渲染帧保存在 `Saved/Verification/Phase2_5/Frames-20261102-141054-30/` 与 `Frames-20261102-141117-60/`；包括 ActualRampUphill.png、ActualPlatformEdge.png。已实际查看 30 FPS 上坡及 60 FPS 平台帧，能看到原关卡、人物与对应地形。

统一测试后又用正常编辑器打开原地图并进入交互 PIE。实际执行空格跳跃、鼠标拖动改变水平/垂直视角、Left Ctrl 闪避及恢复站立；最终停止 PIE（回到 64 个编辑器 Actor）、确认全部已保存并正常退出。短促的 Shift+W 输入不作为持续 750 cm/s 的证明，持续速度与组合操作以上述自动化实际采样为依据。手动运行日志为 `Saved/Verification/Phase2_5/ManualEditor.log`，PIE 时间为北京时间 14:16:59—14:21:25。

## 七、遗留问题与验收边界

1. 首轮 `Run-20261102-140410` 整体未通过：CameraPIE 有两条断言失败，其余五项 Success。W 测试从 X=20000 只到 X=20078.967 即停止，采样时相机已收敛到 lag=0.03，因此未满足位移与当时平滑偏差断言。移动期间最大 lag=25.192。原始失败 JSON / 日志完整保留。
2. 在保持编辑器前台条件下，同代码、同断言重新执行全部六项成功。首轮输入提前停止的原因尚未确证，不能宣称已定位或修复；本次没有修改相机实现或放宽断言。后续若重复出现，应继续采集焦点与输入取消事件。
3. 成功轮启动阶段、项目测试开始之前仍记录 13 条 `LogAutomationTest: Error: Condition failed`，与此前阶段启动记录相同；未在本任务中定位。六项测试 JSON 错误/警告均为零，不能把此结论扩展到整份启动日志。
4. 闪避沿用 2.4 的官方 Dash 基础动作，未制作按方向区分的专用翻滚；无敌帧、体力、伤害均不在第二阶段范围内。
5. 本次是在 30 / 60 FPS 上限下验证功能及物理边界，不是发布版本性能基准；未执行 Windows 打包、发布或第三阶段验收。原有阶段报告的历史限制继续保留。
6. 未执行 Git stage、commit 或 push；首次提交仍需单独明确授权。索引、原有 LFS 对象及资产保持情况见最终核验收据；Saved 证据为本机生成物，不纳入 Git。

## 八、下一任务

第二阶段在上述范围和已记录限制下完成，本次停在 2.5。第三阶段等待单独指令，未创建战斗、属性、敌人、UI 或其他业务系统。
