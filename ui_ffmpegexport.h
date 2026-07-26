/********************************************************************************
** Form generated from reading UI file 'ffmpegexport.ui'
**
** Created by: Qt User Interface Compiler version 6.6.1
**
** WARNING! All changes made in this file will be lost when recompiling UI file!
********************************************************************************/

#ifndef UI_FFMPEGEXPORT_H
#define UI_FFMPEGEXPORT_H

#include <QtCore/QVariant>
#include <QtWidgets/QApplication>
#include <QtWidgets/QFrame>
#include <QtWidgets/QHBoxLayout>
#include <QtWidgets/QLabel>
#include <QtWidgets/QPushButton>
#include <QtWidgets/QRadioButton>
#include <QtWidgets/QSpacerItem>
#include <QtWidgets/QSpinBox>
#include <QtWidgets/QVBoxLayout>
#include <QtWidgets/QWidget>

QT_BEGIN_NAMESPACE

class Ui_FfmpegExport
{
public:
    QVBoxLayout *verticalLayout_2;
    QFrame *frame_2;
    QHBoxLayout *horizontalLayout;
    QFrame *frame;
    QVBoxLayout *verticalLayout;
    QRadioButton *mp4RadioBtn;
    QRadioButton *aviRadioButton;
    QRadioButton *vmwRadioBtn;
    QRadioButton *gifRadioBtn;
    QFrame *frame_3;
    QVBoxLayout *verticalLayout_3;
    QLabel *label;
    QSpinBox *fpsSpinbox;
    QSpacerItem *verticalSpacer;
    QFrame *frame_4;
    QHBoxLayout *horizontalLayout_2;
    QPushButton *selectFolderBtn;
    QLabel *folderLabel;
    QPushButton *exportVideoBtn;

    void setupUi(QWidget *FfmpegExport)
    {
        if (FfmpegExport->objectName().isEmpty())
            FfmpegExport->setObjectName("FfmpegExport");
        FfmpegExport->resize(400, 300);
        verticalLayout_2 = new QVBoxLayout(FfmpegExport);
        verticalLayout_2->setObjectName("verticalLayout_2");
        frame_2 = new QFrame(FfmpegExport);
        frame_2->setObjectName("frame_2");
        frame_2->setFrameShape(QFrame::StyledPanel);
        frame_2->setFrameShadow(QFrame::Raised);
        horizontalLayout = new QHBoxLayout(frame_2);
        horizontalLayout->setObjectName("horizontalLayout");
        frame = new QFrame(frame_2);
        frame->setObjectName("frame");
        frame->setFrameShape(QFrame::StyledPanel);
        frame->setFrameShadow(QFrame::Raised);
        verticalLayout = new QVBoxLayout(frame);
        verticalLayout->setObjectName("verticalLayout");
        mp4RadioBtn = new QRadioButton(frame);
        mp4RadioBtn->setObjectName("mp4RadioBtn");
        mp4RadioBtn->setChecked(true);

        verticalLayout->addWidget(mp4RadioBtn);

        aviRadioButton = new QRadioButton(frame);
        aviRadioButton->setObjectName("aviRadioButton");

        verticalLayout->addWidget(aviRadioButton);

        vmwRadioBtn = new QRadioButton(frame);
        vmwRadioBtn->setObjectName("vmwRadioBtn");

        verticalLayout->addWidget(vmwRadioBtn);

        gifRadioBtn = new QRadioButton(frame);
        gifRadioBtn->setObjectName("gifRadioBtn");

        verticalLayout->addWidget(gifRadioBtn);


        horizontalLayout->addWidget(frame);

        frame_3 = new QFrame(frame_2);
        frame_3->setObjectName("frame_3");
        frame_3->setFrameShape(QFrame::StyledPanel);
        frame_3->setFrameShadow(QFrame::Raised);
        verticalLayout_3 = new QVBoxLayout(frame_3);
        verticalLayout_3->setObjectName("verticalLayout_3");
        label = new QLabel(frame_3);
        label->setObjectName("label");

        verticalLayout_3->addWidget(label);

        fpsSpinbox = new QSpinBox(frame_3);
        fpsSpinbox->setObjectName("fpsSpinbox");
        fpsSpinbox->setMinimum(1);
        fpsSpinbox->setValue(24);

        verticalLayout_3->addWidget(fpsSpinbox);

        verticalSpacer = new QSpacerItem(20, 40, QSizePolicy::Minimum, QSizePolicy::Expanding);

        verticalLayout_3->addItem(verticalSpacer);


        horizontalLayout->addWidget(frame_3);


        verticalLayout_2->addWidget(frame_2);

        frame_4 = new QFrame(FfmpegExport);
        frame_4->setObjectName("frame_4");
        frame_4->setFrameShape(QFrame::StyledPanel);
        frame_4->setFrameShadow(QFrame::Raised);
        horizontalLayout_2 = new QHBoxLayout(frame_4);
        horizontalLayout_2->setObjectName("horizontalLayout_2");
        selectFolderBtn = new QPushButton(frame_4);
        selectFolderBtn->setObjectName("selectFolderBtn");
        selectFolderBtn->setEnabled(true);

        horizontalLayout_2->addWidget(selectFolderBtn);

        folderLabel = new QLabel(frame_4);
        folderLabel->setObjectName("folderLabel");

        horizontalLayout_2->addWidget(folderLabel);


        verticalLayout_2->addWidget(frame_4);

        exportVideoBtn = new QPushButton(FfmpegExport);
        exportVideoBtn->setObjectName("exportVideoBtn");
        exportVideoBtn->setEnabled(false);

        verticalLayout_2->addWidget(exportVideoBtn);


        retranslateUi(FfmpegExport);

        QMetaObject::connectSlotsByName(FfmpegExport);
    } // setupUi

    void retranslateUi(QWidget *FfmpegExport)
    {
        FfmpegExport->setWindowTitle(QCoreApplication::translate("FfmpegExport", "Form", nullptr));
        mp4RadioBtn->setText(QCoreApplication::translate("FfmpegExport", "MP4 H264", nullptr));
        aviRadioButton->setText(QCoreApplication::translate("FfmpegExport", "AVI MPEG-4", nullptr));
        vmwRadioBtn->setText(QCoreApplication::translate("FfmpegExport", "WMV", nullptr));
        gifRadioBtn->setText(QCoreApplication::translate("FfmpegExport", "Gif", nullptr));
        label->setText(QCoreApplication::translate("FfmpegExport", "Frames per second", nullptr));
        selectFolderBtn->setText(QCoreApplication::translate("FfmpegExport", "Select Folder", nullptr));
        folderLabel->setText(QCoreApplication::translate("FfmpegExport", "C:/", nullptr));
        exportVideoBtn->setText(QCoreApplication::translate("FfmpegExport", "Export", nullptr));
    } // retranslateUi

};

namespace Ui {
    class FfmpegExport: public Ui_FfmpegExport {};
} // namespace Ui

QT_END_NAMESPACE

#endif // UI_FFMPEGEXPORT_H
