# 算子交互与工作区升级说明

日期：2026-07-22

## 工作区

- 「图像与结果」支持上下/左右布局切换：`视图 → 图像/结果左右布局`
- 分隔条加宽（8px）并可拖拽；状态写入 `ui/visionSplitterOrientation` / `ui/visionSplitterState`
- `视图 → 恢复图像与结果布局` 仅重置该 Dock 的比例与方向
- 图像预览在「适应」模式下随 Dock 尺寸自动等比适配；1:1 / 滚轮缩放后保持用户视图
- `setImage()` 仅在 `autoFit` 时 refit；预览工具栏预留滚动条高度，避免按钮被遮挡

## 端口暴露（VisionPro 风格）

- 完整契约仍在 `ModuleDescriptor.ports`
- 画布只显示 `NodeModel.exposedPortIds`（默认：主图像链路）
- 属性面板「输入与输出」按主链路/结果/调试分组勾选；已连接/已绑定有标记
- 暴露切换走 `ChangeNodePortExposureCommand`（可撤销/重做）
- 隐藏端口不影响 GraphValidator / BindingResolver / FlowRuntime
- 连线增删后端口胶囊连接态与布局即时刷新
- 插件未设置 `hasExposureHints` / `capabilityVersion=0` 时全暴露；`capabilityVersion>=1` 复用 ROI 注入与 hints
- 详见 [ADR-0012](adr/0012-port-exposure-canvas-contract.md)

## ROI / 模板 / RunRecord

- 带 ROI 参数的算子自动获得可选 `roi` 输入端口（默认隐藏）
- 优先级：上游 ROI/Region > 节点参数 ROI > 空
- 模板仍为项目资源；模板匹配输出 `center` / `region` / `score` / `overlay`
- `VisionContext.runRecords` 与 `Selt::RunRecord` 复用模块结果通道；`retainImageSnapshots` 控制调试/生产图像策略

## 子流程

- 内置 `vision.subflow`：对外 Terminal 来自 `subflowInterface`
- 属性面板可编辑 flowId / 接口 JSON 并刷新端口
- 执行器当前为 Terminal 透传桥（内嵌流程编排后续接入 ProjectService）

## 本地验收（Qt Creator）

1. Re-Configure（layoutVersion=6）并 Build：`Selt_Gui`、`Selt_Gui_port_exposure_tests`、`Selt_Gui_run_record_tests`、`Selt_Gui_runtime_binding_tests`、`Selt_Gui_ui_foundation`（或对应 UI 测试目标）
2. 切换图像/结果左右布局，重启确认保持；窄 Dock 下确认工具栏按钮可点
3. 放入 Blob/测量节点：勾选暴露 → Undo/Redo → 连线/绑定 → 运行查看 runRecords
4. 隐藏已绑定端口，确认运行不受影响且绑定选择器显示「已隐藏」
5. 打开旧 `.selt`，确认节点、连接、模板资源与 `in/out` 别名不丢失
6. 子流程节点：写入接口 JSON → 应用 → 画布仅显示 Terminal

中文路径请自行测试；不自动 Git 提交。
