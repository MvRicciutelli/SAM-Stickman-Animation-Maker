QT       += core gui

greaterThan(QT_MAJOR_VERSION, 4): QT += widgets

# network: the startup update check (updatecheck.cpp)
QT += network
# core-private: QZipReader, to read Krita .bundle files (kritabundle.cpp)
QT += core-private

CONFIG += c++11

VERSION = 0.9.0
# one source of truth for the version: the About box and the update check read APP_VERSION
DEFINES += APP_VERSION=\\\"$$VERSION\\\"
QMAKE_TARGET_COMPANY = "Matteo Vittorio Ricciutelli"
QMAKE_TARGET_PRODUCT = "SAM - Stickman Animation Maker"
QMAKE_TARGET_DESCRIPTION = "SAM - Stickman Animation Maker"
QMAKE_TARGET_COPYRIGHT = "Copyright (C) 2026 Matteo Vittorio Ricciutelli"

# The following define makes your compiler emit warnings if you use
# any Qt feature that has been marked deprecated (the exact warnings
# depend on your compiler). Please consult the documentation of the
# deprecated API in order to know how to port your code away from it.
DEFINES += QT_DEPRECATED_WARNINGS

# You can also make your code fail to compile if it uses deprecated APIs.
# In order to do so, uncomment the following line.
# You can also select to disable deprecated APIs only up to a certain version of Qt.
#DEFINES += QT_DISABLE_DEPRECATED_BEFORE=0x060000    # disables all the APIs deprecated before Qt 6.0.0

# sources in src/, Designer forms in ui/: the repo root stays short so the
# README shows near the top of the GitHub page
INCLUDEPATH += $$PWD/src

SOURCES += \
    src/StickFigure.cpp \
    src/abrbrush.cpp \
    src/advancedcirclewidget.cpp \
    src/advancedlinewidget.cpp \
    src/advancedtab.cpp \
    src/advancedtaperwidget.cpp \
    src/animation.cpp \
    src/animationoptions.cpp \
    src/brushpicker.cpp \
    src/ffmpegexport.cpp \
    src/frame.cpp \
    src/framedata.cpp \
    src/framepresenter.cpp \
    src/gradientdialog.cpp \
    src/imageexport.cpp \
    src/kritabundle.cpp \
    src/main.cpp \
    src/mfvideo.cpp \
    src/mymainwindow.cpp \
    src/myview.cpp \
    src/patternlibrary.cpp \
    src/sleeper.cpp \
    src/stick.cpp \
    src/stickdata.cpp \
    src/stickfiguredata.cpp \
    src/stkformat.cpp \
    src/textureoptionsdialog.cpp \
    src/updatecheck.cpp \
    src/widget.cpp

HEADERS += \
    src/StickFigure.h \
    src/abrbrush.h \
    src/advancedcirclewidget.h \
    src/advancedlinewidget.h \
    src/advancedtab.h \
    src/advancedtaperwidget.h \
    src/animation.h \
    src/animationoptions.h \
    src/brushpicker.h \
    src/ffmpegexport.h \
    src/frame.h \
    src/framedata.h \
    src/framepresenter.h \
    src/gradientdialog.h \
    src/imageexport.h \
    src/kritabundle.h \
    src/mfvideo.h \
    src/mymainwindow.h \
    src/myview.h \
    src/patternlibrary.h \
    src/sleeper.h \
    src/stick.h \
    src/stickdata.h \
    src/stickfiguredata.h \
    src/stkformat.h \
    src/style.h \
    src/textureoptionsdialog.h \
    src/updatecheck.h \
    src/uiItems.h \
    src/widget.h

FORMS += \
    ui/advancedcirclewidget.ui \
    ui/advancedlinewidget.ui \
    ui/advancedtab.ui \
    ui/advancedtaperwidget.ui \
    ui/animationoptions.ui \
    ui/ffmpegexport.ui \
    ui/mymainwindow.ui \
    ui/widget.ui

# Default rules for deployment.
qnx: target.path = /tmp/$${TARGET}/bin
else: unix:!android: target.path = /opt/$${TARGET}/bin
!isEmpty(target.path): INSTALLS += target

RESOURCES += \
    myres.qrc

DISTFILES += \
    resources/Add_multiline_00003.png \
    resources/add_poly_00001.png \
    resources/add_poly_00002.png \
    resources/add_poly_00003.png
# Media Foundation: encoder H.264 fornito da Windows, usato da mfvideo.cpp per l'mp4
# senza ffmpeg (mfuuid porta i GUID MF_MT_*/MFVideoFormat_*)
win32: LIBS += -lmfplat -lmfreadwrite -lmfuuid -lole32

RC_ICONS = resources/samIcon.ico
