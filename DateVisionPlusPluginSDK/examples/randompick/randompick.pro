# -----------------------------------------------------------------------------
# 无序抓取插件（Qt Plugin -> DLL）
#
# 读取「N点标定」保存的标定文件（.ncal），把 YOLO 检测到的目标中心点
# 从图像坐标转换为机器人坐标：
#   x = a*u + b*v + c
#   y = d*u + e*v + f
#
# 默认取第一个目标（按置信度降序），可通过参数选择第 N 个。
#
# 输入：input（图像，可选）、detections（YOLO 检测结果）
# 输出：robot_x / robot_y / robot_point、pixel_x / pixel_y、类别、置信度等
#
# 本 DLL 导出 1 个工具：
#   random_pick  无序抓取（分类：机器人）
#
# 注意：calibdata.* 与 detectionlist.* 位于 examples/common，与 N点标定 共用。
# -----------------------------------------------------------------------------

include($$PWD/../../pluginsdk.pri)

TARGET   = randompick
TEMPLATE = lib

INCLUDEPATH += $$PWD/../common

SOURCES += \
    randompickplugin.cpp \
    randompickdialog.cpp \
    $$PWD/../common/calibdata.cpp \
    $$PWD/../common/detectionlist.cpp

HEADERS += \
    randompickplugin.h \
    randompickdialog.h \
    $$PWD/../common/calibdata.h \
    $$PWD/../common/detectionlist.h
