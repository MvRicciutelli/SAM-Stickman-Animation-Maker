/********************************************************************************
** Form generated from reading UI file 'mymainwindow.ui'
**
** Created by: Qt User Interface Compiler version 6.6.1
**
** WARNING! All changes made in this file will be lost when recompiling UI file!
********************************************************************************/

#ifndef UI_MYMAINWINDOW_H
#define UI_MYMAINWINDOW_H

#include <QtCore/QVariant>
#include <QtGui/QAction>
#include <QtWidgets/QApplication>
#include <QtWidgets/QMainWindow>
#include <QtWidgets/QMenu>
#include <QtWidgets/QMenuBar>
#include <QtWidgets/QStatusBar>
#include <QtWidgets/QWidget>

QT_BEGIN_NAMESPACE

class Ui_MyMainWindow
{
public:
    QAction *actionsave;
    QAction *actionload;
    QAction *actionsave_animation;
    QAction *actionload_animation;
    QAction *actionexport_as_AVI_file;
    QAction *actionsave_animation_current;
    QAction *actionNew_Blank_Animation;
    QAction *actionAnimation_Options;
    QAction *actionExport_as_image_sequence;
    QAction *actionExport_video_using_FFMPEG;
    QAction *actionTerms_and_Conditions;
    QWidget *centralwidget;
    QMenuBar *menubar;
    QMenu *menufile;
    QMenu *menuHelp;
    QMenu *menuAbout;
    QStatusBar *statusbar;

    void setupUi(QMainWindow *MyMainWindow)
    {
        if (MyMainWindow->objectName().isEmpty())
            MyMainWindow->setObjectName("MyMainWindow");
        MyMainWindow->resize(800, 600);
        MyMainWindow->setStyleSheet(QString::fromUtf8("/*\n"
"	Copyright 2013 Emanuel Claesson\n"
"\n"
"	Licensed under the Apache License, Version 2.0 (the \"License\");\n"
"	you may not use this file except in compliance with the License.\n"
"	You may obtain a copy of the License at\n"
"\n"
"		http://www.apache.org/licenses/LICENSE-2.0\n"
"\n"
"	Unless required by applicable law or agreed to in writing, software\n"
"	distributed under the License is distributed on an \"AS IS\" BASIS,\n"
"	WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.\n"
"	See the License for the specific language governing permissions and\n"
"	limitations under the License.\n"
"*/\n"
"\n"
"/*\n"
"	COLOR_DARK     = #191919\n"
"	COLOR_MEDIUM   = #353535\n"
"	COLOR_MEDLIGHT = #5A5A5A\n"
"	COLOR_LIGHT    = #DDDDDD\n"
"	COLOR_ACCENT   = #3D7848\n"
"*/\n"
"\n"
"* {\n"
"	background: #191919;\n"
"	color: #DDDDDD;\n"
"	border: 1px solid #5A5A5A;\n"
"}\n"
"\n"
"QWidget::item:selected {\n"
"	background: #3D7848;\n"
"}\n"
"\n"
"QCheckBox, QRadioButton {\n"
"	border: none;\n"
"}\n"
""
                        "\n"
"QRadioButton::indicator, QCheckBox::indicator {\n"
"	width: 13px;\n"
"	height: 13px;\n"
"}\n"
"\n"
"QRadioButton::indicator::unchecked, QCheckBox::indicator::unchecked {\n"
"	border: 1px solid #5A5A5A;\n"
"	background: none;\n"
"}\n"
"\n"
"QRadioButton::indicator:unchecked:hover, QCheckBox::indicator:unchecked:hover {\n"
"	border: 1px solid #DDDDDD;\n"
"}\n"
"\n"
"QRadioButton::indicator::checked, QCheckBox::indicator::checked {\n"
"	border: 1px solid #5A5A5A;\n"
"	background: #5A5A5A;\n"
"}\n"
"\n"
"QRadioButton::indicator:checked:hover, QCheckBox::indicator:checked:hover {\n"
"	border: 1px solid #DDDDDD;\n"
"	background: #DDDDDD;\n"
"}\n"
"\n"
"QGroupBox {\n"
"	margin-top: 6px;\n"
"}\n"
"\n"
"QGroupBox::title {\n"
"	top: -7px;\n"
"	left: 7px;\n"
"}\n"
"\n"
"QScrollBar {\n"
"	border: 1px solid #5A5A5A;\n"
"	background: #191919;\n"
"}\n"
"\n"
"QScrollBar:horizontal {\n"
"	height: 15px;\n"
"	margin: 0px 0px 0px 32px;\n"
"}\n"
"\n"
"QScrollBar:vertical {\n"
"	width: 15px;\n"
"	margin: 32px 0px 0px 0px;\n"
""
                        "}\n"
"\n"
"QScrollBar::handle {\n"
"	background: #353535;\n"
"	border: 1px solid #5A5A5A;\n"
"}\n"
"\n"
"QScrollBar::handle:horizontal {\n"
"	border-width: 0px 1px 0px 1px;\n"
"}\n"
"\n"
"QScrollBar::handle:vertical {\n"
"	border-width: 1px 0px 1px 0px;\n"
"}\n"
"\n"
"QScrollBar::handle:horizontal {\n"
"	min-width: 20px;\n"
"}\n"
"\n"
"QScrollBar::handle:vertical {\n"
"	min-height: 20px;\n"
"}\n"
"\n"
"QScrollBar::add-line, QScrollBar::sub-line {\n"
"	background:#353535;\n"
"	border: 1px solid #5A5A5A;\n"
"	subcontrol-origin: margin;\n"
"}\n"
"\n"
"QScrollBar::add-line {\n"
"	position: absolute;\n"
"}\n"
"\n"
"QScrollBar::add-line:horizontal {\n"
"	width: 15px;\n"
"	subcontrol-position: left;\n"
"	left: 15px;\n"
"}\n"
"\n"
"QScrollBar::add-line:vertical {\n"
"	height: 15px;\n"
"	subcontrol-position: top;\n"
"	top: 15px;\n"
"}\n"
"\n"
"QScrollBar::sub-line:horizontal {\n"
"	width: 15px;\n"
"	subcontrol-position: top left;\n"
"}\n"
"\n"
"QScrollBar::sub-line:vertical {\n"
"	height: 15px;\n"
"	subcontrol-position"
                        ": top;\n"
"}\n"
"\n"
"QScrollBar:left-arrow, QScrollBar::right-arrow, QScrollBar::up-arrow, QScrollBar::down-arrow {\n"
"	border: 1px solid #5A5A5A;\n"
"	width: 3px;\n"
"	height: 3px;\n"
"}\n"
"\n"
"QScrollBar::add-page, QScrollBar::sub-page {\n"
"	background: none;\n"
"}\n"
"\n"
"QAbstractButton:hover {\n"
"	background: #353535;\n"
"}\n"
"\n"
"QAbstractButton:pressed {\n"
"	background: #5A5A5A;\n"
"}\n"
"\n"
"QAbstractItemView {\n"
"	show-decoration-selected: 1;\n"
"	selection-background-color: #3D7848;\n"
"	selection-color: #DDDDDD;\n"
"	alternate-background-color: #353535;\n"
"}\n"
"\n"
"QHeaderView {\n"
"	border: 1px solid #5A5A5A;\n"
"}\n"
"\n"
"QHeaderView::section {\n"
"	background: #191919;\n"
"	border: 1px solid #5A5A5A;\n"
"	padding: 4px;\n"
"}\n"
"\n"
"QHeaderView::section:selected, QHeaderView::section::checked {\n"
"	background: #353535;\n"
"}\n"
"\n"
"QTableView {\n"
"	gridline-color: #5A5A5A;\n"
"}\n"
"\n"
"QTabBar {\n"
"	margin-left: 2px;\n"
"}\n"
"\n"
"QTabBar::tab {\n"
"	border-radius: 0px;\n"
""
                        "	padding: 4px;\n"
"	margin: 4px;\n"
"}\n"
"\n"
"QTabBar::tab:selected {\n"
"	background: #353535;\n"
"}\n"
"\n"
"QComboBox::down-arrow {\n"
"	border: 1px solid #5A5A5A;\n"
"	background: #353535;\n"
"}\n"
"\n"
"QComboBox::drop-down {\n"
"	border: 1px solid #5A5A5A;\n"
"	background: #353535;\n"
"}\n"
"\n"
"QComboBox::down-arrow {\n"
"	width: 3px;\n"
"	height: 3px;\n"
"	border: 1px solid #5A5A5A;\n"
"}\n"
"\n"
"QAbstractSpinBox {\n"
"	padding-right: 15px;\n"
"}\n"
"\n"
"QAbstractSpinBox::up-button, QAbstractSpinBox::down-button {\n"
"	border: 1px solid #5A5A5A;\n"
"	background: #353535;\n"
"	subcontrol-origin: border;\n"
"}\n"
"\n"
"QAbstractSpinBox::up-arrow, QAbstractSpinBox::down-arrow {\n"
"	width: 3px;\n"
"	height: 3px;\n"
"	border: 1px solid #5A5A5A;\n"
"}\n"
"\n"
"QSlider {\n"
"	border: none;\n"
"}\n"
"\n"
"QSlider::groove:horizontal {\n"
"	height: 5px;\n"
"	margin: 4px 0px 4px 0px;\n"
"}\n"
"\n"
"QSlider::groove:vertical {\n"
"	width: 5px;\n"
"	margin: 0px 4px 0px 4px;\n"
"}\n"
"\n"
"QSlider::handle {\n"
""
                        "	border: 1px solid #5A5A5A;\n"
"	background: #353535;\n"
"}\n"
"\n"
"QSlider::handle:horizontal {\n"
"	width: 15px;\n"
"	margin: -4px 0px -4px 0px;\n"
"}\n"
"\n"
"QSlider::handle:vertical {\n"
"	height: 15px;\n"
"	margin: 0px -4px 0px -4px;\n"
"}\n"
"\n"
"QSlider::add-page:vertical, QSlider::sub-page:horizontal {\n"
"	background: #3D7848;\n"
"}\n"
"\n"
"QSlider::sub-page:vertical, QSlider::add-page:horizontal {\n"
"	background: #353535;\n"
"}\n"
"\n"
"QLabel {\n"
"	border: none;\n"
"}\n"
"\n"
"QProgressBar {\n"
"	text-align: center;\n"
"}\n"
"\n"
"QProgressBar::chunk {\n"
"	width: 1px;\n"
"	background-color: #3D7848;\n"
"}\n"
"\n"
"QMenu::separator {\n"
"	background: #353535;\n"
"}"));
        actionsave = new QAction(MyMainWindow);
        actionsave->setObjectName("actionsave");
        actionload = new QAction(MyMainWindow);
        actionload->setObjectName("actionload");
        actionsave_animation = new QAction(MyMainWindow);
        actionsave_animation->setObjectName("actionsave_animation");
        actionload_animation = new QAction(MyMainWindow);
        actionload_animation->setObjectName("actionload_animation");
        actionexport_as_AVI_file = new QAction(MyMainWindow);
        actionexport_as_AVI_file->setObjectName("actionexport_as_AVI_file");
        actionsave_animation_current = new QAction(MyMainWindow);
        actionsave_animation_current->setObjectName("actionsave_animation_current");
        actionNew_Blank_Animation = new QAction(MyMainWindow);
        actionNew_Blank_Animation->setObjectName("actionNew_Blank_Animation");
        actionAnimation_Options = new QAction(MyMainWindow);
        actionAnimation_Options->setObjectName("actionAnimation_Options");
        actionExport_as_image_sequence = new QAction(MyMainWindow);
        actionExport_as_image_sequence->setObjectName("actionExport_as_image_sequence");
        actionExport_video_using_FFMPEG = new QAction(MyMainWindow);
        actionExport_video_using_FFMPEG->setObjectName("actionExport_video_using_FFMPEG");
        actionTerms_and_Conditions = new QAction(MyMainWindow);
        actionTerms_and_Conditions->setObjectName("actionTerms_and_Conditions");
        centralwidget = new QWidget(MyMainWindow);
        centralwidget->setObjectName("centralwidget");
        MyMainWindow->setCentralWidget(centralwidget);
        menubar = new QMenuBar(MyMainWindow);
        menubar->setObjectName("menubar");
        menubar->setGeometry(QRect(0, 0, 800, 22));
        menufile = new QMenu(menubar);
        menufile->setObjectName("menufile");
        menuHelp = new QMenu(menubar);
        menuHelp->setObjectName("menuHelp");
        menuAbout = new QMenu(menuHelp);
        menuAbout->setObjectName("menuAbout");
        MyMainWindow->setMenuBar(menubar);
        statusbar = new QStatusBar(MyMainWindow);
        statusbar->setObjectName("statusbar");
        MyMainWindow->setStatusBar(statusbar);

        menubar->addAction(menufile->menuAction());
        menubar->addAction(menuHelp->menuAction());
        menufile->addAction(actionNew_Blank_Animation);
        menufile->addAction(actionsave_animation_current);
        menufile->addAction(actionsave_animation);
        menufile->addAction(actionload_animation);
        menufile->addSeparator();
        menufile->addAction(actionExport_video_using_FFMPEG);
        menufile->addAction(actionExport_as_image_sequence);
        menufile->addAction(actionexport_as_AVI_file);
        menufile->addSeparator();
        menufile->addAction(actionsave);
        menufile->addAction(actionload);
        menufile->addSeparator();
        menufile->addAction(actionAnimation_Options);
        menuHelp->addAction(menuAbout->menuAction());
        menuHelp->addAction(actionTerms_and_Conditions);

        retranslateUi(MyMainWindow);

        QMetaObject::connectSlotsByName(MyMainWindow);
    } // setupUi

    void retranslateUi(QMainWindow *MyMainWindow)
    {
        MyMainWindow->setWindowTitle(QCoreApplication::translate("MyMainWindow", "Marionetta V0.9.0", nullptr));
        actionsave->setText(QCoreApplication::translate("MyMainWindow", "Save StickFigure", nullptr));
        actionload->setText(QCoreApplication::translate("MyMainWindow", "Load StickFigure", nullptr));
        actionsave_animation->setText(QCoreApplication::translate("MyMainWindow", "Save Animation As", nullptr));
        actionload_animation->setText(QCoreApplication::translate("MyMainWindow", "Load Animation", nullptr));
        actionexport_as_AVI_file->setText(QCoreApplication::translate("MyMainWindow", "Export video as AVI file", nullptr));
        actionsave_animation_current->setText(QCoreApplication::translate("MyMainWindow", "Save Animation", nullptr));
        actionNew_Blank_Animation->setText(QCoreApplication::translate("MyMainWindow", "New Blank Animation", nullptr));
        actionAnimation_Options->setText(QCoreApplication::translate("MyMainWindow", "Animation Settings", nullptr));
        actionExport_as_image_sequence->setText(QCoreApplication::translate("MyMainWindow", "Export as image sequence", nullptr));
        actionExport_video_using_FFMPEG->setText(QCoreApplication::translate("MyMainWindow", "Export video using FFMPEG", nullptr));
        actionTerms_and_Conditions->setText(QCoreApplication::translate("MyMainWindow", "Terms and Conditions", nullptr));
        menufile->setTitle(QCoreApplication::translate("MyMainWindow", "File", nullptr));
        menuHelp->setTitle(QCoreApplication::translate("MyMainWindow", "Help", nullptr));
        menuAbout->setTitle(QCoreApplication::translate("MyMainWindow", "About", nullptr));
    } // retranslateUi

};

namespace Ui {
    class MyMainWindow: public Ui_MyMainWindow {};
} // namespace Ui

QT_END_NAMESPACE

#endif // UI_MYMAINWINDOW_H
