#ifndef ADVANCEDLINEWIDGET_H
#define ADVANCEDLINEWIDGET_H

#include <QWidget>

namespace Ui {
class AdvancedLineWidget;
}

class AdvancedLineWidget : public QWidget
{
    Q_OBJECT

public:
    explicit AdvancedLineWidget(QWidget *parent = nullptr);
    ~AdvancedLineWidget();

private slots:
    void on_editLineThicknessSlider_valueChanged(int value);

    void on_lineEditThicknessWidthSpinbox_editingFinished();

private:
    Ui::AdvancedLineWidget *ui;
};

#endif // ADVANCEDLINEWIDGET_H
