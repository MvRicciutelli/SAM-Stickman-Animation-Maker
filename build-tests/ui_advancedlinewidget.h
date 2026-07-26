/********************************************************************************
** Form generated from reading UI file 'advancedlinewidget.ui'
**
** Created by: Qt User Interface Compiler version 6.6.1
**
** WARNING! All changes made in this file will be lost when recompiling UI file!
********************************************************************************/

#ifndef UI_ADVANCEDLINEWIDGET_H
#define UI_ADVANCEDLINEWIDGET_H

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

class Ui_AdvancedLineWidget
{
public:
    QVBoxLayout *verticalLayout;
    QLabel *label;
    QFrame *frame;
    QHBoxLayout *horizontalLayout;
    QSlider *editLineThicknessSlider;
    QSpinBox *lineEditThicknessWidthSpinbox;
    QSpacerItem *verticalSpacer;

    void setupUi(QWidget *AdvancedLineWidget)
    {
        if (AdvancedLineWidget->objectName().isEmpty())
            AdvancedLineWidget->setObjectName("AdvancedLineWidget");
        AdvancedLineWidget->resize(400, 300);
        verticalLayout = new QVBoxLayout(AdvancedLineWidget);
        verticalLayout->setObjectName("verticalLayout");
        label = new QLabel(AdvancedLineWidget);
        label->setObjectName("label");

        verticalLayout->addWidget(label);

        frame = new QFrame(AdvancedLineWidget);
        frame->setObjectName("frame");
        frame->setFrameShape(QFrame::StyledPanel);
        frame->setFrameShadow(QFrame::Raised);
        horizontalLayout = new QHBoxLayout(frame);
        horizontalLayout->setObjectName("horizontalLayout");
        editLineThicknessSlider = new QSlider(frame);
        editLineThicknessSlider->setObjectName("editLineThicknessSlider");
        editLineThicknessSlider->setOrientation(Qt::Horizontal);

        horizontalLayout->addWidget(editLineThicknessSlider);

        lineEditThicknessWidthSpinbox = new QSpinBox(frame);
        lineEditThicknessWidthSpinbox->setObjectName("lineEditThicknessWidthSpinbox");

        horizontalLayout->addWidget(lineEditThicknessWidthSpinbox);


        verticalLayout->addWidget(frame);

        verticalSpacer = new QSpacerItem(20, 40, QSizePolicy::Minimum, QSizePolicy::Expanding);

        verticalLayout->addItem(verticalSpacer);


        retranslateUi(AdvancedLineWidget);

        QMetaObject::connectSlotsByName(AdvancedLineWidget);
    } // setupUi

    void retranslateUi(QWidget *AdvancedLineWidget)
    {
        AdvancedLineWidget->setWindowTitle(QCoreApplication::translate("AdvancedLineWidget", "Form", nullptr));
        label->setText(QCoreApplication::translate("AdvancedLineWidget", "Line Thickness", nullptr));
    } // retranslateUi

};

namespace Ui {
    class AdvancedLineWidget: public Ui_AdvancedLineWidget {};
} // namespace Ui

QT_END_NAMESPACE

#endif // UI_ADVANCEDLINEWIDGET_H
