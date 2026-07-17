#include "advancedlinewidget.h"
#include "ui_advancedlinewidget.h"
#include "myview.h"
#include "animation.h"
#include "stick.h"
#include "uiItems.h"
extern qint32 W;
extern qint32 H;

extern MyView *V;
/**
 * @details Publishes this panel's slider and spinbox to the uiItems.h globals so the
 * rest of the app can drive them; ranges are (re)set later in the value-changed slot.
 */
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

/**
 * @details Slider drag: clamps the range to the canvas diagonal, mirrors the value into
 * the spinbox, then sets both the current stick's pen width and the canvas default pen
 * (V->my_pen) so subsequent sticks inherit it; the stick's view is refreshed in drag
 * mode (Refresh(1)) to avoid ghosting. Guarded so it does nothing without a current stick.
 */
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

/**
 * @details Spinbox commit: pushes its value onto the slider, whose valueChanged slot
 * then does the actual pen-width work.
 */
void AdvancedLineWidget::on_lineEditThicknessWidthSpinbox_editingFinished()
{
    edit_line_thickness_slider->setValue(line_edit_thickness_width_spinbox->value());
}
