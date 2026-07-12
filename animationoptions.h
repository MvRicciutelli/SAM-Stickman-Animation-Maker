#ifndef ANIMATIONOPTIONS_H
#define ANIMATIONOPTIONS_H

#include <QWidget>
#include <QObject>

#define NOREPO 0
#define REPO 1
#define REPOSCALE 2
namespace Ui {
class AnimationOptions;
}

class AnimationOptions : public QWidget
{
    Q_OBJECT

public:
    explicit AnimationOptions(QWidget *parent = nullptr, int W =800, int H = 600, int FPS = 24,int handles = 10, int autoSaveInput = 50);
    ~AnimationOptions();
    int width = 1;
    int height = 1;
    int fps = 24;
    int autoSaveInterval = 50;
    int handleSize = 10;
private slots:
    void on_okBtn_clicked();

    void on_widthSpinbox_valueChanged(int arg1);

    void on_heightSpinbox_valueChanged(int arg1);

    void on_widthSpinbox_editingFinished();

    void on_heightSpinbox_editingFinished();

    void on_handlesSizeSpinBox_valueChanged(int arg1);

private:
    Ui::AnimationOptions *ui;
signals:
    void DialogOut(int optionVal);
};

#endif // ANIMATIONOPTIONS_H
