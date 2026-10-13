QT += core gui widgets network testlib
CONFIG += console testcase c++17
CONFIG -= app_bundle
TEMPLATE = app
TARGET = websocketserver_tests
DEFINES += OPENVISIONCORE_STATIC

SDK_ROOT = $$clean_path($$PWD/../../..)
HOST_CORE = $$clean_path($$SDK_ROOT/../DateVisionPlus/core)
INCLUDEPATH += $$SDK_ROOT/include $$PWD/..
include($$SDK_ROOT/3rdparty/opencv.pri)
# Tests use the Qt/OpenCV runtime on PATH; do not copy all SDK DLLs.
QMAKE_POST_LINK =

SOURCES += \
    $$PWD/websocketserver_tests.cpp \
    $$PWD/../websocketserverplugin.cpp \
    $$HOST_CORE/pluginbase.cpp \
    $$HOST_CORE/plugintypes.cpp

HEADERS += \
    $$PWD/../websocketserverplugin.h \
    $$SDK_ROOT/include/pluginbase.h
