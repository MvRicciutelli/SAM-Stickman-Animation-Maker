QT       += core gui

greaterThan(QT_MAJOR_VERSION, 4): QT += widgets

CONFIG += c++11

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
    advancedcirclewidget.cpp \
    advancedlinewidget.cpp \
    advancedtab.cpp \
    advancedtaperwidget.cpp \
    animation.cpp \
    animationoptions.cpp \
    checkupdate.cpp \
    ffmpegexport.cpp \
    frame.cpp \
    imageexport.cpp \
    main.cpp \
    mymainwindow.cpp \
    myview.cpp \
    sleeper.cpp \
    stick.cpp \
    stickdata.cpp \
    widget.cpp

HEADERS += \
    StickFigure.h \
    advancedcirclewidget.h \
    advancedlinewidget.h \
    advancedtab.h \
    advancedtaperwidget.h \
    animation.h \
    animationoptions.h \
    checkupdate.h \
    ffmpegexport.h \
    frame.h \
    imageexport.h \
    mymainwindow.h \
    myview.h \
    sleeper.h \
    stick.h \
    stickdata.h \
    style.h \
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
    version.exe
RC_ICONS = marionettaIcon_00001.ico
