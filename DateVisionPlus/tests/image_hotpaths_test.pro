QT += core gui widgets
CONFIG += console c++17 release
CONFIG -= app_bundle debug
TEMPLATE = app
TARGET = image_hotpaths_test
DEFINES += OPENVISIONCORE_STATIC
INCLUDEPATH += $$PWD/..
include($$PWD/../3rdparty/opencv.pri)

SOURCES += \
    image_hotpaths_test.cpp \
    ../core/cvutils.cpp \
    ../core/pluginbase.cpp \
    ../core/plugintypes.cpp \
    ../plugins/blobanalysisplugin.cpp \
    ../plugins/measureplugin.cpp

HEADERS += \
    ../core/pluginbase.h \
    ../plugins/blobanalysisplugin.h \
    ../plugins/measureplugin.h
