#-------------------------------------------------
#
# Project created by QtCreator 2012-09-19T12:06:21
#
#-------------------------------------------------

QT       += core gui

greaterThan(QT_MAJOR_VERSION, 4): QT += widgets

TARGET = paparazzi-flasher
TEMPLATE = app


SOURCES += main.cpp\
        mainwindow.cpp \
    upgrade/stm32mem.c \
    upgrade/dfu.c \
    dfu_manager.cpp

HEADERS  += mainwindow.h \
    upgrade/stm32mem.h \
    upgrade/dfu.h \
    dfu_manager.h

FORMS    += mainwindow.ui

RESOURCES += \
    main.qrc

macx {
    LIBS += -L/opt/local/lib
    INCLUDEPATH += /opt/local/include
}

LIBS += -lusb
