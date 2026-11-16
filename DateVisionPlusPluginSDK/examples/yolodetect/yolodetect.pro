# -----------------------------------------------------------------------------
# YOLO 目标识别插件（Qt Plugin -> DLL）
#
# 对应 Python 版 user_plugins/yolo_detect.py，模型推理支持两种后端：
#   1) OpenCV DNN（默认，随 OpenCV 一起编译；CUDA 需要 OpenCV 本身带 CUDA）
#   2) ONNX Runtime（运行期动态加载 onnxruntime.dll；GPU 版直接支持 CUDA）
#
# ONNX Runtime 是可选依赖：
#   - 编译期只需要头文件 onnxruntime_c_api.h，不需要导入库（运行期 QLibrary 加载）
#   - 下载 onnxruntime-win-x64（CPU）或 onnxruntime-win-x64-gpu（CUDA）后解压，
#     设置环境变量 ONNXRUNTIME_DIR 指向解压目录（其中应有 include/ 和 lib/），
#     或者把解压目录放到本工程下的 onnxruntime/。未找到头文件时只编译 OpenCV DNN 后端。
#   - 运行期把 onnxruntime.dll 及同目录 DLL 放到主程序目录（或插件参数中指定路径）。
#
# 本 DLL 导出 1 个工具：
#   user_yolo_detect  YOLO目标识别（输入图像 -> 标注图像、目标数量、检测结果等）
#
# 构建与部署：
#   1) 用 Qt Creator 打开本文件，选择【与主程序完全相同】的 Qt 套件与构建配置
#   2) 构建（OpenCV / 导入库由 pluginsdk.pri 自动配置）
#   3) 得到 yolodetect.dll，输出到 <本目录>/bin/plugins
#   4) 把它拷到 DateVisionPlus.exe 所在目录的 plugins/ 子目录，
#      启动主程序即可在「检测定位」分类下看到「YOLO目标识别」
# -----------------------------------------------------------------------------

include($$PWD/../../pluginsdk.pri)

TARGET   = yolodetect
TEMPLATE = lib

SOURCES += \
    yolodetector.cpp \
    onnxruntimebackend.cpp \
    yolodetectdialog.cpp \
    yolodetectplugin.cpp

HEADERS += \
    yolodetector.h \
    onnxruntimebackend.h \
    yolodetectdialog.h \
    yolodetectplugin.h

# -----------------------------------------------------------------------------
# ONNX Runtime（可选）：探测头文件，找不到就只编译 OpenCV DNN 后端
# -----------------------------------------------------------------------------
ONNXRUNTIME_ROOT = $$(ONNXRUNTIME_DIR)
isEmpty(ONNXRUNTIME_ROOT): exists($$PWD/onnxruntime/include/onnxruntime_c_api.h): ONNXRUNTIME_ROOT = $$PWD/onnxruntime
isEmpty(ONNXRUNTIME_ROOT): exists(D:/onnxruntime/include/onnxruntime_c_api.h): ONNXRUNTIME_ROOT = D:/onnxruntime
isEmpty(ONNXRUNTIME_ROOT): exists(C:/onnxruntime/include/onnxruntime_c_api.h): ONNXRUNTIME_ROOT = C:/onnxruntime

!isEmpty(ONNXRUNTIME_ROOT) {
    exists($${ONNXRUNTIME_ROOT}/include/onnxruntime_c_api.h) {
        INCLUDEPATH += $${ONNXRUNTIME_ROOT}/include
        DEFINES += YOLO_HAVE_ONNXRUNTIME
        message([YOLO] ONNX Runtime 头文件: $${ONNXRUNTIME_ROOT}/include)
    } else {
        message("================================================================")
        message(" [YOLO] 未找到 $${ONNXRUNTIME_ROOT}/include/onnxruntime_c_api.h")
        message(" 仅编译 OpenCV DNN 后端（CUDA 需要 OpenCV 本身带 CUDA）")
        message("================================================================")
    }
} else {
    message("================================================================")
    message(" [YOLO] 未配置 ONNX Runtime（可选依赖）")
    message(" 如需 GPU 推理：下载 onnxruntime-win-x64-gpu 并设置环境变量")
    message(" ONNXRUNTIME_DIR 指向解压目录后重新执行 qmake。")
    message(" 当前仅编译 OpenCV DNN 后端。")
    message("================================================================")
}

# 想让 DLL 直接生成到主程序可加载的位置，可在此覆盖（include 之后生效）：
# DESTDIR = <主程序构建目录>/release/plugins
