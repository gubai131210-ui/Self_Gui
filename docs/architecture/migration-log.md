# Migration Log

| Date | Phase | Change | Notes |
|------|-------|--------|-------|
| 2026-07-21 | 0 | Foundation target, architecture docs, CTest | Product behavior preserved |
| 2026-07-21 | 1 | VisionProject / VisionFlow domain scaffold + Document adapter | Multi-flow domain ready |
| 2026-07-21 | 1 | **ProjectService + multi-flow UI + per-flow Undo + migration report** | Active canvas mirrors active VisionFlow; legacy `.selt` bridge |
| 2026-07-21 | UI | **Icon/QSS theme layer, compact toolbar, node catalog MV, layout persistence** | Qt Svg + `resources/selt.qrc`; no Fluent framework dependency |
| 2026-07-21 | 2 | **Typed ports + ParameterBinding + GraphValidator + PortValueStore/BindingResolver + BindingEditor** | Legacy `.selt` constants preserved; optional `document.projectVariables` |
| 2026-07-21 | 3 | **ROI交互增强 + RuntimeDiagnostic + 工业节点(Blur/Canny/Morph/Resize) + InputSourceService + 调度/容器接口** | ADR 0005；旧 `.selt` 版本不变 |
| 2026-07-21 | 4 | **运行进度/忙态 UI + Scheduler 注入 + 插件 descriptor 合并 + 输入源 Dock/live-frame + TemplateMatch 节点** | ADR 0006；示例 `selt_invert_node` |
| 2026-07-21 | 5 | **节点视觉元数据 + 折叠/摘要 + 多形状 ROI + 模板教示/资源库 + templateSource** | ADR 0007；`projectResources` 可选扩展 |
| 2026-07-21 | 3 | INodeExecutor + FlowRuntime DAG | VisionPipeline delegates |
| 2026-07-21 | 4 | INodePlugin + PluginHost | Hybrid plugins |
| 2026-07-21 | 5 | Port direction validation on connect | Graph specialization continues |
| 2026-07-21 | 6 | ExtendedRoi model | Interactive shapes next |
| 2026-07-21 | 7 | ProjectStorage legacy bridge | Container format reserved |
| 2026-07-21 | 8 | RunController once/loop/stop + monitor widget | |
| 2026-07-21 | 9 | Blur/Canny/Morphology/TemplateMatch algorithms | Registry expansion next |
| 2026-07-21 | 10 | ICameraDevice + OpenCV/replay adapters | Vendor SDK plugins later |
| 2026-07-21 | 11 | ProjectVariableStore + SubflowInterface stubs | |
| 2026-07-21 | 12 | AppLog + AppSettings + plugin README | Packaging later |
| 2026-07-21 | 6 | **Directory project container + manifest/resource migration + atomic replacement** | Legacy `.selt` remains readable |
| 2026-07-21 | 6 | **ExecutionScope + immutable FlowExecutionSnapshot + partial DAG execution** | Single-node and upstream-closure runs |
| 2026-07-21 | 6 | **Canvas debug-run actions + scope/snapshot monitor metadata** | Runtime results remain separate from graph edits |
| 2026-07-21 | reliability | **Unicode-safe image loading + template type guards + diagnostic codes** | OpenCV assertions converted to runtime failures |
| 2026-07-21 | reliability | **Plugin metadata/version/ABI diagnostics + monitor filter/copy tools** | Deployment visibility improved |
| 2026-07-21 | industrial | **Measurement domain + circle/line/distance/decision nodes + calibration UI** | ADR 0010 |
| 2026-07-21 | industrial | **Camera frame/trigger metadata + CSV/JSON result sinks + stub camera tests** | Failure-isolated outputs |
| 2026-07-21 | operators | **O0–O5 算子平台：ROI/诊断、预处理/区域/定位/测量/逻辑执行器拆分** | ADR 0011 |
| 2026-07-21 | operators | **Contour/Region/Table 数据类型 + ROI/Builtin Executor 测试门禁** | 中文路径请用 Qt Creator 验证 |
| 2026-07-21 | ui | **VisionMaster 风格布局：工具箱/右侧图像结果/工业暗色主题** | 不改算子与运行逻辑 |
| 2026-07-21 | ui | **算子逐一 SVG 图标 + IconProvider 按 typeId 加载** | `resources/icons/vision/` |
| 2026-07-21 | roadmap | **编辑器路线图基线冻结 + .gitignore 产物隔离** | `docs/architecture/roadmap-baseline.md` |
| 2026-07-21 | editor | **对齐/分布/自动布局菜单、连线吸附替换、收藏/最近、小地图** | Phase 1 |
| 2026-07-21 | editor | **断点、Warning 态、测量过滤、发布校验、资源浏览器、UI schema** | Phase 2–5 基础 |
| 2026-07-21 | algorithm-factor | **计量 helpers + 精度/标定/ROI/噪声基线；卡尺/RANSAC/阈值/亚像素/Hough 参数化** | 见 `algorithm-factor-checklist.md` |
| 2026-07-21 | algorithm-factor | **多目标候选契约 + 性能基线测试 + OpenCV 替代矩阵更新** | `Selt_Gui_algorithm_*_tests` |
| 2026-07-21 | visionmaster-ux | **算子参数分组/缺参补齐、流程模板、DAG 层级布局、橙黑工业风全 UI** | 兼容旧 `.selt`；`Selt_Gui_visionmaster_experience_tests` |
| 2026-07-21 | production | **运行诊断码贯通、停止安全、发布门禁、输入源/插件诊断、部署说明** | `docs/deployment/README.md`；`Selt_Gui_production_hardening_tests` |
| 2026-07-21 | communication | **通讯算子平台：COM/TCP/Modbus 客户端 + 编解码/数值/字符串节点** | Qt Network/SerialPort 可选；`docs/architecture/communication-operators.md`；`Selt_Gui_communication_tests` |
| 2026-07-21 | communication-prod | **连接管理/帧策略/CRC-RTU/通讯Dock/配套算子/形态学梯度·点线距离·圆度** | `communication-production-checklist.md`；loopback + manager 测试 |
| 2026-07-21 | editor-ux | **工具箱单击/双击去重、属性滚动折叠、变量作用域链、容器变量持久化** | `editor-interaction-variable-scopes.md`；`Selt_Gui_editor_variable_ux_tests` |
| 2026-07-22 | measurement-loop | **Blob 选中/排序、几何测量、InspectionResult、变量快照、类型筛选、PLC 模板** | `vision-measurement-closed-loop.md`；`Selt_Gui_measurement_closed_loop_tests` |
| 2026-07-22 | productization | **Bool→PLC 契约、端口对齐、runtime 绑定、连线提示、判定 measurement** | `industrial-productization-session.md` |
| 2026-07-22 | productization | **Blob requireTarget/定位硬失败、Modbus 发布门禁、连线类型推荐提示** | 闭环测试增补 empty/publish 用例 |
| 2026-07-22 | recognition | **QR/条码 OpenCV 后端 + OCR Tesseract 可选 + 识别模板/工具箱能力提示** | `recognition-operators.md`；`Selt_Gui_recognition_tests` |
| 2026-07-22 | editor-ux | **工作区上下/左右布局、端口暴露模型、胶囊端口、ROI 强类型通道** | ADR-0012；`Selt_Gui_port_exposure_tests` |
| 2026-07-22 | platform-stabilize | **端口暴露 Undo、ROI 优先级、RunRecord、子流程 Terminal、插件能力版本** | `operator-interaction-workspace.md`；`Selt_Gui_run_record_tests` |
