# Wasteland Adventure 开发规则

## 项目边界

- 显示名称 Wasteland Adventure；工程和 C++ 模块名 WastelandAdventure。
- 固定使用 UE 5.8.4、Windows x64、C++ + Blueprint、纯单机离线架构。
- 修改前检查相关文件、Git 状态与本文档。保留用户变更和已有历史，禁止无关修改、大规模重构、自动升级引擎。
- 不修改引擎源码或安装配置，不伪造 `.uproject`、`.uasset`、`.umap` 或测试证据。资源必须由 UE 编辑器、官方模板或真实导入流程产生。
- 保留官方 Third Person C++ 模板附带的变体文件；它们不是本项目已实现的 RPG 系统。本阶段不扩展这些变体。

## 架构与代码

- 使用 Unreal C++ 命名、反射、生命周期、智能指针和 UObject 所有权规则；头文件最小依赖，`.generated.h` 最后包含。
- C++ 负责核心状态、规则和稳定接口；Blueprint 负责资产绑定、参数、动画、UI 与关卡表现。避免 Blueprint 承载重复核心规则。
- 按功能模块和组件逐步扩展；有明确需求和可验证行为后再创建类，禁止大量空壳类。
- 单机框架使用 GameMode、PlayerController、Character、GameInstance/LocalPlayer Subsystem 与本地 SaveGame；仅在真实需求出现后引入对应类型。
- 不添加多人联机、RPC、网络复制、Dedicated Server、EOS、在线数据库或无关第三方插件。

## 工作流程与验证

1. 检查 → 计划 → 最小实施 → 实际编译 → 实际运行 → 记录 → 验收。
2. 每次完成修改后实际执行 `Tests/BuildEditor.ps1`；文档修改可得到 Up to date，但它不能替代源代码首次真实编译记录。
3. C++ 修改前结束 PIE 并关闭编辑器，避免 Live Coding 掩盖冷构建问题。配置/资源修改后重新加载并验证相关场景。
4. 将真实日志放在 `Saved/Verification/`，将可复现步骤和结论写入 `Docs/Phase1Acceptance.md` 或对应阶段报告。
5. 静态检查、目录存在、进程启动、空白地图、NullRHI 测试均不能代替玩家实际运行验收。未执行项目必须标记未验证。
6. 失败时先保存真实错误、定位、修复并重跑，不得跳过或伪称成功。
7. 本阶段不新增战斗、技能、AI、背包、装备或任务系统功能。

## Git 与资源

- Source、Content、Config、必要 Plugins 文件、文档、测试脚本及 `.uproject` 纳入版本控制。
- 不提交 Binaries、Intermediate、Saved、DerivedDataCache、.vs、IDE 生成文件、凭据或本机日志。
- `.uasset`、`.umap`、FBX、音频、贴图等二进制资源使用 Git LFS。检查实际索引指针与 LFS 对象，不能只检查 attributes 文本。
- 禁止重写历史、自动推送；首次 commit 必须先列出拟提交范围并取得用户明确批准。
- 覆盖用户文件、删除内容、安装变更或重大配置调整先告知并取得授权。常规需求范围内最小实现不应重复询问。
