#ifndef ADVANCEDTAB_H
#define ADVANCEDTAB_H
#include "myview.h"
#include <QWidget>

namespace Ui {
class AdvancedTab;
}

/**
 * @brief Side-panel tab for the extra parameters of shaped sticks (currently the
 *        rectangle/image width).
 *
 * Part of the tool panels in widget.h. Its rect-width slider is published to the global
 * `advanced_rect_slider` (uiItems.h) so MyView::DrawCmd() can read it when creating a
 * RECT stick; its slots push edits onto the current stick's `img_w_scale` (via the
 * global canvas `V`) and refresh it. The slider drag is wrapped in a single undo step
 * — see the slider-flag note in the .cpp.
 */
class AdvancedTab : public QWidget
{
    Q_OBJECT

public:
    explicit AdvancedTab(QWidget *parent = nullptr);
    ~AdvancedTab();

private slots:
    void on_imgHOffsetSpinbox_valueChanged(int arg1);

    void on_rectWidthSlider_valueChanged(int value);

    void on_rectWidthSpinbox_valueChanged(int arg1);

    void on_rectWidthSlider_sliderReleased();

private:
    Ui::AdvancedTab *ui;

};

#endif // ADVANCEDTAB_H
