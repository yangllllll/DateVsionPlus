QT += core gui testlib
QT -= widgets
CONFIG += testcase c++17 console
CONFIG -= app_bundle
TEMPLATE = app
TARGET = executionworker_test
DEFINES += OPENVISIONCORE_LIBRARY
INCLUDEPATH += $$PWD/..
include($$PWD/../3rdparty/opencv.pri)

SOURCES += \
    $$PWD/executionworker_test.cpp \
    $$PWD/../core/pluginbase.cpp \
    $$PWD/../flowchart/executionengine.cpp \
    $$PWD/../flowchart/executionworker.cpp

HEADERS += \
    $$PWD/../core/coreglobal.h \
    $$PWD/../core/plugintypes.h \
    $$PWD/../core/pluginbase.h \
    $$PWD/../flowchart/executionengine.h \
    $$PWD/../flowchart/executionworker.h
