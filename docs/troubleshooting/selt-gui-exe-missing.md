# Selt_Gui.exe「找不到可执行文件」说明

日期：2026-07-21  
适用：Qt Creator + MinGW Debug（`Desktop_Qt_6_11_1_MinGW_64_bit_Debug`）

## 现象

Qt Creator 输出类似：

```text
Starting ...\build\Desktop_Qt_6_11_1_MinGW_64_bit_Debug\bin\Selt_Gui.exe...
The program ".../bin/Selt_Gui.exe" does not exist or is not executable.
```

## 结论（不是路径配错）

- CMake 规定主程序输出到 **`build/.../bin/Selt_Gui.exe`**（见根目录 `CMakeLists.txt` 的 `CMAKE_RUNTIME_OUTPUT_DIRECTORY`）。
- 报错时通常是：**`bin` 目录里没有这份主程序**（或链接失败后被删掉且未重新生成）。
- 构建目录**根下**有时会残留一份**过期的** `Selt_Gui.exe`（旧配置产物），**不能**当作当前可运行程序；Qt Creator 只认 `bin/`。

## 常见触发原因

1. **重新构建（Rebuild）先删 exe，再链接**  
   若主目标链接失败、中断，或只编了测试目标，`bin/Selt_Gui.exe` 就会空缺。
2. **运行/调试目标选错**  
   选中了 `Selt_Gui_*_tests` 等测试目标，主程序未更新甚至被清掉。
3. **程序仍在运行时点 Rebuild**  
   旧 exe 被删或被占用，新链接失败。
4. **只点了运行、未先构建成功**  
   清理或失败后直接 Run/Debug。

典型时间线示例：

| 时间 | 状态 |
|------|------|
| 正常 Run | `bin/Selt_Gui.exe` 存在并可启动 |
| 随后 Rebuild / 编测试 / Debug 触发构建 | `bin` 里可能只剩 `*_tests.exe` |
| 再点 Run | 报 does not exist or is not executable |

## 如何自查

在资源管理器或 PowerShell 中确认：

```text
D:\QT_CppPrograms\Selt_Gui\build\Desktop_Qt_6_11_1_MinGW_64_bit_Debug\bin\Selt_Gui.exe
```

- **存在且体积合理（数十 MB）**：路径正确，可再 Run。
- **不存在**：先构建主目标，不要急着改 Run 配置。
- 若根目录也有 `...\Debug\Selt_Gui.exe`：多为**旧产物**，以 `bin\` 为准。

## 恢复方法

### 推荐：Qt Creator

1. 左下角运行目标选中 **`Selt_Gui`**（不是某个 `*_tests`）。
2. 菜单：**构建 → 构建项目**（优先用 Build，避免无谓 Rebuild）。
3. 「编译输出」确认 **构建成功**、无红色 `error` / `ld returned 1`。
4. 再确认 `bin\Selt_Gui.exe` 存在后，点运行/调试。

### 备用：仅编译主目标（工程路径无中文时可用）

在构建目录执行（不要 `clean` 全量）：

```powershell
$env:Path = "D:\QT\Tools\mingw1310_64\bin;D:\QT\Tools\CMake_64\bin;" + $env:Path
Set-Location "D:\QT_CppPrograms\Selt_Gui\build\Desktop_Qt_6_11_1_MinGW_64_bit_Debug"
mingw32-make -j4 Selt_Gui
```

成功后应出现：`bin\Selt_Gui.exe`。

> 说明：此前多次「修好」就是让主目标重新链接出 `bin\Selt_Gui.exe`。  
> 工程在 `D:\QT_CppPrograms\...`（无中文）时用命令行编主目标一般安全；若路径含中文用户目录等风险，优先用 Qt Creator Build。

## 预防

1. 日常用 **Build**，少用 **Rebuild**；程序关掉后再构建。
2. 始终以 **`Selt_Gui`** 为主运行/调试目标。
3. 构建失败时先看编译输出，**不要**在失败后反复点绿三角。
4. 需要跑测试时，单独构建对应 `*_tests` 目标，避免误清主程序后未再编 `Selt_Gui`。

## 与输出目录相关的配置

`CMakeLists.txt` 中：

```cmake
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)
```

因此：

- 正确：`build/..._Debug/bin/Selt_Gui.exe`
- 不正确依赖：`build/..._Debug/Selt_Gui.exe`（根目录旧文件）

## 相关文档

- 布局与主题手工验收：`docs/architecture/ui-visionmaster-checklist.md`
- 编辑器路线验收：`docs/architecture/editor-roadmap-checklist.md`
- UI 约定：`docs/architecture/ui-guidelines.md`
