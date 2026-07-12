#ifndef ADVANCEDTAB_H
#define ADVANCEDTAB_H
#include "myview.h"
#include <QWidget>

namespace Ui {
class AdvancedTab;
}

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
