/********************************************************************************
** Form generated from reading UI file 'advancedtab.ui'
**
** Created by: Qt User Interface Compiler version 6.6.1
**
** WARNING! All changes made in this file will be lost when recompiling UI file!
********************************************************************************/

#ifndef UI_ADVANCEDTAB_H
#define UI_ADVANCEDTAB_H

#include <QtCore/QVariant>
#include <QtWidgets/QApplication>
#include <QtWidgets/QFrame>
#include <QtWidgets/QGroupBox>
#include <QtWidgets/QHBoxLayout>
#include <QtWidgets/QLabel>
#include <QtWidgets/QSlider>
#include <QtWidgets/QSpacerItem>
#include <QtWidgets/QSpinBox>
#include <QtWidgets/QVBoxLayout>
#include <QtWidgets/QWidget>

QT_BEGIN_NAMESPACE

class Ui_AdvancedTab
{
public:
    QVBoxLayout *verticalLayout;
    QGroupBox *groupBox_5;
    QVBoxLayout *verticalLayout_18;
    QVBoxLayout *verticalLayout_19;
    QFrame *frame_2;
    QVBoxLayout *verticalLayout_9;
    QFrame *frame_12;
    QHBoxLayout *horizontalLayout_14;
    QLabel *label_13;
    QSpinBox *rectWidthSpinbox;
    QFrame *frame_11;
    QHBoxLayout *horizontalLayout_3;
    QSlider *rectWidthSlider;
    QSpacerItem *verticalSpacer;
    QSpacerItem *verticalSpacer_2;

    void setupUi(QWidget *AdvancedTab)
    {
        if (AdvancedTab->objectName().isEmpty())
            AdvancedTab->setObjectName("AdvancedTab");
        AdvancedTab->resize(400, 655);
        verticalLayout = new QVBoxLayout(AdvancedTab);
        verticalLayout->setObjectName("verticalLayout");
        groupBox_5 = new QGroupBox(AdvancedTab);
        groupBox_5->setObjectName("groupBox_5");
        QSizePolicy sizePolicy(QSizePolicy::Preferred, QSizePolicy::Preferred);
        sizePolicy.setHorizontalStretch(0);
        sizePolicy.setVerticalStretch(0);
        sizePolicy.setHeightForWidth(groupBox_5->sizePolicy().hasHeightForWidth());
        groupBox_5->setSizePolicy(sizePolicy);
        groupBox_5->setMinimumSize(QSize(100, 400));
        groupBox_5->setMaximumSize(QSize(16777215, 400));
        verticalLayout_18 = new QVBoxLayout(groupBox_5);
        verticalLayout_18->setSpacing(0);
        verticalLayout_18->setObjectName("verticalLayout_18");
        verticalLayout_18->setContentsMargins(0, 10, 0, 0);
        verticalLayout_19 = new QVBoxLayout();
        verticalLayout_19->setSpacing(0);
        verticalLayout_19->setObjectName("verticalLayout_19");
        verticalLayout_19->setSizeConstraint(QLayout::SetNoConstraint);
        verticalLayout_19->setContentsMargins(-1, -1, 0, -1);
        frame_2 = new QFrame(groupBox_5);
        frame_2->setObjectName("frame_2");
        frame_2->setMaximumSize(QSize(16777215, 80));
        frame_2->setFrameShape(QFrame::StyledPanel);
        frame_2->setFrameShadow(QFrame::Raised);
        verticalLayout_9 = new QVBoxLayout(frame_2);
        verticalLayout_9->setSpacing(0);
        verticalLayout_9->setObjectName("verticalLayout_9");
        verticalLayout_9->setContentsMargins(4, 8, 4, 4);
        frame_12 = new QFrame(frame_2);
        frame_12->setObjectName("frame_12");
        frame_12->setMinimumSize(QSize(0, 0));
        frame_12->setMaximumSize(QSize(16777215, 30));
        frame_12->setFrameShape(QFrame::StyledPanel);
        frame_12->setFrameShadow(QFrame::Raised);
        horizontalLayout_14 = new QHBoxLayout(frame_12);
        horizontalLayout_14->setObjectName("horizontalLayout_14");
        horizontalLayout_14->setContentsMargins(4, 1, 4, 1);
        label_13 = new QLabel(frame_12);
        label_13->setObjectName("label_13");

        horizontalLayout_14->addWidget(label_13);

        rectWidthSpinbox = new QSpinBox(frame_12);
        rectWidthSpinbox->setObjectName("rectWidthSpinbox");
        rectWidthSpinbox->setMinimum(0);
        rectWidthSpinbox->setMaximum(1000);
        rectWidthSpinbox->setValue(10);

        horizontalLayout_14->addWidget(rectWidthSpinbox);


        verticalLayout_9->addWidget(frame_12);

        frame_11 = new QFrame(frame_2);
        frame_11->setObjectName("frame_11");
        frame_11->setMinimumSize(QSize(0, 0));
        frame_11->setMaximumSize(QSize(16777215, 30));
        frame_11->setFrameShape(QFrame::StyledPanel);
        frame_11->setFrameShadow(QFrame::Raised);
        horizontalLayout_3 = new QHBoxLayout(frame_11);
        horizontalLayout_3->setObjectName("horizontalLayout_3");
        horizontalLayout_3->setContentsMargins(4, 1, 4, 1);
        rectWidthSlider = new QSlider(frame_11);
        rectWidthSlider->setObjectName("rectWidthSlider");
        rectWidthSlider->setMinimum(0);
        rectWidthSlider->setMaximum(1000);
        rectWidthSlider->setValue(10);
        rectWidthSlider->setOrientation(Qt::Horizontal);

        horizontalLayout_3->addWidget(rectWidthSlider);


        verticalLayout_9->addWidget(frame_11);


        verticalLayout_19->addWidget(frame_2);


        verticalLayout_18->addLayout(verticalLayout_19);

        verticalSpacer = new QSpacerItem(20, 40, QSizePolicy::Minimum, QSizePolicy::Expanding);

        verticalLayout_18->addItem(verticalSpacer);


        verticalLayout->addWidget(groupBox_5);

        verticalSpacer_2 = new QSpacerItem(20, 40, QSizePolicy::Minimum, QSizePolicy::Expanding);

        verticalLayout->addItem(verticalSpacer_2);


        retranslateUi(AdvancedTab);

        QMetaObject::connectSlotsByName(AdvancedTab);
    } // setupUi

    void retranslateUi(QWidget *AdvancedTab)
    {
        AdvancedTab->setWindowTitle(QCoreApplication::translate("AdvancedTab", "Form", nullptr));
#if QT_CONFIG(tooltip)
        AdvancedTab->setToolTip(QCoreApplication::translate("AdvancedTab", "<html><head/><body><p>Edit the width of your rectangular sticks</p></body></html>", nullptr));
#endif // QT_CONFIG(tooltip)
        groupBox_5->setTitle(QCoreApplication::translate("AdvancedTab", "Rectangle Edit", nullptr));
        label_13->setText(QCoreApplication::translate("AdvancedTab", "Width", nullptr));
    } // retranslateUi

};

namespace Ui {
    class AdvancedTab: public Ui_AdvancedTab {};
} // namespace Ui

QT_END_NAMESPACE

#endif // UI_ADVANCEDTAB_H
