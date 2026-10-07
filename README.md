# DateVisionPlus 工业视觉检测平台

<div align="center">

**[English](#) | 简体中文**

**免费使用 · AGPL-3.0 · 流程图式视觉检测 · 插件扩展 · TCP通信**

---

### 开发者声明

| 项目 | 信息 |
|------|------|
| **开发者** | 杨佳祺 |
| **联系电话** | 15803820398 |
| **联系邮箱** | yangjiaqi@datekj.top |
| **版权所有** | 郑州德塔工业自动化 |

</div>

---

## 项目简介

DateVisionPlus 是一个**工业视觉检测平台**：用**流程图**的方式把视觉工具（图像源 → 预处理 → 检测定位 → 输出）连成一条检测流水线，支持项目存取、TCP 远程触发与插件扩展。

基于 **Qt 6（Widgets / Network）+ C++17 + OpenCV**，使用 **qmake** 构建，计算过程跑在独立工作线程，运行大流程不卡界面。

仓库包含两个子工程：

| 子工程 | 说明 | 入口 |
|--------|------|------|
| `DateVisionPlus/` | 主程序（界面 + 执行引擎 + 内置工具） | `datevisionplus.pro` |
| `DateVisionPlusPluginSDK/` | 插件 SDK（可独立拷贝分发） | `pluginsdk.pro` |

### 为什么选择 DateVision？

| 对比项 | DateVisionPlus | Halcon / VisionPro | VisionMaster |
|--------|----------------|-------------------|--------------|
| 费用 | **完全免费** | 数万元/年授权 | 需硬件绑定 |
| 开源 | **AGPL-3.0** | 闭源 | 闭源 |
| 插件扩展 | **C++ Qt 插件 DLL + SDK 模板** | 需要 SDK | 受限 |
| 通信 | **内置 TCP Socket 服务端** | 需额外开发 | 需额外开发 |
| 流程图 | **拖拽式** | 有 | 有 |
| 执行性能 | **原生 C++ / OpenCV / 后台线程** | 高 | 中 |
| 学习门槛 | **低（拖拖拽拽 + C++ 可选）** | 高（Halcon 语法） | 中 |

---

## 功能特性

### 核心功能
- **流程图编辑器** — `QGraphicsView/Scene` 实现：拖拽节点、端口连线、框选、缩放/适应视图
- **11 个内置工具** — 图像源、图像输出、灰度化、阈值分割、边缘检测、形态学、图像滤波、斑点分析、模板匹配、测量工具、线查找、线间距
- **插件系统** — 用户 Qt Plugin（DLL）自动扫描加载，SDK 提供空白模板与 5 个完整示例
- **后台执行** — `ExecutionWorker` + `QThread`，执行引擎只持有「节点 id + 端口名」纯数据，与界面完全解耦
- **单次 / 连续运行** — 连续运行采用「上一轮结束立即排下一轮」的 0ms 单发定时器，全速且不留空档
- **属性面板** — 按插件 `ParamDef` 自动生成控件（整数/浮点/滑块/布尔/下拉/字符串/文件）
- **图像预览** — 滚轮缩放、适应窗口；结果表格 + 彩色日志，节点指示灯绿=OK / 红=NG
- **项目保存/加载** — `.dvp` 格式，JSON 可读，含流程图与通信配置

### 检测定位工具
- **线查找 `line_finder`** — 双击节点弹出 ROI 调试对话框，实时预览
- **模板匹配 `pattern_match`** — 双击节点弹出模板选取与匹配预览对话框
- **斑点分析 `blob_analysis`** / **测量工具 `measure`** / **线间距 `line_distance`** — 数值结果可直接被通信输出引用

### 通信功能
- **TCP Socket 服务端** — 内置，配置监听端口（默认 8080）即可启动
- **远程触发检测** — 客户端发送控制字（默认 `TRIGGER`）自动执行流程图
- **自定义输出格式** — 支持 `{节点ID.端口名}` 引用 + 四则运算，例如：
  ```
  {blob_analysis.count}
  {measure.width} * 0.02
  ```
- **响应格式** — `各节点 OK/NG 状态` + 自定义输出，按分隔符拼接后回传

---

## 快速开始

### 环境要求
- **Qt 6.x**（`widgets`、`network`）—— 开发使用 Qt 6.11.2 / MSVC 2022 64bit
- **C++17** 编译器 —— 推荐 MSVC 2019/2022 64bit
- **OpenCV 4.x / 5.x**（`opencv_world` 单库版最省事）
- Windows 10/11

### 编译运行主程序

```bat
:: 1) 设置 OpenCV（若不在默认探测路径）
set OPENCV_DIR=D:\opencv\build

:: 2) 生成 Makefile
cd DateVisionPlus
qmake datevisionplus.pro -spec win32-msvc "CONFIG+=qtquickcompiler"

:: 3) 编译（Qt Creator 用的是 jom，也可 nmake）
jom
```

- 可执行文件输出在构建目录的 `release/` 或 `debug/` 子目录
- `3rdparty/opencv.pri` 会在链接后**自动把 `opencv_*.dll` 拷贝到 exe 同目录**
- 用 Qt Creator 打开 `datevisionplus.pro`，选择 **Desktop Qt 6.x MSVC2022 64bit** 套件直接构建即可

### OpenCV 路径探测顺序
1. 环境变量 `OPENCV_DIR`（指向形如 `D:\opencv\build` 的目录）
2. 未设置时依次探测 `D:/opencv/build` → `C:/opencv/build` → `D:/opencv`
3. 库名可用 `OPENCV_LIB_NAME` 手动指定（如 `opencv_world500`），否则自动探测 500/490/480/470/460

> **MinGW 注意**：MSVC 编译的 `*.lib` 不能被 MinGW 链接。用 MinGW 套件时必须改用 MinGW 版 OpenCV（`*.a`）。

---

## 使用指南

### 基本流程
1. **搭流程** — 从左侧「工具箱」把工具拖到画布（或双击），鼠标从输出端口拖到输入端口完成连线
2. **配参数** — 选中节点后，在右侧「属性」面板修改参数；控件由插件声明自动生成，修改即时下发
3. **运行** — ▶ 运行（单次执行整张图）/ 连续运行 / ■ 停止（中止并退出连续模式）
4. **看结果** — 底部「输出」面板显示日志与结果表，节点指示灯绿=OK / 红=NG；有图像输出的节点结果显示在「预览」面板
5. **存盘** — 文件 → 保存/另存为 `.dvp`（JSON，含流程图与通信配置）

### 连续运行的节拍

`mainwindow.cpp` 顶部：

```cpp
constexpr int kContinuousIdleMs = 0;
```

- `0`：全速——本轮结果回到界面后立刻排队下一轮（单发定时器只在事件队列空闲时投递，不会空转饿死界面）
- 改成 `5 / 10 / 20` 等即为固定间隔（毫秒）

### TCP 通信配置
1. 切换到底部 **「通信」** 标签页
2. 设置监听端口（默认 8080）、控制字（默认 `TRIGGER`）、分隔符、输出格式
3. 点击 **启动服务**
4. 外部客户端连接后发送控制字即触发一次检测
5. 回传内容 = `各节点 OK/NG 状态` + 可选自定义输出，按分隔符拼接

---

## 自定义插件开发

插件 SDK 位于 **`DateVisionPlusPluginSDK/`**，用 Qt Plugin（DLL）编写自定义视觉工具，可独立拷贝分发。

### 三步上手

**1. 建工程** — 复制 `templates/myplugin/`（或 `examples/userpluginsample/`），`.pro` 中改一行 SDK 路径：

```qmake
include($$PWD/../../pluginsdk.pri)   # 指向 SDK 的 pluginsdk.pri

TARGET   = yourplugin
TEMPLATE = lib
```

**2. 写工具** — 继承 `OVP::PluginBase`，实现元信息、`execute()`，并声明端口与参数：

```cpp
#include "pluginbase.h"
#include "plugininterface.h"
#include "opencvcompat.h"

class YourPlugin : public OVP::PluginBase
{
    Q_OBJECT
public:
    QString id() const override   { return "user.your_tool"; }
    QString name() const override { return "你的工具"; }
    QString category() const override { return "用户扩展"; }

    QList<OVP::PortDef> inputPorts() const override
    { return {OVP::port("input", OVP::PortType::Image, "输入图像")}; }

    QList<OVP::PortDef> outputPorts() const override
    { return {OVP::port("output", OVP::PortType::Image, "输出图像")}; }

    bool execute() override
    {
        cv::Mat src = inputImage("input");
        if (src.empty()) { setError("没有输入图像"); return false; }
        cv::Mat dst;
        cv::GaussianBlur(src, dst, cv::Size(5, 5), 0);
        setOutput("output", OVP::imageValue(dst));   // 图像端口必须包装
        return true;
    }
};

class YourProvider : public QObject, public OpenVisionPluginProvider
{
    Q_OBJECT
    Q_PLUGIN_METADATA(IID OpenVisionPluginProvider_iid)
    Q_INTERFACES(OpenVisionPluginProvider)
public:
    QList<Entry> availablePlugins() const override
    { return { Entry{ createYourPlugin } }; }   // 一个 DLL 可导出多个
};
```

**3. 构建并部署** — 把生成的 DLL 拷到 **`DateVisionPlus.exe` 同目录的 `plugins/` 子目录**，启动主程序即可在工具箱看到新分类；也可通过菜单 **工具 → 刷新插件** 热重载。加载明细（成功/失败原因）会打印到输出面板。

### 硬性要求（否则 `QPluginLoader` 加载失败）
1. 与主程序**相同的 Qt 版本**
2. 与主程序**相同的编译器套件**（MSVC / MinGW 不能混）
3. 与主程序**相同的构建配置**——Debug 配 Debug，Release 配 Release
4. OpenCV 版本与主程序一致（推荐都用同一个 `OPENCV_DIR`）

### 官方示例

| 目录 | 工具 ID | 说明 |
|------|---------|------|
| `examples/userpluginsample/` | 多个 | 完整示例：4 个工具 + 专用对话框，适合当起步模板 |
| `examples/dahuacamera/` | `mv_camera` | 大华工业相机：`QLibrary` 动态加载 `MVSDKmd.dll`，未装 SDK 也不会导致 DLL 加载失败 |
| `examples/uvccamera/` | `uvc_camera` | 标准 UVC 摄像头：后台线程连续取图，执行时只输出最新帧，无开关流延迟 |
| `examples/websocketserver/` | — | 把图像转 base64 广播给 WebSocket 客户端 |
| `examples/plc/` | — | snap7 动态加载，检测结果写入 PLC 的 M / DB 区 |

> 相机类插件推荐统一采用「句柄常驻 + 后台线程连续取图 + 帧序号」的结构，避免每次执行都 start → grab → stop 带来的延迟，也不会取到过时图像。

### API 速查

| 方法 | 用途 |
|------|------|
| `id() / name() / category() / description()` | 元信息，`category` 决定工具箱分组 |
| `inputPorts() / outputPorts()` | 端口定义，类型需匹配才能连线（`Any` 通配） |
| `inputParams()` | 参数定义，属性面板自动生成对应控件 |
| `execute()` | 处理逻辑，返回 false 表示 NG（节点亮红点） |
| `createDialog(inputImage, parent)` | 双击节点弹出的设置界面，返回 nullptr 表示没有 |
| `extraData() / setExtraData()` | 自定义数据（ROI、模板等）随项目保存/加载 |
| `inputImage(port)` | 取图像输入 |
| `input(port) / setOutput(port, value)` | 取/写任意端口数据 |
| `paramInt/paramDouble/paramString/paramBool(name)` | 取参数 |
| `setError(msg)` | 设置失败原因，显示在输出面板 |

参数构造辅助：`OVP::intParam / floatParam / sliderParam / boolParam / choiceParam / stringParam / fileParam`

图像相关：`OVP::imageValue(mat)`、`OVP::toMat(variant)`、`OVP::ensureGray/ensureBgr`、`OVP::oddKernelSize`

---

## 项目结构

```
DateVision/
├── DateVisionPlus/                 主程序工程
│   ├── core/                       核心层（插件基类 / 类型 / 表达式求值 / cv::Mat 与 Qt 互转）
│   │   ├── pluginbase.h            PluginBase（继承 + 实现 execute()）
│   │   ├── plugintypes.h           端口/参数定义与工具函数（port、intParam、choiceParam ...）
│   │   ├── plugininterface.h       用户 DLL 插件接口 OpenVisionPluginProvider
│   │   ├── pluginmanager.h         插件管理器（内置注册 + DLL 扫描）
│   │   └── cvutils.h               cv::Mat ↔ QImage/QPixmap
│   ├── flowchart/                  流程图与执行
│   │   ├── flowscene/flowview      场景与视图（网格绘制、连线交互）
│   │   ├── nodeitem/portitem/connectionitem   图形项
│   │   └── executionengine/executionworker    拓扑执行引擎 + 后台线程封装
│   ├── panels/                     停靠面板：工具箱/预览/输出/属性/通信
│   ├── dialogs/                    参数调试对话框（线查找、模板匹配）
│   ├── communication/              TCP 服务端与触发协议
│   ├── plugins/                    内置插件实现 + builtinregistry.cpp（注册入口）
│   │   ├── imagesourceplugin       图像源
│   │   ├── imageoutputplugin       图像输出
│   │   ├── grayscaleplugin         灰度化
│   │   ├── thresholdplugin         阈值分割
│   │   ├── edgedetectionplugin     边缘检测
│   │   ├── morphologyplugin        形态学处理
│   │   ├── imagefilterplugin       图像滤波
│   │   ├── blobanalysisplugin      斑点分析
│   │   ├── patternmatchplugin      模板匹配
│   │   ├── measureplugin           测量工具
│   │   ├── linefinderplugin        线查找
│   │   └── linedistanceplugin      线间距
│   ├── widgets/                    通用控件（带数值显示的滑块）
│   ├── 3rdparty/opencv.pri         OpenCV 依赖探测与 DLL 自动拷贝
│   ├── tools/                      sync_plugin_sdk.bat（同步 core 头文件到插件 SDK）
│   └── datevisionplus.pro          工程文件
└── DateVisionPlusPluginSDK/        插件 SDK（可独立拷贝分发）
    ├── pluginsdk.pro               Qt Creator 工程入口（subdirs 聚合）
    ├── pluginsdk.pri               核心：插件工程 include 这一个文件即可
    ├── 3rdparty/opencv.pri         OpenCV 配置（可用 OPENCV_DIR 覆盖）
    ├── include/                    SDK 头文件（pluginbase / plugintypes /
    │                               plugininterface / coreglobal / cvutils / opencvcompat）
    ├── lib/                        主程序导出的导入库（debug/release 自动选取）
    ├── templates/myplugin/         空白插件模板（复制即可开始）
    └── examples/                   userpluginsample / dahuacamera /
                                    websocketserver / plc / uvccamera
```

> 关键约定：`ExecutionEngine` 只持有「节点 id + 端口名」的纯数据（`LinkDef`），不引用任何 `QGraphicsItem`，因此整张图可以安全地在后台线程参与运算。
>
> `core/` 是头文件的**唯一真身**，SDK 中是副本：改完 `core` 头文件后运行 **`tools/sync_plugin_sdk.bat`** 同步，并按需把主程序构建出的 `.lib` 拷到 `SDK/lib/`。

---

## 技术架构

```
┌─────────────────────────────────────────────────┐
│                   MainWindow                     │
│  ┌──────────┐ ┌──────────┐ ┌──────────────────┐ │
│  │ Toolbox  │ │ Flowchart│ │   Properties     │ │
│  │ (左侧)   │ │ (中央)   │ │   (右侧)         │ │
│  └──────────┘ └──────────┘ └──────────────────┘ │
│  ┌──────────────────────────────────────────────┐│
│  │  Preview  │  Output / Communication        ││
│  │  (底部)   │  (底部标签页)                   ││
│  └──────────────────────────────────────────────┘│
└─────────────────────────────────────────────────┘
          │                    │
          ▼                    ▼
   ┌──────────┐        ┌──────────────┐
   │ Plugin   │◄──────►│  Execution   │
   │ Manager  │        │  Engine      │
   └──────────┘        └──────────────┘
          │                    │
          ▼                    ▼
   ┌──────────┐        ┌──────────────┐
   │ Built-in │        │  Topological │
   │ + DLL    │        │  Sort + Run  │
   └──────────┘        └──────────────┘
                              │
                              ▼
                   ┌────────────────────┐
                   │ ExecutionWorker    │
                   │ (QThread 后台线程) │
                   └────────────────────┘
```

---

## 项目文件（`.dvp`）

JSON 格式，根对象由流程图与通信配置组成：

```json
{
  "version": "2.0",
  "nodes": [ ... ],
  "connections": [ ... ],
  "communication": {
    "port": 8080,
    "control_word": "TRIGGER",
    "output_format": "{measure.width}",
    "delimiter": ",",
    "custom_delimiter": ""
  }
}
```

`nodes` / `connections` 由 `FlowScene::toJson()` 生成（节点位置、插件 id、参数值、端口连线关系）。

---

## 快捷键

| 快捷键 | 功能 |
|--------|------|
| `F5` | 运行流程图（单次） |
| `F6` | 切换连续运行 |
| `Shift+F5` | 停止运行 |
| `Del` | 删除选中节点/连线 |
| `Ctrl+N` | 新建项目 |
| `Ctrl+O` | 打开项目 |
| `Ctrl+S` | 保存项目 |
| `Ctrl+Shift+S` | 另存为 |
| `Ctrl++` / `Ctrl+-` | 放大 / 缩小画布 |
| `Ctrl+0` | 适应窗口 |

---

## 常见问题

**工具箱没有出现我的插件？**
看主程序 **输出** 面板的日志，会写明跳过原因（不是 Qt 插件库 / 未实现接口 / 编译器套件不匹配等）。

**构建时 OpenCV 找不到？**
设置 `OPENCV_DIR`（指向 `...\opencv\build`），或用 `OPENCV_LIB_NAME` 手动指定库名。构建输出会打印实际生效配置：

```
Project MESSAGE: [OpenCV] SDK = D:/opencv/build
Project MESSAGE: [OpenCV] lib name = opencv_world500
Project MESSAGE: [OpenVisionPlus SDK] 导入库: .../lib/debug/OpenVisionPlus.lib
```

**插件链接时找不到 `OpenVisionPlus.lib`？**
SDK 已内置于 `lib/debug`、`lib/release`，会按当前构建配置自动选取。若主程序重新构建过，重新拷贝 lib（位于主程序构建目录的 `debug/` 或 `release/` 下），也可设置环境变量 `OPENVISIONPLUS_LIBDIR`。MinGW 工具链一般不需要该 lib。

---

## 广告赞助

<div align="center">

### 开源不易，如果 DateVision 帮助到了你，欢迎支持！

---

### 商业合作

如需**定制开发**、**技术培训**、**产线集成**，请联系：

- **开发者**：杨佳祺
- **电话**：15803820398
- **邮箱**：yangjiaqi@datekj.top
- **公司**：郑州德塔工业自动化

---

### 推荐项目

| 项目 | 说明 |
|------|------|
| [OpenCV](https://opencv.org/) | 开源计算机视觉库 |
| [Qt 6](https://www.qt.io/) | 跨平台 C++ 应用框架 |
| [PySide6](https://wiki.qt.io/Qt_for_Python) | Qt for Python |
| [DateVisionPlus](https://github.com) | 本项目 - 工业视觉检测平台 |

---

### 开源协议

本项目基于 **GNU AGPL-3.0** 协议开源。简单来说：

- 你可以自由使用、修改、分发
- 但**修改后的代码也必须以 AGPL-3.0 开源**
- 通过网络提供服务也必须公开源码
- 这能有效阻止他人将你的代码闭源后商业售卖

详见 [LICENSE](LICENSE) 文件。

> 如需商业闭源授权或技术支持，请联系：杨佳祺 / 15803820398 / yangjiaqi@datekj.top

</div>

---

<div align="center">

**⭐ 如果觉得有用，请给个 Star！ ⭐**

Made with ❤️ by OpenVision Team

</div>
