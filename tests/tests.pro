QT += core gui widgets testlib core-private
CONFIG += c++17 console
CONFIG -= app_bundle
TEMPLATE = app
TARGET = tst_characterization

INCLUDEPATH += ../src

# absolute path of this directory, so the tests find data/ regardless of the
# shadow-build working directory
DEFINES += TESTS_SOURCE_DIR=\\\"$$PWD\\\"

# App sources under test: the model/undo/serialization layer plus the advanced-tab widgets
# that animation.cpp's updateTab() needs. widget.cpp/mymainwindow.cpp are NOT linked (their
# constructors do file I/O and dialogs); the globals they own are defined in test_globals.cpp.
SOURCES += \
    tst_characterization.cpp \
    test_globals.cpp \
    ../src/abrbrush.cpp \
    ../src/StickFigure.cpp \
    ../src/advancedcirclewidget.cpp \
    ../src/advancedlinewidget.cpp \
    ../src/advancedtab.cpp \
    ../src/advancedtaperwidget.cpp \
    ../src/animation.cpp \
    ../src/brushpicker.cpp \
    ../src/frame.cpp \
    ../src/framedata.cpp \
    ../src/gradientdialog.cpp \
    ../src/mfvideo.cpp \
    ../src/imageexport.cpp \
    ../src/kritabundle.cpp \
    ../src/framepresenter.cpp \
    ../src/myview.cpp \
    ../src/stick.cpp \
    ../src/stickdata.cpp \
    ../src/stickfiguredata.cpp \
    ../src/stkformat.cpp

HEADERS += \
    ../src/abrbrush.h \
    ../src/StickFigure.h \
    ../src/advancedcirclewidget.h \
    ../src/advancedlinewidget.h \
    ../src/advancedtab.h \
    ../src/advancedtaperwidget.h \
    ../src/animation.h \
    ../src/brushpicker.h \
    ../src/frame.h \
    ../src/framedata.h \
    ../src/framepresenter.h \
    ../src/gradientdialog.h \
    ../src/myview.h \
    ../src/stick.h \
    ../src/stickdata.h \
    ../src/stickfiguredata.h \
    ../src/mfvideo.h \
    ../src/imageexport.h \
    ../src/kritabundle.h \
    ../src/stkformat.h \
    ../src/style.h \
    ../src/uiItems.h

# Media Foundation: l'encoder H.264 di Windows usato da mfvideo.cpp
win32: LIBS += -lmfplat -lmfreadwrite -lmfuuid -lole32

FORMS += \
    ../ui/advancedcirclewidget.ui \
    ../ui/advancedlinewidget.ui \
    ../ui/advancedtab.ui \
    ../ui/advancedtaperwidget.ui
