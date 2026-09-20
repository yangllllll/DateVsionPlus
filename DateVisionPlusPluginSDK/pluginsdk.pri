# -----------------------------------------------------------------------------
# DateVisionPlus 用户插件 SDK —— 只需在插件工程 .pro 中加入一行：
#
#     include(<SDK目录>/pluginsdk.pri)
#
# 本文件自动完成：
#   1) 引入 SDK 头文件（插件基类 / 端口参数定义 / 插件接口 / OpenCV 工具）
#   2) 引入与主工程一致的 OpenCV 配置
#   3) 设置 Qt Plugin 编译选项（TEMPLATE=lib 需在自己的 .pro 中另行指定）
#   4) 自动链接主程序导出的导入库 DateVisionPlus.lib（MSVC 需要）
#
# 硬性要求：插件 DLL 必须与主程序使用
#   相同的 Qt 版本 + 相同的编译器套件 + 相同的构建配置(Debug/Release)
# -----------------------------------------------------------------------------
OVPP_SDK_ROOT = $$PWD

# ---- SDK 头文件 ----
INCLUDEPATH += $$OVPP_SDK_ROOT/include

# ---- OpenCV（与主工程一致；可用环境变量 OPENCV_DIR 覆盖路径）----
include($$OVPP_SDK_ROOT/3rdparty/opencv.pri)

# ---- Qt 模块（QColor 等类型来自 QtGui）----
QT += gui widgets

CONFIG += c++17

# ---- Qt Plugin 编译选项 ----
CONFIG += plugin

# ---- 主程序导出的导入库（MSVC 必须，MinGW 通常不需要）----
#
# 把主程序构建生成的 DateVisionPlus.lib 放到下面任一位置即可自动链接：
#   1) <插件工程>/DateVisionPlus.lib
#   2) <插件工程>/lib/DateVisionPlus*.lib
#   3) <SDK目录>/lib/debug/ 或 <SDK目录>/lib/release/（按当前构建配置自动选取）
#   4) <SDK目录>/lib/DateVisionPlus*.lib
#   5) 环境变量 DateVisionPlus_LIBDIR 指向的目录
#
# 注意：主程序用 Debug 构建就拷 Debug 版 lib，插件也必须用 Debug 构建，
#      配置不一致会出现 LNK2038 等不匹配错误。
# -----------------------------------------------------------------------------
# 注意：$$PWD 是本 .pri 所在目录，插件工程目录要用 $$_PRO_FILE_PWD_
OPVP_PROJECT_DIR = $$_PRO_FILE_PWD_

# 按当前构建配置选取 SDK 自带的 lib（debug / release 分开存放）
CONFIG(debug, debug|release) {
    OPVP_SDK_LIB = $$files($$OVPP_SDK_ROOT/lib/debug/DateVisionPlus*.lib)
} else {
    OPVP_SDK_LIB = $$files($$OVPP_SDK_ROOT/lib/release/DateVisionPlus*.lib)
}
isEmpty(OPVP_SDK_LIB): OPVP_SDK_LIB = $$files($$OVPP_SDK_ROOT/lib/DateVisionPlus*.lib)

OPVP_LOCAL_LIBS = $$files($${OPVP_PROJECT_DIR}/DateVisionPlus.lib)
isEmpty(OPVP_LOCAL_LIBS): OPVP_LOCAL_LIBS = $$files($${OPVP_PROJECT_DIR}/lib/DateVisionPlus*.lib)
isEmpty(OPVP_LOCAL_LIBS): OPVP_LOCAL_LIBS = $$OPVP_SDK_LIB

OPVP_LIBDIR = $$(DateVisionPlus_LIBDIR)

!isEmpty(OPVP_LOCAL_LIBS) {
    OPVP_LIB_FILE = $$first(OPVP_LOCAL_LIBS)
    LIBS += $$shell_path($${OPVP_LIB_FILE})
    message([DateVisionPlus SDK] 导入库: $${OPVP_LIB_FILE})
} else:!isEmpty(OPVP_LIBDIR) {
    LIBS += -L$${OPVP_LIBDIR} -lDateVisionPlus
    message([DateVisionPlus SDK] 导入库: $${OPVP_LIBDIR}/DateVisionPlus.lib)
} else {
    win32-msvc {
        message("================================================================")
        message(" [DateVisionPlus SDK] 未找到主程序导入库 DateVisionPlus.lib")
        message(" 请把它拷贝到插件工程目录 $${OPVP_PROJECT_DIR}，或放入 SDK 的 lib/ 目录，")
        message(" 也可设置环境变量 DateVisionPlus_LIBDIR 指向其所在目录。")
        message("================================================================")
    }
}

# ---- 插件 DLL 默认输出到 <插件工程>/bin/plugins ----
# 想直接输出到主程序可加载的位置，可在自己的 .pro 中于 include 之后覆盖：
#     DESTDIR = <主程序构建目录>/debug/plugins
DESTDIR = $${OPVP_PROJECT_DIR}/bin/plugins
