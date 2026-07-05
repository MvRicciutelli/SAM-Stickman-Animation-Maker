#include "advancedtaperwidget.h"
#include "ui_advancedtaperwidget.h"
#include "myview.h"
#include "uiItems.h"
#include <QCheckBox>
#define CS V->myAnimation->currentFrame->currentStickFigure->currentStick
#define CURRENTSTICKFIGURE V->myAnimation->currentFrame->currentStickFigure
#define CURRENTFRAME V->myAnimation->currentFrame
extern int W;
extern int H;
extern myView *V;
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
    if(CS==nullptr)
        return;
    if(taperWSliderFlag == false){
        taperWSliderFlag = true;

        V->storeUndo();
    }
    CS->imgWScale = value;
    CURRENTSTICKFIGURE->refresh();

}

void advancedTaperWidget::on_taperWidthSlider_sliderReleased()
{
    taperWSliderFlag = false;
    V->myAnimation->currentFrame->updateIcon();
}

void advancedTaperWidget::on_taperHeightSpinbox_valueChanged(int arg1)
{
    ui->taperHeightSlider->setValue(arg1);
    if(CS==nullptr)
        return;
    CS->imgHScale = arg1;
    CURRENTSTICKFIGURE->refresh();
    ui->taperHeightSlider->setValue(arg1);
}

void advancedTaperWidget::on_taperHeightSlider_valueChanged(int value)
{
    ui->taperHeightSpinbox->setValue(value);
    if(CS==nullptr)
        return;
    if(taperHSliderFlag == false){
        taperHSliderFlag = true;

        V->storeUndo();
    }
    CS->imgHScale = value;
    CURRENTSTICKFIGURE->refresh();
}

void advancedTaperWidget::on_taperHeightSlider_sliderReleased()
{
    taperHSliderFlag = false;
    V->myAnimation->currentFrame->updateIcon();
}

void advancedTaperWidget::on_taperWidthSpinbox_valueChanged(int arg1)
{
    ui->taperWidthSlider->setValue(arg1);
    if(CS==nullptr)
        return;
    CS->imgWScale = arg1;
    CURRENTSTICKFIGURE->refresh();
    ui->taperWidthSlider->setValue(arg1);
}

void advancedTaperWidget::on_hardTopTaperCheck_stateChanged(int arg1)
{
    if(CS==nullptr)
        return;
    V->storeUndo();
    CS->hardTop=((bool) arg1);
    CURRENTSTICKFIGURE->refresh();
    V->myAnimation->currentFrame->updateIcon();
}

void advancedTaperWidget::on_hardBottomTaperCheck_stateChanged(int arg1)
{
   if(CS==nullptr)
       return;
   V->storeUndo();
   CS->hardBottom=((bool) arg1);
   CURRENTSTICKFIGURE->refresh();
   V->myAnimation->currentFrame->updateIcon();
}
