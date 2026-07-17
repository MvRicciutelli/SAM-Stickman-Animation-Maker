#include "animationoptions.h"
#include "ui_animationoptions.h"

#include <QMessageBox>

float aspect_ratio = 1.0; ///< width/height captured at open, used to keep the ratio locked
/**
 * @details Pre-fills each control from the values MyMainWindow passes in and records the
 * starting aspect ratio, so the "lock aspect ratio" checkbox can keep width and height
 * proportional as the user edits either one.
 */
AnimationOptions::AnimationOptions(QWidget *parent, int W, int H, int FPS,int handles, int auto_save_input) :
    QWidget(parent),
    ui(new Ui::AnimationOptions)
{
    ui->setupUi(this);
    ui->widthSpinbox->setValue(W);
    ui->heightSpinbox->setValue(H);
    ui->fpsSpinbox->setValue(FPS);
    ui->handlesSizeSpinBox->setValue(handles);
    // usa il parametro passato dal chiamante, non il default del membro: altrimenti la
    // dialog mostrava sempre 50 invece dell'intervallo di autosave realmente in uso
    ui->autoSaveSpinbox->setValue(auto_save_input);
    aspect_ratio = (float)W/(float)H;
}

AnimationOptions::~AnimationOptions()
{
    delete ui;
}

/**
 * @details OK handler: copies every control's value onto the public fields, reads which
 * reposition radio is selected, then asks for confirmation (the rescale can't be undone)
 * before emitting DialogOut() — caught by MyMainWindow::GetAnimationParams(). option_val
 * defaults to NOREPO so nothing odd happens if somehow no radio is checked.
 */
void AnimationOptions::on_okBtn_clicked()
{
    int option_val = NOREPO;
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

// gli spinbox width/height applicano l'eventuale blocco del rapporto d'aspetto solo a
// editing finito (sotto), non ad ogni valueChanged, per non ostacolare la digitazione
void AnimationOptions::on_widthSpinbox_valueChanged(int arg1)
{
    Q_UNUSED(arg1);
}

void AnimationOptions::on_heightSpinbox_valueChanged(int arg1)
{
    Q_UNUSED(arg1);
}

/**
 * @details When the ratio lock is on, snaps the height to match the width just edited,
 * using the aspect_ratio captured when the dialog opened.
 */
void AnimationOptions::on_widthSpinbox_editingFinished()
{
    if(ui->lockAspectRatioCheckbox->isChecked()){
        ui->heightSpinbox->setValue((int)qRound(ui->widthSpinbox->value()/aspect_ratio));
    }
}

/**
 * @details Mirror of on_widthSpinbox_editingFinished(): snaps the width to match the
 * height just edited when the ratio lock is on.
 */
void AnimationOptions::on_heightSpinbox_editingFinished()
{
    if(ui->lockAspectRatioCheckbox->isChecked()){
        ui->widthSpinbox->setValue((int)qRound(ui->heightSpinbox->value()*aspect_ratio));
    }
}

void AnimationOptions::on_handlesSizeSpinBox_valueChanged(int arg1)
{
    Q_UNUSED(arg1);
}
