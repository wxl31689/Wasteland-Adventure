# 编码与版本控制规范

- C++ 遵循 Unreal 类型前缀 A/U/F/E/I/T、布尔 b 前缀、PascalCase 公开成员；文件名与主要类型一致，工程代码使用 ASCII 标识符。
- 使用 UPROPERTY/UFUNCTION 只暴露确有需要的接口，合理使用 BlueprintReadOnly、EditDefaultsOnly、Category。反射头的 `.generated.h` 放在最后。
- UObject 引用遵循 GC/所有权语义，不使用裸 `new/delete` 创建 UObject。非 UObject 资源采用 RAII。避免 Tick 承载可事件驱动的逻辑。
- Includes 尽量放在 cpp，头文件使用前置声明；模块依赖只按实际调用添加。禁止为了消除错误而随意扩大引擎或插件依赖。
- Blueprint 资产使用 BP_、WBP_、ABP_、DA_ 等适当前缀；材质 M_/MI_、纹理 T_、骨骼网格 SK_、动画 A_。官方模板资源保留原名，避免无目的迁移和重命名。
- 新功能按领域组织 Source/Content；避免跨领域循环依赖。没有实现需求时不创建空类/空目录树。
- 注释解释约束、意图、单位与边界，不重复代码。面向开发者的文档可用中文；日志保持稳定、可搜索，避免凭据和个人敏感信息。
- 小步提交，提交信息说明行为和原因；提交前核对 diff、真实构建、运行与 LFS。首次 commit 必须经用户批准；不自动推送或改写历史。
- 二进制资源使用 LFS，发生冲突通过 UE 资源工具或人工合并处理，禁止把指针或任意字节冒充真实资源。
- 生成物、缓存、本机 IDE 状态、日志和密钥不入库；`.uproject`、Config、Source、Content 和必要 Plugins 保留。
