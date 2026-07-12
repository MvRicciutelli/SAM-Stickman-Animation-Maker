#ifndef FFMPEGEXPORT_H
#define FFMPEGEXPORT_H

#include <QWidget>

namespace Ui {
class FfmpegExport;
}

class FfmpegExport : public QWidget
{
    Q_OBJECT

public:
    explicit FfmpegExport(QWidget *parent = nullptr);
    ~FfmpegExport();

private slots:
    void on_exportVideoBtn_clicked();

    void on_selectFolderBtn_clicked();

    void on_aviRadioButton_clicked();

    void on_mp4RadioBtn_clicked();

    void on_vmwRadioBtn_clicked();

    void on_gifRadioBtn_clicked();

private:
    Ui::FfmpegExport *ui;
 signals:
  void  ExportVideo(QStringList options);
};
void EmptyDirectory(QString path);
#endif // FFMPEGEXPORT_H
