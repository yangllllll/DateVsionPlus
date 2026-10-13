QT += core gui testlib
QT -= widgets

TEMPLATE = app
TARGET = executionengine_test
CONFIG += console testcase c++17
CONFIG -= app_bundle
DEFINES += OPENVISIONCORE_LIBRARY

INCLUDEPATH += $$PWD/..
include($$PWD/../3rdparty/opencv.pri)

SOURCES += \
    executionengine_test.cpp \
    ../core/pluginbase.cpp \
    ../flowchart/executionengine.cpp

HEADERS += \
    ../core/coreglobal.h \
    ../core/plugintypes.h \
    ../core/pluginbase.h \
    ../flowchart/executionengine.h
