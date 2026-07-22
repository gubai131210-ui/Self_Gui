# ADR-0011：算子平台化与内置 OpenCV 算子扩展边界

## 状态

已接受。

## 背景

工业视觉流程需要大量可复用算子（预处理、区域、定位、测量、逻辑）。若继续把执行器堆积在单一 `builtinexecutors.cpp`，ROI/诊断/参数读取各自为政，后续批量扩展和测试成本会失控。

## 决策

1. **保持 Registry → Executor → Algorithm 三层**  
   `VisionNodeRegistry` 声明描述符；分类 Executor 适配 `ExecutionRequest`；纯算法只依赖 `VisionImage`/`DataValue` 与 OpenCV，不依赖 QWidget。

2. **统一 ROI / 诊断 / OpenCV 边界**  
   - `RoiApplier`：兼容旧 `RoiRect` 与 `ExtendedRoi`，支持 MaskOutside/Crop，无效 ROI 返回 `invalid_roi`。  
   - `DiagnosticCodes`：稳定诊断码（`image_empty`、`invalid_parameter`、`measure_failed`、`degenerate_geometry`、`capability_limited` 等）。  
   - `runOpenCv`：算法边界捕获 `cv::Exception`，由执行器映射诊断码；`FlowRuntime` 保留最终兜底。
   - 测量 helper：`requireImageWithRoi` / `finalizeLengthMeasurement` / `finalizeAngularMeasurement` 统一 ROI、标定与失败语义。

3. **执行器按分类拆分**  
   `vision/nodes/executors/{preprocess,region,locate,measure,logicoutput}executors.cpp`，由 `registerBuiltInOpenCvExecutors()` 统一注册。常用稳定算子内置；厂商 SDK/实验算法走插件。

4. **参数读取优先 typedParameters**  
   新算子通过 helper 读取 `typedParameters`，`parameters` 仅作旧工程兼容视图。

5. **门禁**  
   `allBuiltinNodesHaveExecutor` 每批必过；算法测试覆盖成功/空图/非法参数；集成测试至少一条 3–5 节点 DAG。

6. **插件化评估（后续 O6）**  
   稳定后可将内置 OpenCV 算子抽成 `SeltOpenCvNodes` 插件目标，但不改变当前描述符 typeId。

## 影响

- 节点目录新增预处理/区域/定位/测量增强/逻辑/输出分类。  
- `Contour` / `Region` / `Table` / 几何端口可在 DAG 中绑定。  
- 测试新增 `test_roi_applier`、`test_builtin_executors`，并扩展 vision / integration。
