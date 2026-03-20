# vsgqtviewer — 仿真态势感知平台

> 基于 [VulkanSceneGraph (vsg)](https://github.com/vsg-dev/VulkanSceneGraph) + [Rocky](https://github.com/pelicanmapping/rocky) + Qt6 构建的三维地图态势感知系统。  
> 支持 ACMI 格式飞行数据回放、实时 UDP 遥测接收、3D 模型态势显示、ImGui 浮空标签渲染。

---

## 一、仓库内已包含的资源（无需额外下载）

克隆仓库后 `assets/` 目录中已内置所有运行时资源，**构建时会自动复制到 bin/**：

```
assets/
├── models/          ← 3D 飞机模型（.glb）
│   ├── F-16A.glb
│   ├── su-27.glb
│   ├── AIM-120.glb
│   ├── AIM-9.glb
│   ├── R27.glb
│   └── R73.glb
├── rocky/           ← Rocky 地图引擎着色器和数据
│   ├── shaders/
│   └── data/
│       └── times.vsgb
└── fonts/           ← VSG 字体文件
    └── times.vsgb
```

---

## 二、需要自行安装的依赖

以下依赖**必须提前安装**，无法随仓库分发（含 GPU 驱动相关内容）。

### 2.1 基础工具

| 工具 | 版本 | 下载地址 |
|------|------|---------|
| **Vulkan SDK** | ≥ 1.3 | <https://www.lunarg.com/vulkan-sdk/> |
| **CMake** | ≥ 3.10 | <https://cmake.org/download/> |
| **Qt6** | ≥ 6.4（需含 MSVC 2019 x64 组件） | <https://www.qt.io/download> |
| **MSVC 2019/2022** | — | Visual Studio 安装程序（选 C++ 桌面开发） |

> **Qt6 安装时必须勾选的组件**：  
> `Qt 6.x.x → MSVC 2019 64-bit`  
> `Qt 6.x.x → Qt Quick + Qt Quick Controls`  

### 2.2 C++ 依赖库（需源码编译）

以下三个库需要**按顺序**自行编译并安装到统一目录（例如 `D:\deps\install`）。

#### ① VulkanSceneGraph (vsg)

```bat
git clone https://github.com/vsg-dev/VulkanSceneGraph.git
cd VulkanSceneGraph
cmake -B build -DCMAKE_INSTALL_PREFIX=D:\deps\install -DCMAKE_BUILD_TYPE=Release
cmake --build build --config Release --parallel
cmake --install build --config Release
```

#### ② vsgXchange（模型格式加载插件，支持 .glb/.gltf 等）

```bat
git clone https://github.com/vsg-dev/vsgXchange.git
cd vsgXchange
cmake -B build -DCMAKE_PREFIX_PATH=D:\deps\install -DCMAKE_INSTALL_PREFIX=D:\deps\install -DCMAKE_BUILD_TYPE=Release
cmake --build build --config Release --parallel
cmake --install build --config Release
```

#### ③ Rocky（三维地球地图引擎）

> Rocky 依赖 `gdal`、`glm`、`imgui` 等，推荐用 [vcpkg](https://github.com/microsoft/vcpkg) 管理。

```bat
# 先安装 vcpkg 依赖
vcpkg install gdal glm imgui --triplet x64-windows

git clone https://github.com/pelicanmapping/rocky.git
cd rocky
cmake -B build ^
    -DCMAKE_TOOLCHAIN_FILE=D:\vcpkg\scripts\buildsystems\vcpkg.cmake ^
    -DCMAKE_PREFIX_PATH="D:\deps\install;D:\vcpkg\installed\x64-windows" ^
    -DCMAKE_INSTALL_PREFIX=D:\deps\install ^
    -DCMAKE_BUILD_TYPE=Release
cmake --build build --config Release --parallel
cmake --install build --config Release
```

---

## 三、编译本项目

### 3.1 克隆仓库

```bat
git clone <your-repo-url>
cd vsgQt-master
```

### 3.2 方式 A：Qt Creator（推荐）

1. 打开 Qt Creator → **文件 → 打开文件或项目**  
2. 选择 `vsgQt-master/CMakeLists.txt`（**根目录**的 CMakeLists，不是子目录的）  
3. 选择 Kit：`Desktop Qt 6.x MSVC2019 64-bit`  
4. 在项目配置页面，点击 **CMake 配置 → 添加** 变量：

   | 变量名 | 值（根据你的安装路径修改） |
   |--------|--------------------------|
   | `CMAKE_PREFIX_PATH` | `D:\deps\install;D:\Qt\6.6.0\msvc2019_64;D:\vcpkg\installed\x64-windows` |
   | `CMAKE_BUILD_TYPE` | `Release` |

5. 点击 **Configure** → 确认无报错  
6. 点击 **Build（锤子图标）** 开始构建

### 3.2 方式 B：命令行

```bat
cmake -B build ^
    -DCMAKE_PREFIX_PATH="D:\deps\install;D:\Qt\6.6.0\msvc2019_64;D:\vcpkg\installed\x64-windows" ^
    -DCMAKE_BUILD_TYPE=Release

cmake --build build --config Release --parallel
```

### 3.3 构建成功后的 bin 目录

构建成功后，`POST_BUILD` 脚本会**自动**将所有运行时资源复制到输出目录：

```
build/bin/Release/          （或 Qt Creator 的 build-xxx/bin/）
├── vsgqtviewer.exe         ← 主程序
├── qml/                    ✅ 自动复制（界面文件）
├── img/                    ✅ 自动复制（UI图片）
├── models/                 ✅ 自动复制（3D 飞机模型）
├── share/rocky/            ✅ 自动复制（Rocky 着色器）
├── fonts/                  ✅ 自动复制（VSG 字体）
└── data/                   ✅ 自动复制（示例 .acmi 数据）
```

> ⚠️ **DLL 文件需要手动部署**（见第四节）

---

## 四、部署 DLL（Windows 必须）

构建完成后，还需要将依赖的 DLL 复制到 `bin/` 目录（或将它们加入 `PATH`）。

**最简单的方法**：用 Qt 自带的 `windeployqt` 工具自动收集 Qt 相关 DLL：

```bat
cd build\bin\Release
"C:\Qt\6.x.x\msvc2019_64\bin\windeployqt.exe" --qmldir ..\..\..\examples\vsgqtviewer\qml vsgqtviewer.exe
```

**还需要手动复制**以下非 Qt 的 DLL（从各自的 install/bin 目录取）：

| DLL | 来源 |
|-----|------|
| `vsg.dll` | `D:\deps\install\bin\` |
| `vsgXchange.dll` | `D:\deps\install\bin\` |
| `rocky.dll` | `D:\deps\install\bin\` |
| `vulkan-1.dll` | Vulkan SDK `Bin\` 或系统目录 |
| `imgui.dll` | `D:\vcpkg\installed\x64-windows\bin\` |

---

## 五、运行

```bat
cd build\bin\Release
vsgqtviewer.exe
```

启动后将看到三维地球界面（需要网络连接加载在线地图瓦片）。

### 5.1 加载飞行数据（ACMI 回放）

使用项目自带的 Python 脚本向主程序推送飞行数据（须提前安装 Python 3.x，无额外依赖）：

```bat
# ★ 脚本路径：examples\vsgqtviewer\acmi_replay_sender.py
# 请在项目根目录（vsgQt-master/）下打开终端运行：

# 极速灌送真实飞行数据（瞬间将整个文件推入）
py examples\vsgqtviewer\acmi_replay_sender.py --fast --file examples\vsgqtviewer\data\2v2_flight.acmi

# 实时速度推流回放（可以看到完美的密级插帧平滑效果，推荐体验）
py examples\vsgqtviewer\acmi_replay_sender.py --speed 1 --file examples\vsgqtviewer\data\2v2_flight.acmi

# 循环极速推流
py examples\vsgqtviewer\acmi_replay_sender.py --fast --loop --file examples\vsgqtviewer\data\2v2_flight.acmi
```

| 参数 | 说明 |
|------|------|
| `--file` | 指定 .acmi 数据路径 |
| `--fast` | 极速推送：瞬间填满程序数据缓冲，后续由程序按自身时钟控制播放速度 |
| `--speed` | 实时推流模拟倍速，默认 10。若设 1 则是真实时间频率发送UDP包 |
| `--loop` | 循环发送 |
| `--demo` | 快速演示无需数据文件 |

> ⚠️ **关于高倍速下"只能插一两帧"的说明：**
> 如果您在界面点击了高倍速，这意味着真实的物理一秒钟内，模型要走完好几十秒的路程。根据渲染器固定帧率刷新规律，留给每段 0.2s 数据的总分配渲染时间极少，您必然只能看到它被采样一到两次帧画面。这属于正常的时空压缩，并非插值失效。

### 5.2 快速演示（无需 .acmi 文件）

```bat
py examples\vsgqtviewer\acmi_replay_sender.py --demo
```

### 5.3 遥测显示程序（可选）

点击界面中实体的"数据"按钮，程序会尝试启动外部遥测显示程序 `appchart_merged.exe`。  
如需此功能，将该程序放到 `bin/` 同目录下，或设置环境变量：

```bat
set TELEMETRY_EXE=D:\your\path\appchart_merged.exe
```

---

## 六、项目结构

```
vsgQt-master/
├── CMakeLists.txt              ← 根入口（Qt Creator 从这里打开）
├── README.md
├── assets/                     ← 内置运行时资源（随仓库分发）
│   ├── models/                 ← .glb 3D 模型
│   ├── rocky/                  ← Rocky 着色器 + 数据
│   └── fonts/                  ← VSG 字体
├── src/vsgQt/                  ← vsgQt 集成库源码
├── include/vsgQt/              ← vsgQt 头文件
└── examples/
    └── vsgqtviewer/            ← 主应用
        ├── CMakeLists.txt
        ├── src/                ← C++ 源文件
        ├── include/            ← 头文件
        ├── qml/                ← QML 界面文件
        ├── img/                ← UI 图片资源
        ├── py/                 ← Python 辅助脚本
        └── data/               ← 示例 .acmi 数据
```

---

## 七、运行时资源查找规则（给开发者）

程序按以下**优先顺序**查找资源，无需修改代码即可适配不同机器：

| 资源 | 查找顺序 |
|------|---------|
| Rocky 着色器 | `ROCKY_SHARE_DIR` 环境变量 → `bin/share/rocky/`（POST_BUILD 自动复制） |
| 3D 模型 | `MODELS_DIR` 环境变量 → `bin/models/`（POST_BUILD 自动复制） |
| 字体 | `bin/fonts/` → `ROCKY_SHARE_DIR/data/` → Windows 系统字体 |
| 遥测程序 | `TELEMETRY_EXE` 环境变量 → `bin/appchart_merged.exe` |

---

## 八、常见问题

**Q: CMake Configure 报 `Could not find rocky` / `Could not find vsg`？**  
A: 检查 `CMAKE_PREFIX_PATH` 是否包含所有依赖的 install 目录，以分号 `;` 分隔。

**Q: 运行时控制台输出 `ROCKY_FILE_PATH 路径不存在`？**  
A: 确认构建完成后 `bin/share/rocky/shaders/` 目录存在（POST_BUILD 应自动复制）。  
如仍不存在，手动将 `assets/rocky/` 目录复制到 `bin/share/rocky/`。

**Q: 启动后地球是黑色/无纹理？**  
A: 地图纹理默认从 `readymap.org` 在线加载，需要互联网连接。  
若在内网环境，需修改 `main.cpp` 中 `TMSImageLayer` 的 URI 为本地瓦片服务地址。

**Q: 3D 模型不显示？**  
A: 检查 `bin/models/` 目录是否存在且包含 `.glb` 文件。  
ACMI 数据中的机型名称需要与 `inferModelFile()` 函数中的关键词匹配（如 "F-16"、"Su-27"）。

**Q: 缺少 DLL，程序启动报错？**  
A: 运行 `windeployqt` 后再检查控制台报错信息，依照缺失的 DLL 名称从对应 install/bin 复制,别忘了qml那个也要

**Q: Qt Creator 里代码有大量红色下划线错误？**  
A: 这是 clangd 语言服务器找不到头文件的误报，**不影响实际构建**。  
需要先完成一次 CMake Configure，生成 `compile_commands.json` 后 IDE 提示才会正常。

---

## 九、许可证

本项目基于 [MIT License](LICENSE.md) 开源。  
内含 3D 模型资源版权归原始作者所有，仅供学习和演示使用。
