# -----------------------------------------------------------------------------
# 物料识别（检测区）插件（Qt Plugin -> DLL）
#
# 配合「YOLO目标识别」使用：
#   detections  <- YOLO 的 detections 输出（QVariantList<QVariantMap>）
#   zone_id     -> 存在物料的检测区中编号最小的那个（没有则为 -1）
#
# 检测区在弹窗右侧的图像画布上用鼠标拖拽建立（归一化坐标随图像缩放），
# 保存在 extraData 中随项目一起保存。
#
# 本 DLL 导出 1 个工具：
#   material_zone  物料识别(检测区)（分类：检测定位）
#
# 构建与部署：
#   1) 用 Qt Creator 打开本文件，选择【与主程序完全相同】的 Qt 套件与构建配置
#   2) 构建（OpenCV / 导入库由 pluginsdk.pri 自动配置）
#   3) 得到 materialzone.dll，输出到 <本目录>/bin/plugins
#   4) 把它拷到 DateVisionPlus.exe 所在目录的 plugins/ 子目录
# -----------------------------------------------------------------------------

include($$PWD/../../pluginsdk.pri)

TARGET   = materialzone
TEMPLATE = lib

SOURCES += \
    materialzoneplugin.cpp \
    materialzonedialog.cpp

HEADERS += \
    materialzoneplugin.h \
    materialzonedialog.h

# 想让 DLL 直接生成到主程序可加载的位置，可在此覆盖（include 之后生效）：
# DESTDIR = <主程序构建目录>/release/plugins
