#include "advancedtab.h"
#include "ui_advancedtab.h"
#include "myview.h"
#include "uiItems.h"
extern int W;
extern int H;
extern MyView *V;
bool rectWSliderFlag = false;


AdvancedTab::AdvancedTab(QWidget *parent) :
    QWidget(parent),
    ui(new Ui::AdvancedTab)
{
    ui->setupUi(this);
    ui->rectWidthSlider->setRange(0,W);
    ui->rectWidthSpinbox->setRange(0,W);
    advanced_rect_slider = ui->rectWidthSlider;
}

AdvancedTab::~AdvancedTab()
{
    delete ui;
}

void AdvancedTab::on_imgHOffsetSpinbox_valueChanged(int arg1)
{

}

void AdvancedTab::on_rectWidthSlider_valueChanged(int value)
{
    ui->rectWidthSpinbox->setValue(value);
    if(V->my_animation->CurrentFrame()->CurrentStick()==nullptr)
        return;
    if(rectWSliderFlag == false){
        rectWSliderFlag = true;

        V->StoreUndo();
    }
    V->my_animation->CurrentFrame()->CurrentStick()->img_w_scale = value;
    V->my_animation->CurrentFrame()->CurrentStickFigure()->Refresh();

}

void AdvancedTab::on_rectWidthSpinbox_valueChanged(int arg1)
{
    ui->rectWidthSlider->setValue(arg1);
    if(V->my_animation->CurrentFrame()->CurrentStick()==nullptr)
        return;
    V->my_animation->CurrentFrame()->CurrentStick()->img_w_scale = arg1;
    V->my_animation->CurrentFrame()->CurrentStickFigure()->Refresh();
    ui->rectWidthSlider->setValue(arg1);
}

void AdvancedTab::on_rectWidthSlider_sliderReleased()
{
    rectWSliderFlag = false;
}
