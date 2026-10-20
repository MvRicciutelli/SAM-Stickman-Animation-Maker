#ifndef ADVANCEDCIRCLEWIDGET_H
#define ADVANCEDCIRCLEWIDGET_H

#include <QWidget>

namespace Ui {
class AdvancedCircleWidget;
}

class AdvancedCircleWidget : public QWidget
{
    Q_OBJECT

public:
    explicit AdvancedCircleWidget(QWidget *parent = nullptr);
    ~AdvancedCircleWidget();

private slots:
    void on_circleSquashSlider_valueChanged(int value);

private:
    Ui::AdvancedCircleWidget *ui;
};

#endif // ADVANCEDCIRCLEWIDGET_H
