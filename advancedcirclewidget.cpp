#include "advancedcirclewidget.h"
#include "ui_advancedcirclewidget.h"
#include "uiItems.h"
extern MyView *V;

AdvancedCircleWidget::AdvancedCircleWidget(QWidget *parent) :
    QWidget(parent),
    ui(new Ui::AdvancedCircleWidget)
{
    ui->setupUi(this);
    circle_squash_slider = ui->circleSquashSlider;
    circle_squash_spinbox = ui->circleSquashSpinbox;
}

AdvancedCircleWidget::~AdvancedCircleWidget()
{
    delete ui;
}

void AdvancedCircleWidget::on_circleSquashSlider_valueChanged(int value)
{
    circle_squash_spinbox->setValue(value);
    if(V->my_animation->CurrentFrame()->CurrentStickFigure()!=nullptr){
        if(V->my_animation->CurrentFrame()->CurrentStick()!=nullptr){
            if(V->my_animation->CurrentFrame()->CurrentStick()->stick_type== StickData::CIRCLE){
                if(value<0){

                    V->my_animation->CurrentFrame()->CurrentStick()->img_w_scale = (1-(-(float)value)/100);
                }
                else{
                    V->my_animation->CurrentFrame()->CurrentStick()->img_w_scale = (float)(1+(float)value/20);
                }
                if(V->my_animation->CurrentFrame()->CurrentStick()->view)
                    V->my_animation->CurrentFrame()->CurrentStick()->view->refresh(0);
            }
        }
    }
}
