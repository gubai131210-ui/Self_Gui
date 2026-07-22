# ADR-0010：工业测量单位、标定、设备与结果输出边界

## 状态

已接受。

## 背景

工程可靠性闭环完成后，需要把“可运行图像流程”提升为可交付的工业检测基础能力：几何测量、物理单位、判定、相机触发和结果导出。这些能力若散落在 UI 参数字符串中，后续扩展和追溯成本会迅速上升。

## 决策

1. **测量领域模型独立于 UI**  
   `MeasurementResult` / `MeasurementDefinition` / `CalibrationModel` / `CalibrationStore` 承载单位、置信度、容差和标定。算法节点输出像素或经执行上下文标定换算后的结果，不直接依赖 QWidget。

2. **标定以运行快照注入**  
   `RunController` 将当前激活标定写入 `RuntimeExecuteOptions` / `ExecutionContext`。运行期间修改标定只影响下一次执行；本次结果仍使用快照标定。

3. **设备插件边界**  
   相机与触发仅通过 `ICameraDevice` / `ITriggerSource` 接入。厂商 SDK 不得进入 `SeltVisionRuntime` 直接依赖。测试使用 `StubCameraDevice` 验证打开、抓帧、软触发、超时和断开。

4. **结果输出可替换且失败隔离**  
   `IResultSink` + `ResultSinkRegistry` 支持 CSV/JSON Lines 等适配器。输出失败只记诊断，不影响图像与测量结果回看。

5. **插件能力元数据**  
   `PluginMetadata.capabilities` 声明 `algorithm-node` / `camera-device` / `result-sink` 等能力，工程可诊断缺失依赖而不破坏可编辑性。

## 测量契约补充（增量）

1. **长度类测量**（圆/线/距/卡尺/拟合几何）经 `finalizeLengthMeasurement`：
   - 有有效标定快照：换算 `width/height/area/center`，写入 `unit` 与 `calibrationId`。
   - 无标定：保持 `unit=px`，清空 `calibrationId`，不伪造物理单位。
2. **角度类测量**经 `finalizeAngularMeasurement`：单位固定为 `deg`，不参与长度标定换算。
3. **失败路径**：`failResult` 强制 `measurement.valid=false` 且 `confidence=0`，不把失败值写成伪造的 0 成功结果。
4. **退化几何**：共线拟合圆、零长度直线等使用诊断码 `degenerate_geometry`。
5. **受限能力**：`subpixelRefine` 当前为透传，诊断码 `capability_limited`，不宣称亚像素精度提升。

## 影响

- 节点目录新增圆/线/距离测量与范围判定；矩形测量改为旋转矩形几何。
- 运行监视器与测量面板展示帧号、设备、标定和判定状态。
- 标定管理通过 UI 入口维护，当前先持久化到应用设置；后续可迁移到工程容器扩展字段。
- 工业 DAG 回归：`circleMeasure → toleranceDecision`、`findContours → fitCircle`。
