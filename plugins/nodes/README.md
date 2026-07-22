# 插件节点目录

将编译后的 Qt 插件（`.dll` / `.so` / `.dylib`）放到本目录，应用启动时由 `PluginHost` 扫描加载。

## 要求

1. 实现 `Selt::INodePlugin`（IID: `selt.vision.nodeplugin/1.0`）。
2. `metadata().sdkVersion <= VersionInfo::pluginSdkVersion`。
3. 提供非空 `pluginId`、`nodeTypeIds` 和 `descriptors()`。
4. 建议补充 `pluginVersion`、`abiTag` 和 `compiler`，便于工程容器记录依赖并做部署诊断。
4. 在 `registerExecutors()` 中注册执行器工厂。
5. **必须与主程序使用同一套 Qt / MinGW / OpenCV ABI**（Qt Creator 里同一 Kit）。

## 冲突策略

- 与内置 `vision.*` 节点 ID 冲突的插件会被拒绝并写入诊断，不影响旧工程打开。
- 插件加载失败只记录诊断，不会阻止主程序启动。
- 加载成功后 descriptor 会合并进 `VisionNodeRegistry`，节点库会在主窗口启动时 `reloadCatalog()`。

## 示例插件

独立工程：[`plugins/examples/invert_node/`](../examples/invert_node/)

```text
# 在 Qt Creator 中打开该 CMakeLists，选用与 Selt_Gui 相同的 MinGW Kit
# 构建后把 selt_invert_node.dll 复制到：
#   <Selt_Gui 运行目录>/plugins/nodes/
```

启动主程序后，节点库应出现「插件 / 反色(示例插件)」。失败信息可在底部「运行监视」诊断区查看；工程容器会记录插件 ID、SDK 版本和节点类型依赖。

## 当前状态

本目录默认不包含示例插件二进制；请按上面步骤自行编译复制。
