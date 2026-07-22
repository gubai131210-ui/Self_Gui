# 识别算子后端接入

日期：2026-07-22

## 后端

| 能力 | 宏 | 说明 |
|------|----|------|
| 二维码 | `SELT_HAS_OPENCV_QR` | OpenCV `QRCodeDetector`（objdetect） |
| 条码 | `SELT_HAS_OPENCV_BARCODE` | OpenCV `cv::barcode::BarcodeDetector` |
| OCR | `SELT_HAS_TESSERACT` | 可选 Tesseract；未安装时节点返回 `capability_limited` |

Configure 会打印 QR / barcode / Tesseract 状态。

## 节点

- `vision.barcodeDecode`：条码/二维码识别（分类「识别」）
- `vision.ocr`：OCR识别（分类「识别」）

输出：`text` / `confidence` / `found` / `overlay`（条码另有 `symbology`）

## 模板

- `qr_preview`：采集 → 二维码识别 → 预览 / 变量写回
- `ocr_preview`：采集 → OCR → 预览 / 变量写回

## 本机验收（Qt Creator）

1. 重新 Configure，确认输出中有 QR/barcode/Tesseract 状态
2. Build `Selt_Gui` + `Selt_Gui_recognition_tests`
3. 工具箱「识别」分类可见两个算子；悬停可见后端能力提示
4. 插入「二维码识别预览」模板，用含 QR 的图片运行
5. OCR：未装 Tesseract 时应看到明确的能力受限诊断，而不是崩溃
6. 运行结果页应显示结构化端口表（类型/值/状态），诊断区含策略与失败阶段
7. 结果预览可切换：结果图 / 原图 / ROI / 预处理调试图

## 相关文档

- [industrial-operator-foundation.md](industrial-operator-foundation.md) — 下一阶段预处理/卡尺/模板/标定基础设计

Tesseract 可选配置：

```text
Tesseract_ROOT=...
TESSDATA_PREFIX=.../tessdata
```

语言包（如 `chi_sim.traineddata`）不提交到仓库。

中文路径请自行 Configure / Build / Run；不自动 Git 提交。
