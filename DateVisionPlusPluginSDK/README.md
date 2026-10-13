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
├── examples/userpluginsample/ # 完整示例：4 个工具 + 一个专用对话框（无模板目录时可直接复制它）
├── examples/dahuacamera/      # 大华相机插件：MVSDK 取图 + 预览对话框
├── examples/websocketserver/  # WebSocket 图像流推送：图像转 base64 广播给客户端
├── examples/plc/              # PLC 传值：snap7 动态加载，检测结果写入 M/DB 区
├── examples/uvccamera/        # UVC 摄像头插件：后台线程连续取图，执行时直接输出最新帧
└── examples/poseestimation/   # PnP 位姿估计：2D/3D 对应点、RANSAC、重投影预览
```

## 快速开始（3 步）

### 1. 建工程

复制 `templates/myplugin/`（模板目录缺失时可直接复制 `examples/userpluginsample/`）
为你的插件目录，`.pro` 中改一行 SDK 路径：

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

## 示例插件一览

| 目录 | 工具 ID | 说明 |
|---|---|---|
| `examples/userpluginsample/` | 多个 | 完整示例：4 个工具 + 专用对话框，适合当起步模板 |
| `examples/dahuacamera/` | `mv_camera` | 大华工业相机：`QLibrary` 动态加载 `MVSDKmd.dll`，未装 SDK 也不会导致 DLL 加载失败 |
| `examples/websocketserver/` | — | 把图像转 base64 广播给 WebSocket 客户端 |
| `examples/plc/` | — | snap7 动态加载，检测结果写入 PLC 的 M / DB 区 |
| `examples/uvccamera/` | `uvc_camera` | 标准 UVC 摄像头：后台线程连续取图，执行时只输出最新帧 |
| `examples/poseestimation/` | `pose_estimation` | 已知相机内参及 2D/3D 对应点求解位姿，支持 RANSAC、LM 优化与重投影预览 |

### PnP 位姿估计插件（`examples/poseestimation/`）

工具箱分类为「三维视觉」。双击节点可编辑相机内参、畸变系数和对应点表；也可以由上游端口提供实时二维点。
输出旋转向量、平移、物体到相机及其逆变换、重投影误差和可选标注图像。
普通求解至少需要 4 个独立对应点，RANSAC 至少需要 6 个。

详细输入格式、坐标方向、使用示例和测试命令见 [PnP 插件说明](examples/poseestimation/README.md)。

### UVC 摄像头插件（`examples/uvccamera/`）

依赖 OpenCV 的 `videoio` 模块即可，不需要任何厂商 SDK。

- **取图模型**：`UvcGrabber`（继承 `QThread`）在 `run()` 里打开 `cv::VideoCapture` 后
  循环 `cap.read()`，每帧写入内部变量并累加帧序号；UI 关闭对话框**不会**关闭摄像头，
  只有插件析构时才停止线程。
- **执行时复用采集线程**：相机已启动时，`execute()` 读取后台最新帧；首次执行或断线重连仍需打开相机。
  采集线程交换双缓冲，读取端在锁外复制像素；旧缓冲区没有读者时才会复用，避免整帧复制阻塞采集。
  输出保留独立的图像内存，下游修改不会影响采集缓存：

  ```cpp
  // 等待一帧比上次更新的图（默认，杜绝过时/重复帧）
  grabber.waitFrame(frame, timeoutMs, lastSeq, &error, &seq);
  // 或直接取当前最新帧（最快，参数 wait_new_frame 关闭时走这条）
  grabber.lastFrame(frame, &seq);
  ```

  `seq` 与返回图像在同一次加锁中读取，后续等待应使用该序号，避免再次调用
  `frameSeq()` 时采集线程已更新到另一帧。

- **参数**：`camera_index`、`width / height / fps`（0 = 摄像头默认值）、
  `backend`（自动 / MSMF / DirectShow / V4L2 / AVFoundation / 任意，后端用数值常量以兼容 OpenCV 4/5）、
  `wait_new_frame`、`timeout`。
- 双击节点打开对话框：刷新摄像头列表（连续 2 个序号打不开即停止扫描）、打开/关闭、
  设置分辨率与帧率、实时预览并显示实测帧率与帧号。

> 相机类插件推荐统一采用这种「句柄常驻 + 后台线程连续取图 + 帧序号」的结构：
> 避免每次执行都 start → grab → stop 带来的延迟，也不会取到过时图像。

## 高频路径优化与验证

- **UVC**：采集帧通过双缓冲交换发布，仅复用没有读者的缓冲区；读者只在锁内取得帧引用和序号，在锁外生成独立输出。
- **大华相机**：SDK 像素转换直接写入最终图像，减少一次整帧复制；已有连续取流不再被
  `grab()` 关闭，临时取流和 SDK 帧通过作用域守卫释放。Bayer GB8 原始灰度输出按行处理 padding。
- **WebSocket**：输入版本和 JPEG 质量未变时复用完整编码帧，多客户端共用一次封装；
  慢客户端有待发送数据时跳过本轮，全部客户端繁忙时跳过编码。仍按设定帧率重发最后一帧。
  工作线程通过加锁的最新帧缓存和合并的配置请求与网络线程交互，socket 和 timer 由对象所属线程操作。
- **PLC**：单字节读写使用栈内存；连接按 IP、rack、slot 完整匹配后复用；
  多字节读写按 Snap7 元素大小分配缓冲区并检查长度。相同结果仍每轮写入，默认「执行后断开」保持不变。

离线回归工程使用合成帧、模拟 SDK 或本机回环，不需要连接实际相机或 PLC：

| 测试工程 | 主要覆盖 |
|---|---|
| `examples/uvccamera/tests/camera_tests.pro` | 图像所有权、并发帧序号、停止唤醒、大华直接转换与取流清理 |
| `examples/websocketserver/tests/websocketserver_tests.pro` | 编码缓存、慢客户端、跨线程执行、握手连接清理 |
| `examples/plc/tests/plc-tests.pro` | 模拟 Snap7 动态库、连接复用、周期写入、字长与缓冲区边界 |

WebSocket 回归工程需要相邻 `DateVisionPlus/core` 下的基类实现源码；分发 SDK 后单独构建插件不受影响。

使用与主程序一致的 Qt / MSVC / OpenCV，在开发命令行中建立独立 Release 构建目录。例如从 SDK 目录执行：

```bat
mkdir build\camera-tests
cd build\camera-tests
qmake ..\..\examples\uvccamera\tests\camera_tests.pro -spec win32-msvc "CONFIG+=release" "CONFIG-=debug debug_and_release"
jom -j 4
release\camera_tests.exe
```

WebSocket 的完整命令见 [测试说明](examples/websocketserver/tests/README.md)。PLC 在独立目录
对 `plc-tests.pro` 执行 qmake 和 jom 后，运行 `bin\plc_tests.exe "<构建目录绝对路径>\bin\snap7_mock.dll"`；
测试程序会验证模拟库的专用符号，不会连接真实 PLC。

运行时需要将 Qt 和 OpenCV 的 `bin` 加入 PATH。局部合成基准不包含实际设备采集或网络延迟，
不能视为完整检测流程的提速比例；部署前仍应使用实际设备验证连续运行、重连与停止。

2026-10-13 本机验证：Qt 6.11.2 / MSVC Release / OpenCV 5.0，五个示例插件构建通过，
相机、WebSocket 与 PLC 离线回归通过。UVC 的 80 张 1080p 图像缓冲准备与发布基准，
预热后按新旧交替顺序运行 7 轮，中位数由 44.43 ms 降至 23.68 ms；不包含设备采集、解码或读者深拷贝。

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
