# 通讯算子平台说明

日期：2026-07-21  
定位：与视觉算子同级的 DAG 节点能力（`communication.*`），IO 适配层独立于 `FlowRuntime`。

## 能力范围（首期客户端）

| 节点 | typeId | 说明 |
|------|--------|------|
| COM请求 | `communication.serial.request` | 串口发送/接收，HEX/ASCII/UTF-8 |
| TCP请求 | `communication.tcp.request` | TCP 客户端发送/接收 |
| Modbus读 | `communication.modbus.read` | 线圈/离散输入/保持/输入寄存器 |
| Modbus写 | `communication.modbus.write` | 线圈/保持寄存器；**默认写保护** |
| 编码/解码 | `communication.encode` / `decode` | 多寄存器拼接、浮点、ASCII、HEX、十进制 ASCII |
| 数值/字符串 | `communication.numeric` / `string` | 运算、拼接、格式化、进制转换 |

## 技术栈与许可证

- **优先**：Qt 6 官方 `Network` / `SerialPort`（可选 `SerialBus`）。
- Modbus 应用层帧按 [Modbus Application Protocol 1.1b](https://www.modbus.org/file/secure/modbusprotocolspecification.pdf) 自研实现（TCP ADU + 常用功能码），不引入 GPL/AGPL 第三方库。
- 无硬件路径：节点默认 `useMock=true`，使用进程内 `MockTransport` / `MockModbusMap`。

## Modbus 语义

- **地址**：协议 **零基**；若设备手册写 40001，节点地址填 `0`（并阅读参数提示）。
- **线圈**：按位打包，低地址对应字节 LSB。
- **保持寄存器**：每寄存器 2 字节，高字节在前。
- **多寄存器**：`wordOrder` 显式选择 ABCD/CDAB/BADC/DCBA；字符串指定寄存器数量与填充。
- **写保护**：`allowWrite` 默认 `false`，未开启则返回 `comm_write_protected`。

## 依赖与部署

CMake 探测：

- `SELT_HAS_QT_NETWORK`
- `SELT_HAS_QT_SERIALPORT`
- `SELT_HAS_QT_SERIALBUS`

运行目录除现有 Qt/OpenCV 外，现场 COM/Modbus RTU 还需：

- `Qt6Network.dll`
- `Qt6SerialPort.dll`
- （可选）`Qt6SerialBus.dll`

详见 [docs/deployment/README.md](../deployment/README.md)。

## 生产化补充（2026-07-21）

- `CommunicationManager`：配置复用、Mock/真实打开、取消、简单重连。
- 接收帧策略：`fixed_length` / `idle_gap` / `terminator` / `max_bytes`。
- Modbus RTU：CRC16 追加与校验；异常/CRC 失败返回稳定诊断码。
- UI：视图 →「通讯配置」Dock；监视器显示能力与部署提示。
- 新数据算子：`checksum` / `frame.wrap` / `clampScale` / `registerTable`。
- 新视觉算子：`morphGradient` / `pointLineDistance` / `circularityMeasure`。
- 验收：`docs/architecture/communication-production-checklist.md`。
