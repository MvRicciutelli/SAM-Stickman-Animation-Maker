#include "advancedtaperwidget.h"
#include "ui_advancedtaperwidget.h"
#include "myview.h"
#include "uiItems.h"
#include <QCheckBox>
extern int W;
extern int H;
extern MyView *V;
bool taperWSliderFlag = false;
bool taperHSliderFlag = false;

AdvancedTaperWidget::AdvancedTaperWidget(QWidget *parent) :
    QWidget(parent),
    ui(new Ui::AdvancedTaperWidget)
{
    ui->setupUi(this);
    ui->taperWidthSlider->setRange(0,W);
    ui->taperWidthSpinbox->setRange(0,W);
    ui->taperHeightSlider->setRange(0,W);
    ui->taperHeightSpinbox->setRange(0,W);
    taper_w_slider = ui->taperWidthSlider;
    taper_h_slider = ui->taperHeightSlider;
    hard_top_check = ui->hardTopTaperCheck;
    hard_bottom_check = ui->hardBottomTaperCheck;
}

AdvancedTaperWidget::~AdvancedTaperWidget()
{
    delete ui;
}

void AdvancedTaperWidget::on_taperWidthSlider_valueChanged(int value)
{
    ui->taperWidthSpinbox->setValue(value);
    if(V->my_animation->CurrentFrame()->CurrentStick()==nullptr)
        return;
    if(taperWSliderFlag == false){
        taperWSliderFlag = true;

        V->storeUndo();
    }
    V->my_animation->CurrentFrame()->CurrentStick()->img_w_scale = value;
    V->my_animation->CurrentFrame()->CurrentStickFigure()->Refresh();

}

void AdvancedTaperWidget::on_taperWidthSlider_sliderReleased()
{
    taperWSliderFlag = false;
    V->my_animation->current_frame->updateIcon();
}

void AdvancedTaperWidget::on_taperHeightSpinbox_valueChanged(int arg1)
{
    ui->taperHeightSlider->setValue(arg1);
    if(V->my_animation->CurrentFrame()->CurrentStick()==nullptr)
        return;
    V->my_animation->CurrentFrame()->CurrentStick()->img_h_scale = arg1;
    V->my_animation->CurrentFrame()->CurrentStickFigure()->Refresh();
    ui->taperHeightSlider->setValue(arg1);
}

void AdvancedTaperWidget::on_taperHeightSlider_valueChanged(int value)
{
    ui->taperHeightSpinbox->setValue(value);
    if(V->my_animation->CurrentFrame()->CurrentStick()==nullptr)
        return;
    if(taperHSliderFlag == false){
        taperHSliderFlag = true;

        V->storeUndo();
    }
    V->my_animation->CurrentFrame()->CurrentStick()->img_h_scale = value;
    V->my_animation->CurrentFrame()->CurrentStickFigure()->Refresh();
}

void AdvancedTaperWidget::on_taperHeightSlider_sliderReleased()
{
    taperHSliderFlag = false;
    V->my_animation->current_frame->updateIcon();
}

void AdvancedTaperWidget::on_taperWidthSpinbox_valueChanged(int arg1)
{
    ui->taperWidthSlider->setValue(arg1);
    if(V->my_animation->CurrentFrame()->CurrentStick()==nullptr)
        return;
    V->my_animation->CurrentFrame()->CurrentStick()->img_w_scale = arg1;
    V->my_animation->CurrentFrame()->CurrentStickFigure()->Refresh();
    ui->taperWidthSlider->setValue(arg1);
}

void AdvancedTaperWidget::on_hardTopTaperCheck_stateChanged(int arg1)
{
    if(V->my_animation->CurrentFrame()->CurrentStick()==nullptr)
        return;
    V->storeUndo();
    V->my_animation->CurrentFrame()->CurrentStick()->hard_top=((bool) arg1);
    V->my_animation->CurrentFrame()->CurrentStickFigure()->Refresh();
    V->my_animation->current_frame->updateIcon();
}

void AdvancedTaperWidget::on_hardBottomTaperCheck_stateChanged(int arg1)
{
   if(V->my_animation->CurrentFrame()->CurrentStick()==nullptr)
       return;
   V->storeUndo();
   V->my_animation->CurrentFrame()->CurrentStick()->hard_bottom=((bool) arg1);
   V->my_animation->CurrentFrame()->CurrentStickFigure()->Refresh();
   V->my_animation->current_frame->updateIcon();
}
