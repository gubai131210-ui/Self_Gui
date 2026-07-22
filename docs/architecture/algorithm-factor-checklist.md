# 工业算法因子验收清单（手工）

在 Qt Creator 中构建后，按下列项人工核对。中文路径环境不以临时命令行输出替代最终验收。

## 发布门禁（必须）

- [ ] `scripts/verify_vision_icons.py` 通过（图标/资源完整）
- [ ] 全部内置算子在 `NodeExecutorRegistry` 有执行器（`Selt_Gui_vision_integration_tests::allBuiltinNodesHaveExecutor`）
- [ ] 算法单元/因子测试：`Selt_Gui_industrial_measurement_tests`、`Selt_Gui_algorithm_factor_tests`
- [ ] DAG 集成：`Selt_Gui_vision_integration_tests`（含标定测量→判定）
- [ ] 无伪造物理单位：无标定时测量 `unit=px` 且无 `calibrationId`
- [ ] 性能基线记录：`Selt_Gui_algorithm_benchmark_tests`（Debug 仅记录，不作产线承诺）

## 计量与标定

- [ ] 洁净合成图：圆直径误差 ≤ 1 px；卡尺双边距 ≤ 1.5 px；重复标准差 ≤ 0.2 px
- [ ] 有标定快照：直径按 `unitPerPixel` 换算为 mm，结果含 `unit`/`calibrationId`
- [ ] 修改标定后仅影响下一次运行（快照语义）
- [ ] 标定原点/旋转：点坐标与长度变换正确

## ROI / 噪声 / 尺度

- [ ] MaskOutside ROI 下圆心与全图一致（误差 ≤ 1.5 px）
- [ ] Crop ROI 诊断明确；越界 ROI 失败码可辨
- [ ] 洁净图必过；中噪记录误差上界；高噪允许失败且不得输出随机伪有效值
- [ ] 64/128/512 尺度扫描目标仍可检测

## 算法增强

- [ ] 卡尺：极性/步长/平滑可配；双边缘置信度输出
- [ ] 拟合：RANSAC 模式抗离群；共线/点数不足拒绝
- [ ] 阈值：Manual/Otsu/Adaptive；失败消息含实际策略
- [ ] 亚像素：接图像时真实细化；无图像时透传 + `capability_limited`
- [ ] Hough/Blob/多模板：`candidateCount`/`selectedIndex`/`confidence` 可读

## 诊断码稳定性

- [ ] `image_empty` / `invalid_roi` / `invalid_parameter`
- [ ] `measure_failed` / `match_failed` / `no_target` / `degenerate_geometry`
- [ ] `capability_limited` / `low_confidence`
