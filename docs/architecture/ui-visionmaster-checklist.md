# VisionMaster 风格 UI 验收清单（手工）

在 Qt Creator 中 Configure / Build 后验证。中文路径场景请使用 Qt Creator，避免命令行损坏文件。

## Qt Creator 步骤（中文路径）

1. 打开工程：`CMakeLists.txt`
2. Kit：`Desktop Qt 6.x MinGW 64-bit`
3. 左下角 **Configure**（或 Build → Run CMake）
4. 构建目标：`Selt_Gui`（Debug）
5. 运行：`Selt_Gui.exe`（通常位于 `build/.../bin/`）
6. 可选回归：在 Qt Creator 的 Tests 面板或 CTest 中运行
   - `Selt_Gui_ui_tests`
   - `Selt_Gui_vision_tests`
   - `Selt_Gui_builtin_executor_tests`
   - `Selt_Gui_industrial_measurement_tests`
   - `Selt_Gui_algorithm_factor_tests`
   - `Selt_Gui_algorithm_benchmark_tests`
   - `Selt_Gui_visionmaster_experience_tests`
   - `Selt_Gui_vision_integration_tests`
   - `Selt_Gui_measurement_domain_tests`

> 算法因子手工清单：`docs/architecture/algorithm-factor-checklist.md`

> 说明：本机若目录含中文，请勿在外部 PowerShell 中强行覆盖构建产物；以 Qt Creator 完整 Configure → Build → Run 为准。

## 主题
- [ ] 视图 → 主题 → 深色：炭黑面板 + 橙色选中/强调 `#ff8c00`（默认工业模式）
- [ ] 浅色与系统主题仍可切换，重启后主题持久化
- [ ] 菜单、工具栏、Tab、表格、SpinBox、QToolBox、QGroupBox 无低对比度文字
- [ ] 状态栏运行状态：空闲/运行中(蓝)/成功(绿)/失败(红) 颜色可区分

## 布局（layoutVersion=5）
- [ ] 左侧「工具箱」默认可见，最小宽度不至于挤死中央画布
- [ ] 右侧「图像与结果」默认可见（上图下表）
- [ ] 「检查器」默认隐藏，可通过视图菜单打开
- [ ] 旧布局版本（&lt;5）或损坏状态会回退默认 Dock
- [ ] 「视图 → 恢复默认布局」后：左工具箱、右图像结果、中央画布比例可用
- [ ] 兼容底栏 `VisionResultDock` 保持隐藏，不抢占布局
- [ ] `SELT_HAS_OPENCV=0` 时无视觉 Dock，编辑器仍可启动

## 工具箱
- [ ] 图标模式与列表模式 MIME 均为 `application/x-selt-node-type`
- [ ] 图标模式可拖拽到画布创建节点；双击也可添加
- [ ] 分类按流程阶段排序：输入 → 预处理 → 区域 → 定位 → 测量 → 判定 → 逻辑 → 输出
- [ ] 空搜索显示全部分类；有搜索只显示匹配；无匹配显示空态提示
- [ ] 窄窗口自动切列表；变宽后若偏好图标模式会自动恢复
- [ ] 目录数据来自 `VisionNodeRegistry`，无 UI 硬编码清单

## 流程模板
- [ ] 执行 → 插入流程模板：基础测量 / 定位判定 / 卡尺判定 / 完整测量链路
- [ ] 模板节点已正交连线；可继续编辑；不伪造物理单位
- [ ] 「自动布局」按 DAG 层级从左到右排列

## 属性面板
- [ ] 参数按分组显示（阈值/采样/ROI/拟合等）
- [ ] 绑定态/常量态与 tooltip 可见
- [ ] 「复位默认参数」可撤销恢复

## 画布
- [ ] 深色网格 + 橙色主网格/原点方向提示
- [ ] 橙色选中节点、灰色默认连线、选中/关联连线高亮橙色
- [ ] 运行状态徽章颜色正确（Run/OK/ERR）
- [ ] 拖放节点与工具箱 MIME 一致

## 图像区
- [ ] 适应 / 1:1 / +/- / 棋盘格 / 清除 ROI / Overlay 过滤可用
- [ ] 无图时 HUD 为空态（X/Y/RGB 为 `-`），工具栏仍可操作
- [ ] 鼠标离开图像或在图像外不显示伪造像素
- [ ] 灰度图显示 Gray，同时保留 RGB 参考
- [ ] 窄 Dock 下工具条可横向滚动，按钮不被严重截断
- [ ] ROI 只通过信号回写属性，不直接改工程模型；Overlay 过滤不改 `VisionContext`

## 结果与帮助
- [ ] 结果清单列：序号、节点、类型、数值、单位、判定、置信度、Data Available（只读）
- [ ] 无效测量不显示伪造 `0`；失败节点显示为 `-` / fail
- [ ] 重复运行完整刷新；清除结果同步清空表与运行监视
- [ ] 运行监视显示节点状态、耗时、失败类型、节点 ID、诊断日志
- [ ] 帮助页随选中节点读取 `ModuleDescriptor`；取消选择恢复空态
- [ ] 右侧窄尺寸下结果 Tab 仍可切换（滚动/省略）

## 流程栏 / 工具栏 / 状态栏
- [ ] 当前流程 Tab 橙色强调；过窄或流程过多回退 Combo
- [ ] Tab 与 Combo 同一 `activeFlowChosen` 状态源，无双状态分叉
- [ ] 新建/重命名/删除流程仍走既有信号
- [ ] 工具栏运行/连续运行/停止共用 QAction；连续运行中 Loop 为 checked
- [ ] 状态栏显示运行状态、坐标、缩放

## 工业测量链（本轮增量）
- [ ] 圆测量 → 容差判定：有效直径、单位 `px`（无标定时）、判定 ok/ng
- [ ] 轮廓提取 → 拟合圆：`measurement.valid`，直径合理
- [ ] 激活标定后：距离/圆/线等长度单位变为物理单位，并带 `calibrationId`
- [ ] 取消标定后：结果回退为像素，不伪造 mm
- [ ] 卡尺空图/无效参数失败时结果表不显示伪造 0
- [ ] 共线点拟合圆失败；平行距零长度直线失败
- [ ] `subpixelRefine` 成功但模式为 passthrough，不宣称精度提升
- [ ] 旧 `.selt`、Undo/Redo、F5、连续运行、停止、ROI、结果写出不受影响

## 功能回归（不得破坏）
- [ ] 打开旧 `.selt`、保存、Undo/Redo
- [ ] F5 运行 / 连续运行 / 停止
- [ ] 模板匹配、测量、结果写出
- [ ] 1024×640 / 1366×768 / 1920×1080 窗口可用性
- [ ] 深色、浅色、系统主题和布局持久化均可恢复
