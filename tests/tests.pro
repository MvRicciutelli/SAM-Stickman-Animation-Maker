QT += core gui widgets testlib
CONFIG += c++17 console
CONFIG -= app_bundle
TEMPLATE = app
TARGET = tst_characterization

INCLUDEPATH += ..

# absolute path of this directory, so the tests find data/ regardless of the
# shadow-build working directory
DEFINES += TESTS_SOURCE_DIR=\\\"$$PWD\\\"

# App sources under test: the model/undo/serialization layer plus the advanced-tab widgets
# that animation.cpp's updateTab() needs. widget.cpp/mymainwindow.cpp are NOT linked (their
# constructors do file I/O and dialogs); the globals they own are defined in test_globals.cpp.
SOURCES += \
    tst_characterization.cpp \
    test_globals.cpp \
    ../abrbrush.cpp \
    ../StickFigure.cpp \
    ../advancedcirclewidget.cpp \
    ../advancedlinewidget.cpp \
    ../advancedtab.cpp \
    ../advancedtaperwidget.cpp \
    ../animation.cpp \
    ../brushpicker.cpp \
    ../frame.cpp \
    ../framedata.cpp \
    ../gradientdialog.cpp \
    ../mfvideo.cpp \
    ../imageexport.cpp \
    ../framepresenter.cpp \
    ../myview.cpp \
    ../stick.cpp \
    ../stickdata.cpp \
    ../stickfiguredata.cpp \
    ../stkformat.cpp

HEADERS += \
    ../abrbrush.h \
    ../StickFigure.h \
    ../advancedcirclewidget.h \
    ../advancedlinewidget.h \
    ../advancedtab.h \
    ../advancedtaperwidget.h \
    ../animation.h \
    ../brushpicker.h \
    ../frame.h \
    ../framedata.h \
    ../framepresenter.h \
    ../gradientdialog.h \
    ../myview.h \
    ../stick.h \
    ../stickdata.h \
    ../stickfiguredata.h \
    ../mfvideo.h \
    ../imageexport.h \
    ../stkformat.h \
    ../style.h \
    ../uiItems.h

# Media Foundation: l'encoder H.264 di Windows usato da mfvideo.cpp
win32: LIBS += -lmfplat -lmfreadwrite -lmfuuid -lole32

FORMS += \
    ../advancedcirclewidget.ui \
    ../advancedlinewidget.ui \
    ../advancedtab.ui \
    ../advancedtaperwidget.ui
