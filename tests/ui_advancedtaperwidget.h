/********************************************************************************
** Form generated from reading UI file 'advancedtaperwidget.ui'
**
** Created by: Qt User Interface Compiler version 6.6.1
**
** WARNING! All changes made in this file will be lost when recompiling UI file!
********************************************************************************/

#ifndef UI_ADVANCEDTAPERWIDGET_H
#define UI_ADVANCEDTAPERWIDGET_H

#include <QtCore/QVariant>
#include <QtWidgets/QApplication>
#include <QtWidgets/QCheckBox>
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

class Ui_AdvancedTaperWidget
{
public:
    QVBoxLayout *verticalLayout;
    QGroupBox *groupBox_5;
    QVBoxLayout *verticalLayout_2;
    QVBoxLayout *verticalLayout_19;
    QFrame *frame_2;
    QVBoxLayout *verticalLayout_9;
    QFrame *frame_12;
    QHBoxLayout *horizontalLayout_14;
    QLabel *label_13;
    QSpinBox *taperWidthSpinbox;
    QFrame *frame_11;
    QHBoxLayout *horizontalLayout_3;
    QSlider *taperWidthSlider;
    QFrame *frame_3;
    QVBoxLayout *verticalLayout_10;
    QFrame *frame_13;
    QHBoxLayout *horizontalLayout_15;
    QLabel *label_14;
    QSpinBox *taperHeightSpinbox;
    QFrame *frame_14;
    QHBoxLayout *horizontalLayout_4;
    QSlider *taperHeightSlider;
    QCheckBox *hardTopTaperCheck;
    QCheckBox *hardBottomTaperCheck;
    QSpacerItem *verticalSpacer_2;
    QSpacerItem *verticalSpacer;

    void setupUi(QWidget *AdvancedTaperWidget)
    {
        if (AdvancedTaperWidget->objectName().isEmpty())
            AdvancedTaperWidget->setObjectName("AdvancedTaperWidget");
        AdvancedTaperWidget->resize(400, 639);
        verticalLayout = new QVBoxLayout(AdvancedTaperWidget);
        verticalLayout->setObjectName("verticalLayout");
        groupBox_5 = new QGroupBox(AdvancedTaperWidget);
        groupBox_5->setObjectName("groupBox_5");
        QSizePolicy sizePolicy(QSizePolicy::Preferred, QSizePolicy::Preferred);
        sizePolicy.setHorizontalStretch(0);
        sizePolicy.setVerticalStretch(0);
        sizePolicy.setHeightForWidth(groupBox_5->sizePolicy().hasHeightForWidth());
        groupBox_5->setSizePolicy(sizePolicy);
        groupBox_5->setMinimumSize(QSize(100, 400));
        groupBox_5->setMaximumSize(QSize(16777215, 400));
        verticalLayout_2 = new QVBoxLayout(groupBox_5);
        verticalLayout_2->setObjectName("verticalLayout_2");
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

        taperWidthSpinbox = new QSpinBox(frame_12);
        taperWidthSpinbox->setObjectName("taperWidthSpinbox");
        taperWidthSpinbox->setMinimum(-100);
        taperWidthSpinbox->setMaximum(100);
        taperWidthSpinbox->setValue(10);

        horizontalLayout_14->addWidget(taperWidthSpinbox);


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
        taperWidthSlider = new QSlider(frame_11);
        taperWidthSlider->setObjectName("taperWidthSlider");
        taperWidthSlider->setMinimum(-100);
        taperWidthSlider->setMaximum(100);
        taperWidthSlider->setValue(10);
        taperWidthSlider->setOrientation(Qt::Horizontal);

        horizontalLayout_3->addWidget(taperWidthSlider);


        verticalLayout_9->addWidget(frame_11);


        verticalLayout_19->addWidget(frame_2);

        frame_3 = new QFrame(groupBox_5);
        frame_3->setObjectName("frame_3");
        frame_3->setMaximumSize(QSize(16777215, 80));
        frame_3->setFrameShape(QFrame::StyledPanel);
        frame_3->setFrameShadow(QFrame::Raised);
        verticalLayout_10 = new QVBoxLayout(frame_3);
        verticalLayout_10->setSpacing(0);
        verticalLayout_10->setObjectName("verticalLayout_10");
        verticalLayout_10->setContentsMargins(4, 8, 4, 4);
        frame_13 = new QFrame(frame_3);
        frame_13->setObjectName("frame_13");
        frame_13->setMinimumSize(QSize(0, 0));
        frame_13->setMaximumSize(QSize(16777215, 30));
        frame_13->setFrameShape(QFrame::StyledPanel);
        frame_13->setFrameShadow(QFrame::Raised);
        horizontalLayout_15 = new QHBoxLayout(frame_13);
        horizontalLayout_15->setObjectName("horizontalLayout_15");
        horizontalLayout_15->setContentsMargins(4, 1, 4, 1);
        label_14 = new QLabel(frame_13);
        label_14->setObjectName("label_14");

        horizontalLayout_15->addWidget(label_14);

        taperHeightSpinbox = new QSpinBox(frame_13);
        taperHeightSpinbox->setObjectName("taperHeightSpinbox");
        taperHeightSpinbox->setMinimum(-100);
        taperHeightSpinbox->setMaximum(100);
        taperHeightSpinbox->setValue(10);

        horizontalLayout_15->addWidget(taperHeightSpinbox);


        verticalLayout_10->addWidget(frame_13);

        frame_14 = new QFrame(frame_3);
        frame_14->setObjectName("frame_14");
        frame_14->setMinimumSize(QSize(0, 0));
        frame_14->setMaximumSize(QSize(16777215, 30));
        frame_14->setFrameShape(QFrame::StyledPanel);
        frame_14->setFrameShadow(QFrame::Raised);
        horizontalLayout_4 = new QHBoxLayout(frame_14);
        horizontalLayout_4->setObjectName("horizontalLayout_4");
        horizontalLayout_4->setContentsMargins(4, 1, 4, 1);
        taperHeightSlider = new QSlider(frame_14);
        taperHeightSlider->setObjectName("taperHeightSlider");
        taperHeightSlider->setMinimum(-100);
        taperHeightSlider->setMaximum(100);
        taperHeightSlider->setValue(10);
        taperHeightSlider->setOrientation(Qt::Horizontal);

        horizontalLayout_4->addWidget(taperHeightSlider);


        verticalLayout_10->addWidget(frame_14);


        verticalLayout_19->addWidget(frame_3);

        hardTopTaperCheck = new QCheckBox(groupBox_5);
        hardTopTaperCheck->setObjectName("hardTopTaperCheck");

        verticalLayout_19->addWidget(hardTopTaperCheck);

        hardBottomTaperCheck = new QCheckBox(groupBox_5);
        hardBottomTaperCheck->setObjectName("hardBottomTaperCheck");

        verticalLayout_19->addWidget(hardBottomTaperCheck);

        verticalSpacer_2 = new QSpacerItem(20, 40, QSizePolicy::Minimum, QSizePolicy::Expanding);

        verticalLayout_19->addItem(verticalSpacer_2);


        verticalLayout_2->addLayout(verticalLayout_19);


        verticalLayout->addWidget(groupBox_5);

        verticalSpacer = new QSpacerItem(20, 40, QSizePolicy::Minimum, QSizePolicy::Expanding);

        verticalLayout->addItem(verticalSpacer);


        retranslateUi(AdvancedTaperWidget);

        QMetaObject::connectSlotsByName(AdvancedTaperWidget);
    } // setupUi

    void retranslateUi(QWidget *AdvancedTaperWidget)
    {
        AdvancedTaperWidget->setWindowTitle(QCoreApplication::translate("AdvancedTaperWidget", "Form", nullptr));
#if QT_CONFIG(tooltip)
        AdvancedTaperWidget->setToolTip(QCoreApplication::translate("AdvancedTaperWidget", "<html><head/><body><p>Taper the beginning and the end of your sticks, choose its aspect</p></body></html>", nullptr));
#endif // QT_CONFIG(tooltip)
        groupBox_5->setTitle(QCoreApplication::translate("AdvancedTaperWidget", "Tapered stick Edit", nullptr));
        label_13->setText(QCoreApplication::translate("AdvancedTaperWidget", "Base Width", nullptr));
        label_14->setText(QCoreApplication::translate("AdvancedTaperWidget", "Top Width", nullptr));
        hardTopTaperCheck->setText(QCoreApplication::translate("AdvancedTaperWidget", "hard top", nullptr));
        hardBottomTaperCheck->setText(QCoreApplication::translate("AdvancedTaperWidget", "hard bottom", nullptr));
    } // retranslateUi

};

namespace Ui {
    class AdvancedTaperWidget: public Ui_AdvancedTaperWidget {};
} // namespace Ui

QT_END_NAMESPACE

#endif // UI_ADVANCEDTAPERWIDGET_H
