# OpenCV MinGW C++ 配置说明

本文记录在 **Qt 6 MinGW 64-bit** 环境下，从官方 Windows 安装包出发，自行编译 **MinGW 版 OpenCV**，并接入本项目（Selt_Gui）的完整流程。

> 适用场景：Qt Kit 为 `Desktop Qt 6.x MinGW 64-bit`，不能直接使用官方预编译的 MSVC（`vc16`）OpenCV。

---

## 一、背景与结论

### 1.1 为什么官方安装包不能直接用

官方下载包（例如 `opencv-4.12.0-windows.exe`）解压后通常包含：

| 内容 | 典型路径 | 说明 |
|------|----------|------|
| 头文件 | `...\opencv\build\include` | 可用 |
| 源码 | `...\opencv\sources` | 可用来自己编译 |
| MSVC 库 | `...\opencv\build\x64\vc16\lib` | 仅 Visual Studio |
| MSVC DLL | `...\opencv\build\x64\vc16\bin` | 仅 Visual Studio |

**没有** MinGW 预编译库（没有 `x64/mingw`、没有 `.a` 等 MinGW 产物）。

因此：

```text
Qt MinGW  +  OpenCV MSVC (vc16)   ❌ 不能链接
Qt MSVC   +  OpenCV MSVC (vc16)   ✅ 可以
Qt MinGW  +  OpenCV MinGW (自编译) ✅ 可以
```

本项目当前 Kit 是 **MinGW**，所以必须用 **同一套 Qt MinGW 工具链** 从源码编译 OpenCV。

### 1.2 本机参考环境（记录用）

| 项目 | 本机示例 |
|------|----------|
| Qt 安装 | `D:\QT` |
| MinGW | `D:\QT\Tools\mingw1310_64`（gcc/g++ 13.1.0） |
| CMake GUI | CMake 4.2.x |
| OpenCV 官方包安装目录 | `D:\Opencv_4.12` 或 `D:\OpenCV\opencv` |
| 源码目录 | `D:\OpenCV\opencv\sources` |
| MinGW 构建目录 | `D:\OpenCV\opencv\opencv-mingw-build` |
| 编译器 | `D:\QT\Tools\mingw1310_64\bin\gcc.exe` / `g++.exe` |
| Make | `D:\QT\Tools\mingw1310_64\bin\mingw32-make.exe` |
| CPU | AMD Ryzen 7 5800H（8 核 16 线程） |
| 内存 | 16 GB |

> 路径因机器而异，下文以实际存在的目录为准。关键原则是：**编译 OpenCV 的 MinGW 必须与 Qt Creator Kit 一致**。

### 1.3 本项目需要的 OpenCV 模块

视觉 Demo 最少需要：

- `core`
- `imgproc`
- `imgcodecs`

其他模块（dnn、gapi、video、python 绑定等）可关，以缩短编译时间。

本项目通过 `cmake/FindOpenCVSetup.cmake` 查找 MinGW 版 OpenCV；成功后定义 `SELT_HAS_OPENCV`。

---

## 二、完整配置流程

### 步骤 0：准备目录与工具

1. 安装/确认 Qt MinGW Kit 可用。
2. 安装 CMake（含 CMake GUI）。
3. 安装官方 OpenCV Windows 包，确认存在源码目录，例如：

```text
D:\OpenCV\opencv\sources
```

4. 新建构建目录（建议全英文路径）：

```text
D:\OpenCV\opencv\opencv-mingw-build
```

5. 新建 ASCII 临时目录（强烈建议）：

```text
D:\temp
```

并在当前 PowerShell 会话中设置：

```powershell
$env:TEMP = "D:\temp"
$env:TMP  = "D:\temp"
```

> 原因：用户名含中文时，MinGW 默认把临时文件写到 `C:\Users\中文用户名\AppData\Local\Temp`，容易导致编译器测试或编译失败。

6. 确认 MinGW 工具存在：

```text
D:\QT\Tools\mingw1310_64\bin\gcc.exe
D:\QT\Tools\mingw1310_64\bin\g++.exe
D:\QT\Tools\mingw1310_64\bin\mingw32-make.exe
```

注意：正确 PATH 应加 **`...\mingw1310_64\bin`**，不要加：

```text
...\mingw1310_64\x86_64-w64-mingw32\bin   ❌（没有 gcc / mingw32-make）
```

---

### 步骤 1：用 CMake GUI 配置 OpenCV

1. 打开 **CMake GUI**。
2. 填写：

| 字段 | 值 |
|------|-----|
| Where is the source code | `D:\OpenCV\opencv\sources` |
| Where to build the binaries | `D:\OpenCV\opencv\opencv-mingw-build` |

3. 点击 **Configure**。
4. 生成器选择：**MinGW Makefiles**。
5. 若提示找不到编译器 / make，先 **Add Entry** 添加：

| Name | Type | Value |
|------|------|-------|
| `CMAKE_MAKE_PROGRAM` | FILEPATH | `D:/QT/Tools/mingw1310_64/bin/mingw32-make.exe` |
| `CMAKE_C_COMPILER` | FILEPATH | `D:/QT/Tools/mingw1310_64/bin/gcc.exe` |
| `CMAKE_CXX_COMPILER` | FILEPATH | `D:/QT/Tools/mingw1310_64/bin/g++.exe` |

路径建议使用正斜杠 `/`。

6. 再次 Configure，直到编译器被识别，例如：

```text
The CXX compiler identification is GNU 13.1.0
The C compiler identification is GNU 13.1.0
...
Configuring done
```

7. 建议设置的选项（可加速、减少无关依赖）：

| 选项 | 建议值 | 说明 |
|------|--------|------|
| `CMAKE_BUILD_TYPE` | `Release` | 先编 Release |
| `CMAKE_INSTALL_PREFIX` | 构建目录或 `...\install` | 安装前缀 |
| `BUILD_SHARED_LIBS` | ON | 动态库 |
| `BUILD_LIST` | `core,imgproc,imgcodecs` | 仅编需要的模块（推荐） |
| `BUILD_TESTS` | OFF | |
| `BUILD_PERF_TESTS` | OFF | |
| `BUILD_EXAMPLES` | OFF | |
| `BUILD_opencv_apps` | OFF | 可选 |
| `BUILD_opencv_python3` | OFF | C++ 项目不需要 |
| `WITH_FFMPEG` | OFF | 可选，减少依赖 |
| `WITH_OPENEXR` | OFF | 可选；OpenEXR 易拖慢且易偶发失败 |

若不改 `BUILD_LIST`，默认会编很多模块（dnn、gapi 等），时间更长。

8. 再点 **Configure**，确认无红色未解决项后，点 **Generate**。

成功标志：

```text
Configuring done
Generating done
```

日志中应能看到类似：

```text
CMake generator:             MinGW Makefiles
CMake build tool:            D:/QT/Tools/mingw1310_64/bin/mingw32-make.exe
C++ Compiler:                D:/QT/Tools/mingw1310_64/bin/g++.exe
```

---

### 步骤 2：编译 OpenCV

在 PowerShell 中（务必先设置 TEMP/TMP）：

```powershell
$env:TEMP = "D:\temp"
$env:TMP  = "D:\temp"
cd D:\OpenCV\opencv\opencv-mingw-build

# 推荐：单线程，稳定优先
D:\QT\Tools\mingw1310_64\bin\mingw32-make.exe -j1
```

#### 关于 `-j1` 与 `-j8`

| 参数 | 含义 | 对本机建议 |
|------|------|------------|
| `-j1` | 一次只编译一个源文件 | **推荐**：稳定 |
| `-j8` | 最多 8 个并行编译任务 | 16GB 内存下易偶发失败 |

说明：

- `-jN` **只影响编译过程的并行度**，不影响编出来的库如何被调用。
- `-j1` / `-j8` 在相同 CMake 配置下，最终库的 API、链接方式、运行时行为应一致。
- 本机实测：`-j8` 容易在 0%~30% 阶段出现无详细信息的 `Error 1`；`-j1` 更稳，失败后重跑通常可继续。

CPU 为 8 核时，理论上可用 `-j8`，但 16GB 内存 + `-O3` 并行时峰值内存很高，稳定性优先请用 `-j1`；若内存充足且想加速，可谨慎尝试 `-j2`。

编译耗时可能较长（数十分钟到数小时，取决于模块数量）。

---

### 步骤 3：安装 OpenCV

编译全部成功后：

```powershell
$env:TEMP = "D:\temp"
$env:TMP  = "D:\temp"
cd D:\OpenCV\opencv\opencv-mingw-build
D:\QT\Tools\mingw1310_64\bin\mingw32-make.exe install
```

安装完成后，确认存在：

```text
...\install\lib\cmake\opencv4\OpenCVConfig.cmake
```

MinGW 安装后的实际布局（本机已验证）：

```text
D:\OpenCV\opencv\opencv-mingw-build\install\x64\mingw\lib\OpenCVConfig.cmake
D:\OpenCV\opencv\opencv-mingw-build\install\OpenCVConfig.cmake
D:\OpenCV\opencv\opencv-mingw-build\install\x64\mingw\bin\libopencv_*.dll
```

> 注意：不是 Linux 常见的 `install\lib\cmake\opencv4\`。

PowerShell 检查示例：

```powershell
Test-Path "D:\OpenCV\opencv\opencv-mingw-build\install\x64\mingw\lib\OpenCVConfig.cmake"
```

返回 `True` 即安装配置文件存在。

---

### 步骤 4：接入 Selt_Gui 项目

1. 打开 Qt Creator，加载本工程。
2. 环境变量或 CMake 中设置：

```text
OpenCV_DIR = D:/OpenCV/opencv/opencv-mingw-build/install/x64/mingw/lib
```

Path 中加入（运行时找 DLL）：

```text
D:\OpenCV\opencv\opencv-mingw-build\install\x64\mingw\bin
```

3. 重新 Configure。
4. 确认日志中出现类似：

```text
OpenCV version: 4.12.0
OpenCV_DIR: ...
```

即 `SELT_HAS_OPENCV` 生效，视觉模块会参与编译。

5. 编译并运行程序，测试菜单：

```text
视觉 → 插入示例流程 → 执行流程
```

6. 运行时若找不到 OpenCV DLL，将 MinGW 版 OpenCV 的 `bin` 目录加入 PATH，或把相关 DLL 复制到可执行文件旁。

> **不要** 把 `OpenCV_DIR` 指到官方包的 `...\opencv\build` 或 `...\x64\vc16`（MSVC）。

本项目默认候选路径见 `cmake/FindOpenCVSetup.cmake`（优先
`D:/OpenCV/opencv/opencv-mingw-build/install/x64/mingw/lib`）。若实际安装路径不同，务必手动设置 `OpenCV_DIR`。

---

## 三、流程简图

```text
官方 opencv-*-windows.exe
        │
        ▼
  sources（源码） + vc16（MSVC，弃用）
        │
        │  CMake GUI + MinGW Makefiles
        │  使用 Qt 同款 mingw1310_64
        ▼
  opencv-mingw-build（生成 Makefile）
        │
        │  mingw32-make -j1
        │  mingw32-make install
        ▼
  MinGW 版 OpenCV（include / lib / bin / OpenCVConfig.cmake）
        │
        │  OpenCV_DIR=.../install/x64/mingw/lib
        ▼
  Selt_Gui（Qt MinGW）启用 SELT_HAS_OPENCV，链接并运行视觉 Demo
```

---

## 四、过程中可能出现的问题及解决方法

### 4.1 官方包装好了，感觉和之前一样

**现象**：安装到 `D:\Opencv_4.12` 等目录后，仍是 `x64\vc16` 结构。

**原因**：官方 Windows 包本身就是 MSVC 预编译 + 源码，不是 MinGW 版。

**解决**：用 `sources` 目录 + Qt MinGW **自行编译**，不要指望直接链接 `vc16` 库。

---

### 4.2 CMake：找不到 MinGW Makefiles / CMAKE_MAKE_PROGRAM

**现象**：

```text
CMake was unable to find a build program corresponding to "MinGW Makefiles".
CMAKE_MAKE_PROGRAM is not set.
CMAKE_CXX_COMPILER not set / CMAKE_C_COMPILER not set
```

**原因**：PATH 未包含正确的 MinGW `bin`，或加错了子目录。

**解决**：

1. PATH 添加：`D:\QT\Tools\mingw1310_64\bin`
2. **不要** 添加：`...\x86_64-w64-mingw32\bin`
3. CMake GUI 中手动设置 `CMAKE_MAKE_PROGRAM`、`CMAKE_C_COMPILER`、`CMAKE_CXX_COMPILER`
4. 修改环境变量后，**完全关闭并重新打开** CMake GUI / 终端
5. 新开 PowerShell 验证：

```powershell
where.exe mingw32-make
where.exe g++
```

应指向 `mingw1310_64\bin`。

---

### 4.3 CMake：编译器 broken / identification unknown

**现象**：

```text
The CXX compiler identification is unknown
Check for working CXX compiler: ...\g++.exe - broken
```

**常见原因**：

1. 临时目录含中文路径
2. 未正确指定编译器
3. Miniconda 等其它 mingw 抢占 PATH

**解决**：

1. 设置 `TEMP`/`TMP` 为 `D:\temp`（纯英文路径）
2. 显式指定 Qt 的 gcc/g++
3. 编译 OpenCV 时尽量避免依赖 Miniconda 的 `mingw-w64\bin`，或用绝对路径指定编译器

---

### 4.4 Configure 成功，但有大量 “Could NOT find …”

**现象**：Python、numpy、BLAS、LAPACK、Java、VTK、AVIF、NASM 等找不到。

**说明**：对纯 C++ 视觉 Demo **通常可忽略**。

关注最终汇总是否仍有 `Configuring done`，以及 `To be built` 是否包含 `core` / `imgproc` / `imgcodecs`。

---

### 4.5 CMake Deprecation Warning（Compatibility with CMake < 3.10）

**现象**：OpenCV 源码 `cmake_minimum_required` 版本偏旧的警告。

**说明**：可忽略，不影响编译。

---

### 4.6 `mingw32-make`：一开始就 Error 1，且无 g++ 报错详情

**现象**：

```text
Building C object ...
mingw32-make[2]: *** [...] Error 1
```

没有具体 `error:` 文本。

**常见原因**：

1. TEMP 仍是中文路径
2. `-j8` 并行过高导致偶发失败/内存压力
3. PATH 混乱

**解决**：

```powershell
$env:TEMP = "D:\temp"
$env:TMP  = "D:\temp"
cd D:\OpenCV\opencv\opencv-mingw-build
D:\QT\Tools\mingw1310_64\bin\mingw32-make.exe -j1
```

务必使用 **完整路径** 调用 `mingw32-make.exe`（仅输入 `mingw32-make.exe` 可能因 PATH 找不到）。

---

### 4.7 `opencv_imgproc` 等已成功，全量编译又在其它目标失败

**现象示例**：

- `opencv_core` / `opencv_imgproc`：`Built target` 成功
- 随后在 `protobuf`、`openexr`（如 `ImfInputFile.cpp`）等处 `Error 1`

**原因**：多为 **偶发编译失败**（内存、进程异常、中间状态），不是源码必然错误。可单独编译失败文件或目标时发现可以成功。

**解决**：

1. 继续用 `-j1` 重跑（会跳过已完成目标）：

```powershell
D:\QT\Tools\mingw1310_64\bin\mingw32-make.exe -j1
```

2. 或只编失败目标，例如：

```powershell
D:\QT\Tools\mingw1310_64\bin\mingw32-make.exe -j1 IlmImf
D:\QT\Tools\mingw1310_64\bin\mingw32-make.exe -j1 opencv_imgproc
```

3. 若 OpenEXR 反复出问题：在 CMake 中设 `WITH_OPENEXR=OFF`，重新 Configure/Generate 后再编译（Demo 读 jpg/png 通常不需要 OpenEXR）。

4. 编译期间尽量关闭其它占内存程序。

---

### 4.8 PowerShell：无法识别 `mingw32-make.exe`

**现象**：

```text
无法将“mingw32-make.exe”项识别为 cmdlet...
```

**原因**：当前会话 PATH 未包含 MinGW `bin`。

**解决**：始终使用完整路径：

```powershell
D:\QT\Tools\mingw1310_64\bin\mingw32-make.exe -j1
```

或临时：

```powershell
$env:Path = "D:\QT\Tools\mingw1310_64\bin;" + $env:Path
```

---

### 4.9 误把 `OpenCV_DIR` 指到 MSVC 目录

**现象**：CMake 找到 OpenCV 但链接失败，或本项目脚本警告并忽略该路径。

**解决**：指向 MinGW 安装后的：

```text
.../install/x64/mingw/lib
```

不要指向：

```text
...\opencv\build
...\opencv\build\x64\vc16
```

---

### 4.10 程序能编过，运行时缺 DLL

**现象**：启动时提示缺少 `libopencv_*.dll` 或相关依赖 DLL。

**解决**：将 OpenCV 的 `bin`（及必要时 MinGW 运行时 DLL）加入 PATH，或复制到 exe 同目录。

---

### 4.11 想加快编译又怕不稳定

**建议策略**：

1. 首选：`BUILD_LIST=core,imgproc,imgcodecs` + 关闭测试/示例 + `WITH_OPENEXR=OFF`
2. 编译：先 `-j1` 跑通
3. 通了之后若机器空闲，可尝试 `-j2`，观察任务管理器内存；接近占满则退回 `-j1`
4. 不要为“调用库更快”而用 `-j8`——**`-j` 不影响最终库运行性能**

---

## 五、检查清单（完成后自测）

- [ ] CMake 使用的是 **MinGW Makefiles** + Qt 自带 `mingw1310_64`
- [ ] `TEMP`/`TMP` 指向英文路径（如 `D:\temp`）
- [ ] `mingw32-make -j1` 完整编过并 `install`
- [ ] 存在 `OpenCVConfig.cmake`（MinGW 安装树下）
- [ ] Selt_Gui 的 `OpenCV_DIR` 指向该 cmake 目录
- [ ] Configure 日志显示找到 OpenCV，`SELT_HAS_OPENCV` 启用
- [ ] 程序可执行视觉示例流程

---

## 六、相关项目文件

| 文件 | 作用 |
|------|------|
| `cmake/FindOpenCVSetup.cmake` | 查找 MinGW OpenCV，拒绝 vc16，设置 `SELT_HAS_OPENCV` |
| `CMakeLists.txt` | 主工程；按 `SELT_HAS_OPENCV` 条件编译视觉模块 |
| `vision/` | 视觉算法、流水线、节点注册 |
| `tests/test_vision.cpp` | 算法层单测 |
| `tests/test_vision_integration.cpp` | 注册表/执行/ROI 集成单测 |
| `docs/视觉模块化编辑器设计说明.md` | 视觉流程编辑器架构与扩展规范 |

---

## 七、版本记录

| 日期 | 说明 |
|------|------|
| 2026-07 | 初版：记录 Qt MinGW + OpenCV 4.12 自编译接入流程，及 Configure/编译常见问题 |
| 2026-07 | 补充视觉模块化编辑器设计文档与集成测试入口 |

---

*文档维护：随本机路径或 Qt/OpenCV 版本变化时，请同步更新「本机参考环境」与安装路径示例。*
