# -----------------------------------------------------------------------------
# OpenCV 依赖配置（同时支持 OpenCV 4.x 与 5.x）
#
# 推荐方式：设置环境变量 OPENCV_DIR，例如  OPENCV_DIR = D:\opencv\build
# 未设置时按下述顺序自动探测：D:/opencv/build -> C:/opencv/build -> D:/opencv
#
# OPENCV_LIB_NAME 可手动指定（如 opencv_world500）；留空时自动探测已安装的 world 库。
# 若使用分模块编译的 OpenCV，请在下方 LIBS 中逐个添加所需模块。
#
# 重要：MinGW 工具链只能链接 MinGW 版 OpenCV（*.a / *.dll.a），
#       MSVC 编译的 *.lib 无法被 MinGW 直接链接（需改用 MSVC 套件）。
# -----------------------------------------------------------------------------

OPENCV_SDK = $$(OPENCV_DIR)
isEmpty(OPENCV_SDK): exists(D:/opencv/build/include): OPENCV_SDK = D:/opencv/build
isEmpty(OPENCV_SDK): exists(C:/opencv/build/include): OPENCV_SDK = C:/opencv/build
isEmpty(OPENCV_SDK): exists(D:/opencv/include): OPENCV_SDK = D:/opencv
isEmpty(OPENCV_SDK): OPENCV_SDK = D:/opencv/build

OPENCV_LIB_NAME = $$(OPENCV_LIB_NAME)

# ---- 头文件目录（兼容 include 与 include/opencv4 两种布局）----
OPENCV_INCLUDE = $${OPENCV_SDK}/include
!exists($${OPENCV_INCLUDE}/opencv2): exists($${OPENCV_INCLUDE}/opencv4): OPENCV_INCLUDE = $${OPENCV_INCLUDE}/opencv4
INCLUDEPATH += $${OPENCV_INCLUDE}

win32 {
    # ---- 库目录：MinGW 与 MSVC 分别探测 ----
    win32-g++ {
        OPENCV_LIB_DIR = $${OPENCV_SDK}/x64/mingw/lib
        !exists($${OPENCV_LIB_DIR}): OPENCV_LIB_DIR = $${OPENCV_SDK}/x64/MinGW/lib
        !exists($${OPENCV_LIB_DIR}): OPENCV_LIB_DIR = $${OPENCV_SDK}/lib
    } else {
        OPENCV_LIB_DIR = $${OPENCV_SDK}/x64/vc17/lib
        !exists($${OPENCV_LIB_DIR}): OPENCV_LIB_DIR = $${OPENCV_SDK}/x64/vc16/lib
        !exists($${OPENCV_LIB_DIR}): OPENCV_LIB_DIR = $${OPENCV_SDK}/x64/vc15/lib
        !exists($${OPENCV_LIB_DIR}): OPENCV_LIB_DIR = $${OPENCV_SDK}/lib
    }

    # ---- 自动探测 world 库版本（500/490/480/470/460 ...）----
    isEmpty(OPENCV_LIB_NAME): exists($${OPENCV_LIB_DIR}/opencv_world500.lib): OPENCV_LIB_NAME = opencv_world500
    isEmpty(OPENCV_LIB_NAME): exists($${OPENCV_LIB_DIR}/opencv_world490.lib): OPENCV_LIB_NAME = opencv_world490
    isEmpty(OPENCV_LIB_NAME): exists($${OPENCV_LIB_DIR}/opencv_world480.lib): OPENCV_LIB_NAME = opencv_world480
    isEmpty(OPENCV_LIB_NAME): exists($${OPENCV_LIB_DIR}/opencv_world470.lib): OPENCV_LIB_NAME = opencv_world470
    isEmpty(OPENCV_LIB_NAME): exists($${OPENCV_LIB_DIR}/opencv_world460.lib): OPENCV_LIB_NAME = opencv_world460
    isEmpty(OPENCV_LIB_NAME): OPENCV_LIB_NAME = opencv_world480

    LIBS += -L$${OPENCV_LIB_DIR}

    CONFIG(debug, debug|release) {
        LIBS += -l$${OPENCV_LIB_NAME}d
    } else {
        LIBS += -l$${OPENCV_LIB_NAME}
    }

    message([OpenCV] SDK = $${OPENCV_SDK})
    message([OpenCV] include = $${OPENCV_INCLUDE})
    message([OpenCV] lib dir = $${OPENCV_LIB_DIR})
    message([OpenCV] lib name = $${OPENCV_LIB_NAME})

    # ---- 运行期 DLL：构建后自动拷贝到可执行文件所在目录 ----
    #     （否则运行时会报 "找不到 opencv_world500d.dll"）
    OPENCV_BIN_DIR = $${OPENCV_SDK}/x64/vc17/bin
    !exists($${OPENCV_BIN_DIR}): OPENCV_BIN_DIR = $${OPENCV_SDK}/x64/vc16/bin
    !exists($${OPENCV_BIN_DIR}): OPENCV_BIN_DIR = $${OPENCV_SDK}/x64/vc15/bin
    !exists($${OPENCV_BIN_DIR}): OPENCV_BIN_DIR = $${OPENCV_SDK}/x64/mingw/bin
    !exists($${OPENCV_BIN_DIR}): OPENCV_BIN_DIR = $${OPENCV_SDK}/bin

    exists($${OPENCV_BIN_DIR}) {
        win32-g++ {
            # MinGW 的可执行文件直接生成在构建根目录
            OPENCV_EXE_DIR = $$OUT_PWD
        } else {
            # MSVC 的可执行文件生成在 debug/ 或 release/ 子目录
            CONFIG(debug, debug|release) {
                OPENCV_EXE_DIR = $$OUT_PWD/debug
            } else {
                OPENCV_EXE_DIR = $$OUT_PWD/release
            }
        }

        QMAKE_POST_LINK += $$QMAKE_COPY $$shell_path($${OPENCV_BIN_DIR}/opencv_*.dll) $$shell_path($${OPENCV_EXE_DIR}/)
        message([OpenCV] 构建后自动拷贝 opencv_*.dll -> $${OPENCV_EXE_DIR})
    } else {
        message("================================================================")
        message(" [OpenCV] 未找到 DLL 目录，运行时需手动把 OpenCV 的 bin 加入 PATH")
        message("================================================================")
    }

    # ---- 环境自检：把问题在 qmake 阶段就说清楚 ----
    !exists($${OPENCV_INCLUDE}/opencv2) {
        message("================================================================")
        message(" [OpenCV] 未找到头文件目录: $${OPENCV_INCLUDE}/opencv2")
        message(" 请设置环境变量 OPENCV_DIR 指向形如 <opencv>/build 的目录")
        message("================================================================")
    }
    !exists($${OPENCV_LIB_DIR}) {
        message("================================================================")
        message(" [OpenCV] 未找到库目录: $${OPENCV_LIB_DIR}")
        message(" 请检查 OPENCV_DIR 是否指向正确的构建目录")
        message("================================================================")
    }
    win32-g++ {
        OPENCV_MINGW_LIBS = $$files($${OPENCV_LIB_DIR}/*opencv_world*.a)
        isEmpty(OPENCV_MINGW_LIBS) {
            message("================================================================")
            message(" [OpenCV] MinGW 工具链未在 $${OPENCV_LIB_DIR} 找到导入库 (*.a / *.dll.a)")
            message(" MSVC 编译的 *.lib 不能被 MinGW 链接，请选择其一：")
            message("   1) 改用 Qt 的 MSVC 2019/2022 64-bit 套件（推荐）；")
            message("   2) 安装 MinGW 版 OpenCV（MSYS2: pacman -S mingw-w64-x86_64-opencv）；")
            message("   3) 用 MinGW 自行编译 OpenCV 源码。")
            message("================================================================")
        }
    }
}

unix:!macx {
    CONFIG += link_pkgconfig
    PKGCONFIG += opencv4
}

macx {
    INCLUDEPATH += /opt/homebrew/opt/opencv/include/opencv4 \
                   /usr/local/opt/opencv/include/opencv4
    LIBS += -L/opt/homebrew/opt/opencv/lib -L/usr/local/opt/opencv/lib \
            -lopencv_core -lopencv_imgproc -lopencv_imgcodecs -lopencv_highgui
}
