# Dependency Rules

## Allowed

- `SeltStudio` → GraphEditor, VisionDisplay, FlowCore, Runtime, PluginSdk
- `SeltVisionRuntime` → FlowCore, VisionData, Foundation, Communication adapters
- `SeltOpenCvNodes` → PluginSdk, VisionData, OpenCV
- `SeltGraphEditor` → FlowCore, Foundation, Qt Widgets
- `SeltVisionDisplay` → VisionData, Foundation, Qt Widgets
- `communication/*` → Qt Network/SerialPort (no QWidget); Mock transports for tests
- `CommunicationManager` → clients/codec only; UI (`CommunicationDock`) may call manager, but manager must not include QWidget
- Executor 节点可通过 `connectionId` 复用 manager；未填时保持临时客户端（兼容旧流程）

## Forbidden

- GraphEditor / MainWindow → OpenCV headers
- OpenCvNodes → QWidget / MainWindow
- PluginSdk → OpenCV (keep ABI Qt/Core only where possible)
- Runtime → UI widgets
- Communication codecs → QWidget / FlowRuntime internals

## Deprecation Marker

Use `[[deprecated("phase-N: use Xxx instead")]]` or comments:

```cpp
// SELT_DEPRECATED(phase-3): replace with INodeExecutor registry
```
