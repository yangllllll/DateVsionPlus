# -----------------------------------------------------------------------------
# OpenVisionPlus 用户插件示例（Qt Plugin -> DLL）
#   对应 Python 版本: user_plugins/example_plugins.py
#
# 本 DLL 导出 4 个工具：
#   user_flip    图像翻转（带专用对话框，双击节点打开）
#   user_invert  图像反色
#   user_number  数字比较
#   user_and     逻辑与
#
# 构建与部署：
#   1) 用 Qt Creator 打开本文件，选择【与主程序完全相同】的 Qt 套件与构建配置
#   2) 构建（导入库已随 SDK 提供于 <SDK>/lib/debug 与 <SDK>/lib/release，自动链接）
#      若提示找不到导入库，请把主程序构建生成的 OpenVisionPlus.lib 拷到本目录
#   3) 构建得到 userpluginsample.dll，输出到 <本目录>/bin/plugins
#   4) 把 DLL 拷贝到 OpenVisionPlus.exe 所在目录的 plugins/ 子目录
#      （Qt Creator 运行主程序时，通常是 build-OpenVisionPlus-*/debug|release/plugins/）
#   5) 启动主程序：工具箱出现「用户插件」「数学运算」两个分类
#      "输出"面板会打印 DLL 加载日志；菜单 工具 -> 刷新插件 可热重载
# -----------------------------------------------------------------------------

include($$PWD/../../pluginsdk.pri)

TARGET  = userpluginsample
TEMPLATE = lib

SOURCES += \
    flipdialog.cpp \
    sampleplugins.cpp

HEADERS += \
    flipdialog.h \
    sampleplugins.h

FORMS += \
    flipdialog.ui

# 想让 DLL 直接生成到主程序可加载的位置，可在此覆盖（include 之后生效）：
# DESTDIR = <主程序构建目录>/debug/plugins
