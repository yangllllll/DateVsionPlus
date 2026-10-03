# -----------------------------------------------------------------------------
# UVC 摄像头插件（Qt Plugin -> DLL）
#
# 本 DLL 导出 1 个工具：
#   uvc_camera  UVC 摄像头（无输入端口，输出「采集图像」；双击节点打开预览对话框）
#
# 工作方式：
#   1) 在对话框里「打开摄像头」后，后台线程用 cv::VideoCapture 连续取图，
#      每帧写入内部变量（带帧序号）。
#   2) 流程执行时不做任何开关流动作，直接输出该变量中的最新帧
#      （默认还会等一帧比上次更新的图，保证不会取到过时图像）。
#   3) 摄像头未打开时，execute() 会按参数自动打开并启动后台线程。
#
# 参数：
#   camera_index    摄像头序号
#   width / height  分辨率（0 = 摄像头默认值）
#   fps             帧率（0 = 摄像头默认值）
#   backend         采集后端（自动 / MSMF / DirectShow / V4L2 / AVFoundation / 任意）
#   wait_new_frame  等待新帧（关闭则直接输出当前最新帧，速度最快）
#   timeout         等待新帧的超时时间
#
# 依赖：Qt + OpenCV（含 videoio 模块，opencv_world 已包含），无需额外 SDK。
#
# 构建与部署：
#   1) 用 Qt Creator 打开本文件，选择【与主程序完全相同】的 Qt 套件与构建配置
#   2) 构建（OpenCV / 导入库由 pluginsdk.pri 自动配置）
#   3) 得到 uvccamera.dll，输出到 <本目录>/bin/plugins
#   4) 把它拷贝到 DateVisionPlus.exe 所在目录的 plugins/ 子目录，
#      启动主程序即可在「输入输出」分类中看到「UVC 摄像头」
# -----------------------------------------------------------------------------

include($$PWD/../../pluginsdk.pri)

TARGET   = uvccamera
TEMPLATE = lib

SOURCES += \
    uvccapture.cpp \
    uvccameradialog.cpp \
    uvccameraplugin.cpp

HEADERS += \
    uvccapture.h \
    uvccameradialog.h \
    uvccameraplugin.h

# 想让 DLL 直接生成到主程序可加载的位置，可在此覆盖（include 之后生效）：
# DESTDIR = <主程序构建目录>/release/plugins
