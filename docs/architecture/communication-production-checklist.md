# 通讯生产化验收清单

日期：2026-07-21

## 自动测试（Qt Creator）

- [ ] Configure 日志出现 `Qt Network=... SerialPort=... SerialBus=...`
- [ ] Build `Selt_Gui`
- [ ] Build/Run `Selt_Gui_communication_tests`（含 CRC、Manager 复用、TCP loopback、校验/帧封装、新视觉算子注册）
- [ ] Build/Run `Selt_Gui_vision_integration_tests`（`allBuiltinNodesHaveExecutor`）

## 人工验收

- [ ] 视图 → 通讯配置：查看能力摘要与部署提示
- [ ] 保存 TCP/COM/Modbus 配置，Mock 测试连接成功
- [ ] 插入 `COM请求`/`TCP请求`/`Modbus读`/`Modbus写`（默认 Mock）
- [ ] 节点填写相同 `connectionId` 时连接可复用（不重复打开）
- [ ] 收帧策略 `frameMode`：fixed_length / idle_gap / terminator / max_bytes
- [ ] Modbus 写在 `allowWrite=false` 时失败（`comm_write_protected`）
- [ ] 运行监视器可见通讯诊断码/`comm_*` 建议与部署提示
- [ ] 停止运行后通讯取消标记生效（不卡死）；再次运行前取消标记被清除
- [ ] 打开旧 `.selt`，视觉节点仍可运行
- [ ] 新算子可见：校验和、帧封装、限幅缩放、寄存器表、形态学梯度、点线距离、圆度测量

## 现场（可选）

- [ ] 安装 Qt SerialPort 后真实 COM 打开/失败诊断正确
- [ ] Modbus TCP 连真实/模拟从站
- [ ] Modbus RTU CRC 错误可诊断（当前 Kit 无 SerialPort 时自研 CRC 路径仍可用 Mock）

## 相关文档

- `docs/architecture/communication-operators.md`
- `docs/architecture/communication-production-baseline.md`
- `docs/deployment/README.md`
