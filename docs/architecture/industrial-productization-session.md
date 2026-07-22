# 工业视觉产品化优化进度（自动会话）

日期：2026-07-22（用户离开期间自动推进）

## 本轮重点（P0 闭环修复）

### 数据契约
- `Bool → Real/Int` 兼容与 `convertTo`，支持 OK/NG 写入数值/线圈端口
- `Modbus写` 增加 `okIn` 布尔门控输入；模板 `vision_to_plc` 改为 `ok → okIn` + coils
- `inspectionPack` 输出改为唯一 ID：`pass` / `resultValue` / `state` / `message` / `measurement`
- `variableWrite` 输出对齐：`out` / `key` / `ok`
- `blobLocate` 补齐 `confidence` / `candidateCount`
- `blobAnalyze` 补齐 `candidateCount`
- `toleranceDecision` / `rangeDecision` 输出 `measurement` 并写入 `decisionState`

### 运行时变量
- `VariableScopeChain` 增加 `runtimeValues` 指针
- `BindingResolver` 可解析运行时键，诊断码：`variable_undefined` / `variable_type_mismatch` / `variable_shadowed`
- 运行结束将 runtime 写回键值写入运行日志

### Blob / 定位一致性
- `blobAnalyze` 新增参数 `requireTarget`（默认 false）：数量检测软成功；面积检测模板开启硬失败
- 无主目标时仍输出稳定零值（`area/center/circularity/confidence`）
- `blobLocate`：空目标或选中索引越界 → `Failed` + `noTarget`

### 发布门禁 / 通讯安全
- `validateForPublish` 对 Modbus 写：
  - `allowWrite=false` → Info `modbus_write_protected`
  - `allowWrite=true && useMock=false` → Warning `modbus_real_write`
  - Mock 写出 → Info `modbus_mock_write`
- 模板默认仍 `allowWrite=false`、`useMock=true`

### 编辑器
- 连线拖拽时状态栏显示「可连接」或具体类型错误
- 开始连线时提示端口类型，可引导工具箱按类型筛选
- 工具箱类型筛选扩展 ROI/轮廓/叠加/字节等

### 测试
- `Selt_Gui_measurement_closed_loop_tests` 覆盖：
  - `portContractsAndBoolToPlc`
  - `runtimeValuesResolveInBinding`
  - `blobEmptyTargetBehavior`
  - `publishWarnsUnsafeModbusWrite`

## 验收（Qt Creator）

请在 Qt Creator 中自行 Configure / Build / Run（中文用户路径下避免强依赖命令行构建）：

- [ ] Configure + Build `Selt_Gui`
- [ ] Run `Selt_Gui_measurement_closed_loop_tests`（期望全部通过）
- [ ] 插入「视觉判定→PLC写出」模板，确认 `ok→okIn` 可连线
- [ ] 插入「单Blob面积检测」：属性页可见 `requireTarget=true`
- [ ] 空图跑 Blob 定位应失败；Blob 分析（不要求目标）应成功且 count=0
- [ ] 发布校验：Modbus `allowWrite=true` + `useMock=false` 出现真实写出警告
- [ ] 双击属性页：判定节点有 measurement 输出
- [ ] 运行后日志可见「运行时变量写回」段落（若使用变量写入）
- [ ] 旧 `.selt` 仍可打开

## 安全默认

- `allowWrite=false`、`useMock=true` 保持不变；模板不会自动写真实 PLC

本会话**不**自动 Git 提交。
