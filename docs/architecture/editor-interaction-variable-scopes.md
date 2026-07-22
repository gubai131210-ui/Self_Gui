# 编辑器交互与变量作用域

日期：2026-07-21

## 交互语义

| 操作 | 行为 |
|------|------|
| 工具箱单击 | 仅选中/预览说明，**不创建**节点 |
| 工具箱双击 / Enter | 在视口中心**创建一次** |
| 拖拽到画布 | 在落点创建一次 |
| 画布节点双击 | 打开属性页，不复制节点 |

根因修复：移除 `activated`+`doubleClicked` / `itemActivated`+`itemDoubleClicked` 双重连接；保留 80ms 同 type 事件去重。

## 属性面板

- `QScrollArea` 包裹动态参数，避免无限向下挤压
- 按 `ModuleParamDef.group` 生成可折叠 `QGroupBox`
- 支持参数过滤与「仅已绑定」
- Dock 与双击弹窗仍共用 `PropertyPanel`

## 变量作用域链

`全局 → 工程 → 流程 → 分组 → 算子`（近处覆盖远处）

- `VariableScopeChain` / `GlobalVariableStore`
- `VisionFlow::flowVariables` + `groupVariables`
- `.seltproject` 容器：`manifest.extensions.projectVariables` + 流程 JSON 的 `flowScopedVariables`
- 视图 → **变量管理** Dock

Image 等视觉对象：配置态存引用/元数据，像素为 runtime-only。

## 验收清单（Qt Creator）

- [ ] Configure + Build `Selt_Gui`
- [ ] Build/Run `Selt_Gui_editor_variable_ux_tests`
- [ ] 工具箱单击不创建；双击只出现 1 个节点
- [ ] 拖拽创建正常
- [ ] 属性页可滚动、分组可折叠、过滤可用
- [ ] 变量管理 Dock：全局/工程/流程变量增删
- [ ] 保存/打开 `.seltproject` 后工程变量与流程变量仍在
- [ ] 打开旧 `.selt` 仍可运行

请在中文路径环境下自行 Configure / Build / Run；不自动 Git 提交。
