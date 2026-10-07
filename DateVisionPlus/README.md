# DateVisionPlus

工业视觉检测平台：以**流程图**的方式把视觉工具（图像源 → 预处理 → 检测定位 → 输出）连成一条检测流水线，支持项目存取、TCP 远程触发与插件扩展。

基于 **Qt 6（Widgets / Network）+ C++17 + OpenCV**，使用 **qmake** 构建。

---

## 一、功能特性

| 模块 | 说明 |
| --- | --- |
| 流程图画布 | `QGraphicsView/Scene` 实现：拖拽节点、端口连线、框选、缩放/适应视图 |
| 执行引擎 | 按**拓扑排序**执行，支持环检测、中止（`requestStop`）；与界面完全解耦 |
| 后台执行 | 计算过程跑在**独立工作线程**（`ExecutionWorker` + `QThread`），运行大流程不卡界面 |
| 单次 / 连续运行 | 连续运行采用「上一轮结束立即排下一轮」的 0ms 单发定时器，不留固定空档 |
| 属性面板 | 按插件的 `ParamDef` 自动生成控件（整数/浮点/滑块/布尔/下拉/字符串/文件） |
| 预览与结果 | 图像预览（滚轮缩放、适应窗口）、结果表格、彩色日志 |
| 通信面板 | 内置 TCP 服务端：接收控制字触发检测，并按模板回传结果 |
| 插件机制 | 内置工具静态注册 + 用户 Qt Plugin（DLL）自动扫描加载 |
| 项目文件 | `.dvp`（JSON）保存流程图与通信配置 |

### 内置工具

| 分类 | ID | 名称 |
| --- | --- | --- |
| 输入输出 | `image_source` | 图像源 |
| 输入输出 | `image_output` | 图像输出 |
| 图像处理 | `grayscale` | 灰度化 |
| 图像处理 | `threshold` | 阈值分割 |
| 图像处理 | `edge_detection` | 边缘检测 |
| 图像处理 | `morphology` | 形态学处理 |
| 图像处理 | `image_filter` | 图像滤波 |
| 检测定位 | `blob_analysis` | 斑点分析 |
| 检测定位 | `pattern_match` | 模板匹配 |
| 检测定位 | `measure` | 测量工具 |
| 检测定位 | `line_finder` | 线查找 |
| 检测定位 | `line_distance` | 线间距 |

其中 `line_finder`、`pattern_match` 双击节点会弹出专属参数调试对话框（ROI 选取、实时预览）。

---

## 二、目录结构

```
DateVisionPlus/
├── core/                核心层（插件基类 / 类型 / 表达式求值 / cv::Mat 与 Qt 互转）
│   ├── pluginbase.h     插件基类 PluginBase（inherit + 实现 execute()）
│   ├── plugintypes.h    端口/参数定义与工具函数（port、intParam、choiceParam ...）
│   ├── plugininterface.h 用户 DLL 插件接口 OpenVisionPluginProvider
│   ├── pluginmanager.h  插件管理器（内置注册 + DLL 扫描）
│   └── cvutils.h        cv::Mat ↔ QImage/QPixmap
├── flowchart/           流程图与执行
│   ├── flowscene/view   场景与视图（网格绘制、连线交互）
│   ├── nodeitem/portitem/connectionitem  图形项
│   └── executionengine/executionworker   拓扑执行引擎 + 后台线程封装
├── panels/              停靠面板：工具箱/预览/输出/属性/通信
├── dialogs/             参数调试对话框（线查找、模板匹配）
├── communication/       TCP 服务端与触发协议
├── plugins/             内置插件实现 + builtinregistry.cpp（注册入口）
├── widgets/             通用控件（带数值显示的滑块）
├── 3rdparty/opencv.pri  OpenCV 依赖探测与 DLL 自动拷贝
├── tools/               sync_plugin_sdk.bat（同步 core 头文件到插件 SDK）
└── datevisionplus.pro   工程文件
```

> 关键约定：`ExecutionEngine` 只持有「节点 id + 端口名」的纯数据（`LinkDef`），不引用任何 `QGraphicsItem`，因此整张图可以安全地在后台线程参与运算。

---

## 三、环境依赖

- **Qt 6.x**（`widgets`、`network`）—— 开发使用 Qt 6.11.2 / MSVC 2022 64bit
- **C++17** 编译器 —— 推荐 **MSVC 2019/2022 64bit**
- **OpenCV 4.x / 5.x**（`opencv_world` 单库版最省事）

OpenCV 路径通过 `3rdparty/opencv.pri` 自动探测：

1. 优先使用环境变量 `OPENCV_DIR`（指向形如 `D:\opencv\build` 的目录）；
2. 未设置时依次探测 `D:/opencv/build` → `C:/opencv/build` → `D:/opencv`；
3. 库名可用 `OPENCV_LIB_NAME` 手动指定（如 `opencv_world500`），否则自动探测 500/490/480/470/460。

> **MinGW 注意**：MSVC 编译的 `*.lib` 不能被 MinGW 链接。用 MinGW 套件时必须改用 MinGW 版 OpenCV（`*.a`），否则 qmake 阶段会给出明确提示。

---

## 四、编译与运行

```bat
:: 1) 设置 OpenCV（若不在默认探测路径）
set OPENCV_DIR=D:\opencv\build

:: 2) 生成 Makefile
qmake datevisionplus.pro -spec win32-msvc "CONFIG+=qtquickcompiler"

:: 3) 编译（Qt Creator 用的是 jom，也可 nmake）
jom            :: release/debug 取决于当前套件与配置
```

- 可执行文件输出在构建目录的 `release/` 或 `debug/` 子目录。
- `opencv.pri` 会在链接后**自动把 `opencv_*.dll` 拷贝到 exe 同目录**，一般无需手动配置 PATH。
- 用 Qt Creator 打开 `datevisionplus.pro`，选择 **Desktop Qt 6.x MSVC2022 64bit** 套件直接构建即可。

---

## 五、使用说明

1. **搭流程**：从左侧「工具箱」把工具拖到画布（或双击），鼠标从输出端口拖到输入端口完成连线。
2. **配参数**：选中节点后，在右侧「属性」面板修改参数；参数控件由插件声明自动生成，修改即时下发。
3. **运行**：
   - ▶ 运行：单次执行整张图；
   - 连续运行：上一轮结束立即执行下一轮（切换点见下文）；
   - ■ 停止：中止当前执行并退出连续模式。
4. **看结果**：底部「输出」面板显示日志与结果表，节点指示灯绿=OK / 红=NG；有图像输出的节点结果会显示在「预览」面板。
5. **存盘**：文件 → 保存/另存为 `.dvp`（JSON，含流程图与通信配置）。

### 连续运行的节拍

`mainwindow.cpp` 顶部：

```cpp
constexpr int kContinuousIdleMs = 0;
```

- `0`：全速——本轮结果回到界面后立刻排队下一轮（单发定时器只在事件队列空闲时投递，不会空转饿死界面）；
- 改成 `5 / 10 / 20` 等即为固定间隔（毫秒）。

### TCP 通信

通信面板可配置：监听端口（默认 8080）、检测控制字、分隔符、输出格式。

- 客户端发送**控制字**即触发一次检测；
- 回传内容 = `各节点 OK/NG 状态` + 可选自定义输出，按分隔符拼接；
- 输出格式支持 `{节点ID.端口名}` 引用与四则运算，例如：

```
{blob_analysis.count}
{measure.width} * 0.02
```

---

## 六、插件开发

- 插件 SDK 位于 **`../DateVisionPlusPluginSDK`**（`include/` 头文件、`lib/` 导入库、`examples/` 示例）。
- `core/` 是头文件的**唯一真身**，SDK 中是副本：改完 `core` 头文件后运行 **`tools/sync_plugin_sdk.bat`** 同步，并按需把主程序构建出的 `.lib` 拷到 `SDK/lib/`。

写一个插件分两步：

1. 继承 `OVP::PluginBase`，实现 `id()/name()/category()` 与 `execute()`，并声明端口（`inputPorts/outputPorts`）与参数（`inputParams`）：

```cpp
class MyPlugin : public OVP::PluginBase
{
    Q_OBJECT
public:
    QString id() const override   { return QStringLiteral("my_plugin"); }
    QString name() const override { return QStringLiteral("我的工具"); }
    QString category() const override { return QStringLiteral("检测定位"); }

    QList<OVP::PortDef> inputPorts() const override
    { return { OVP::port(QStringLiteral("image"), OVP::PortType::Image) }; }
    QList<OVP::PortDef> outputPorts() const override
    { return { OVP::port(QStringLiteral("count"), OVP::PortType::Number) }; }

    QList<OVP::ParamDef> inputParams() const override
    { return { OVP::intParam(QStringLiteral("min_area"), QStringLiteral("最小面积"), 100, 0, 100000) }; }

    bool execute() override
    {
        const cv::Mat image = OVP::toMat(input(QStringLiteral("image")));
        setOutput(QStringLiteral("count"), 42);
        return true;
    }
};
```

2. 在 DLL 中实现 `OpenVisionPluginProvider`（`Q_PLUGIN_METADATA` + `Q_INTERFACES`），把插件工厂暴露出来。

编译得到的 DLL 放到**程序运行目录下的 `plugins/` 目录**，启动即自动加载；也可用工具栏的「刷新插件」热重载。加载明细（成功/失败原因）会打印到输出面板。

参考示例：`SDK/examples/` 下的 `userpluginsample`（最小骨架）、`dahuacamera`、`uvccamera`、`plc`、`websocketserver`。

---

## 七、项目文件（`.dvp`）

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
