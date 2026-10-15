QT       += core gui

greaterThan(QT_MAJOR_VERSION, 4): QT += widgets

CONFIG += c++11

VERSION = 0.9.0
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

SOURCES += \
    StickFigure.cpp \
    abrbrush.cpp \
    advancedcirclewidget.cpp \
    advancedlinewidget.cpp \
    advancedtab.cpp \
    advancedtaperwidget.cpp \
    animation.cpp \
    animationoptions.cpp \
    brushpicker.cpp \
    ffmpegexport.cpp \
    frame.cpp \
    framedata.cpp \
    framepresenter.cpp \
    gradientdialog.cpp \
    imageexport.cpp \
    main.cpp \
    mfvideo.cpp \
    mymainwindow.cpp \
    myview.cpp \
    patternlibrary.cpp \
    sleeper.cpp \
    stick.cpp \
    stickdata.cpp \
    stickfiguredata.cpp \
    stkformat.cpp \
    textureoptionsdialog.cpp \
    widget.cpp

HEADERS += \
    StickFigure.h \
    abrbrush.h \
    advancedcirclewidget.h \
    advancedlinewidget.h \
    advancedtab.h \
    advancedtaperwidget.h \
    animation.h \
    animationoptions.h \
    brushpicker.h \
    ffmpegexport.h \
    frame.h \
    framedata.h \
    framepresenter.h \
    gradientdialog.h \
    imageexport.h \
    mfvideo.h \
    mymainwindow.h \
    myview.h \
    patternlibrary.h \
    sleeper.h \
    stick.h \
    stickdata.h \
    stickfiguredata.h \
    stkformat.h \
    style.h \
    textureoptionsdialog.h \
    uiItems.h \
    widget.h

FORMS += \
    advancedcirclewidget.ui \
    advancedlinewidget.ui \
    advancedtab.ui \
    advancedtaperwidget.ui \
    animationoptions.ui \
    ffmpegexport.ui \
    mymainwindow.ui \
    widget.ui

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

RC_ICONS = samIcon.ico
