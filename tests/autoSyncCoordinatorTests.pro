QT += core testlib xml
QT -= gui
CONFIG += console testcase c++11
CONFIG -= app_bundle
TEMPLATE = app
TARGET = autoSyncCoordinatorTests
INCLUDEPATH += ..
DEFINES += TEST_SOURCE_DIR=\\\"$$PWD/..\\\"
SOURCES += autoSyncCoordinatorTests.cpp ../autoSyncCoordinator.cpp ../patchTransferCodec.cpp ../quickSettingCodec.cpp
HEADERS += ../autoSyncCoordinator.h
