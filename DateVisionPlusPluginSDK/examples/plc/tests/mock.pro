TEMPLATE = lib
CONFIG += shared c++17
CONFIG -= qt debug_and_release
TARGET = snap7_mock
DESTDIR = $$OUT_PWD/bin
OBJECTS_DIR = $$OUT_PWD/mock-obj
SOURCES += mock.cpp
HEADERS += mock.h
