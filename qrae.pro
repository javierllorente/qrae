#-------------------------------------------------
#
# Project created by QtCreator 2012-04-02T00:24:13
#
#-------------------------------------------------

QT += core gui webkit sql

greaterThan(QT_MAJOR_VERSION, 4) {
    QT += webkitwidgets
}

TARGET = qrae
TEMPLATE = app

VERSION = 0.4.99
DEFINES *= QRAE_VERSION=\\\"""$$VERSION"\\\""

win32: RC_ICONS = qrae.ico
osx: ICON = qrae.icns


SOURCES += main.cpp\
        mainwindow.cpp\
        libqrae.cpp \
    proxysettings.cpp \
    settings.cpp \
    history.cpp

HEADERS  += mainwindow.h\
         libqrae.h \
    proxysettings.h \
    settings.h \
    history.h

FORMS    += mainwindow.ui \
    settings.ui

OTHER_FILES +=

RESOURCES = application.qrc
