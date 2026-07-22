# 工业算子扩展与浅色主题 — 验收说明

日期：2026-07-22

## 已完成

### UI 浅色工业主题
- `UiStyle` 支持 Dark/Light 双色板；默认浅色
- 折叠框标题使用清淡天蓝 `#dcecfa` / 悬停 `#c6e0f5`
- 交互强调色改为天蓝 `#4a90d9`（浅色）；深色仍保留橙色兼容
- QSS：`resources/theme/theme_light.qss`（qrc 实际加载路径）
- ThemeController 应用主题时同步 `UiStyle::setActivePalette`

### 通用预处理算子
新增节点：
- `vision.gammaCorrect` — Gamma 校正
- `vision.contrastBrightness` — 对比度/亮度
- `vision.claheEnhance` — CLAHE
- `vision.sharpen` — 锐化

（形态学节点本就存在，未破坏旧契约）

### 卡尺 / 模板 / 标定
- 卡尺输出增加 `edgeStrength` / `unit` / `calibrationId` / `point2`；低置信度与未标定诊断
- 多模板匹配支持尺度/角度搜索参数与 `scale` / `angle` / `found` 输出
- 新增诊断码 `calibration_missing`；`finalizeLengthMeasurement` 可回填该码

## 请你本地验证（Qt Creator）

1. Configure + Build `Selt_Gui`
2. 可选：Build/Run `Selt_Gui_ui_tests`、`Selt_Gui_industrial_measurement_tests`
3. 启动后确认默认浅色；折叠框为天蓝色标题
4. 工具箱「预处理」可见 Gamma / 对比度亮度 / CLAHE / 锐化
5. 卡尺未标定时结果单位为 `px`，诊断可出现 `calibration_missing`
6. 多模板匹配高级参数可设置尺度/角度范围

可执行文件：

`build/Desktop_Qt_6_11_1_MinGW_64_bit_Debug/bin/Selt_Gui.exe`

中文路径请自行构建运行；本说明不自动 Git 提交。
