# OpenVisionPlus 插件 SDK

用 Qt Plugin（DLL）为 OpenVisionPlus 编写自定义视觉工具。本目录可**独立拷贝分发**，
不需要主程序源码树（OpenCV 仍需自行安装）。

## 打开方式

Qt Creator：**文件 → 打开文件或项目 → 选择 `pluginsdk.pro`**（不是选目录，也不是 `.pri`）。

```
OpenVisionPlusPluginSDK/
├── pluginsdk.pro              # Qt Creator 工程入口（subdirs 聚合，打开这个）
├── pluginsdk.pri              # 核心：插件工程 include 这一个文件即可
├── 3rdparty/opencv.pri        # OpenCV 配置（可用环境变量 OPENCV_DIR 覆盖）
├── include/                   # SDK 头文件
│   ├── pluginbase.h           #   插件基类（必须继承）
│   ├── plugintypes.h          #   端口/参数定义与数据打包辅助
│   ├── plugininterface.h      #   DLL 导出接口 OpenVisionPluginProvider
│   ├── coreglobal.h           #   符号导出宏
│   ├── cvutils.h              #   cv::Mat <-> QImage、颜色表等
│   └── opencvcompat.h         #   OpenCV 4.x / 5.x 兼容头
├── lib/                       # 主程序导出的导入库（MSVC 链接用）
│   ├── debug/OpenVisionPlus.lib
│   └── release/OpenVisionPlus.lib
├── templates/myplugin/        # 空白插件模板（复制即可开始）
├── examples/userpluginsample/ # 完整示例：4 个工具 + 一个专用对话框
├── examples/dahuacamera/      # 大华相机插件：MVSDK 取图 + 预览对话框
├── examples/websocketserver/  # WebSocket 图像流推送：图像转 base64 广播给客户端
└── examples/plc/               # PLC 传值：snap7 动态加载，检测结果写入 M/DB 区
```

## 快速开始（3 步）

### 1. 建工程

复制 `templates/myplugin/` 为你的插件目录，`.pro` 中改一行 SDK 路径：

```qmake
include($$PWD/../../pluginsdk.pri)   # 指向本 SDK 的 pluginsdk.pri

TARGET   = yourplugin
TEMPLATE = lib
```

### 2. 写工具

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

### 3. 构建并部署

把生成的 DLL 拷到 **`OpenVisionPlus.exe` 同目录的 `plugins/` 子目录**，
启动主程序即可在工具箱看到新分类；也可通过菜单 **工具 → 刷新插件** 热重载。

## API 速查

| 方法 | 用途 |
|---|---|
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

## 硬性要求（否则 `QPluginLoader` 加载失败）

1. 与主程序**相同的 Qt 版本**
2. 与主程序**相同的编译器套件**（MSVC / MinGW 不能混）
3. 与主程序**相同的构建配置**——Debug 配 Debug，Release 配 Release
4. OpenCV 版本与主程序一致（推荐都用同一个 `OPENCV_DIR`）

## 导入库说明（MSVC）

MSVC 链接插件时需要主程序导出的 `OpenVisionPlus.lib`。本 SDK 已内置于
`lib/debug`、`lib/release`，会**按当前构建配置自动选取**，通常无需干预。

若主程序重新构建过，请重新拷贝 lib（位置在主程序构建目录的 `debug/` 或 `release/` 下）。
也可放到插件工程目录，或设置环境变量 `OPENVISIONPLUS_LIBDIR` 指向其所在目录。
MinGW 工具链一般不需要该 lib。

## 环境自检

构建时"编译输出"面板会打印实际生效的配置：

```
Project MESSAGE: [OpenCV] SDK = D:/opencv/build
Project MESSAGE: [OpenCV] lib name = opencv_world500
Project MESSAGE: [OpenVisionPlus SDK] 导入库: .../lib/debug/OpenVisionPlus.lib
```

运行时若工具箱没有出现新工具，看主程序 **输出** 面板的日志，会写明跳过原因
（不是 Qt 插件库 / 未实现接口 / 编译器套件不匹配等）。
