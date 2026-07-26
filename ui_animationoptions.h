/********************************************************************************
** Form generated from reading UI file 'animationoptions.ui'
**
** Created by: Qt User Interface Compiler version 6.6.1
**
** WARNING! All changes made in this file will be lost when recompiling UI file!
********************************************************************************/

#ifndef UI_ANIMATIONOPTIONS_H
#define UI_ANIMATIONOPTIONS_H

#include <QtCore/QVariant>
#include <QtWidgets/QApplication>
#include <QtWidgets/QCheckBox>
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

class Ui_AnimationOptions
{
public:
    QVBoxLayout *verticalLayout_3;
    QFrame *frame_3;
    QHBoxLayout *horizontalLayout_2;
    QFrame *frame;
    QVBoxLayout *verticalLayout_2;
    QLabel *label;
    QLabel *label_2;
    QLabel *label_3;
    QLabel *label_4;
    QLabel *label_5;
    QSpacerItem *verticalSpacer_2;
    QFrame *frame_2;
    QVBoxLayout *verticalLayout;
    QSpinBox *widthSpinbox;
    QSpinBox *heightSpinbox;
    QSpinBox *fpsSpinbox;
    QSpinBox *autoSaveSpinbox;
    QSpinBox *handlesSizeSpinBox;
    QSpacerItem *verticalSpacer;
    QCheckBox *lockAspectRatioCheckbox;
    QFrame *frame_4;
    QVBoxLayout *verticalLayout_4;
    QRadioButton *noneBtn;
    QRadioButton *repositionBtn;
    QRadioButton *rescaleAndRepositionBtn;
    QPushButton *okBtn;

    void setupUi(QWidget *AnimationOptions)
    {
        if (AnimationOptions->objectName().isEmpty())
            AnimationOptions->setObjectName("AnimationOptions");
        AnimationOptions->resize(400, 304);
        verticalLayout_3 = new QVBoxLayout(AnimationOptions);
        verticalLayout_3->setObjectName("verticalLayout_3");
        frame_3 = new QFrame(AnimationOptions);
        frame_3->setObjectName("frame_3");
        frame_3->setFrameShape(QFrame::StyledPanel);
        frame_3->setFrameShadow(QFrame::Raised);
        horizontalLayout_2 = new QHBoxLayout(frame_3);
        horizontalLayout_2->setObjectName("horizontalLayout_2");
        frame = new QFrame(frame_3);
        frame->setObjectName("frame");
        frame->setFrameShape(QFrame::StyledPanel);
        frame->setFrameShadow(QFrame::Raised);
        verticalLayout_2 = new QVBoxLayout(frame);
        verticalLayout_2->setObjectName("verticalLayout_2");
        label = new QLabel(frame);
        label->setObjectName("label");
        label->setMinimumSize(QSize(0, 25));
        label->setMaximumSize(QSize(16777215, 25));

        verticalLayout_2->addWidget(label);

        label_2 = new QLabel(frame);
        label_2->setObjectName("label_2");
        label_2->setMinimumSize(QSize(0, 25));
        label_2->setMaximumSize(QSize(16777215, 25));

        verticalLayout_2->addWidget(label_2);

        label_3 = new QLabel(frame);
        label_3->setObjectName("label_3");
        label_3->setMinimumSize(QSize(0, 25));
        label_3->setMaximumSize(QSize(16777215, 25));

        verticalLayout_2->addWidget(label_3);

        label_4 = new QLabel(frame);
        label_4->setObjectName("label_4");
        label_4->setMinimumSize(QSize(0, 25));

        verticalLayout_2->addWidget(label_4);

        label_5 = new QLabel(frame);
        label_5->setObjectName("label_5");
        label_5->setMinimumSize(QSize(0, 25));

        verticalLayout_2->addWidget(label_5);

        verticalSpacer_2 = new QSpacerItem(20, 40, QSizePolicy::Minimum, QSizePolicy::Expanding);

        verticalLayout_2->addItem(verticalSpacer_2);


        horizontalLayout_2->addWidget(frame);

        frame_2 = new QFrame(frame_3);
        frame_2->setObjectName("frame_2");
        frame_2->setFrameShape(QFrame::StyledPanel);
        frame_2->setFrameShadow(QFrame::Raised);
        verticalLayout = new QVBoxLayout(frame_2);
        verticalLayout->setObjectName("verticalLayout");
        widthSpinbox = new QSpinBox(frame_2);
        widthSpinbox->setObjectName("widthSpinbox");
        widthSpinbox->setMinimumSize(QSize(0, 25));
        widthSpinbox->setMaximumSize(QSize(16777215, 25));
        widthSpinbox->setMinimum(1);
        widthSpinbox->setMaximum(3000);
        widthSpinbox->setValue(800);

        verticalLayout->addWidget(widthSpinbox);

        heightSpinbox = new QSpinBox(frame_2);
        heightSpinbox->setObjectName("heightSpinbox");
        heightSpinbox->setMinimumSize(QSize(0, 25));
        heightSpinbox->setMaximumSize(QSize(16777215, 25));
        heightSpinbox->setMinimum(1);
        heightSpinbox->setMaximum(3000);
        heightSpinbox->setValue(600);

        verticalLayout->addWidget(heightSpinbox);

        fpsSpinbox = new QSpinBox(frame_2);
        fpsSpinbox->setObjectName("fpsSpinbox");
        fpsSpinbox->setMinimumSize(QSize(0, 25));
        fpsSpinbox->setMaximumSize(QSize(16777215, 25));
        fpsSpinbox->setValue(24);

        verticalLayout->addWidget(fpsSpinbox);

        autoSaveSpinbox = new QSpinBox(frame_2);
        autoSaveSpinbox->setObjectName("autoSaveSpinbox");
        autoSaveSpinbox->setMinimumSize(QSize(0, 25));
        autoSaveSpinbox->setValue(50);

        verticalLayout->addWidget(autoSaveSpinbox);

        handlesSizeSpinBox = new QSpinBox(frame_2);
        handlesSizeSpinBox->setObjectName("handlesSizeSpinBox");
        handlesSizeSpinBox->setMinimumSize(QSize(0, 25));
        handlesSizeSpinBox->setMinimum(3);
        handlesSizeSpinBox->setValue(10);

        verticalLayout->addWidget(handlesSizeSpinBox);

        verticalSpacer = new QSpacerItem(20, 40, QSizePolicy::Minimum, QSizePolicy::Expanding);

        verticalLayout->addItem(verticalSpacer);


        horizontalLayout_2->addWidget(frame_2);

        lockAspectRatioCheckbox = new QCheckBox(frame_3);
        lockAspectRatioCheckbox->setObjectName("lockAspectRatioCheckbox");
        lockAspectRatioCheckbox->setChecked(true);

        horizontalLayout_2->addWidget(lockAspectRatioCheckbox);


        verticalLayout_3->addWidget(frame_3);

        frame_4 = new QFrame(AnimationOptions);
        frame_4->setObjectName("frame_4");
        frame_4->setFrameShape(QFrame::StyledPanel);
        frame_4->setFrameShadow(QFrame::Raised);
        verticalLayout_4 = new QVBoxLayout(frame_4);
        verticalLayout_4->setObjectName("verticalLayout_4");
        noneBtn = new QRadioButton(frame_4);
        noneBtn->setObjectName("noneBtn");

        verticalLayout_4->addWidget(noneBtn);

        repositionBtn = new QRadioButton(frame_4);
        repositionBtn->setObjectName("repositionBtn");

        verticalLayout_4->addWidget(repositionBtn);

        rescaleAndRepositionBtn = new QRadioButton(frame_4);
        rescaleAndRepositionBtn->setObjectName("rescaleAndRepositionBtn");
        rescaleAndRepositionBtn->setChecked(true);

        verticalLayout_4->addWidget(rescaleAndRepositionBtn);


        verticalLayout_3->addWidget(frame_4);

        okBtn = new QPushButton(AnimationOptions);
        okBtn->setObjectName("okBtn");

        verticalLayout_3->addWidget(okBtn);


        retranslateUi(AnimationOptions);

        QMetaObject::connectSlotsByName(AnimationOptions);
    } // setupUi

    void retranslateUi(QWidget *AnimationOptions)
    {
        AnimationOptions->setWindowTitle(QCoreApplication::translate("AnimationOptions", "Form", nullptr));
        label->setText(QCoreApplication::translate("AnimationOptions", "Width", nullptr));
        label_2->setText(QCoreApplication::translate("AnimationOptions", "Height", nullptr));
        label_3->setText(QCoreApplication::translate("AnimationOptions", "Frames per second", nullptr));
        label_4->setText(QCoreApplication::translate("AnimationOptions", "Autosave operation Interval", nullptr));
        label_5->setText(QCoreApplication::translate("AnimationOptions", "Handles size", nullptr));
        lockAspectRatioCheckbox->setText(QCoreApplication::translate("AnimationOptions", "lock aspect ratio", nullptr));
        noneBtn->setText(QCoreApplication::translate("AnimationOptions", "No repositioning", nullptr));
        repositionBtn->setText(QCoreApplication::translate("AnimationOptions", "Reposition Animation", nullptr));
        rescaleAndRepositionBtn->setText(QCoreApplication::translate("AnimationOptions", "Rescale and Reposition Animation", nullptr));
        okBtn->setText(QCoreApplication::translate("AnimationOptions", "OK", nullptr));
    } // retranslateUi

};

namespace Ui {
    class AnimationOptions: public Ui_AnimationOptions {};
} // namespace Ui

QT_END_NAMESPACE

#endif // UI_ANIMATIONOPTIONS_H
