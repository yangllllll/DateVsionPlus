# Compile the real application sources with a Qt Test entry point.
include(../datevisionplus.pro)
TARGET = runtime_test
QT += testlib
CONFIG += console testcase
CONFIG -= app_bundle lrelease embed_translations
SOURCES -= main.cpp
for(source, SOURCES): TEST_SOURCES += $$clean_path($$PWD/../$$source)
for(header, HEADERS): TEST_HEADERS += $$clean_path($$PWD/../$$header)
for(form, FORMS): TEST_FORMS += $$clean_path($$PWD/../$$form)
for(resource, RESOURCES): TEST_RESOURCES += $$clean_path($$PWD/../$$resource)
SOURCES = $$TEST_SOURCES $$PWD/runtime_test.cpp
HEADERS = $$TEST_HEADERS
FORMS = $$TEST_FORMS
RESOURCES = $$TEST_RESOURCES
TRANSLATIONS =
