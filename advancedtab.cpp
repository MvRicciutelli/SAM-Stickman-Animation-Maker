#include "advancedtab.h"
#include "ui_advancedtab.h"
#include "myview.h"
#include "uiItems.h"
extern qint32 W;
extern qint32 H;
extern MyView *V;
bool rect_w_slider_flag = false; ///< true while a slider drag is in progress, so only its FIRST step snapshots undo

/**
 * @details Ranges the rect-width slider/spinbox to the canvas width and publishes the
 * slider to the global `advanced_rect_slider` so MyView::DrawCmd() can seed a new RECT
 * stick's width from it.
 */
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
    Q_UNUSED(arg1);
}

/**
 * @details Slider drag: mirrors the value into the spinbox and sets the current stick's
 * img_w_scale, then refreshes the figure. Undo is snapshotted only on the drag's FIRST
 * step (guarded by rect_w_slider_flag, reset in on_rectWidthSlider_sliderReleased()),
 * so a whole drag is one undoable edit rather than one per intermediate value.
 */
void AdvancedTab::on_rectWidthSlider_valueChanged(int value)
{
    ui->rectWidthSpinbox->setValue(value);
    if(V->my_animation->CurrentFrame()->CurrentStick()==nullptr)
        return;
    if(rect_w_slider_flag == false){
        rect_w_slider_flag = true;

        V->StoreUndo();
    }
    V->my_animation->CurrentFrame()->CurrentStick()->img_w_scale = value;
    V->my_animation->CurrentFrame()->CurrentStickFigure()->Refresh();

}

/**
 * @details Spinbox edit: keeps the slider in step and applies the same img_w_scale
 * change (no separate undo snapshot — the spinbox is a discrete edit, not a drag).
 */
void AdvancedTab::on_rectWidthSpinbox_valueChanged(int arg1)
{
    ui->rectWidthSlider->setValue(arg1);
    if(V->my_animation->CurrentFrame()->CurrentStick()==nullptr)
        return;
    V->my_animation->CurrentFrame()->CurrentStick()->img_w_scale = arg1;
    V->my_animation->CurrentFrame()->CurrentStickFigure()->Refresh();
    ui->rectWidthSlider->setValue(arg1);
}

/**
 * @details End of a slider drag: re-arms the undo guard so the next drag snapshots again.
 */
void AdvancedTab::on_rectWidthSlider_sliderReleased()
{
    rect_w_slider_flag = false;
}
