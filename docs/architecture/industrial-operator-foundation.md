# 工业视觉算子基础设计（下一阶段）

日期：2026-07-22

## 目标

在识别系统稳定后，建立可复用的工业视觉算法基础，避免每个算子各自拼装孤立的 OpenCV 调用。

## 优先顺序

1. **通用预处理**
   - 灰度、Gamma、对比度、CLAHE、去噪、锐化、形态学
   - 统一输入/输出：`Image` + 可选 `ROI` + `debug` 预处理图
   - 参数分组：基本 / 高级 / 调试；与识别增强策略复用同一候选构建器

2. **工业边缘 / 卡尺**
   - 边缘极性、边缘强度、多点拟合、亚像素
   - 输出：距离、边缘点列、置信度、判定、overlay
   - 与现有 `CaliperAlgorithm` 对齐，并沉淀 `EdgeProbe` 公共组件

3. **形状 / 模板定位**
   - 尺度、旋转、ROI、评分、异常匹配
   - 教示模板与运行期匹配共用 `TemplateTeachService` / `TemplateMatch*` 契约
   - 结果端口：位姿、分数、found、overlay

4. **标定**
   - 像素坐标 → 实际坐标
   - 标定状态、单位传播、校准 ID 进入 `MeasurementResult`
   - UI/结果页统一展示单位与标定缺失警告

5. **识别与测量统一的数据质量模型**
   - 复用本阶段已落地的契约：
     - `RecognitionResult`（found / confidence / corners / strategy / failureStage / diagnosticCode）
     - `PortValueRecord`（类型、摘要、详情、状态）
     - `ModuleRunResult.outputPortRecords`
   - 后续测量算子也写入同一结构化端口表，避免 debug 字符串反推

## 建议目录结构

```text
vision/algorithms/
  preprocess/        # 可组合预处理
  metrology/         # 卡尺、拟合、亚像素
  locate/            # 模板/形状定位
  recognition/       # 已有识别（可逐步迁入）
vision/algorithms/common/
  candidatepipeline.h  # 多策略候选（识别已用，预处理复用）
  geometryquality.h    # 几何质量检查 / 去重
```

## 设计模式

- **Strategy**：预处理 / 增强候选策略可插拔
- **Facade**：节点 executor 只做参数解析与端口填充
- **Adapter**：OpenCV 后端差异（QR/barcode/Tesseract）统一诊断码
- **DTO**：`PortValueRecord` / `RecognitionResult` / `MeasurementResult` 跨算法、运行时、UI

## 验收门槛（下一迭代）

- 新增预处理节点不复制识别里的 CLAHE/锐化代码 ✅（已提供独立 Gamma/CLAHE/锐化算子）
- 卡尺结果进入结构化端口表，结果页无需解析 `Real(...)` ✅（edgeStrength/unit/calibrationId）
- 标定缺失时诊断码稳定，不静默用像素当毫米 ✅（`calibration_missing`）

详见 [industrial-validation.md](industrial-validation.md)。

中文路径请在 Qt Creator 中自行构建验证；本文档仅作设计基线，不自动提交。
