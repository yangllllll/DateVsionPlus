QT += widgets testlib
CONFIG += testcase c++17 console
CONFIG -= app_bundle
TEMPLATE = app
TARGET = outputpanel_test
DEFINES += OPENVISIONCORE_LIBRARY
INCLUDEPATH += $$PWD/..
include($$PWD/../3rdparty/opencv.pri)

SOURCES += \
    $$PWD/outputpanel_test.cpp \
    $$PWD/../panels/outputpanel.cpp \
    $$PWD/../core/plugintypes.cpp

HEADERS += \
    $$PWD/../panels/outputpanel.h \
    $$PWD/../core/plugintypes.h \
    $$PWD/../core/coreglobal.h

FORMS += $$PWD/../panels/outputpanel.ui
