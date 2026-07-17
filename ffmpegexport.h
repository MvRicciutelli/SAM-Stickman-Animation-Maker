#ifndef FFMPEGEXPORT_H
#define FFMPEGEXPORT_H

#include <QWidget>

namespace Ui {
class FfmpegExport;
}

/**
 * @brief The ffmpeg export dialog: picks a container/codec and an output file, then
 *        hands the assembled ffmpeg command line back to the main window.
 *
 * Opened (single-instance) by MyMainWindow::ExportUsingFFMPEG(). The format radios
 * (mp4/wmv/avi/gif) choose the codec and keep the chosen output file's extension in
 * step; "Export" builds the ffmpeg argument list and emits ExportVideo(), which
 * MyMainWindow::StartFFMPEGRender() renders the frames for and then runs ffmpeg on.
 * WA_DeleteOnClose is set so closing the window actually frees the dialog.
 */
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

    /**
     * @brief Swaps the chosen output file's extension for a new one.
     * @param extension new extension without the dot (e.g. "mp4", "avi").
     *
     * Shared by the four format-radio slots: if an output file has been picked, its part
     * after the last '.' is replaced so the name always matches the selected format.
     * No-op when no file is chosen yet or the name has no '.'.
     */
    void ReplaceOutputExtension(const QString &extension);
 signals:
  /**
   * @brief Emitted on Export with the full ffmpeg argument list.
   * @param options ffmpeg command line (framerate, format, size, input pattern, codec,
   *        quality, output file); consumed by MyMainWindow::StartFFMPEGRender().
   */
  void  ExportVideo(QStringList options);
};

/**
 * @brief Deletes every plain file in a directory (non-recursive).
 * @param path directory to empty; subdirectories are left alone.
 *
 * Used to clear the scratch render folder before a new export
 * (MyMainWindow::on_actionexport_as_AVI_file_triggered() / StartFFMPEGRender()).
 */
void EmptyDirectory(QString path);
#endif // FFMPEGEXPORT_H
