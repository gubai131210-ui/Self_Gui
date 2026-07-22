# ADR-0012：画布端口暴露与执行契约分离

- 状态：Accepted
- 日期：2026-07-22

## 背景

多输出算子（Blob、测量、识别等）若在画布上全量显示端口，会形成密集圆点，难以辨认与连线。需要 VisionPro 风格的“选择暴露”，同时不能破坏旧 `.selt`、插件与运行时契约。

## 决策

1. **`ModuleDescriptor.ports` 是完整能力契约**（唯一事实来源）。执行、校验、参数绑定始终按完整契约工作。
2. **`NodeModel.exposedPortIds` 仅控制画布可见性**。`portExposureCustomized=false` 时使用描述符默认暴露集；已连接端口强制可见，禁止静默断线。
3. **内置算子**通过 `applyBuiltinPortExposureHints` 设置 `hasExposureHints=true`：主图像链路默认暴露，结果/调试端口默认隐藏。
4. **插件未声明 hints**（`hasExposureHints=false`）时保持全暴露，兼容旧插件。
5. **可选 ROI 输入端口**可与参数面板 ROI 并存：连线优先写入参数；未连接时仍用本地 ROI。

## 后果

- 画布更干净；属性面板「输入与输出」负责勾选暴露。
- 升级旧工程时重建端口契约并同步暴露，不丢已有连线引用的端口。
- 运行监视 / ResultSink / 变量写回不受画布隐藏影响。
