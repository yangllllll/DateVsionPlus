# -----------------------------------------------------------------------------
# PLC 传值插件（Qt Plugin -> DLL）
#   对应 Python 版本: user_plugins/plc.py 中的 PLC_SendPlugin（snap7 通讯）
#
# 本 DLL 导出 1 个工具：
#   plc_send  PLC传值
#     输入：input（布尔）
#     输出：status（布尔，PLC 连接状态）
#     参数：PLC地址 / 机架号 / 插槽号 / 区域 / DB块号 / 字节地址 / 执行后断开
#
# 默认行为与 Python 版一致：连接 snap7 -> 向 M 区第 100 字节写 1（OK）/0（NG）
# -> 立即断开；取消「执行后断开」可复用连接，连续运行时更快。
#
# snap7 依赖（本仓库已自带，无需联网下载）：
#   snap7-full-1.4.2/release/Windows/Win64/snap7.dll   （64 位）
#   snap7-full-1.4.2/release/Windows/Win32/snap7.dll   （32 位）
# 插件在**运行期**用 QLibrary 动态加载 snap7.dll，编译期不需要头文件/导入库，
# 因此 MSVC 与 MinGW 套件都能构建；找不到 DLL 时插件仍可加载，只在连接时报错。
#
# 构建与部署：
#   1) 用 Qt Creator 打开本文件，选择【与主程序完全相同】的 Qt 套件与构建配置
#   2) 构建（导入库由 pluginsdk.pri 自动链接）
#   3) 得到 plc.dll，输出到 <本目录>/bin/plugins
#   4) 把 plc.dll 与 snap7.dll（与主程序同为 64 位）一起拷贝到
#      DateVisionPlus.exe 所在目录的 plugins/ 子目录，
#      启动主程序即可在「PLC控制」分类中看到「PLC传值」
#
# DLL 查找顺序：环境变量 SNAP7_PATH -> 主程序目录 -> 主程序目录/plugins
#               -> 主程序目录/snap7 -> 系统 PATH
# -----------------------------------------------------------------------------

include($$PWD/../../pluginsdk.pri)

TARGET   = plc
TEMPLATE = lib

SOURCES += \
    plcplugin.cpp \
    plcdialog.cpp \
    snap7client.cpp

HEADERS += \
    plcplugin.h \
    plcdialog.h \
    snap7client.h

# 想让 DLL 直接生成到主程序可加载的位置，可在此覆盖（include 之后生效）：
# DESTDIR = <主程序构建目录>/release/plugins

# 可选：构建后自动把 snap7.dll 拷到插件输出目录（与主程序位数一致）
# SNAP7_ARCH = Win64      # 32 位套件改为 Win32
# SNAP7_DLL  = $$PWD/../../snap7-full-1.4.2/release/Windows/$${SNAP7_ARCH}/snap7.dll
# exists($$SNAP7_DLL) {
#     QMAKE_POST_LINK = $$QMAKE_COPY $$shell_path($$SNAP7_DLL) $$shell_path($$OUT_PWD/) $$escape_expand(\\n\\t) $$QMAKE_POST_LINK
#     message([snap7] 构建后自动拷贝 $$SNAP7_DLL -> $$OUT_PWD)
# }
