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

advancedTaperWidget::advancedTaperWidget(QWidget *parent) :
    QWidget(parent),
    ui(new Ui::advancedTaperWidget)
{
    ui->setupUi(this);
    ui->taperWidthSlider->setRange(0,W);
    ui->taperWidthSpinbox->setRange(0,W);
    ui->taperHeightSlider->setRange(0,W);
    ui->taperHeightSpinbox->setRange(0,W);
    taperWSlider = ui->taperWidthSlider;
    taperHSlider = ui->taperHeightSlider;
    hardTopCheck = ui->hardTopTaperCheck;
    hardBottomCheck = ui->hardBottomTaperCheck;
}

advancedTaperWidget::~advancedTaperWidget()
{
    delete ui;
}

void advancedTaperWidget::on_taperWidthSlider_valueChanged(int value)
{
    ui->taperWidthSpinbox->setValue(value);
    if(V->myAnimation->CurrentFrame()->CurrentStick()==nullptr)
        return;
    if(taperWSliderFlag == false){
        taperWSliderFlag = true;

        V->storeUndo();
    }
    V->myAnimation->CurrentFrame()->CurrentStick()->imgWScale = value;
    V->myAnimation->CurrentFrame()->CurrentStickFigure()->refresh();

}

void advancedTaperWidget::on_taperWidthSlider_sliderReleased()
{
    taperWSliderFlag = false;
    V->myAnimation->currentFrame->updateIcon();
}

void advancedTaperWidget::on_taperHeightSpinbox_valueChanged(int arg1)
{
    ui->taperHeightSlider->setValue(arg1);
    if(V->myAnimation->CurrentFrame()->CurrentStick()==nullptr)
        return;
    V->myAnimation->CurrentFrame()->CurrentStick()->imgHScale = arg1;
    V->myAnimation->CurrentFrame()->CurrentStickFigure()->refresh();
    ui->taperHeightSlider->setValue(arg1);
}

void advancedTaperWidget::on_taperHeightSlider_valueChanged(int value)
{
    ui->taperHeightSpinbox->setValue(value);
    if(V->myAnimation->CurrentFrame()->CurrentStick()==nullptr)
        return;
    if(taperHSliderFlag == false){
        taperHSliderFlag = true;

        V->storeUndo();
    }
    V->myAnimation->CurrentFrame()->CurrentStick()->imgHScale = value;
    V->myAnimation->CurrentFrame()->CurrentStickFigure()->refresh();
}

void advancedTaperWidget::on_taperHeightSlider_sliderReleased()
{
    taperHSliderFlag = false;
    V->myAnimation->currentFrame->updateIcon();
}

void advancedTaperWidget::on_taperWidthSpinbox_valueChanged(int arg1)
{
    ui->taperWidthSlider->setValue(arg1);
    if(V->myAnimation->CurrentFrame()->CurrentStick()==nullptr)
        return;
    V->myAnimation->CurrentFrame()->CurrentStick()->imgWScale = arg1;
    V->myAnimation->CurrentFrame()->CurrentStickFigure()->refresh();
    ui->taperWidthSlider->setValue(arg1);
}

void advancedTaperWidget::on_hardTopTaperCheck_stateChanged(int arg1)
{
    if(V->myAnimation->CurrentFrame()->CurrentStick()==nullptr)
        return;
    V->storeUndo();
    V->myAnimation->CurrentFrame()->CurrentStick()->hardTop=((bool) arg1);
    V->myAnimation->CurrentFrame()->CurrentStickFigure()->refresh();
    V->myAnimation->currentFrame->updateIcon();
}

void advancedTaperWidget::on_hardBottomTaperCheck_stateChanged(int arg1)
{
   if(V->myAnimation->CurrentFrame()->CurrentStick()==nullptr)
       return;
   V->storeUndo();
   V->myAnimation->CurrentFrame()->CurrentStick()->hardBottom=((bool) arg1);
   V->myAnimation->CurrentFrame()->CurrentStickFigure()->refresh();
   V->myAnimation->currentFrame->updateIcon();
}
