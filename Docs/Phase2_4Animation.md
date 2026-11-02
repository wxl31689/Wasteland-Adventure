# 任务 2.4：角色动画状态

## 1. 当前任务

2026-11-01 开始，2026-11-02 完成收尾。按用户要求仅执行 2.4 角色动画状态，完成后停止，不开始 2.5 或第三阶段。工程为 `F:\levn\game\Projects\WastelandAdventure`，UE 5.8.4 / Win64 / Development Editor；闪避继续使用 Left Ctrl。

状态：实际冷构建、修复后的完整真实渲染 PIE、冷重载及正常关卡图形复查均通过。历史验收没有替代本轮测试。

## 2. 实现功能

- 复用实际人物 `BP_ThirdPersonCharacter_C`、Quinn Simple 网格和 `ABP_Unarmed_C`。现有动画状态图、IK、Idle、Walk / Run、Jump、Fall Loop、Land 和 DefaultSlot 全部保留。
- 原动画蓝图继续读取 CharacterMovement 的真实二维速度、加速度及 IsFalling；没有用按键状态伪造移动动画，也没有创建第二套动画蓝图。
- 原 BlendSpace 的 9 个走路样本从 300 调到 500 cm/s，播放倍率 1.666667；9 个跑步样本从 600 调到 750 cm/s，播放倍率 1.25；9 个 Idle 样本保留。速度轴上限 750、网格分 3 段。角色实际移动速度保留 500 / 750。
- 使用 UE 真正复制原 MM_Dash 得到 `AS_Dodge_InPlace`，关闭动画根运动并强制参考姿势锁根，原 MM_Dash 不改。
- 闪避成功时人物面向接受的位移方向，通过原 DefaultSlot 播放临时动态蒙太奇；完整 0.966667 s 片段按 Dodge Duration 配速，默认 0.25 s 对应约 3.866667 倍率，淡入淡出初值 0.04 s（最多持续时间的四分之一）。
- 闪避正常结束、输入取消、离开地面、失去控制权或结束游戏时沿用原生命周期清理位移并停止本次蒙太奇。空中及冷却拒绝不重复播放动作。

## 3. 文件变更

修改 6 项：

- `Source/WastelandAdventure/WastelandAdventureCharacter.h`
- `Source/WastelandAdventure/WastelandAdventureCharacter.cpp`
- `Source/WastelandAdventure/WastelandAdventure.Build.cs`
- `README.md`
- `Content/Characters/Mannequins/Anims/Unarmed/BS_Idle_Walk_Run.uasset`
- `Content/ThirdPerson/Blueprints/BP_ThirdPersonCharacter.uasset`

新增 7 项：

- `Source/WastelandAdventure/Tests/AnimationPIETest.cpp`
- `Source/WastelandAdventure/Tests/AnimationAssetCommands.cpp`
- `Tests/InspectAnimation.py`
- `Tests/ConfigureAnimation.py`
- `Tests/RunAnimationPIE.ps1`
- `Content/ThirdPerson/Animations/AS_Dodge_InPlace.uasset`
- `Docs/Phase2_4Animation.md`

既有文件、两个修改资源和原动画蓝图均提前备份，路径为 `Saved/Verification/Phase2_4/Baseline/`，另有 D 盘工作备份。原 753 个 Content 文件的哈希清单保留。原 `ABP_Unarmed`、MM_Dash、骨骼、输入资源及所有地图均不在修改范围。

仓库仍无首次 commit。未暂存、提交、重置、清理或推送。旧索引资源指针仍指向原版本，本次资源的工作文件已改变，不能把旧的“工作文件等于索引 OID”检查套用后宣称通过；提交前需要单独审查暂存范围。

最终资源核验：Content 共 754 个文件，原 753 个中 751 个哈希不变，仅上述 2 个原资源修改，新增 1 个动画。原索引仍为 859 文件，原 753 个索引 LFS 本地对象哈希核验通过；对 3 个当前新增/修改资源实际执行本机 Git LFS clean，生成真实指针并核验工作文件、指针 OID、大小及本地 LFS 对象哈希，均一致。没有因此暂存文件。证据：`Saved/Verification/Phase2_4/AssetVerification.json` 和 `LfsPointers/`。

## 4. 核心代码

Character 的 DodgeAnimation 和 DodgeAnimationBlendTime 为 Blueprint 可配置属性，ActiveDodgeMontage 为临时实例引用。原 StartDodge 成功后播放动画；FinishDodge 及其既有中断入口停止本次蒙太奇，不停止无关动画。若所配置片段启用根运动或未强制锁根，拒绝播放并记录警告，保留碰撞安全的移动原型。

移动仍由原 RootMotionSource / CharacterMovement 负责，动画不提供第二份位移。未增加游戏 Tick；现有动画蓝图的 UpdateAnimation 与引擎姿势求值继续工作，这是动画随真实移动更新所需的原有流程。

`AnimationAssetCommands.cpp` 仅在编辑器构建中注册一次性资源配置命令，使用 UE 原生 EditSampleValue / ValidateSampleData / ResampleData / SavePackages 完成 BlendSpace 派生插值数据重建，不进入游戏运行时。AnimGraphRuntime 依赖也仅加到 Editor 测试配置，以读取真实运行节点。

ConfigureAnimation.py 先检查基线哈希及目标动画不存在，再由 UE 复制、配置和保存资产；不会覆盖已有闪避动画。正常测试不重复执行配置。

接口依据：[UE 动态蒙太奇接口](https://dev.epicgames.com/documentation/unreal-engine/BlueprintAPI/Animation/Montage/PlaySlotAnimationasDynamicMontag-)；具体签名与可用性另通过本机 5.8.4 头文件、实际 UHT / C++ 编译及运行核验。

## 5. 编译情况

真实执行：

```powershell
& 'F:\levn\game\Projects\WastelandAdventure\Tests\BuildEditor.ps1'
```

目标 `WastelandAdventureEditor Win64 Development`；脚本调用官方 Build.bat，带 `-WaitMutex -NoEngineChanges -NoHotReloadFromIDE -MaxParallelActions=1`。修改 C++ 前已结束 PIE 并正常关闭已保存编辑器。

| 日志（Saved/Verification 下） | 实际结果 |
| --- | --- |
| `EditorBuild-20261101-141721.log` | 退出码 0，246.18 s，UHT 生成 3 个文件；50 个动作，实际编译 Character、新 AnimationPIETest、原模块及 DLL 链接。 |
| `EditorBuild-20261101-142739.log` | 退出码 0，35.56 s；实际编译 AnimationAssetCommands、修订后的 AnimationPIETest，链接 DLL。 |

官方 ProjectFiles 生成记录位于 `Phase2_4/GenerateProjectFiles.log`（29.61 s）及 `GenerateProjectFiles-AssetRebuild.log`（20.81 s），退出码均为 0。VS2022 不支持 .NET10 C# 工具项目的既有提示未阻断本机 VS2022 / MSVC C++ 构建。

2026-11-02 最终报告及资源核验后的增量构建结果另存 `Saved/Verification/Phase2_4/FinalVerification.json`，不代替上述真实源码编译记录。

## 6. 运行验收

采用真实图形编辑器、默认 D3D12，加载官方 `/Game/ThirdPerson/Lvl_ThirdPerson`。AnimationPIE 使用真实玩家、原动画蓝图及实际骨骼求值；临时物理地面只在 PIE 世界存在，不保存地图。自动场地在地图外，因此导出帧背景为天空、地面不可见，不作为正常关卡画面的替代证明。

失败及修复：

- 检查脚本曾遇到 Blueprint 属性保护和 UE Text 无法直接 JSON 序列化；改用本机实际提供的 BlueprintEditorLibrary / BlueprintGraphEditor 只读接口并按文本序列化。检查失败原始日志保留，没有伪造资源。
- `Run-20261101-142210`：真实动画测试发现 500 cm/s 的行走权重为 Walk 0.667 / Jog 0.333，3 个行走断言失败。其他跳跃、下落、落地、闪避、取消分项运行成功，CameraPIE / DodgePIE / SprintPIE 为 Success，但整轮不计通过。
- 原生重建命令确认资源控制点已经是 500 / 750，倍率也是 1.666667 / 1.25；问题为 Python 属性修改后派生插值数据未正确更新。调用 UE 原生校验、重采样和保存后，保留同样 >0.95 权重断言重跑。

修复后 `Run-20261101-142851/Report/index.json`：编辑器退出码 0，AnimationPIE、CameraPIE、DodgePIE、SprintPIE 全部 Success，各 0 errors / 0 warnings。原日志为同目录 AnimationPIE.log。

| 检查 | 该轮实际结果 |
| --- | --- |
| Idle | 真实原状态机进入 Idle，静止速度 0。 |
| Walk | 原运行节点输入及过滤坐标为 500，UE 插值结果 Walk 1.000 / Jog 0.000，节点实际相关；动画时间与求值骨骼均推进。 |
| Run | Shift 后实际 750，Jog 1.000；加速过程中出现走跑混合权重。 |
| Run → Walk → Idle | 释放 Shift 后回 500 / Walk 1.000，释放 W 后回 Idle、速度停止，无动作锁死。 |
| Jump / Fall / Land | 真实 Space 输入，连续观察到 Jump、Fall Loop、Land、Locomotion；IsFalling 与 CharacterMovement 一致，正常落地。 |
| 空中闪避 | 拒绝位移且不创建闪避蒙太奇，保留跳跃动画。 |
| Dodge | 真实 Left Ctrl 输入，动态蒙太奇实际引用 AS_Dodge_InPlace，DefaultSlot 权重达到 1，动画有效时长与 0.25 s 位移窗口匹配。 |
| 网格与位移 | 25 次运动采样，网格根偏移最大 0 cm，骨盆相对角变化最大 73.490°，地面接触保持；位移 450.180 cm，无动画额外位移。 |
| 结束 / 重复 / 中断 | 重复请求不重启实例；正常结束后蒙太奇退出、槽权重归零；重新移动恢复 Walk。FlushPressedKeys 中断后动作退出、Idle 恢复，冷却保留。 |
| 原功能 | 摄像机、奔跑、闪避的完整既有 PIE 测试重新通过。 |

已检查引擎原始导出 `Frames-20261101-142923/Walk.png`、Run.png、Jump.png、Dodge.png，均为实际玩家的非空渲染画面，动作姿势不同；未修改图片。

冷重载持久化验收：关闭重建进程后，重新启动正常编辑器，仅加载磁盘保存结果，未执行 ConfigureAssets 或 RebuildBlendSpace。`Cold-20261101-143103/Report/index.json` 中四项仍全部 Success，各 0 errors / 0 warnings。日志的资源配置/重建标记数为 0；从 AnimationPIE 开始至报告写出，关键 Error / Fatal / Assertion / Ensure 匹配数为 0。启动时编辑器后台限速到 3 FPS，切至前台后达到自动测试帧率门槛并完成，没有放宽断言。

正常关卡图形复查：2026-11-02 03:26:32—03:29:00（北京时间），在官方第三人称关卡实际点击 Play，看到玩家及关卡正常渲染；聚焦视口后按 Space 看见起跳，再观察到落地 Idle；按 Left Ctrl 看见闪避姿势和位移，随后恢复正常站立。Shift+F1 后点击 Stop，恢复绿色 Play，Actor 数从 73 回到 64，状态栏为全部已保存；随后正常关闭编辑器。日志在同一冷重载目录的 `Editor.log`，该 PIE 运行区间关键错误匹配数为 0。这些是定性复查，速度、采样权重、根偏移和位移以真实自动 PIE 数据为准。

复现：安全关闭编辑器后执行 BuildEditor.ps1，再执行 RunAnimationPIE.ps1（正常验收不加 ConfigureAssets 或 RebuildBlendSpace）。原始报告存入新 Run 目录；必要的兼容排查可显式加 `-RHI D3D11`，本轮通过记录使用默认 D3D12。

## 7. 遗留问题

- 动作为复用官方 Dash 的基础前冲闪避表现，未制作新的专用方向翻滚动画；后续美术可替换已暴露的锁根动画引用。
- 原有启动阶段 13 条引擎 `LogAutomationTest: Error: Condition failed` 的历史问题不在本任务修改范围；与项目测试开始后的错误分别检查，不声称整份启动日志零错误。
- 本次是动画状态验收，不是帧率性能基准、正式打包验收或第二阶段综合验收。未把第二阶段整体标为完成。

## 8. 下一任务

仅说明：2.5 状态与碰撞综合验收。本次完成后停止，不自动开始 2.5 或第三阶段。
