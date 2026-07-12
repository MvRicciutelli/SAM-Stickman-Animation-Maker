#include "advancedcirclewidget.h"
#include "ui_advancedcirclewidget.h"
#include "uiItems.h"
extern MyView *V;

advancedCircleWidget::advancedCircleWidget(QWidget *parent) :
    QWidget(parent),
    ui(new Ui::advancedCircleWidget)
{
    ui->setupUi(this);
    circleSquashSlider = ui->circleSquashSlider;
    circleSquashSpinbox = ui->circleSquashSpinbox;
}

advancedCircleWidget::~advancedCircleWidget()
{
    delete ui;
}

void advancedCircleWidget::on_circleSquashSlider_valueChanged(int value)
{
    circleSquashSpinbox->setValue(value);
    if(V->myAnimation->CurrentFrame()->CurrentStickFigure()!=nullptr){
        if(V->myAnimation->CurrentFrame()->CurrentStick()!=nullptr){
            if(V->myAnimation->CurrentFrame()->CurrentStick()->stickType== StickData::CIRCLE){
                if(value<0){

                    V->myAnimation->CurrentFrame()->CurrentStick()->imgWScale = (1-(-(float)value)/100);
                }
                else{
                    V->myAnimation->CurrentFrame()->CurrentStick()->imgWScale = (float)(1+(float)value/20);
                }
                if(V->myAnimation->CurrentFrame()->CurrentStick()->view)
                    V->myAnimation->CurrentFrame()->CurrentStick()->view->refresh(0);
            }
        }
    }
}
