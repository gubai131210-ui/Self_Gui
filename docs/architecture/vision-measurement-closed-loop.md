# 视觉测量与运行闭环

日期：2026-07-22

## 目标

从「可搭节点」推进到可验证的工业检测闭环：

图像输入 → 预处理 → Blob/区域 → 几何测量 → OK/NG 判定 → 结果/PLC

## Blob / 区域增强

- `selectIndex`：排序后选择主输出目标
- `sortBy`：`area` / `circularity` / `x` / `y`
- `requireTarget`（Blob分析）：默认 false；面积类模板开启，空目标硬失败
- Blob定位：空目标 / 索引越界一律失败
- `RegionStats` 增加圆度、长宽比、矩形度、实心度、外接矩形
- 端口：图像、区域、数量、中心、面积、圆度、选中索引、置信度

## 新增几何 / 判定算子

| 类型 ID | 名称 |
|---------|------|
| `vision.pointPointDistance` | 点到点距离 |
| `vision.perpendicularityMeasure` | 垂直度 |
| `vision.positionDeviation` | 位置偏差 |
| `vision.referencePoint` | 基准点 |
| `vision.variableWrite` | 运行时变量写入 |
| `vision.inspectionPack` | 检测结果打包 |

`InspectionResult` 与 `MeasurementResult.decisionState` 兼容。

## 运行时变量快照

- `VariableScopeSnapshot`：全局/工程/流程拷贝 + `runtimeValues` 写回层
- `FlowRuntime` / `RunController` 注入作用域链到 `BindingResolver`
- `ExecutionContext::variableScopeSnapshot()` 可供算子写回

## 编辑器体验

- 工具箱增加「按端口数据类型」筛选
- 节点展开后显示端口名称；悬停显示方向与类型

## 流程模板

- `blob_area_inspect`：单 Blob 面积检测
- `blob_count_inspect`：多 Blob 数量检测
- `geometry_circle_inspect`：圆位置偏差检测
- `vision_to_plc`：判定 → Modbus 写出（默认 Mock）

## 验收清单（Qt Creator）

- [ ] Configure + Build `Selt_Gui`
- [ ] Build/Run `Selt_Gui_measurement_closed_loop_tests`
- [ ] Blob 分析：修改 `selectIndex`/`sortBy`，主输出变化正确
- [ ] 插入「单Blob面积检测」「视觉判定→PLC写出」模板
- [ ] 工具箱类型筛选（图像/点/测量）可用
- [ ] 流程变量在运行时参与绑定解析
- [ ] 旧 `.selt` 仍可打开运行

中文路径下请自行 Configure / Build / Run；不自动 Git 提交。
