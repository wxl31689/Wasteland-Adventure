# Wasteland Adventure

第三人称 3D 单机角色冒险 RPG 正式工程。官方第三人称模板基线已完成任务 2.1 奔跑、2.2 闪避、2.3 摄像机、2.4 角色动画状态及 2.5 综合验收。第二阶段按报告记录的范围与限制验收通过，第三阶段尚未开始。

## 技术栈与环境

| 项目 | 本机实际版本 / 路径 |
| --- | --- |
| UE | 5.8.4，`F:\levn\game\Epic\UE_5.8` |
| VS | Community 2022 17.14.44，`F:\levn\game\VS2022Community`；使用 C++ 的游戏开发 |
| MSVC | 14.44.35229；family 目录 `VC\Tools\MSVC\14.44.35207` |
| Windows SDK | 10.0.26100.0，`C:\Program Files (x86)\Windows Kits\10` |
| Git / LFS | 2.33.0.windows.2 / 2.13.3，`F:\levn\app\Git\cmd` |
| 工程 | `F:\levn\game\Projects\WastelandAdventure\WastelandAdventure.uproject` |

显示名称包含空格；工程、模块、目标和资源路径使用 `WastelandAdventure`。

## 工程来源与配置

由 UE 5.8 官方 Games → Third Person → C++ 创建器生成，目标 Desktop，项目配置设为 Scalable，关闭硬件光线追踪和 Ray Tracing Proxies。默认地图 `/Game/ThirdPerson/Lvl_ThirdPerson`，默认 GameMode 为官方 BP_ThirdPersonGameMode。

用户已批准 UE 5.8 的版本差异：旧版 Starter Content 不适用，使用官方 Characters、LevelPrototyping、Input 资源；C++ 模板附带全部变体文件，保留但不将其认定为已开发的 RPG 功能。

## 目录

```text
WastelandAdventure/
  WastelandAdventure.uproject
  Config/
  Content/                  # 官方模板真实二进制资源
  Source/WastelandAdventure/ # 基础第三人称类及官方变体
  Docs/
  Tests/
  AGENTS.md
  .gitignore
  .gitattributes
```

解决方案和 Binaries、Intermediate、Saved 等为本机生成物，不纳入 Git；可通过 `.uproject` 重新生成解决方案。

## 构建与启动

在 PowerShell 中执行：

```powershell
Set-Location 'F:\levn\game\Projects\WastelandAdventure'
& .\Tests\BuildEditor.ps1
& .\Tests\VerifyFoundation.ps1
& 'F:\levn\game\Epic\UE_5.8\Engine\Binaries\Win64\UnrealEditor.exe' '.\WastelandAdventure.uproject'
```

也可双击 `.uproject`，或通过 Epic Launcher 打开。修改 C++ 前退出编辑器，使用 Development / Win64 / WastelandAdventureEditor 构建。显式构建命令：

```powershell
& 'F:\levn\game\Epic\UE_5.8\Engine\Build\BatchFiles\Build.bat' WastelandAdventureEditor Win64 Development '-Project=F:\levn\game\Projects\WastelandAdventure\WastelandAdventure.uproject' -WaitMutex -NoEngineChanges -NoHotReloadFromIDE
```

打开第三人称地图后点击 Play 并聚焦视口；WASD 移动，Left Shift 按住奔跑、松开恢复，Left Ctrl 触发闪避，鼠标控制视角，空格跳跃，Esc 或 Stop 停止。第一次加载可能需要编译着色器。

正常速度保留模板实测值 500 cm/s，奔跑初值为 750 cm/s。可在 `BP_ThirdPersonCharacter` 的 Class Defaults → Movement → Sprint 调整 Walk Speed / Sprint Speed。更改 C++ 默认值后应关闭编辑器、正常构建并重启；不依靠 Live Coding 验收反射字段。

原有 Input Action、IMC 和动画蓝图状态图保持原样；人物蓝图配置闪避动画引用，C++ 创建仅供当前 Pawn 使用的临时 Enhanced Input 奔跑与闪避映射。停止、取消输入、输入清空、失去控制权或结束游戏时清除相应移动状态和闪避动画。不新增游戏 Tick 或体力功能。

闪避配置在 Class Defaults → Movement → Dodge：Dodge Distance 初值 450 cm、Dodge Duration 0.25 s、Dodge Speed 1800 cm/s、Dodge Cooldown 1 s。实际速度取 `min(DodgeSpeed, DodgeDistance / DodgeDuration)`，距离和速度都是上限；碰撞会缩短实际距离。冷却从成功触发时计算，至少覆盖持续时间。Dodge Key 可在 Input 分类调整，默认 Left Ctrl（EKeys::LeftControl）。

有 WASD 输入时沿相机朝向转换后的输入方向闪避；无输入时沿人物前方。空中禁止闪避，闪避期间不接受新的移动方向或跳跃，视角仍可控制。原 Shift 持有意图在结束后继续生效；松开 Shift 恢复正常速度。输入清空/取消只中断位移，保留冷却；失去控制权和结束游戏清理定时器。

闪避动作复用官方 MM_Dash，经 UE 复制生成 `Content/ThirdPerson/Animations/AS_Dodge_InPlace.uasset`，关闭资源根运动并强制锁根，原 MM_Dash 保留原样。动作通过现有 DefaultSlot 播放，完整片段按 Dodge Duration 配速，起止与中断淡出；位移继续由原有碰撞安全 RootMotionSource 负责。闪避开始时人物朝向所接受的移动方向。动画引用和淡入淡出时间在人物蓝图 Class Defaults → Animation → Dodge 配置。未加入无敌帧、伤害或体力消耗。

关闭编辑器后运行 `Tests/RunSprintPIE.ps1`，可在真实编辑器 PIE 世界中执行 `WastelandAdventure.Character.SprintPIE` 输入与物理回归测试。原始结果保存在 `Saved/Verification/Phase2_1/`；图形检查和自动测试的实际结论见 `Docs/Phase2_1Sprint.md`。

任务 2.2 的真实 PIE 测试入口为 `Tests/RunDodgePIE.ps1`，结果保存在 `Saved/Verification/Phase2_2/`；范围、真实构建/运行结果和未验证项见 `Docs/Phase2_2Dodge.md`。测试场地和参数修改仅存在于 PIE，不保存资产。

摄像机复用原 CameraBoom / FollowCamera：保留 400 cm 臂长、Camera 通道球形碰撞和即时鼠标旋转，启用位置平滑（Camera Lag Speed 12、Camera Lag Max Distance 75 cm、子步长 1/60 s）。在 `BP_ThirdPersonCharacter` 的 CameraBoom 组件 → Camera Lag 中调整现有参数。俯仰范围在 `BP_ThirdPersonPlayerController` 的 Class Defaults → Camera → Pitch 配置，初值 -65° 至 +65°；BeginPlay 应用到现有 PlayerCameraManager，反向端点自动排序并限制到 ±89°。

任务 2.3 的真实 PIE 测试入口为 `Tests/RunCameraPIE.ps1`，结果保存在 `Saved/Verification/Phase2_3/`；实际构建、摄像机/原功能回归及图形检查见 `Docs/Phase2_3Camera.md`。`Tests/InspectCamera.py` 可从 UE Python 控制台只读导出当前默认地图的蓝图摄像机默认值。运行时俯仰范围以 PIE 测试的实际 CameraManager 实例为准。

动画继续使用 `ABP_Unarmed` 的 Idle、Walk / Run、Jump、Fall Loop、Land 状态和原有 IK。GroundSpeed 取实际二维速度，IsFalling 来自 CharacterMovement。原 BlendSpace 的走/跑采样行从 300/600 调整为已验收的 500/750 cm/s，并按速度比例调整样本播放率；角色移动速度不变。

任务 2.4 的真实测试入口为 `Tests/RunAnimationPIE.ps1`，同时执行 AnimationPIE、CameraPIE、DodgePIE、SprintPIE。AnimationPIE 读取真实状态机、运行中的 BlendSpace 节点、实际骨骼变换和蒙太奇权重，结果位于 `Saved/Verification/Phase2_4/`，详见 `Docs/Phase2_4Animation.md`。`Tests/ConfigureAnimation.py` 是本次带基线哈希保护的一次性 UE 资源配置脚本，正常运行测试不加 `-ConfigureAssets`；禁止覆盖已有闪避资源。

关闭编辑器后运行 `Tests/RunPhase2Acceptance.ps1` 执行第二阶段完整回归：AnimationPIE、CameraPIE、DodgePIE、SprintPIE，以及 Integration30FPS / Integration60FPS。测试使用有图形渲染的真实 PIE、实际蓝图人物、Enhanced Input、运动物理、动画与摄像机；综合场景包括奔跑跳跃、闪避后立即移动、快速 Shift / Left Ctrl、输入清空、墙体碰撞、原地图真实斜坡和平台边缘。临时碰撞夹具与帧率上限只用于测试，不保存地图或资产。

2026-11-02 的完整复测六项均 Success，编辑器与验证脚本退出码均为 0；综合测试实测约 30.0 / 59.8 FPS。这是两个帧率条件下的功能验收，未据此承诺发布版本性能。首轮 CameraPIE 曾有两条断言失败，同代码、同断言的前台复测通过，最初输入提前停止的原因尚未确证。原始失败与成功记录均保存在 `Saved/Verification/Phase2_5/`，详情见 [2.5 综合验收报告](Docs/Phase2_5Acceptance.md)。

## 验收状态

第一阶段已有真实完整 Editor 构建（400.34 秒）和显式增量构建（245.42 秒，退出码 0），历史分项限制保留在 `Docs/Phase1Acceptance.md`。任务 2.1—2.4 的记录见相应阶段报告；第二阶段最终综合验收见 `Docs/Phase2_5Acceptance.md`。本次实际编译了新增综合测试并完成六项真实 PIE 回归，第二阶段按已记录范围验收通过，第三阶段未开始。

项目仓库为 [wxl31689/Wasteland-Adventure](https://github.com/wxl31689/Wasteland-Adventure)。首次上传包括已完成的第一、二阶段及当前已保存的地图编辑；地图在 2.5 验收后有新增资源与 Actor 修改，上轮测试不覆盖这些后续地形编辑。快照范围、Git LFS 获取方式及本机凭据排除说明见 [仓库快照说明](Docs/RepositorySnapshot.md)。各阶段报告中的“未提交/未推送”描述保留为当时的历史状态。
