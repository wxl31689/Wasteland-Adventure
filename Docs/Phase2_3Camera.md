# 任务 2.3：摄像机系统

## 1. 当前任务

2026-11-01，按用户明确选择仅执行 2.3，验收后停止。保留已完成的奔跑和 Left Ctrl 闪避；不开始 2.4、2.5 或第三阶段。验收状态：实际冷构建、完整 D3D12 PIE 自动验收和正常编辑器图形复查均通过。

工程：`F:\levn\game\Projects\WastelandAdventure\WastelandAdventure.uproject`；UE 5.8.4，Win64，Development Editor。

## 2. 实现功能

- 复用实际默认 Pawn `BP_ThirdPersonCharacter_C` 的 CameraBoom / FollowCamera，保留 400 cm 臂长、原鼠标输入与 Camera 通道碰撞球（12 cm）。
- 位置平滑初值：Camera Lag Speed 12，Max Distance 75 cm，开启子步积分，Max Time Step 1/60 s。保留即时旋转，无旋转滞后。
- 复用 `BP_ThirdPersonPlayerController_C` 的现有 PlayerCameraManager，俯仰范围初值 -65° 至 +65°，Blueprint 可配置。
- 未增加第二套摄像机、游戏 Tick、输入资产或动画资源。

修改前通过 UE 原生 Python 读取实际地图、GameMode 和蓝图默认对象；原始 JSON 位于 `Saved/Verification/Phase2_3/CameraDefaults-*.json`。原配置 position_lag=false、rotation_lag=false、arm_length=400、collision=true、probe_size=12、probe_channel=Camera。蓝图默认对象没有显式 CameraManager 类，运行时实例由真实 PIE 核验，不以类默认对象推断运行结果。

## 3. 文件变更

修改：

- `Source/WastelandAdventure/WastelandAdventureCharacter.cpp`
- `Source/WastelandAdventure/WastelandAdventurePlayerController.h`
- `Source/WastelandAdventure/WastelandAdventurePlayerController.cpp`
- `README.md`

新增：

- `Source/WastelandAdventure/Tests/CameraPIETest.cpp`
- `Tests/InspectCamera.py`
- `Tests/RunCameraPIE.ps1`
- `Docs/Phase2_3Camera.md`

既有文件修改前备份在 `Saved/Verification/Phase2_3/Baseline/`，另有 D 盘工作备份。首次 commit 未获批准，未暂存、提交或推送本次改动。最终检查：全部 753 个 Content 文件的数量及 SHA-256 与本次修改前基线一致；VerifyFoundation / VerifyRepository 通过，索引仍为 859 文件，753 个真实 LFS 指针、工作文件及本地对象哈希均通过核验。本次改动文件差异格式检查通过。

## 4. 核心代码

Character 构造函数只调整既有 SpringArm 的位置平滑参数。已有 SpringArm 原生更新、球形碰撞 sweep 和回缩负责相机行为，不新增角色 Tick；原 Look / DoLook 路径不改。

PlayerController::BeginPlay 调用父类后，把可配置端点应用到实际 PlayerCameraManager。非有限值回退默认值，端点限制到 ±89°，颠倒时排序。相机管理器原有输入更新负责俯仰限制，不另写一套旋转逻辑。

配置位置：Character Blueprint → CameraBoom → Camera Lag；Controller Blueprint → Class Defaults → Camera → Pitch。更改原生默认值后使用关闭编辑器、冷构建、重启流程。

CameraPIE 测试加载官方第三人称地图并启动实际渲染 PIE，获取真实玩家及其组件，注入 MouseX / MouseY 配对轴和 W / Space / Left Ctrl 输入。临时地面和墙体只在 PIE 世界创建、不保存。平滑距离由原生 SpringArm 未碰撞位置反推，与实际 arm origin 比较；运动期间采样有限值、方向和滞后界限，检查停止收敛。碰撞检查真实 Camera 通道球形 sweep、墙面前的相机位置、障碍移除后的恢复。

## 5. 编译情况

关闭已保存编辑器后执行官方 UBT ProjectFiles，退出码 0，26.84 s。日志：`Saved/Verification/Phase2_3/GenerateProjectFiles.log`。VS2022 不支持 .NET10 C# 工具项目的提示属于解决方案生成提示，本次 C++ 使用已安装的 VS2022，不升级工具链。

实际命令：

```powershell
& 'F:\levn\game\Projects\WastelandAdventure\Tests\BuildEditor.ps1'
```

脚本实际调用 `Build.bat WastelandAdventureEditor Win64 Development -Project=... -WaitMutex -NoEngineChanges -NoHotReloadFromIDE -MaxParallelActions=1`。

首次真实构建成功，退出码 0，103.13 s；UHT 生成 2 个文件，执行 8 个构建动作，包括 CameraPIETest.cpp、Character、PlayerController 和 DLL 链接。日志：`Saved/Verification/EditorBuild-20261101-131447.log`。

调整测试输入时长及清空残留鼠标增量的步骤后重新实际编译 CameraPIETest.cpp、链接 DLL：退出码 0，28.14 s。日志：`Saved/Verification/EditorBuild-20261101-132901.log`。游戏相机源码未因测试失败改变。

保存 README 和本报告后再次执行 BuildEditor：Up to date，退出码 0，12.99 s，日志 `Saved/Verification/EditorBuild-20261101-133527.log`。此项仅为最终增量检查，实际源码编译证据为前两次构建日志。

## 6. 运行验收

`Tests/RunCameraPIE.ps1` 使用真实图形编辑器，依次运行 CameraPIE、DodgePIE、SprintPIE；同时核验进程退出码和三个测试的 JSON Success 状态。不会把启动进程或退出码单独当成通过。

第一轮默认 D3D12 启动在 PIE 开始前发生 `DXGI_ERROR_DEVICE_HUNG`，进程退出码 3，没有生成测试报告。保留 `Saved/Verification/Phase2_3/Run-20261101-131718/CameraPIE.log` 和引擎生成的 GPU 转储；此轮为失败，不计入验收。

D3D11 重试使用 `Tests/RunCameraPIE.ps1 -RHI D3D11`。`-d3d11` 仅作用于该进程，没有修改 DefaultEngine.ini、引擎或显卡设置，也没有使用 NullRHI。

失败及修复记录（原始报告保留，不覆盖）：

| 运行目录 | 实际结果及处理 |
| --- | --- |
| `Run-20261101-131838` | D3D11 首次生成着色器缓存，CameraPIE 达到 150 s 等待上限而失败；DodgePIE / SprintPIE 均 Success。缓存生成完成后保持源码及断言重跑。 |
| `Run-20261101-132729` | CameraPIE 完成真实移动、碰撞、跳跃与闪避，但反向俯仰输入只从 +65° 送到 -38.25°，未达到 -65°，该断言失败。DodgePIE / SprintPIE 均 Success。修正测试将鼠标输入持续 2 s、每次 20 单位，并在恢复水平视角前等待残留输入消费；没有放宽端点断言。 |

两轮测试中相机实际最大滞后分别可见 `WALK_CAMERA`、`JUMP_CAMERA`、`DODGE_CAMERA` 采样日志；其中一次测得行走 33.330 cm、跳跃 28.190 cm、闪避 75.000 cm。它们是失败轮中的分项记录，不能替代完整通过结果。

最终自动验收：默认 D3D12 重跑 `Run-20261101-132944`，编辑器退出码 0，CameraPIE / DodgePIE / SprintPIE 全部 Success，三个测试各 0 errors / 0 warnings。报告：`Saved/Verification/Phase2_3/Run-20261101-132944/Report/index.json`；原始日志同目录 `CameraPIE.log`。

| 检查 | 最终实际结果 |
| --- | --- |
| 组件复用 | 真实蓝图玩家只有 1 个 CameraComponent、1 个 SpringArm；原有附着关系与 400 cm 臂长保留。运行时使用 `/Script/Engine.PlayerCameraManager`。 |
| 鼠标水平 / 垂直 | 实际配对轴输入改变 yaw / pitch，摄像机方向即时跟随。 |
| 俯仰范围 | 相同原有输入链实际达到 +65.00° 与 -65.00°，重复输入仍受限。 |
| 行走平滑跟随 | 实际 W 输入移动；48 个运动采样，最大滞后 33.329 cm；松开后 0.7 s 检查收敛到 <1 cm。 |
| 墙体碰撞 | Camera 通道球形 sweep 命中，400 cm 臂长回缩至约 163 cm，相机球停在墙面前至少 12 cm；移除墙体后恢复完整 400 cm。 |
| 跳跃 / 落地稳定 | 真实 Space 输入、角色升高、正常落地；89 个采样，最大滞后 28.190 cm，无非有限变换或异常旋转，落地后收敛。 |
| Left Ctrl 闪避稳定 | 真实 LeftControl 触发约 450 cm 位移；24 个采样，最大滞后 75.000 cm；结束后恢复正常，0.6 s 后滞后 <1 cm。 |
| 原功能回归 | 本轮完整 DodgePIE、SprintPIE Success，涵盖方向移动、500/750/500 速度、跳跃、鼠标、冷却、墙体和平台边缘、取消输入等既有断言。 |
| 日志 | 从 CameraPIE Test Started 到三项测试队列完成的范围内，PIE / Script / Blueprint / World / Windows / AutomationController Error、Fatal、Assertion、Ensure 匹配数 0；不能据此宣称整个启动日志无错误。 |

图形复查（正常交互编辑器、默认 D3D12）：实际观察官方地图与玩家正常渲染；聚焦游戏视口后拖动鼠标，画面水平及垂直视角均变化；按 Space 看见角色起跳，随后回到落地姿态；按 Left Ctrl 看见角色向附近墙体移动，相机随动；Shift+F1 后点击 Stop，恢复绿色 Play，关卡 Actor 数由 73 回到 64，状态栏显示全部已保存。随后正常关闭编辑器，无资源保存提示。

本轮图形过程为定性复查，角度、滞后距离与回缩距离以自动 PIE 的实际组件数据为准。没有把短按键或静态截图作为持续移动速度的测量。日志：`Saved/Verification/Phase2_3/CameraEditor.log`，本轮 PIE 世界从 13:32:13 至 13:34:15；该运行区间的关键错误匹配数 0。

复现步骤：关闭编辑器运行 `Tests/BuildEditor.ps1`，再运行 `Tests/RunCameraPIE.ps1` 并检查三个 JSON Success。手动打开默认地图、Play 并聚焦视口，移动鼠标至上下限、WASD / Shift 行走奔跑、Space 跳跃、Left Ctrl 闪避，靠近现有关卡墙体旋转视角并离开，最后 Stop。临时兼容排查可显式加 `-RHI D3D11`，默认仍使用原 D3D12。

## 7. 遗留问题

- 首次默认 D3D12 发生 GPU 启动崩溃，后续同一默认渲染模式已实际完整运行通过。崩溃根因未定位，保留为环境稳定性观察项，不能声称已修复显卡问题。必要时可用 `Tests/RunCameraPIE.ps1 -RHI D3D11` 进行临时兼容排查。
- 启动日志仍有此前即存在的 13 条 `LogAutomationTest: Error: Condition failed`；它们位于项目测试开始前，本次三个测试结果及运行范围没有这些错误。未修改引擎来掩盖该信息。
- 原 Inspector 首次读取空 CameraManager 类导致 Python 异常；已改成如实输出 null，重跑读取成功。没有因此更改蓝图资源。
- 全工程 `git diff --check` 发现此前已有 `Config/DefaultEngine.ini:262` 的 EOF 空行；本次三个源码文件的单独检查通过，未改写既有配置来消除历史格式问题。
- 专用闪避动画仍属于 2.4 范围，当前保持已有位移原型。

## 8. 下一任务

仅说明：2.4 角色动画状态。未自动开始；第二阶段整体和第三阶段均未标记完成。
