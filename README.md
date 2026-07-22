# JiKong K1 上位机（Qt / C++）

用于连接 **JiKong K1** 固件，通过 USB HID Feature Report
读取、编辑键盘按键映射并下发+持久化到设备。键盘 UI 复刻自网页配置工具
`config-tool-web` 中“捕获触发键”弹窗的 108 键键盘 SVG 布局与外观。

## 功能

- 自绘 108 键键盘（圆角外壳 + 顶部高光 + 键帽），视觉与网页版一致；
- 点击键盘选“源键”再选“目标键”即可新增一条重映射；
- 右侧表格展示当前全部映射（源/目标/生效层/类型）；
- “从设备读取”会读回设备上**全部**映射（含鼠标宏等非键盘映射并原样保留），
  “下发到设备”按网页版完全一致的流程写入并持久化，不会误删其它配置；
- 通信协议（命令码、32 字节帧、小端、CRC32）与固件严格一致。

## 依赖

- CMake ≥ 3.16
- Qt 5 或 Qt 6（Widgets 模块）
- hidapi（开发库 + 头文件）

## 构建

### Windows（推荐 vcpkg）

```powershell
vcpkg install hidapi qtbase
cmake -B build -S . -DCMAKE_TOOLCHAIN_FILE=<vcpkg>/scripts/buildsystems/vcpkg.cmake
cmake --build build --config Release
```

若手动安装 hidapi，可用 `-DHIDAPI_ROOT=<hidapi路径>` 指定（其下需有 `include/` 与 `lib/`）。

### Linux

```bash
sudo apt install cmake qtbase5-dev libhidapi-dev   # 或 qt6-base-dev
cmake -B build -S .
cmake --build build -j
```

Linux 下访问 HID 设备通常需要 udev 规则（VID 4a4b / PID 4b31）或以 root 运行。

### macOS

```bash
brew install cmake qt hidapi
cmake -B build -S . -DCMAKE_PREFIX_PATH=$(brew --prefix qt)
cmake --build build -j
```

## 使用

1. 连接设备，点“连接设备”。程序会自动匹配 VID `0x4A4B` / PID `0x4B31`
   并选中 usage page `0xFF4B` 的配置接口，随后自动读取当前配置。
2. 点“新增映射”，先在键盘上点你实际按下的**源键**（高亮黄色），
   再点希望输出的**目标键**，即生成一条映射。
3. 需要删除时，在右侧表格选中该行点“删除选中”。
4. 点“下发到设备”写入并持久化（PERSIST_CONFIG 会擦写 flash，稍等即可）。

## 协议一致性说明

关键常量与网页 `config-tool-web/js/constants.js`、`protocol.js`、`crc.js` 对齐：

- Report ID：`100`（config）；帧长固定 `32` 字节；配置版本 `19`。
- 帧格式：`[version][command][fields... 小端][CRC32 小端(前 28 字节)]`。
- mapping 字节布局（ADD_MAPPING / GET_MAPPING）：
  `U32 target_usage, U32 source_usage, I32 scaling, U8 layer_mask, U8 flags, U8 hub_ports`
  其中 `hub_ports = (target_port<<4)|source_port`，`flags` 为 STICKY/TAP/HOLD 位。
- 保存流程：`SUSPEND → SET_CONFIG(原样回写标量) → CLEAR_MAPPING →
  逐条 ADD_MAPPING → PERSIST_CONFIG → RESUME`。

## 目录结构

```
qt-config-tool/
├── CMakeLists.txt
└── src/
    ├── main.cpp            程序入口
    ├── protocol.h/.cpp     协议常量、CRC32、32 字节帧构建
    ├── device.h/.cpp       hidapi 通信层（连接/读配置/读写映射/持久化）
    ├── keyboardlayout.h/.cpp  108 键布局数据（移植自 keyboard-diagram.js）
    ├── keyboardwidget.h/.cpp   自绘键盘控件（复刻 SVG 外观 + 点击/高亮）
    ├── usages.h/.cpp       usage <-> 可读名称
    └── mainwindow.h/.cpp   主界面与交互流程
```
