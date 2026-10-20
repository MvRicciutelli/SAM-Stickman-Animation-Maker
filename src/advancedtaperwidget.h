#ifndef ADVANCEDTAPERWIDGET_H
#define ADVANCEDTAPERWIDGET_H

#include <QWidget>

namespace Ui {
class AdvancedTaperWidget;
}

class AdvancedTaperWidget : public QWidget
{
    Q_OBJECT

public:
    explicit AdvancedTaperWidget(QWidget *parent = nullptr);
    ~AdvancedTaperWidget();

private slots:
    void on_taperWidthSlider_valueChanged(int value);

    void on_taperWidthSlider_sliderReleased();

    void on_taperHeightSpinbox_valueChanged(int arg1);

    void on_taperHeightSlider_valueChanged(int value);

    void on_taperHeightSlider_sliderReleased();

    void on_taperWidthSpinbox_valueChanged(int arg1);

    void on_hardTopTaperCheck_stateChanged(int arg1);

    void on_hardBottomTaperCheck_stateChanged(int arg1);

private:
    Ui::AdvancedTaperWidget *ui;
    // true while the respective slider drag is in progress, so only its FIRST step
    // snapshots undo (were file-scope globals in the .cpp; only this class touches them)
    bool taper_w_slider_flag = false;
    bool taper_h_slider_flag = false;
};

#endif // ADVANCEDTAPERWIDGET_H
