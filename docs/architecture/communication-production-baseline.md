# 通讯生产化基线审计（阶段 0）

日期：2026-07-21

## 已有能力

| 模块 | 状态 |
|------|------|
| `communication/codec.*` | HEX/ASCII/十进制、线圈、寄存器字序、浮点/字符串 |
| `communication/clients.*` | Tcp/Serial/Modbus 客户端骨架 + Qt Network/SerialPort 条件编译 |
| `communication/mocktransport.*` | 无硬件 Mock + Modbus 内存表 |
| `communication.*` 节点 | serial/tcp/modbus read-write/encode/decode/numeric/string |
| `DataTypeId::ByteArray` | 已接入 |
| `Selt_Gui_communication_tests` | 基础编解码与 Mock 回归 |

## CMake / Qt Kit 缺口

- Configure 打印：`Network` / `SerialPort` / `SerialBus` 三态。
- 常见现场：`Network=1`，`SerialPort=0`，`SerialBus=0`。
- 无 SerialPort 时真实 COM/RTU 不可用，必须给出 `comm_not_available` 与部署提示，且主程序仍可构建。

## 生产化缺口（本阶段目标）

1. 连接复用与取消：`CommunicationManager`
2. 接收帧策略：固定长度 / 帧尾 / 空闲间隔 / 最大长度
3. Modbus RTU：CRC16、帧边界、异常映射
4. 通讯配置资源 + 监视器诊断
5. 校验/帧处理等配套算子
6. 少量高频视觉算子
7. loopback/异常回归与验收清单

## 兼容约束

- 不改既有 `vision.*` / 已发布 `communication.*` typeId 语义。
- `.selt` 格式不变；新增参数带默认值。
- 不引入 GPL/AGPL 第三方库。
