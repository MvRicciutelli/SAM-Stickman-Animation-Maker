/********************************************************************************
** Form generated from reading UI file 'advancedcirclewidget.ui'
**
** Created by: Qt User Interface Compiler version 6.6.1
**
** WARNING! All changes made in this file will be lost when recompiling UI file!
********************************************************************************/

#ifndef UI_ADVANCEDCIRCLEWIDGET_H
#define UI_ADVANCEDCIRCLEWIDGET_H

#include <QtCore/QVariant>
#include <QtWidgets/QApplication>
#include <QtWidgets/QFrame>
#include <QtWidgets/QHBoxLayout>
#include <QtWidgets/QLabel>
#include <QtWidgets/QSlider>
#include <QtWidgets/QSpacerItem>
#include <QtWidgets/QSpinBox>
#include <QtWidgets/QVBoxLayout>
#include <QtWidgets/QWidget>

QT_BEGIN_NAMESPACE

class Ui_AdvancedCircleWidget
{
public:
    QVBoxLayout *verticalLayout;
    QLabel *label;
    QFrame *frame;
    QHBoxLayout *horizontalLayout;
    QSlider *circleSquashSlider;
    QSpinBox *circleSquashSpinbox;
    QSpacerItem *verticalSpacer;

    void setupUi(QWidget *AdvancedCircleWidget)
    {
        if (AdvancedCircleWidget->objectName().isEmpty())
            AdvancedCircleWidget->setObjectName("AdvancedCircleWidget");
        AdvancedCircleWidget->resize(400, 300);
        verticalLayout = new QVBoxLayout(AdvancedCircleWidget);
        verticalLayout->setObjectName("verticalLayout");
        label = new QLabel(AdvancedCircleWidget);
        label->setObjectName("label");

        verticalLayout->addWidget(label);

        frame = new QFrame(AdvancedCircleWidget);
        frame->setObjectName("frame");
        frame->setFrameShape(QFrame::StyledPanel);
        frame->setFrameShadow(QFrame::Raised);
        horizontalLayout = new QHBoxLayout(frame);
        horizontalLayout->setObjectName("horizontalLayout");
        circleSquashSlider = new QSlider(frame);
        circleSquashSlider->setObjectName("circleSquashSlider");
        circleSquashSlider->setMinimum(-100);
        circleSquashSlider->setMaximum(100);
        circleSquashSlider->setOrientation(Qt::Horizontal);

        horizontalLayout->addWidget(circleSquashSlider);

        circleSquashSpinbox = new QSpinBox(frame);
        circleSquashSpinbox->setObjectName("circleSquashSpinbox");
        circleSquashSpinbox->setMinimum(-100);
        circleSquashSpinbox->setMaximum(100);

        horizontalLayout->addWidget(circleSquashSpinbox);


        verticalLayout->addWidget(frame);

        verticalSpacer = new QSpacerItem(20, 40, QSizePolicy::Minimum, QSizePolicy::Expanding);

        verticalLayout->addItem(verticalSpacer);


        retranslateUi(AdvancedCircleWidget);

        QMetaObject::connectSlotsByName(AdvancedCircleWidget);
    } // setupUi

    void retranslateUi(QWidget *AdvancedCircleWidget)
    {
        AdvancedCircleWidget->setWindowTitle(QCoreApplication::translate("AdvancedCircleWidget", "Form", nullptr));
        label->setText(QCoreApplication::translate("AdvancedCircleWidget", "Circle Squash", nullptr));
    } // retranslateUi

};

namespace Ui {
    class AdvancedCircleWidget: public Ui_AdvancedCircleWidget {};
} // namespace Ui

QT_END_NAMESPACE

#endif // UI_ADVANCEDCIRCLEWIDGET_H
