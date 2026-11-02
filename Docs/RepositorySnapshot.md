# 当前仓库快照

日期：2026-11-02。仓库：<https://github.com/wxl31689/Wasteland-Adventure>。

## 已完成范围

- UE 5.8.4 官方 Third Person C++ 工程初始化、开发环境配置及 Git LFS 规则。
- 第二阶段：奔跑、Left Ctrl 闪避、摄像机、角色动画状态及综合验收。实际编译与六项真实图形 PIE 回归结论、首轮失败和已知限制见 [2.5 验收报告](Phase2_5Acceptance.md)。
- 官方模板附带的变体代码与资源保留；这些内容不代表本项目已开发第三阶段战斗或其他 RPG 系统。
- 当前已保存的地图编辑一并归档。与 2.5 验收基线相比，Content 新增 51 个资源、8 个原有外部 Actor 资源发生变化；包括 Modeling Tools 生成的模型和 World Partition 外部 Actor。它们是地图依赖资源，不能按编译缓存排除。上述后续地形编辑尚未重复执行完整 PIE 验收，历史验收结论不扩展到这些新编辑。

## 获取与构建

先安装 Git LFS、UE 5.8.4、Visual Studio 2022 的“使用 C++ 的游戏开发”工作负载。在 PowerShell 中执行：

```powershell
git lfs install
git clone https://github.com/wxl31689/Wasteland-Adventure.git
Set-Location Wasteland-Adventure
git lfs pull
& .\Tests\BuildEditor.ps1 -EngineRoot 'F:\levn\game\Epic\UE_5.8'
& .\Tests\RunPhase2Acceptance.ps1 -EngineRoot 'F:\levn\game\Epic\UE_5.8'
```

请将 EngineRoot 替换为本机实际引擎目录。通过 `.uproject` 重新生成本机解决方案，不依赖作者机器的 Binaries 或 Intermediate。运行验收脚本前关闭编辑器。

## 上传范围与本机数据

版本库包含 Source、Content、Config、Docs、Tests、`.uproject`、`.vsconfig`、Git/LFS 规则与开发规则。Unreal 二进制资产在工作区保持真实二进制，在 Git 中保存 LFS 指针，并同时上传对应 LFS 对象。

Binaries、Intermediate、Saved、DerivedDataCache、IDE 生成文件及本机原始日志不上传。阶段报告保留可复现命令、实际结果与本机证据路径；克隆后原始 Saved 日志不会自动存在，可重新运行测试生成本机证据。

编辑器在本机 `Config/DefaultEngine.ini` 生成的 AndroidFileServer SecurityToken 不进入提交；本机配置保持原样，因此该文件在上传后可显示一处仅本机的未提交差异。以后提交配置时也需排除令牌值。历史报告中的 Git 状态描述均指验收时点。
