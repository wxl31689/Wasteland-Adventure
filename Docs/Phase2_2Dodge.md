# 任务 2.2 — 角色闪避位移原型验收

日期：2026-11-01，Asia/Shanghai。工程：`F:\levn\game\Projects\WastelandAdventure`。

## 1. 当前任务

用户明确选择“先执行 2.2：闪避系统，完成验收后停止”。仅完成本任务，不进入 2.3、动画专项或第三阶段。当前位移原型的真实 C++ 构建、Dodge PIE、Sprint 回归和普通编辑器操作已通过；专用闪避动画未接入。

开始前重新检查原 Character、PlayerController、Enhanced Input、MovementComponent 和测试实现。当前主角是 BP_ThirdPersonCharacter_C / AWastelandAdventureCharacter，控制器 BP_ThirdPersonPlayerController_C；原 IMC_Default、IMC_MouseLook、Quinn 和 ABP_Unarmed 保留。原奔跑已实现，基础主角未实现本次闪避。平台变体的 Dash 不属于当前主角，未把它当作已经交付的闪避。

Git 开始状态：main 尚无首次提交；索引仍为第一阶段的 859 个文件，852 个 A、7 个 AM，另有 4 个上次新增未跟踪文件。本次保留已有索引及未提交内容。建议建立可恢复的基线提交，尚未获得提交批准。

## 2. 实现功能

| 配置 | 初值 | 规则 |
| --- | --- | --- |
| Dodge Key | Left Ctrl | 用户追加指定；EKeys::LeftControl。EditDefaultsOnly FKey，可在角色 Class Defaults → Input 调整 |
| Dodge Distance | 450 cm | 平地位移目标上限，允许物理积分的微小误差；碰撞可缩短距离 |
| Dodge Duration | 0.25 s | 固定移动窗口，由 Timer 和 Root Motion Source 生命周期控制 |
| Dodge Speed | 1800 cm/s | 速度上限；实际速度为 min(Speed, Distance / Duration) |
| Dodge Cooldown | 1.0 s | 从成功触发时计时，至少等于 Duration，避免短冷却允许动作重入 |

四个移动参数均为 BlueprintReadOnly、EditDefaultsOnly UPROPERTY；运行状态 bIsDodging、bDodgeOnCooldown 只读，IsDodging 和 GetDodgeCooldownRemaining 可供表现层查询。

- 有方向输入时，按本帧 IA_Move 值和控制器 Yaw 换算世界方向；无输入时沿角色前方，摄像机朝向不影响无输入的后备方向。
- 仅在有控制器、允许移动、Movement 激活且位于地面时触发。已请求跳跃、空中、闪避中或冷却中拒绝触发。
- 绑定 Enhanced Input Started，按住键不会在冷却后自动重复闪避。
- 移动窗口内锁定闪避方向，普通移动输入不改变轨迹；摄像机输入继续生效。
- 闪避中跳跃请求不排队；跳跃结束并落地后可正常闪避。
- 结束后清除闪避驱动力和残余速度，恢复普通控制；当前 Shift 持有意图决定恢复 500 或 750 上限，松开 Shift 的原逻辑保留。
- 输入清空或 CancelDodge 中断位移但保留冷却；失去控制权及 EndPlay 清理位移、定时器和冷却。离开平台地面时中断闪避，交回正常重力和坠落。
- 非有限数、非正距离/时长/速度或负冷却会拒绝触发，不消耗冷却。

未实现死亡、无敌帧、体力消耗、战斗或伤害。

## 3. 文件变更

修改：

- `Source/WastelandAdventure/WastelandAdventureCharacter.h`
- `Source/WastelandAdventure/WastelandAdventureCharacter.cpp`
- `Source/WastelandAdventure/WastelandAdventurePlayerController.cpp`
- `README.md`

新增：

- `Source/WastelandAdventure/Tests/DodgePIETest.cpp`
- `Tests/RunDodgePIE.ps1`
- `Docs/Phase2_2Dodge.md`

没有改动原 SprintPIETest、RunSprintPIE、Build.cs 或官方变体。原有 753 个 Content 文件与本次开始前的 SHA-256 全部一致，数量未变；没有重写 Blueprint、IMC、地图、动画或人物资源。

恢复材料位于 `Saved/Verification/Phase2_2/Baseline/`，包含本次改动前的三个源码文件、README 和 Content 哈希。正常 UBT 项目文件生成更新了本机解决方案及生成物，它们保持忽略，不是本次待提交资源。

## 4. 核心代码

复用原 Character 和运行时局部输入上下文，添加临时 IA_Dodge 和配置键映射；不创建伪造输入资产，不修改已有 IMC。

TryDodge 检查状态和参数后查询 EnhancedPlayerInput 的本帧 MoveAction 值，避免同帧移动与闪避时使用过期方向。成功才调用 StartDodge 并开启冷却，bool 返回值可供后续体力成本使用。

StartDodge 添加 `FRootMotionSource_ConstantForce`（Override，优先级 500），由原 CharacterMovementComponent 执行扫掠、地面与墙体碰撞。没有在游戏实现中调用 Teleport 或直接写角色位置。源的部分末帧处理启用，结束速度设为 0。

Timer 调用 FinishDodge、ResetDodgeCooldown；CanJumpInternal 和原 DoJumpStart 阻止动作中的跳跃排队；OnMovementModeChanged 处理离地中断；原 PlayerController::FlushPressedKeys 在清除输入后取消闪避。没有新增 Character Tick、复杂状态机或移动组件。

## 5. 编译情况

C++ 修改前已通过图形编辑器正常停止旧 PIE，确认已保存并正常关闭编辑器。使用 UE 5.8.4、VS2022 MSVC 14.44.35229、Windows SDK 10.0.26100.0，未使用 Live Coding 验收。

实际构建入口：

```powershell
& 'F:\levn\game\Projects\WastelandAdventure\Tests\BuildEditor.ps1'
```

目标和参数：

```powershell
& 'F:\levn\game\Epic\UE_5.8\Engine\Build\BatchFiles\Build.bat' WastelandAdventureEditor Win64 Development '-Project=F:\levn\game\Projects\WastelandAdventure\WastelandAdventure.uproject' -WaitMutex -NoEngineChanges -NoHotReloadFromIDE -MaxParallelActions=1
```

| 原始日志 | 实际结果 |
| --- | --- |
| Saved/Verification/EditorBuild-20261101-122300.log | UHT 写入 3 个生成文件；角色和控制器实际编译/链接成功，120.14 秒，退出码 0。构建列表尚未包含新测试，未据此声称测试已编译。 |
| Saved/Verification/Phase2_2/GenerateProjectFiles.log | 使用官方 UBT -ProjectFiles 更新项目文件，26.70 秒，Succeeded。 |
| Saved/Verification/EditorBuild-20261101-122637.log | 明确编译 DodgePIETest.cpp、链接新模块，16.82 秒，Succeeded，退出码 0。 |

项目文件生成中的 VS2022/.NET10 C# 工具项目提示已保留；本次 C++ 构建实际使用已安装的 VS2022 MSVC 并成功。文档安装后的最终增量构建和检查结果另记 `Saved/Verification/Phase2_2/FinalVerification.log`。

## 6. 运行验收

### 初次实现的真实 PIE 自动化（Left Alt 历史记录）

以下保留首次实现时使用 Left Alt 的原始证据。用户随后指定默认键改为 Left Ctrl；本节末单独记录新按键复测，不改写旧报告。

执行 `Tests/RunDodgePIE.ps1`：正常渲染 Editor、实际第三人称地图及实际蓝图玩家，使用 UE5.8 PlayerController 输入事件进入 Enhanced Input 管线。不是 NullRHI、静态检查或只调用位移函数。

测试使用临时碰撞地面、墙和平台，在设置场地时定位玩家，随后所有闪避位移均由实际游戏实现执行；临时场地及实例参数均不保存为资产。

报告：`Saved/Verification/Phase2_2/Run-20261101-122719/Report/index.json`。

结果：**Success；succeeded=1、failed=0、notRun=0；编辑器退出码 0；PIE 区间关键错误 0。** 同目录 `DodgePIE.log` 保留完整 Output Log。

| 用例 | 实测结果 | 状态 |
| --- | --- | --- |
| 无方向按键触发 | 读取实际 DodgeKey=LeftAlt，沿角色 +X 位移，摄像机 Yaw=90 不改变后备方向 | 通过 |
| 默认速度/距离 | 活动速度 1800；X 20000 → 20450.180，约 450.18 cm | 通过 |
| 固定持续时间/停止 | 活动期间检测为闪避；窗口结束后状态清除、速度 0 | 通过 |
| 闪避中跳跃与重复请求 | 保持地面；重复请求返回 false，未重置动作 | 通过 |
| 长按与冷却 | 到期后仍长按不自动重复；冷却归零；冷却中的新按键被拒绝 | 通过 |
| 方向输入 | D 输入沿 +Y 约移动 450.18 cm；动作中改 W 不改变闪避方向 | 通过 |
| 恢复移动及奔跑 | 结束后 W 产生正常速度；Shift 上限恢复 750，释放恢复 500 | 通过 |
| 空中尝试 | 原跳跃正常；空中闪避被拒绝且冷却保持 0；正常落地 | 通过 |
| 输入清空中断 | FlushPressedKeys 立即终止，后续位移为 0，保留剩余冷却 | 通过 |
| 墙体碰撞 | 到墙前约 188.97 cm 停止，未穿透；保持地面，速度 0 | 通过 |
| 失去控制权 | 中断并清除冷却，重新 Possess 后可再次通过输入闪避 | 通过 |
| 速度参数 | 速度上限 900、时长 0.25，实际位移约 225.09 cm | 通过 |
| 距离/时长参数 | 距离 100、时长 0.5，速度 200；0.3 秒仍活动，最终约 100.02 cm | 通过 |
| 短冷却约束 | 冷却配置 0.1，小于 0.5 时长；动作中仍禁止重复，结束后正确归零 | 通过 |
| 平台边缘 | 离地后闪避中断，水平位移约 210.03 cm，正常坠落并落地 | 通过 |
| 非法参数 | 时长 0 被拒绝，没有动作和冷却状态污染 | 通过 |

另重新执行原 `Tests/RunSprintPIE.ps1`，当前模块的报告为 `Saved/Verification/Phase2_1/Run-20261101-123028/Report/index.json`：**Success，退出码 0，PIE 关键错误 0**。W/A/S/D、500/750 切换、释放、转向、跳跃落地、输入清空、相机双轴和碰撞均重新通过，不沿用上一任务结果。

### 初次实现的普通编辑器图形检查（Left Alt 历史记录）

Codex 实际打开当前模块，点击 Play、聚焦视口、按 Left Alt，观察到玩家相对原场景位置前移；随后拖动鼠标观察视角改变、按空格观察离地跳跃，最后按 Esc 正常停止，Play 按钮恢复绿色，Actor 从 73 回到 64。`ManualEditor.log` 确认正常清理，普通 PIE 关键错误数为 0。

图形操作只作为输入、画面和结束流程的补充；精确距离、持续时间、长按、冷却和边界结果来自上述真实 PIE 数据，没有将短按截图冒充数值验收。

### 用户追加按键修订：Left Ctrl

用户随后指定 Left Ctrl 闪避。将原角色构造函数的默认 DodgeKey 改为 `EKeys::LeftControl`，同步 README 和当前复测步骤；没有修改输入资产或其他闪避规则。

实际构建日志 `Saved/Verification/EditorBuild-20261101-124140.log`：重新编译 WastelandAdventureCharacter.cpp 并链接，34.00 秒，Succeeded，退出码 0。

重新执行 RunDodgePIE.ps1，新报告 `Saved/Verification/Phase2_2/Run-20261101-124255/Report/index.json`：**Success，编辑器退出码 0**。实际加载的 BP_ThirdPersonCharacter_C 输出 `key=LeftControl`，上述闪避、冷却、方向、跳跃、恢复和碰撞用例全部以新按键重新通过；本轮 PIE 关键错误数为 0。普通图形操作的历史记录仍为初次 Left Alt 版本，没有改写成 Left Ctrl 的人工结果。

本次修订的最后文档增量构建与差异检查保存在 `Saved/Verification/Phase2_2/LeftCtrlFinalVerification.log`。

### 编辑器复测步骤

1. 打开 `.uproject` 和 Lvl_ThirdPerson，Play 后点击视口。
2. 无方向按 Left Ctrl，确认向角色前方闪避；分别持有 W/A/S/D 后按 Left Ctrl 检查方向。
3. 快速重复按键、持续按住按键，确认活动期间和冷却期间不能再次触发；冷却后松开重按可触发。
4. 闪避中按空格应拒绝；正常跳跃在空中按闪避应拒绝；落地后恢复。
5. 闪避后立即移动、持有和松开 Shift，确认恢复正常移动/奔跑；朝墙闪避确认没有穿透，离开平台边缘确认正常坠落。
6. 切换视角并正常 Stop/Esc。数值复测先正常关闭编辑器，再运行 RunDodgePIE.ps1 和 RunSprintPIE.ps1。
7. 参数或按键在 BP_ThirdPersonCharacter → Class Defaults 的 Movement → Dodge / Input 调整；重新启动 PIE 以创建新映射。本次没有保存更改过的蓝图资源。

## 7. 遗留问题

- 本次为位移原型。未接入专用闪避动画、Montage/Slot 或无敌帧；模板其他变体虽有 Dash 资源，尚未验证当前 ABP 配置，留待动画任务，不伪造资产。
- 本次没有新增阻断性编译或 PIE 错误。中文编辑器启动前的既有引擎自检消息保留，PIE 区间单独统计；不能把整个日志说成完全没有错误。
- 高画质、长时间手感、多帧率专项和 2.5 全部组合测试未执行，不标记第二阶段整体通过。当前沿用本机已有低画质设置。
- 尚无首次 Git commit。本次不自动暂存、提交或推送；已有未提交内容保留，建议审阅后批准建立基线。

## 8. 下一任务

下一项为 **2.3：摄像机系统**。本次按用户选择停止，不自动继续，也不开发第三阶段生命值或战斗系统。
