#include "advancedtab.h"
#include "ui_advancedtab.h"
#include "myview.h"
#include "uiItems.h"
extern int W;
extern int H;
extern myView *V;
bool rectWSliderFlag = false;


advancedTab::advancedTab(QWidget *parent) :
    QWidget(parent),
    ui(new Ui::advancedTab)
{
    ui->setupUi(this);
    ui->rectWidthSlider->setRange(0,W);
    ui->rectWidthSpinbox->setRange(0,W);
    advancedRectSlider = ui->rectWidthSlider;
}

advancedTab::~advancedTab()
{
    delete ui;
}

void advancedTab::on_imgHOffsetSpinbox_valueChanged(int arg1)
{

}

void advancedTab::on_rectWidthSlider_valueChanged(int value)
{
    ui->rectWidthSpinbox->setValue(value);
    if(V->myAnimation->CurrentFrame()->CurrentStick()==nullptr)
        return;
    if(rectWSliderFlag == false){
        rectWSliderFlag = true;

        V->storeUndo();
    }
    V->myAnimation->CurrentFrame()->CurrentStick()->imgWScale = value;
    V->myAnimation->CurrentFrame()->CurrentStickFigure()->refresh();

}

void advancedTab::on_rectWidthSpinbox_valueChanged(int arg1)
{
    ui->rectWidthSlider->setValue(arg1);
    if(V->myAnimation->CurrentFrame()->CurrentStick()==nullptr)
        return;
    V->myAnimation->CurrentFrame()->CurrentStick()->imgWScale = arg1;
    V->myAnimation->CurrentFrame()->CurrentStickFigure()->refresh();
    ui->rectWidthSlider->setValue(arg1);
}

void advancedTab::on_rectWidthSlider_sliderReleased()
{
    rectWSliderFlag = false;
}
