# -----------------------------------------------------------------------------
# 大华相机插件（Qt Plugin -> DLL）
#   对应 Python 版本: user_plugins/mv_camera.py 中的 MVCameraPlugin
#
# 本 DLL 导出 1 个工具：
#   mv_camera  大华相机（无输入端口，输出「采集图像」；双击节点打开预览对话框）
#
# 运行期依赖：
#   - 机器上需有大华 MVSDKmd.dll（安装大华 MV Viewer 即可）。
#     插件在运行期动态加载它，未安装也不会导致本插件 DLL 加载失败，
#     只会在枚举/取图时给出明确的错误提示。
#   - DLL 查找顺序：参数「MVSDK 路径」-> 环境变量 DAHUA_MVSDK_PATH
#     -> 主程序目录（含 plugins 子目录）-> MV Viewer 默认安装目录 -> 系统 PATH
#
# 构建与部署：
#   1) 用 Qt Creator 打开本文件，选择【与主程序完全相同】的 Qt 套件与构建配置
#   2) 构建（导入库由 pluginsdk.pri 自动链接）
#   3) 得到 dahuacamera.dll，输出到 <本目录>/bin/plugins
#   4) 把它拷贝到 DateVisionPlus.exe 所在目录的 plugins/ 子目录，
#      启动主程序即可在「输入输出」分类中看到「大华相机」
# -----------------------------------------------------------------------------

include($$PWD/../../pluginsdk.pri)

TARGET   = dahuacamera
TEMPLATE = lib

SOURCES += \
    dahuaimvsdk.cpp \
    mvcameradialog.cpp \
    mvcameraplugin.cpp

HEADERS += \
    dahuaimvsdk.h \
    mvcameradialog.h \
    mvcameraplugin.h

# 想让 DLL 直接生成到主程序可加载的位置，可在此覆盖（include 之后生效）：
# DESTDIR = <主程序构建目录>/release/plugins
