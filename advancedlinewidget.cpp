#include "advancedlinewidget.h"
#include "ui_advancedlinewidget.h"
#include "myview.h"
#include "animation.h"
#include "stick.h"
#include "uiItems.h"
extern int W;
extern int H;

extern MyView *V;
AdvancedLineWidget::AdvancedLineWidget(QWidget *parent) :
    QWidget(parent),
    ui(new Ui::AdvancedLineWidget)
{
    ui->setupUi(this);
    edit_line_thickness_slider = ui->editLineThicknessSlider;
    line_edit_thickness_width_spinbox = ui->lineEditThicknessWidthSpinbox;
}

AdvancedLineWidget::~AdvancedLineWidget()
{
    delete ui;
}

void AdvancedLineWidget::on_editLineThicknessSlider_valueChanged(int value)
{
    if(V->my_animation->CurrentFrame()->CurrentStickFigure() != nullptr){
        if(V->my_animation->CurrentFrame()->CurrentStick() != nullptr){
            edit_line_thickness_slider->setRange(0,0.5*sqrt(pow(W,2)+pow(H,2)));
            line_edit_thickness_width_spinbox->setRange(0,0.5*sqrt(pow(W,2)+pow(H,2)));
            line_edit_thickness_width_spinbox->setValue(value);
            V->my_animation->CurrentFrame()->CurrentStick()->pen.setWidth(value);
            V->my_pen.setWidth(value);
            if(V->my_animation->CurrentFrame()->CurrentStick()->view)
                V->my_animation->CurrentFrame()->CurrentStick()->view->Refresh(1);
        }
    }
}

void AdvancedLineWidget::on_lineEditThicknessWidthSpinbox_editingFinished()
{
    edit_line_thickness_slider->setValue(line_edit_thickness_width_spinbox->value());
}
