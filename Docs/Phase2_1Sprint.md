# 任务 2.1 — Shift 按住奔跑验收

日期：2026-11-01，Asia/Shanghai。正式工程：`F:\levn\game\Projects\WastelandAdventure`。

## 1. 当前任务

仅任务 2.1：角色奔跑系统。本次功能、实际 Editor 编译与真实 PIE 回归通过；第二阶段整体没有完成，未执行 2.2。

未把粘贴说明里的历史“全部通过”当作测试结果。开始时，第一阶段文档仍有未完成项；本次重新读取蓝图、启动新编译模块并执行输入和物理回归。

修改前仓库无 HEAD/首次提交，859 个第一阶段文件已暂存，`Config/DefaultEngine.ini` 另有此前的未暂存变化。保留索引及文件，没有 commit、reset、clean 或 push。origin 为 `https://github.com/wxl31689/Wasteland-Adventure.git`。

## 2. 实现功能

- Left Shift 按住请求奔跑，释放恢复正常速度。
- 正常速度保留蓝图和模板实测值 **500 cm/s**，奔跑初值 **750 cm/s**。没有机械采用示例 450，避免改动原有正常移动基线。
- WalkSpeed / SprintSpeed 为 `EditDefaultsOnly, BlueprintReadOnly` UPROPERTY，可在角色蓝图 Class Defaults 的 Movement → Sprint 调整；bIsSprinting 提供只读运行状态。
- 处理 Enhanced Input Started、Completed、Canceled；输入清空、失去控制权和 EndPlay 也清除奔跑。
- 重复按键只选择速度上限，不叠加速度。不改速度向量、不瞬移、不修改制动/跳跃/空气控制参数，不添加 Tick。
- 为后续体力规则保留 StartSprint、StopSprint 和集中 UpdateMovementSpeed 扩展点；本次不实现体力、闪避、战斗或网络逻辑。

实际蓝图/资源检查结果（由 Unreal Python 反射读取，并非按名称猜测）：

| 项目 | 本机实际值 |
| --- | --- |
| 地图及 GameMode | Lvl_ThirdPerson；地图 override 与项目默认均为 BP_ThirdPersonGameMode |
| Default Pawn | BP_ThirdPersonCharacter_C，确实派生自原 AWastelandAdventureCharacter |
| PlayerController | BP_ThirdPersonPlayerController_C |
| 原有映射 | IMC_Default、IMC_MouseLook |
| 原有 Action | IA_Move、IA_Jump、IA_Look、IA_MouseLook |
| 原有按键 | WASD/方向键/手柄左轴；空格/手柄正面下键跳跃；Mouse XY 2D/手柄右轴视角 |
| 移动组件 | MaxWalkSpeed 500；JumpZVelocity 500；AirControl 0.35；BrakingDecelerationWalking 2000 |
| 摄像机 | 原 CameraBoom + FollowCamera；臂长 400，跟随控制旋转，碰撞检测已启用 |
| 人物与动画 | SKM_Quinn_Simple；ABP_Unarmed |

变体中的 Dash 属于既有平台变体；当前基础 Pawn 不使用它，本次没有移植或扩展。

## 3. 文件变更

修改：

- `Source/WastelandAdventure/WastelandAdventureCharacter.h`
- `Source/WastelandAdventure/WastelandAdventureCharacter.cpp`
- `Source/WastelandAdventure/WastelandAdventurePlayerController.h`
- `Source/WastelandAdventure/WastelandAdventurePlayerController.cpp`
- `Source/WastelandAdventure/WastelandAdventure.Build.cs`：仅 Editor 目标增加 UnrealEd 私有依赖，供真实 PIE 自动化使用。
- `README.md`：控制方式、参数调整、测试入口和当前阶段说明。

新增：

- `Source/WastelandAdventure/Tests/SprintPIETest.cpp`：仅开发编辑器编译的真实输入/物理回归。
- `Tests/InspectCharacter.py`：只读读取蓝图、资源、输入、移动与摄像机默认值，不保存资产。
- `Tests/RunSprintPIE.ps1`：启动真实渲染的 Editor/PIE；同时核对编辑器退出码和 JSON 中测试状态，避免退出码 0 掩盖行为失败。
- `Docs/Phase2_1Sprint.md`：本记录。

原有 753 个 Content 文件逐个与修改前 SHA-256 比较，全部一致，文件数量也一致。没有改写 `.uasset`、`.umap`、GameMode、动画蓝图或原有输入资源。Config 文件最后写入时间均早于本任务；此前 DefaultEngine.ini 与暂存区的差异没有覆盖。本次没有修改引擎安装目录、生成代码或模板变体。

五个原有源码文件的修改前副本以及 Content 哈希清单位于 `Saved/Verification/Phase2_1/Baseline/`，README 修改前也保留副本。它们是本机恢复材料，不能替代建议建立的 Git 基线。

## 4. 核心代码

Character 在本地输入组件就绪后，用 `NewObject` 创建带 GC 引用的临时 Boolean Input Action 和只映射 Left Shift 的 IMC，以优先级 1 添加到现有 LocalPlayer 的 Enhanced Input Subsystem。原 IMC 继续负责移动、跳跃和视角，既有二进制资源不需要重新保存。

Started 调用 StartSprint；Completed 和 Canceled 调用 StopSprint。UpdateMovementSpeed 将 MaxWalkSpeed 设为正常或奔跑配置值，并限制正常速度不小于 0、奔跑速度不低于正常速度。状态切换由 CharacterMovementComponent 的原加速、制动和碰撞处理。

现有 PlayerController 的 FlushPressedKeys 调用 Super 后清除当前角色奔跑状态；角色 UnPossessed/EndPlay 移除自己的 IMC 并恢复速度。没有另建状态机、移动组件或摄像机。

## 5. 编译情况

真实使用 UE 5.8.4、MSVC 14.44.35229、Windows SDK 10.0.26100.0。编辑器在 C++ 修改前正常停止并关闭，采用冷模块构建，不依赖 Live Coding。

```powershell
& 'F:\levn\game\Projects\WastelandAdventure\Tests\BuildEditor.ps1'
```

脚本实际调用：

```powershell
& 'F:\levn\game\Epic\UE_5.8\Engine\Build\BatchFiles\Build.bat' WastelandAdventureEditor Win64 Development '-Project=F:\levn\game\Projects\WastelandAdventure\WastelandAdventure.uproject' -WaitMutex -NoEngineChanges -NoHotReloadFromIDE -MaxParallelActions=1
```

真实过程：

| 构建日志（Saved/Verification） | 结果 |
| --- | --- |
| EditorBuild-20261101-041023.log | 主动中止：复核发现测试需调用的 FlushPressedKeys 应保持 public，未宣称成功。UHT 已写入 3 个生成文件。 |
| EditorBuild-20261101-041302.log | 退出码 6，C4458：控制器局部变量 Character 遮蔽 AController::Character。保留错误记录。 |
| EditorBuild-20261101-041727.log | 将局部变量改名 SprintCharacter 后，实际编译/链接成功，18.86 秒，退出码 0。其余已编译源码复用，不冒充完整重编译。 |
| EditorBuild-20261101-042056.log | 修正鼠标测试后，重新编译 SprintPIETest.cpp 并链接成功，25.13 秒，退出码 0。 |

文档安装后的最后增量构建、结构、资源保全及差异检查另记于 `Saved/Verification/Phase2_1/FinalVerification.log`，其中包含实际构建日志路径。

## 6. 运行验收

### 自动化：真实 Editor PIE 输入与物理

执行 `Tests/RunSprintPIE.ps1`。使用正常渲染 Editor 和真实第三人称地图，非 NullRHI、非只检查代码、非只调用 StartSprint。测试通过 PlayerController 的 UE5.8 输入事件接口将 W/A/S/D、Left Shift、Space、MouseX/MouseY 送入原 Enhanced Input 管线，并等待真实游戏帧和物理运动。

首轮 `Run-20261101-041800/Report/index.json` **Fail**：移动/奔跑等断言通过，唯一错误是测试直接注入 Mouse2D，未触发实际配对鼠标轴。根据本机 PlayerInput.cpp 改为注入 MouseX/MouseY，保留失败报告。同时将日志路径参数由 LOG 修正为 ABSLOG，确保绝对路径日志真实落盘。

第二轮 `Run-20261101-042144/Report/index.json`：**Success；succeeded=1，failed=0，notRun=0，Editor exit=0**。原始输出日志为同目录 `SprintPIE.log`。

| 检查 | 实际结果 | 状态 |
| --- | --- | --- |
| W 前进 | X 从 0 到 348.310，实际水平速度 500 | 通过 |
| A、S、D | 分别观测左移、后移和右移坐标变化 | 通过 |
| Shift + W | 真实水平速度达到 750，上限 750 | 通过 |
| 松开 Shift | 继续移动时速度恢复 500，上限 500 | 通过 |
| 奔跑转向 | 持续 Shift 下转为 D，Y 增加，速度仍为 750 | 通过 |
| 停止移动 | 制动后水平速度 0，无持续滑动 | 通过 |
| 跳跃与落地 | Shift 持有时空格跳跃；Z 92.150 → 167.348 → 92.150，恢复地面状态 | 通过 |
| 空中释放 Shift | MaxWalkSpeed 恢复 500，落地未卡奔跑 | 通过 |
| 输入清空路径 | FlushPressedKeys 后立即恢复 500 | 通过 |
| 连续快速 Shift | 多次 500/750 切换，不叠加，最后恢复 500 | 通过 |
| 鼠标水平/垂直 | ControlRotation 从 P=0,Y=0 到 P=1.75,Y=3.5 | 通过 |
| 地面/墙壁碰撞 | 真实地面保持接触；临时碰撞墙前停住，速度 0，未穿透 | 通过 |
| Output Log | PIE 开始至测试结束的 Error/Fatal/Ensure/Blueprint Runtime Error 为 0 | 通过 |

测试墙仅在 PIE 世界临时创建，结束时销毁，未保存成地图或资源。Canceled 绑定存在；普通 Shift 释放的 Completed 和引擎输入清空路径已实际执行，未额外构造带 Hold 触发器的 Ongoing→Canceled 专项场景。

### 图形界面：Codex 实际操作

普通新编辑器会话（独立于上述自动化）已实际点击 Play，看到 Quinn 和模板场景；拖动鼠标后场景水平/垂直视角变化；按空格观察到离地跳跃动画；按 Esc 观察到 Play 按钮恢复绿色、Actor 数从 73 回到 64。日志确认 PIE 正常清理。本次普通 PIE 区间关键错误数同样为 0，日志 `ManualEditor.log`。

图形工具的 W/Shift+W 短按不足以可靠展示持续速度，因此没有把短按截图当作长按速度通过；长按、释放、方向、碰撞等结论来自上表真实 PIE 自动化数据。没有用户人工验收结果冒充 Codex 操作。

### 可复现编辑器步骤

1. 打开正式 `.uproject`，确认 Lvl_ThirdPerson。
2. 点击 Play 并点击视口取得键鼠焦点。
3. 按住 W 观察正常移动；继续按 W 并按住 Left Shift，观察加速；只松开 Shift，观察恢复。
4. 持有 Shift 时切换 A/S/D；放开移动键，确认停止。
5. 普通移动及奔跑时按空格，观察起跳、空中控制和落地；拖动鼠标检查视角。
6. 朝墙和斜坡移动，确认碰撞；按住 Shift 后切出窗口再返回，确认没有卡奔跑。
7. 按 Esc 或 Stop 正常结束，检查 Output Log。可调整 Class Defaults → Movement → Sprint 参数后复测。
8. 需要精确速度/物理回归时，先正常关闭编辑器，再运行 `Tests/RunSprintPIE.ps1`，以新报告结果为准。

## 7. 遗留问题

- 无本次奔跑功能的阻断性编译或 PIE 错误。直接物理长按手感、长时间游玩、多帧率和系统窗口失焦操作仍可由用户补充体验验收；自动化已验证输入清空这一引擎处理路径。
- 开始 PIE 之前，中文 UE 编辑器仍有 13 条 `LogAutomationTest: Error: Condition failed`。本机 UnifiedErrorTests.cpp 使用英文固定文本比较，而启动自检日志已本地化，推断与该引擎自检有关；不是本次项目回归测试错误。未修改引擎、切换用户语言或隐藏日志。
- 本机此前有内存/显存压力，当前采用已有低画质档位，未进行性能或高画质验收。
- 未专门调校 750 cm/s 动画步幅，未实施动画阶段 2.4；人物和动画资产原样保留。未构建独立 Windows 游戏包，不将 PIE 等同于打包验收。
- 仍无首次 Git commit；本次没有自动暂存新改动，原有暂存区保持不变。建议用户审阅这次结果后，明确批准建立可恢复基线，再进行后续开发。

## 8. 下一任务

下一项为 2.2：角色闪避系统。本次按要求停止，不实现、不自动进入该任务，也不将第二阶段整体标记为完成。
