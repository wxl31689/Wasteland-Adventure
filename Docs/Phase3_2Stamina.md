# Wasteland Adventure — 3.2 体力系统验收

日期：2026-11-03，Asia/Shanghai。任务依据：[用户提供的分享对话，末条 3.2 开发说明](https://chatgpt.com/share/6ae8b003-a6f4-83ec-a277-8ab7ae3daf7e)。截图仅作后续视觉参考，本任务未制作正式 HUD。

结论：3.2 通过真实编译链接、数值测试、30/60 FPS 图形 PIE 与原有八项回归。本次停止于 3.2，没有进入攻击、战斗动画、AI 或技能开发。历史报告及失败记录保持原样。

## 一、本次实现功能

独立 UWAStaminaComponent 管理初始/最大值、原子消耗、显式补充、持续奔跑扣费、自动恢复、恢复等待与状态事件。默认最大/初始体力 100，奔跑 20/秒，成功闪避 25，自动恢复 15/秒，停止消耗动作后等待 1 秒。

所有参数可在 Unreal Editor 类默认值配置。非法最大值、非有限值及负费率恢复到安全默认值，初始值限制在 0—最大值内。负数/非有限消耗与恢复被拒绝，超额消耗不部分扣费；零消耗视为成功的无变化操作。普通显式恢复允许在等待阶段补充，但奔跑/闪避期间也会被拒绝；自动恢复必须遵守 1 秒等待。

## 二、新增文件

| 文件 | 用途 |
| --- | --- |
| `Source/WastelandAdventure/Components/WAStaminaComponent.h` | Blueprint 接口、配置及三类动态事件。 |
| `Source/WastelandAdventure/Components/WAStaminaComponent.cpp` | 数值、按需 Tick、等待定时器与生命周期。 |
| `Source/WastelandAdventure/Tests/WAStaminaTestObserver.h` | Transient 动态事件测试接收器。 |
| `Source/WastelandAdventure/Tests/WAStaminaTestObserver.cpp` | 校验事件状态并累计实际扣费。 |
| `Source/WastelandAdventure/Tests/StaminaComponentTest.cpp` | 独立真实 UE 世界内的组件规则测试。 |
| `Source/WastelandAdventure/Tests/StaminaPIETest.cpp` | 30/60 FPS 下的正式蓝图玩家输入、物理与体力集成测试。 |
| `Tests/RunStaminaAcceptance.ps1` | 运行并验证全部 11 项测试，保存真实成功/失败结果。 |
| `Docs/Phase3_2Stamina.md` | 本报告。 |

## 三、修改文件与保护

| 文件 | 范围 |
| --- | --- |
| `Source/WastelandAdventure/WastelandAdventureCharacter.h/.cpp` | 在现有 3.1 接入之上添加体力默认组件、耗尽通知、Shift 释放锁、闪避费用与起步空间检查。 |
| `Source/WastelandAdventure/Tests/DodgePIETest.cpp` | 独立闪避场景起点补满体力；原断言不变。 |
| `Source/WastelandAdventure/Tests/MovementIntegrationPIETest.cpp` | ResetAt 场景夹具补满体力；原断言不变。 |
| `README.md` | 使用、验收、当前状态。 |
| `Docs/DevelopmentPlan.md` | 更新仅完成 3.1 / 3.2，后续仍为计划。 |

旧运动测试含多次独立闪避，原先没有资源预算；新增费用后，在场景起点准备可支付资源，使它们继续检验原运动行为，而不是被前一个场景耗空。未关闭体力系统、修改费率或削弱运动断言。新体力测试单独连续计费、耗尽并验证失败请求。

3.1 当时尚未提交的源码、报告和接入已完整保存为本任务基线；本次在其上追加。生命值组件及测试、PlayerController、Build.cs、输入资源、相机/动画参数、所有 805 个 Content 文件均未修改。第二阶段报告与 Docs/Phase3_1Health.md 逐字节保护。开工时的 AGENTS 与本机配置保留；详细 SHA-256 基线在 Saved/Verification/Phase3_2/Baseline/。

## 四、体力组件架构

InitializeStamina 幂等；TryConsumeStamina 原子地返回成功/失败；RestoreStamina 返回实际补充量；ResetStamina 回满最大值并保留正在执行的动作状态，不替代物理松开 Shift。提供当前值、最大值、比例、支付能力、耗尽、奔跑、恢复及剩余等待时间查询。

OnStaminaChanged 提供旧值、新值和带符号 Delta；OnStaminaDepleted 只在正数降至零时发送；OnStaminaRecovered 只在零恢复为正数时发送，不代表恢复满，也不自动重启奔跑。均为 BlueprintAssignable，只对实际变化通知。通知期间拒绝同步重入的数值修改，监听器读取已提交状态。

组件无复制、无 GAS，不更改生命值。Tick 仅在奔跑实际扣费或恢复时开启；等待使用世界游戏时间定时器，满体力且无消耗时关闭 Tick。闪避期间暂停持续奔跑扣费与恢复；动作结束后，若 Shift 奔跑意图仍有效则继续扣费，否则开始恢复等待。EndPlay 清除定时器与 Tick。

体力更新为 PrePhysics，并作为 CharacterMovement 的 Tick 前置依赖；耗尽通知先恢复正常速度，再执行该帧移动，避免残留奔跑速度。

Blueprint 配置：冷构建后打开 BP_ThirdPersonCharacter，选择继承的 StaminaComponent 调整六项默认参数，不要再添加第二个体力组件。PIE 中可从实际玩家查询组件或查看 Current Stamina。未来 UI 先读取初值再绑定变化事件；本任务没有新增 Widget 或 UAsset。

## 五、奔跑集成结果

复用原 StartSprint / StopSprint 及同一 Enhanced Input 映射。按原 Shift 持有奔跑状态扣费：原地持有 Shift 也属于该状态；跳跃时保留原奔跑意图；闪避时暂停持续扣费。未引入第二套输入或 Character Tick。

耗尽时组件停止连续消耗，Character 立即把速度上限恢复为 500 并设置释放锁。即使恢复出体力，继续持有 Shift 或重复 StartSprint 也不能自动重启。松开/取消输入后，新的 Shift 按下且体力为正才可再次奔跑。输入清空、失去控制权沿用原清理入口；它们视为结束当前持有意图。

两种帧率下已验证实际 W+Shift 达到 750，停止后回到 500；耗尽后继续正常行走，恢复后保持旧 Shift 不会反复启动，松开重按后恢复奔跑。

## 六、闪避集成结果

继续使用 Left Ctrl。先检查原有状态、冷却、地面、跳跃和参数，再检查支付能力及起步方向 2 cm 胶囊空间。若贴墙完全不能起步，直接拒绝，不扣费、不播放蒙太奇、不启动冷却。可起步后被远处墙体缩短的闪避属于已接受动作，扣一次 25，不退款。

原 RootMotionSource 成功建立后才尝试扣费；失败会撤销该来源，并保持动画/冷却未启动。起步重入保护避免费用事件回调再次触发同一动作。未改变原方向选择、450 cm / 0.25 秒 / 1800 cm/s 上限、1 秒冷却、根运动算法或动画淡入淡出。

已验证 100→75→50 两次成功闪避；24 体力、空中、活动中、冷却和贴墙失败均不扣费。恢复等待不会被失败请求延后。冲墙缩短、动作取消和平台越边仍保持原有安全行为；已接受后取消仍保留费用与冷却。

## 七、自动化测试结果

运行目录：`Saved/Verification/Phase3_2/Run-20261103-002333/`。Report/index.json 中 11 项全部 Success，每项 errors=0、warnings=0，failed=0、notRun=0。RunnerResult.json 记录 EditorExitCode=0、PASS；脚本退出码 0。实际测试持续约 165.43 秒。

| 新测试 | 结果及实测 |
| --- | --- |
| Stamina.ComponentRules | 初值、范围、超额原子拒绝、无效数值、事件边界、配置归一化、显式恢复与确定性扣费全部通过。 |
| Stamina.Integration30FPS | Success；1083 帧，平均仿真 29.801 FPS；约 1 秒实测恢复 15.0001、扣费 20.0004。 |
| Stamina.Integration60FPS | Success；2094 帧，平均仿真 59.522 FPS；约 1 秒实测恢复 15.0001、扣费 20.0002。 |

两种帧率均验证：等待 0.85 秒不恢复且 Tick 关闭；等待结束按 15/秒恢复；真实 Shift 每秒约扣 20；停止后等待再恢复；耗尽即时恢复速度与释放锁；成功闪避各扣 25；失败请求零费用/零新冷却/零蒙太奇；无同时恢复/持续扣费；12 次快速 Shift 周期；16 次快速 Left Ctrl 按下均只接受两次，累计扣费恰为 50；恢复满后关闭 Tick。

规则测试使用真实注册组件所在的独立 UE World，直接驱动组件的确定性时间片；它不代替图形运行。两项 Stamina PIE 使用实际蓝图玩家、真实引擎输入与物理、真实世界定时器和组件 Tick。

复现：先关闭编辑器，运行 `Tests/BuildEditor.ps1` 与 `Tests/RunStaminaAcceptance.ps1`。加 `-ManualStart` 时，编辑器等待人工保持前台；停止当前 PIE 后，在 UE 命令栏执行 `Automation RunTests WastelandAdventure.`。本轮使用该方式，开始完整测试后不再发送 UI 输入。

## 八、真实编译结果

通过 Tests/BuildEditor.ps1 实际执行：

```powershell
& 'F:\levn\game\Epic\UE_5.8\Engine\Build\BatchFiles\Build.bat' WastelandAdventureEditor Win64 Development '-Project=F:\levn\game\Projects\WastelandAdventure\WastelandAdventure.uproject' -WaitMutex -NoEngineChanges -NoHotReloadFromIDE -MaxParallelActions=1
```

`Saved/Verification/EditorBuild-20261103-002015.log`：Succeeded，退出码 0，144.23 秒。UHT 生成 7 个文件；执行 15 项编译、DLL / lib 链接与元数据任务，没有使用 Live Coding 代替冷构建。新增组件、测试和 Character 均实际编译。

收尾文档修改后的构建另见 Saved/Verification/Phase3_2/FinalVerification.json，其 Up to date 不能替代上述源代码编译。

## 九、图形 PIE 运行验收

使用 UnrealEditor.exe 默认 D3D12 图形渲染和当前正式地图，未使用 NullRHI。真实 BP_ThirdPersonCharacter 自动拥有原生 StaminaComponent，未创建或保存测试资产。

在正常可见的 PIE 中，先通过 UE Python 调用真实组件接口，日志记录初始 100、消耗一次 25 后 75，随后自动恢复 100、is_component_tick_enabled=False。对应手动标记 WA_STAMINA_MANUAL 的本地时间为 00:27:02 与 00:28:27，日志与该轮自动测试同目录。该手动检查只是状态快照；速率与延迟以受控的 PIE 自动采样为依据。

手动 PIE 已正常停止，编辑器显示 90 个编辑器 Actor、所有资产已保存；之后从前台启动完整测试并自动退出。没有改动地图、控件或渲染配置。

## 十、第二阶段回归

AnimationPIE、CameraPIE、DodgePIE、Character.Integration30FPS、Character.Integration60FPS、SprintPIE 全部 Success。覆盖 WASD、Shift、Space、Left Ctrl、方向、原有闪避参数、动画、相机旋转/俯仰/碰撞、斜坡、墙体、平台边缘及输入取消。旧闪避与综合测试仅增加了前述场景预算准备，没有修改原断言；其他四项测试源码未改。

## 十一、3.1 生命值回归

Health.ComponentRules 与 Health.ActorPIE 全部 Success，原生命值测试及组件核心文件未改。伤害、治疗、死亡只触发一次、死后普通治疗无效、显式重置、Actor 单次伤害转交与真实玩家控制共存均通过。3.1 验收报告保持原 SHA-256，不把本轮结论改写成历史结果。

## 十二、已知问题与仓库边界

- 保留第二阶段相机首轮两条断言失败、同代码复测成功且根因未确证的记录。
- 启动阶段仍出现 13 条历史 LogAutomationTest 条件错误，未定位；本轮项目测试开始至 Queue Empty 的关键错误数为 0，不能因此宣称整个启动日志无错误。
- 保留 3.1 首轮动画五条断言失败、前台同代码复测通过的记录；本轮动画测试一次通过，不等于修复历史偶发问题。
- 编辑器启动时出现过内存压力提示；本轮功能测试全部通过，但平均帧率只是当前机器和场景下的实际记录，不是发布性能承诺。未修改系统设置或关闭用户其他应用。
- 未执行打包/Shipping 验收；没有正式 HUD、攻击或死亡表现。生命值死亡状态仍按 3.1 的既定范围处理，没有新增动作限制或重生流程。
- 本任务开始时 3.1 依赖尚未入库。依据用户此前授权，验收完成后将这些既有成果和 3.2 整合至 main；提交及远程核对结果另存 RepositoryUpdate.json。历史报告中的“未提交”描述指其验收时点，原文不改。本机 AndroidFileServer 令牌和 Saved / 构建缓存不上传。

## 十三、下一阶段建议与停止

3.2 已完成。建议下一次单独制定 3.3 普通攻击的动作、命中与验收范围；本次不自动开发。继续追踪既有焦点/输入相关偶发回归和启动条件错误。
