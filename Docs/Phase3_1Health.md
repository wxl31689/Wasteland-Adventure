# Wasteland Adventure — 3.1 独立生命值组件验收

日期：2026-11-02，Asia/Shanghai。引擎：UE 5.8.4，Windows / C++ + Blueprint，纯单机。

结论：3.1 已通过真实 UHT / MSVC 编译与链接、生命值规则测试、真实玩家图形 PIE、手动 Details 数值检查及第二阶段完整回归。本次止于 3.1，未开发体力、攻击、技能、AI、死亡动画或背包，未自动提交 Git。首轮失败及历史限制完整保留在第九节。

## 一、实现功能

新增 `UWAHealthComponent`，默认 MaxHealth / CurrentHealth 为 100，死亡状态为 false。公开 InitializeHealth、ApplyDamage、RestoreHealth、GetCurrentHealth、GetMaxHealth、GetHealthPercent、IsDead、ResetHealth。

- HP 范围保持在 0 与 MaxHealth 之间；负数、零、NaN 与无穷大的伤害/治疗无效。
- 伤害和治疗返回实际增减量，过量伤害归零，过量治疗只补到上限。
- InitializeHealth 幂等；重复调用不治疗，也不复活。BeginPlay 自动初始化。
- 死亡状态锁定，同一条生命只发送一次 OnDeath；死亡后继续伤害或普通治疗均不改变状态。只有显式 ResetHealth 清除死亡并回满 HP。
- OnHealthChanged 和 OnDeath 使用 BlueprintAssignable 动态多播事件。变化事件参数为组件、旧 HP、新 HP、带符号 Delta；无实际变化不发送事件。致死时先提交数值与死亡状态，再发送变化事件和死亡事件。
- 通知过程中拒绝同步重入的伤害、治疗及重置，防止监听器递归或在同一次死亡通知中重置状态；需要事件后修改状态时，应在通知返回后或下一帧显式调用。
- 不使用组件 Tick、网络复制、GAS，也不自动销毁 Actor、禁用移动、改变相机或触发布娃娃。

## 二、新增文件

| 文件 | 用途 |
| --- | --- |
| `Source/WastelandAdventure/Components/WAHealthComponent.h` | 独立组件、Blueprint 接口、属性与事件。 |
| `Source/WastelandAdventure/Components/WAHealthComponent.cpp` | 生命周期、唯一扣血逻辑、边界和死亡锁。 |
| `Source/WastelandAdventure/Tests/WAHealthTestObserver.h` | 测试用 Transient UObject，作为真实动态事件接收器。 |
| `Source/WastelandAdventure/Tests/WAHealthTestObserver.cpp` | 记录事件次数、Delta、通知状态及重入尝试；游戏不实例化此对象。 |
| `Source/WastelandAdventure/Tests/HealthComponentTest.cpp` | ComponentRules：十项规定规则及额外边界验证。 |
| `Source/WastelandAdventure/Tests/HealthPIETest.cpp` | ActorPIE：正式蓝图玩家、引擎伤害入口、独立 Actor、图形帧与控制共存测试。 |
| `Tests/RunHealthAcceptance.ps1` | 统一运行新增两项与原有六项测试，检查真实退出码和 JSON 结果。 |
| `Docs/Phase3_1Health.md` | 本报告。 |

## 三、修改文件与保护范围

| 文件 | 最小变更 |
| --- | --- |
| `Source/WastelandAdventure/WastelandAdventureCharacter.h` | 前置声明、只读 HealthComponent 引用和访问器。 |
| `Source/WastelandAdventure/WastelandAdventureCharacter.cpp` | 仅新增组件 include 和构造函数中的 CreateDefaultSubobject。 |
| `README.md` | 加入 3.1 使用方式、验收入口和范围。 |
| `Docs/DevelopmentPlan.md` | 标注仅完成第三阶段 3.1，后续仍为计划。 |

Character 接入的必要原因已在修改前报告：让正式玩家出生时自动实例化组件，而不是只在测试中临时挂载。未修改任一移动、奔跑、闪避、摄像机或动画函数；Character.cpp 去掉上述两条新增语句后与基线一致。Build.cs、PlayerController、原有六项测试源码、Variant_Combat 和所有资产未改。

开工时已有未提交的 AGENTS.md 与 Config/DefaultEngine.ini，均保持原样。未改动 `Docs/Phase2_5Acceptance.md` 或其中已知问题。805 个 Content 文件、配置、历史报告和 Git 索引的最终核对结果见 `Saved/Verification/Phase3_1/FinalVerification.json`；原始 SHA-256 清单位于该目录的 Baseline/FileHashes.json。

## 四、核心代码与 Blueprint 配置

正式玩家构造函数仅增加：

```cpp
HealthComponent = CreateDefaultSubobject<UWAHealthComponent>(TEXT("HealthComponent"));
```

统一实际扣血入口为组件 ApplyDamage。引擎路径为 `UGameplayStatics::ApplyDamage → Actor::TakeDamage → OnTakeAnyDamage → UWAHealthComponent::ApplyDamage`。组件只监听 AnyDamage；不再监听 PointDamage / RadialDamage，不把伤害回发给 Actor，也没有在 Character 中维护第二份 HP 或重写 TakeDamage。因此一次引擎伤害事件只扣一次血；点伤害同时产生点/通用通知时也只处理一次。

Blueprint 为同一次命中选择一次引擎 Actor 入口（推荐），或直接调用组件 ApplyDamage，不能把两条路径再各调用一遍。引擎 Actor 的 TakeDamage 返回值仍遵循引擎原语义，不代表组件实际 HP 损失；需要准确扣血量时使用组件返回值或变化事件 Delta。若其他 Actor 子类重写 TakeDamage 且不调用 Super，AnyDamage 不会自动到达组件，接入前需另行评估，不能与官方 Combat 模板的旧 HP 逻辑并挂。

编辑器配置步骤：

1. 关闭编辑器，正常构建后打开工程。打开 BP_ThirdPersonCharacter，在组件列表选择继承的 HealthComponent；正式玩家已经有组件，不要重复添加。
2. 在蓝图类默认值的该组件中配置 Health → Max Health，默认 100。Current Health 与 Is Dead 为运行时只读值；开始 PIE 后从世界大纲选择实际玩家，再选择 HealthComponent 查看。
3. 在蓝图 Event Graph 中取得 GetHealthComponent，绑定 OnHealthChanged / OnDeath；监听器读取已提交状态。界面首次绑定时主动读取 GetCurrentHealth / GetMaxHealth / GetHealthPercent，不依赖默认 100→100 的无变化初始化事件。
4. 对普通 Actor 蓝图可通过 Add Component 添加 WA Health Component 一次，再正常 Compile / Save。本次没有为测试创建或保存 Blueprint/UAsset，也没有替玩家重复增加组件。
5. ResetHealth 由明确的新生命流程调用；本任务不实现重生、死亡动画或移动限制。Python 查询中 `h.is_dead` 是只读属性，C++ / Blueprint 的 IsDead 接口按反射正常使用。

## 五、实际编译命令与退出码

通过 Tests/BuildEditor.ps1 实际执行：

```powershell
& 'F:\levn\game\Epic\UE_5.8\Engine\Build\BatchFiles\Build.bat' WastelandAdventureEditor Win64 Development '-Project=F:\levn\game\Projects\WastelandAdventure\WastelandAdventure.uproject' -WaitMutex -NoEngineChanges -NoHotReloadFromIDE -MaxParallelActions=1
```

真实源代码构建：`Saved/Verification/EditorBuild-20261102-224801.log`，Result: Succeeded，BUILD_EXIT_CODE=0，150.72 秒。UHT 生成 7 个文件，执行 14 项编译/链接/元数据任务，包含 WAHealthComponent、两个新增测试、事件接收器、Character 与模块；DLL / lib 均实际链接成功。没有依赖 Live Coding。

文档和脚本更新后的最终 Tests/BuildEditor.ps1 执行结果单独保存于 FinalVerification.json；其 Up to date 不能替代上述源代码实际编译记录。

## 六、自动化测试结果

最终成功目录：`Saved/Verification/Phase3_1/Run-20261102-225807/`。Report/index.json 中八项全部 Success，每项 errors=0、warnings=0；succeeded=8、failed=0、notRun=0。RunnerResult.json 记录实际编辑器退出码 0；验证脚本退出码也为 0，输出 HEALTH_ACCEPTANCE_PASS。

ComponentRules 覆盖下表，ActorPIE 在真实蓝图玩家上重复核验关键数值与事件：

| 必测项 | 实测结果 |
| --- | --- |
| 1 初始生命值 | 100，最大值 100，比例 1，存活。 |
| 2 伤害 20 | 80，变化 Delta=-20。 |
| 3 恢复 10 | 90，变化 Delta=+10。 |
| 4 治疗超过最大值 | 保持 100，返回实际补充量。 |
| 5 超过当前值的伤害 | 归零并锁定死亡，扣血量不超过原 HP。 |
| 6 死亡事件次数 | 本条生命一次；重置后下一条生命可以再次触发一次。 |
| 7 死亡后再次受伤 | 返回 0，无额外 HP 变化或死亡通知。 |
| 8 死亡后普通治疗 | 无效，HP 仍为 0；重复初始化也不能复活。 |
| 9 ResetHealth | 显式恢复 100 并清除死亡，之后伤害与治疗正常。 |
| 10 负数伤害/治疗 | 无效，无变化事件。 |

额外测试：零、NaN、Infinity 无效；大额有限治疗有界；自定义 MaxHealth=250 和比例 0.5 正确；非法最大值恢复安全默认值；动态事件观察一致状态；同步通知重入受到保护；无 Tick/复制；普通 Actor 动态注册组件可运行；引擎通用/点伤害不重复扣血；销毁组件解除伤害监听；正式玩家只有一个生命值组件。

复现命令（先关闭编辑器）：

```powershell
& .\Tests\BuildEditor.ps1
& .\Tests\RunHealthAcceptance.ps1
```

需要人工控制焦点时使用 `-ManualStart`：脚本启动真实编辑器并等待，在前台完成检查、停止 PIE 后，在 UE 命令栏输入 `Automation RunTests WastelandAdventure.`。脚本在编辑器测试结束退出后执行同样的八项 JSON 与退出码校验。最终成功轮使用此方式；未修改任一 C++ 测试断言。

## 七、图形运行验收结果

使用默认 D3D12 图形渲染，加载当前正式 ThirdPerson 地图并运行真实 BP_ThirdPersonCharacter；没有使用 NullRHI。原生组件自动初始化，绑定原 ABP_Unarmed，原角色可见。

测试之外，还在同一编辑器的手动 PIE 中选择实际玩家的 HealthComponent，直接观察 Details 中的 Current Health / Is Dead：初始 100 / false；通过引擎 Actor 伤害入口施加 20 后为 80 / false；致死后普通治疗与初始化仍为 0 / true；显式重置后为 100 / false。手动检查结束后停止 PIE、恢复原大纲面板，地图显示全部已保存；未保存资产改动。

对应真实 LogPython 输出（北京时间）为 23:01:14、23:05:30、23:06:11、23:07:19，均在成功轮 HealthAcceptance.log 中，以 `WA_HEALTH_MANUAL` 标记。首次只读查询的 Python 调用错误及更正也保留在该日志，见第九节。

自动化原生视口帧位于 `Saved/Verification/Phase3_1/Frames-20261102-231106/`，包含 Initial100.png、Damage80.png、Dead0.png、Reset100.png。视口帧展示真实关卡和人物；它们没有血条或可见 HP 文字，不能单凭画面推断生命值。数值结论由实际运行断言、日志和上述实时 Details 观察共同支持。

## 八、第二阶段回归结果

最终轮完整执行原有六项：AnimationPIE、CameraPIE、DodgePIE、Integration30FPS、Integration60FPS、SprintPIE，全部 Success。原有测试源码与断言未修改。

| 回归范围 | 结果 |
| --- | --- |
| WASD、Shift 奔跑/释放、Space 跳跃 | 原测试与生命值 ActorPIE 均通过；实际 500 / 750 cm/s，跳跃、落地和停止正常。 |
| 闪避 | 继续使用已验收的 Left Ctrl，距离/持续时间/冷却、墙体、空中拒绝及恢复通过。任务说明里的 Alt 按旧版描述处理，没有改键或扩展输入逻辑。 |
| 摄像机 | 原 CameraPIE 的鼠标轴、俯仰限制、位置平滑、靠墙和恢复通过。 |
| 原有动画 | 原 AnimationPIE 的真实状态机、BlendSpace 样本、骨骼、闪避与取消恢复通过，仍使用 ABP_Unarmed。 |
| 30 / 60 FPS | 仿真 29.983 / 59.932 FPS，墙钟 29.981 / 59.935 FPS；各 510 / 960 个采样。 |
| 斜坡、边缘与快速输入 | 当前地图真实斜坡 42 / 81 个采样均着地；约 200.25 cm 高平台越边取消；快速 Shift / Left Ctrl 未绕过状态或冷却。 |

本次完整回归覆盖当前已保存地图，并不把结果改写回第二阶段的历史报告。两种帧率测试是功能回归，不是发布版本性能承诺。

## 九、已知问题与失败记录

1. 第二阶段首轮相机两条断言失败、同代码复测成功但根因未确认的历史记录原样保留。本任务没有修复或掩盖该历史问题。
2. 启动阶段 13 条 `LogAutomationTest: Error: Condition failed` 仍存在，发生在本项目测试开始前，未在本任务中定位。不能宣称整份编辑器日志无错误；八项测试各自 JSON 的错误/警告均为零。
3. 本任务首轮 `Run-20261102-225113` 未整体通过：AnimationPIE 在 Return to walk 采样为 Idle，速度及动画速度均为 0，引发 5 条断言失败；此前 Walk / Run 为实际 500 / 750，其余七项（含两项生命值测试）成功。该次发生过窗口输入/焦点干扰提示，但尚不能据此确证根因。原失败 JSON / 日志完整保留；稳定前台、同代码同断言的完整复测八项成功，没有放宽断言或修改动画核心。
4. 手动 Python 查询首次将 `h.is_dead` 当函数调用，产生一次 `TypeError: 'bool' object is not callable`（三行 traceback）。改为读取属性后查询成功；没有因此修改组件，也不把这次脚本调用错误算成游戏组件缺陷或宣称日志无错误。
5. 尚未执行 Windows 打包或 Shipping 验收；没有新增伤害来源、攻击流程、死亡表现、重生或体力。生命值归零目前只改变组件状态并通知，不限制原有角色控制。
6. 原有未提交文件保持，按本次明确要求未执行 Git stage、commit、push 或合并。Saved 证据和本机日志不纳入版本控制。

## 十、下一任务建议与停止

建议先审阅本报告和 Blueprint 接入约定，再单独下达下一任务。3.1 到此完成；不自动开始 3.2 体力系统或攻击系统。后续继续跟踪输入/焦点相关回归的偶发失败及历史启动条件错误。
