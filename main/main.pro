QT       += core gui network

greaterThan(QT_MAJOR_VERSION, 4): QT += widgets

CONFIG += c++17

# You can make your code fail to compile if it uses deprecated APIs.
# In order to do so, uncomment the following line.
#DEFINES += QT_DISABLE_DEPRECATED_BEFORE=0x060000    # disables all the APIs deprecated before Qt 6.0.0

SOURCES += \
    Utility/DumpUtil.cpp \
    Utility/IcrCriticalSection.cpp \
    Utility/ImCharset.cpp \
    Utility/ImPath.cpp \
    Utility/LogBuffer.cpp \
    Utility/LogUtil.cpp \
    datamodel.cpp \
    fixtimebuyer.cpp \
    goodsavailabilitychecker.cpp \
    goodsbuyer.cpp \
    httpthread.cpp \
    main.cpp \
    mainwindow.cpp \
    multiselectiondialog.cpp \
    plandialog.cpp \
    planitemwidget.cpp \
    planmanager.cpp \
    planrunner.cpp \
    settingmanager.cpp \
    uiutil.cpp \
    userinfomanager.cpp

HEADERS += \
    Utility/DumpUtil.h \
    Utility/IcrCriticalSection.h \
    Utility/ImCharset.h \
    Utility/ImPath.h \
    Utility/LogBuffer.h \
    Utility/LogMacro.h \
    Utility/LogUtil.h \
    datamodel.h \
    fixtimebuyer.h \
    goodsavailabilitychecker.h \
    goodsbuyer.h \
    httpthread.h \
    mainwindow.h \
    multiselectiondialog.h \
    plandialog.h \
    planitemwidget.h \
    planmanager.h \
    planrunner.h \
    settingmanager.h \
    uiutil.h \
    userinfomanager.h

FORMS += \
    mainwindow.ui \
    multiselectiondialog.ui \
    plandialog.ui \
    planitemwidget.ui

# Enable PDB generation
QMAKE_CFLAGS_RELEASE += /Zi
QMAKE_CXXFLAGS_RELEASE += /Zi
QMAKE_LFLAGS_RELEASE += /DEBUG

# Enable log context
DEFINES += QT_MESSAGELOGCONTEXT

# QXlsx code for Application Qt project
include(../QXlsx/QXlsx.pri)
INCLUDEPATH += ../QXlsx/header/

# curl
INCLUDEPATH += ../curl/include
LIBS += -L"$$_PRO_FILE_PWD_/../curl/lib" -llibcurl

# Cryptopp
INCLUDEPATH += ../Cryptopp/include
CONFIG(debug, debug|release) {
    LIBS += -L"$$_PRO_FILE_PWD_/../Cryptopp/lib/Debug" -lcryptlib
} else {
    LIBS += -L"$$_PRO_FILE_PWD_/../Cryptopp/lib/Release" -lcryptlib
}
