# ---------------------------------------------------------------
# OpenVisionPlus - 工业视觉检测平台 (Qt6 + C++ + OpenCV)
# 标准 Qt Creator qmake 工程结构
# ---------------------------------------------------------------

QT += widgets network

TARGET = DateVisionPlus
TEMPLATE = app

CONFIG += c++17

# 主程序导出核心库符号，供用户插件 DLL（Qt Plugin）链接
DEFINES += DATEVISIONCORE_LIBRARY

# 工程头文件根目录（uic 生成的 #include "panels/xxx.h" 依赖此项）
INCLUDEPATH += $$PWD

# OpenCV 依赖配置（见 3rdparty/opencv.pri）
include($$PWD/3rdparty/opencv.pri)

SOURCES += \
    main.cpp \
    core/cvutils.cpp \
    core/expressionevaluator.cpp \
    core/pluginbase.cpp \
    core/pluginmanager.cpp \
    core/plugintypes.cpp \
    communication/tcpserver.cpp \
    dialogs/linefinderdialog.cpp \
    dialogs/patternmatchdialog.cpp \
    dialogs/roigraphics.cpp \
    flowchart/connectionitem.cpp \
    flowchart/executionengine.cpp \
    flowchart/flowscene.cpp \
    flowchart/flowview.cpp \
    flowchart/nodeitem.cpp \
    flowchart/portitem.cpp \
    mainwindow.cpp \
    panels/communicationpanel.cpp \
    panels/imageviewer.cpp \
    panels/outputpanel.cpp \
    panels/plugintreewidget.cpp \
    panels/previewpanel.cpp \
    panels/propertiespanel.cpp \
    panels/toolboxpanel.cpp \
    plugins/blobanalysisplugin.cpp \
    plugins/builtinregistry.cpp \
    plugins/edgedetectionplugin.cpp \
    plugins/grayscaleplugin.cpp \
    plugins/imagefilterplugin.cpp \
    plugins/imageoutputplugin.cpp \
    plugins/imagesourceplugin.cpp \
    plugins/linedistanceplugin.cpp \
    plugins/linefinderplugin.cpp \
    plugins/measureplugin.cpp \
    plugins/morphologyplugin.cpp \
    plugins/patternmatchplugin.cpp \
    plugins/thresholdplugin.cpp \
    widgets/sliderwithvalue.cpp

HEADERS += \
    core/coreglobal.h \
    core/cvutils.h \
    core/opencvcompat.h \
    core/expressionevaluator.h \
    core/pluginbase.h \
    core/plugininterface.h \
    core/pluginmanager.h \
    core/plugintypes.h \
    communication/tcpserver.h \
    dialogs/linefinderdialog.h \
    dialogs/patternmatchdialog.h \
    dialogs/roigraphics.h \
    flowchart/connectionitem.h \
    flowchart/executionengine.h \
    flowchart/flowscene.h \
    flowchart/flowview.h \
    flowchart/nodeitem.h \
    flowchart/portitem.h \
    mainwindow.h \
    panels/communicationpanel.h \
    panels/imageviewer.h \
    panels/outputpanel.h \
    panels/plugintreewidget.h \
    panels/previewpanel.h \
    panels/propertiespanel.h \
    panels/toolboxpanel.h \
    plugins/blobanalysisplugin.h \
    plugins/builtinregistry.h \
    plugins/edgedetectionplugin.h \
    plugins/grayscaleplugin.h \
    plugins/imagefilterplugin.h \
    plugins/imageoutputplugin.h \
    plugins/imagesourceplugin.h \
    plugins/linedistanceplugin.h \
    plugins/linefinderplugin.h \
    plugins/measureplugin.h \
    plugins/morphologyplugin.h \
    plugins/patternmatchplugin.h \
    plugins/thresholdplugin.h \
    widgets/sliderwithvalue.h

FORMS += \
    dialogs/linefinderdialog.ui \
    dialogs/patternmatchdialog.ui \
    mainwindow.ui \
    panels/communicationpanel.ui \
    panels/outputpanel.ui \
    panels/previewpanel.ui \
    panels/propertiespanel.ui \
    panels/toolboxpanel.ui

RESOURCES += \
    resources/resources.qrc

TRANSLATIONS += \
    openvisionplus_zh_CN.ts

CONFIG += lrelease
CONFIG += embed_translations

# Default rules for deployment.
qnx: target.path = /tmp/$${TARGET}/bin
else: unix:!android: target.path = /opt/$${TARGET}/bin
!isEmpty(target.path): INSTALLS += target
