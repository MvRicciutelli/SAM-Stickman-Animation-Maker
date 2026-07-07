QT += core gui widgets testlib
CONFIG += c++17 console
CONFIG -= app_bundle
TEMPLATE = app
TARGET = tst_characterization

INCLUDEPATH += ..

# App sources under test: the model/undo/serialization layer plus the advanced-tab widgets
# that animation.cpp's updateTab() needs. widget.cpp/mymainwindow.cpp are NOT linked (their
# constructors do file I/O and dialogs); the globals they own are defined in test_globals.cpp.
SOURCES += \
    tst_characterization.cpp \
    test_globals.cpp \
    ../StickFigure.cpp \
    ../advancedcirclewidget.cpp \
    ../advancedlinewidget.cpp \
    ../advancedtab.cpp \
    ../advancedtaperwidget.cpp \
    ../animation.cpp \
    ../frame.cpp \
    ../myview.cpp \
    ../stick.cpp \
    ../stickdata.cpp

HEADERS += \
    ../StickFigure.h \
    ../advancedcirclewidget.h \
    ../advancedlinewidget.h \
    ../advancedtab.h \
    ../advancedtaperwidget.h \
    ../animation.h \
    ../frame.h \
    ../myview.h \
    ../stick.h \
    ../stickdata.h \
    ../style.h \
    ../uiItems.h

FORMS += \
    ../advancedcirclewidget.ui \
    ../advancedlinewidget.ui \
    ../advancedtab.ui \
    ../advancedtaperwidget.ui
