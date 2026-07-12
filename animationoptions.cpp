#include "animationoptions.h"
#include "ui_animationoptions.h"

#include <QMessageBox>

float AR = 1.0;
AnimationOptions::AnimationOptions(QWidget *parent, int W, int H, int FPS,int handles, int auto_save_input) :
    QWidget(parent),
    ui(new Ui::AnimationOptions)
{
    ui->setupUi(this);
    ui->widthSpinbox->setValue(W);
    ui->heightSpinbox->setValue(H);
    ui->fpsSpinbox->setValue(FPS);
    ui->handlesSizeSpinBox->setValue(handles);
    ui->autoSaveSpinbox->setValue(auto_save_interval);
    AR = (float)W/(float)H;
}

AnimationOptions::~AnimationOptions()
{
    delete ui;
}

void AnimationOptions::on_okBtn_clicked()
{
    int option_val;
    width               = ui->widthSpinbox->value();
    height              = ui->heightSpinbox->value();
    fps                 = ui->fpsSpinbox->value();
    auto_save_interval    = ui->autoSaveSpinbox->value();
    handle_size          = ui->handlesSizeSpinBox->value();

    if(ui->repositionBtn->isChecked())
        option_val = REPO;
    else if(ui->rescaleAndRepositionBtn->isChecked())
        option_val = REPOSCALE;
    else if(ui->noneBtn->isChecked())
        option_val = NOREPO;
    int ret  = QMessageBox::warning(this, tr("My Application"),
                                    tr("This will rescale the animation, this action cannot be undone.\n"
                                       "Do you want to proceed?"),
                                    QMessageBox::Yes | QMessageBox::No
                                    );
    if(ret == QMessageBox::Yes)
        emit DialogOut(option_val);

}

void AnimationOptions::on_widthSpinbox_valueChanged(int arg1)
{

}

void AnimationOptions::on_heightSpinbox_valueChanged(int arg1)
{

}

void AnimationOptions::on_widthSpinbox_editingFinished()
{
    if(ui->lockAspectRatioCheckbox->isChecked()){
        ui->heightSpinbox->setValue((int)qRound(ui->widthSpinbox->value()/AR));
    }
}

void AnimationOptions::on_heightSpinbox_editingFinished()
{
    if(ui->lockAspectRatioCheckbox->isChecked()){
        ui->widthSpinbox->setValue((int)qRound(ui->heightSpinbox->value()*AR));
    }
}

void AnimationOptions::on_handlesSizeSpinBox_valueChanged(int arg1)
{

}
