# 部署与运行目录说明（生产化）

日期：2026-07-21  
相关：ADR-0009、`docs/Opencv_MinGW_cpp配置说明.md`、`docs/troubleshooting/selt-gui-exe-missing.md`

## 目标布局

开发构建与安装后建议保持同一相对结构：

```text
<app>/
  Selt_Gui.exe
  Qt6Core.dll / Qt6Gui.dll / Qt6Widgets.dll / Qt6Svg.dll ...
  Qt6Network.dll                  # TCP/IP、Modbus TCP
  Qt6SerialPort.dll               # COM、Modbus RTU（若启用）
  Qt6SerialBus.dll                # 可选；官方 Modbus API 探测用
  platforms/qwindows.dll          # Qt 平台插件
  libopencv_core*.dll
  libopencv_imgproc*.dll
  libopencv_imgcodecs*.dll
  libopencv_videoio*.dll          # 可选；缺失时相机不可用，文件/回放仍可用
  plugins/nodes/*.dll             # 节点插件（如 selt_invert_node）
```

图标与主题已打进 `resources/selt.qrc`，一般**不需要**再拷贝 `resources/icons`。

## CMake 现状

| 项 | 行为 |
|----|------|
| 主程序输出 | `${CMAKE_BINARY_DIR}/bin/Selt_Gui.exe` |
| 示例插件输出 | `${CMAKE_BINARY_DIR}/bin/plugins/nodes/` |
| `install(TARGETS Selt_Gui)` | 安装 exe |
| `qt_generate_deploy_app_script` | 安装 Qt 运行库与平台插件 |
| `install(DIRECTORY .../plugins)` | 可选安装已构建的 `plugins/` |
| OpenCV DLL | **不会**自动安装；需加入 PATH 或拷到 `<app>/` |

Configure 时 CMake 会打印：

- `Selt_Gui: OpenCV vision enabled/disabled`
- `OpenCV videoio enabled/disabled`
- `Selt_Gui: Qt Network=... SerialPort=... SerialBus=...`

`SELT_HAS_OPENCV=0` 时产物为**编辑器外壳**（无视觉算子/插件），不是残缺视觉版。

通讯节点默认 `useMock=true` 可在无硬件下联调；现场 COM/TCP/Modbus 请关闭 Mock，并确保对应 Qt DLL 在 PATH 或 exe 同级。详见 `docs/architecture/communication-operators.md`。

## 现场排障

1. **exe 不存在**：见 `docs/troubleshooting/selt-gui-exe-missing.md`；确认运行目标是 `Selt_Gui`，产物在 `bin/`。
2. **缺 Qt DLL**：用 Qt Creator 部署脚本或手动拷贝 Kit 对应 `mingw_64/bin`。
3. **缺 OpenCV DLL**：将 OpenCV `install/x64/mingw/bin` 加入 PATH，或拷贝到 exe 同级。
4. **插件未加载**：检查 `plugins/nodes/`，打开检查器「说明」页或运行监视中的插件诊断 / 部署要求。
5. **相机不可用**：确认 `SELT_HAS_OPENCV_VIDEOIO=1`；否则仅文件/目录回放可用。
6. **通讯失败**：检查 Network/SerialPort DLL；Modbus 写需 `allowWrite=true`；地址为协议零基。

## 人工验收（Qt Creator）

1. Configure → Build `Selt_Gui` → 确认 `bin/Selt_Gui.exe`。
2. 运行：插入流程模板 → F5 → 停止 → 再 F5。
3. 故意断开测量节点输入 → **执行 → 发布前校验** 应报 Error（`missing_required_input`）。
4. 打开非法图片路径 → 运行监视出现设备/执行诊断码。
5. 检查器「说明」可见插件加载诊断与部署要求摘要。
