#ifndef MYMAINWINDOW_H
#define MYMAINWINDOW_H

#include <QMainWindow>
#include "myview.h"
#include "widget.h"
#include <QObject>
#include <checkupdate.h>
namespace Ui {
class MyMainWindow;
}

class MyMainWindow : public QMainWindow
{
    Q_OBJECT

public:
    explicit MyMainWindow(QWidget *parent = nullptr);
    ~MyMainWindow();
    MyView *view;
    Widget *central_w;

    void DoConnections();
    void ExportImageSequence(QString path, int option);
    void ExportUsingFFMPEG();
    void ExportAvi();
public slots:
    void GetAnimationParams(int option_val);
    void StartFFMPEGRender(QStringList options);
    void AutoSave();
    void UpdatePresent(QString ver);
    void ImageRenderOver(int option);
private slots:
    void on_actionsave_triggered();

    void on_actionload_triggered();

    void on_actionsave_animation_triggered();

    void on_actionload_animation_triggered();

    void on_actionexport_as_AVI_file_triggered();

    void on_actionsave_animation_current_triggered();

    void on_actionNew_Blank_Animation_triggered();

    void on_actionAnimation_Options_triggered();

    void on_actionExport_video_using_FFMPEG_triggered();

    void on_actionExport_as_image_sequence_triggered();

    void on_actionAbout_triggered();


    void on_actionTerms_and_Conditions_triggered();

private:
    Ui::MyMainWindow *ui;


    // QWidget interface
protected:
    virtual void closeEvent(QCloseEvent *event) override;
};

#endif // MYMAINWINDOW_H
