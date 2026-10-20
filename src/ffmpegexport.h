#ifndef FFMPEGEXPORT_H
#define FFMPEGEXPORT_H

#include <QWidget>

class QPushButton;

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
    QPushButton *locate_ffmpeg_btn; ///< opens the file dialog that points at the user's ffmpeg

    /**
     * @brief Asks the user for their own ffmpeg.exe, remembers it and re-enables the
     *        formats that need it.
     */
    void LocateFfmpeg();

    /**
     * @brief Enables the three ffmpeg-only format radios per whether an ffmpeg is
     *        currently available, and refreshes the locate button's tooltip.
     */
    void RefreshFfmpegAvailability();

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

  /**
   * @brief Emitted on Export when the chosen format is mp4, which does not use ffmpeg.
   * @param file destination .mp4 path.
   * @param fps frame rate chosen in the dialog.
   *
   * Consumed by MyMainWindow::StartMediaFoundationRender(): the frames are rendered the
   * same way, then encoded by the Windows H.264 encoder (mfvideo.h) instead of ffmpeg.
   */
  void  ExportVideoMF(QString file, int fps);
};

/**
 * @brief Deletes every plain file in a directory (non-recursive).
 * @param path directory to empty; subdirectories are left alone.
 *
 * Used to clear the scratch render folder before a new export
 * (MyMainWindow::on_actionexport_as_AVI_file_triggered() / StartFFMPEGRender()).
 */
void EmptyDirectory(QString path);

/**
 * @brief Path of the ffmpeg executable the user pointed the app at.
 * @return that path when it still exists, otherwise an empty string.
 *
 * ffmpeg is not shipped with SAM (see NOTICE.md) - it is the optional, advanced
 * half of video export, located by the user through FfmpegExport's "Locate FFmpeg" button.
 * An empty return is not fatal: mp4 export goes through Windows' own encoder (mfvideo.h).
 */
QString ResolveFfmpeg();
#endif // FFMPEGEXPORT_H
