# -----------------------------------------------------------------------------
# N 点标定插件（Qt Plugin -> DLL）
#
# 录入 N 组「图像坐标 -> 机器人坐标」对应点（>=3），最小二乘拟合二维仿射变换：
#   x = a*u + b*v + c
#   y = d*u + e*v + f
#
# 图像坐标可直接用 YOLO 检测框中心填充（先运行一次流程拿到 detections），
# 机器人坐标在表格中手动输入。标定结果保存在 extraData（随项目）并可另存为
# .ncal 标定文件，供「无序抓取」插件使用。
#
# 本 DLL 导出 1 个工具：
#   npoint_calib  N点标定（分类：标定）
#
# 注意：calibdata.* 与 detectionlist.* 位于 examples/common，被本插件与
# 无序抓取插件共用。
# -----------------------------------------------------------------------------

include($$PWD/../../pluginsdk.pri)

TARGET   = npointcalib
TEMPLATE = lib

INCLUDEPATH += $$PWD/../common

SOURCES += \
    npointcalibplugin.cpp \
    npointcalibdialog.cpp \
    $$PWD/../common/calibdata.cpp \
    $$PWD/../common/detectionlist.cpp

HEADERS += \
    npointcalibplugin.h \
    npointcalibdialog.h \
    $$PWD/../common/calibdata.h \
    $$PWD/../common/detectionlist.h
