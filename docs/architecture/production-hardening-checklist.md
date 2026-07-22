# 生产化稳定迭代验收清单

在 Qt Creator 中 Configure / Build 后验证。中文路径请用 Qt Creator，避免命令行覆盖损坏产物。

## 自动化测试（可选）

- `Selt_Gui_production_hardening_tests`
- `Selt_Gui_run_controller_tests`
- `Selt_Gui_runtime_guard_tests`
- `Selt_Gui_plugin_diagnostic_tests`
- `Selt_Gui_visionmaster_experience_tests`
- `Selt_Gui_project_container_tests`

## 运行时安全

- [ ] 忙时再次点「单次执行 / 连续运行」被拒绝，不重复启动
- [ ] 「停止」后状态回到空闲，可再次运行
- [ ] 节点失败时运行监视显示失败类型 + **诊断码**（不再显示「无」）
- [ ] 日志行形如 `[执行失败/image_empty] ...`

## 发布校验

- [ ] 缺必需输入时，编辑期可为 Warning；**发布前校验**升为 Error
- [ ] 禁用节点 / 断点仍有提示
- [ ] 含测量/定位节点时有标定单位提示（不伪造 mm）

## 输入源与插件

- [ ] 打开非法图片路径：输入源 Dock / 运行监视可见诊断
- [ ] 关闭输入源后诊断清空
- [ ] 检查器「说明」包含插件诊断与部署要求
- [ ] 启动时监视器可显示插件部署摘要

## 流程模板

- [ ] `定位→判定`：Blob → 判定 → 预览（端口合法）
- [ ] `完整测量链路`：轮廓输出接到矩形测量输入（非死分支）
- [ ] 模板插入后可继续编辑；无伪造物理单位

## 部署

- [ ] 阅读 `docs/deployment/README.md`
- [ ] `bin/Selt_Gui.exe` 与 `bin/plugins/nodes` 布局符合说明
- [ ] 缺 OpenCV videoio 时文件/回放仍可用

## 兼容

- [ ] 打开旧 `.selt`、保存、Undo/Redo
- [ ] 不改 typeId / 端口 ID / `.selt` 格式
