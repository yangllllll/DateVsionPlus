QT = core testlib
CONFIG += console testcase c++17
CONFIG -= app_bundle
TEMPLATE = app
TARGET = camera_tests

include($$PWD/../../../3rdparty/opencv.pri)
INCLUDEPATH += $$PWD/.. $$PWD/../../dahuacamera
SOURCES += $$PWD/camera_tests.cpp \
           $$PWD/../uvccapture.cpp \
           $$PWD/../../dahuacamera/dahuaimvsdk.cpp
HEADERS += $$PWD/../uvccapture.h $$PWD/../../dahuacamera/dahuaimvsdk.h

win32-msvc: QMAKE_CXXFLAGS += /utf-8
