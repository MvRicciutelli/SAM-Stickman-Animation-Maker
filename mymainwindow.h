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

/**
 * @brief The application's top-level window: holds the canvas and toolbar and owns the
 *        File/Export menu.
 *
 * MyMainWindow is the shell around the editor. Its `view` is the MyView canvas (the
 * document lives there, see myview.h) and `central_w` is the Widget with the toolbar
 * and side panels (widget.h). This class itself only adds the menu-bar actions —
 * save/load a single figure (*.stck) or a whole animation (*.mAnim), the New/Options
 * commands, and the three video/image export paths — plus the autosave and
 * close-with-unsaved-changes handling. The heavy lifting is delegated: model edits to
 * `view`/`view->my_animation`, rendering to the ImageExport thread, encoding to the
 * bundled image2Avi.exe or to ffmpeg.
 */
class MyMainWindow : public QMainWindow
{
    Q_OBJECT

public:
    /**
     * @brief Sets up the UI and starts the background update check.
     * @param parent parent widget (Qt ownership).
     */
    explicit MyMainWindow(QWidget *parent = nullptr);
    ~MyMainWindow();
    MyView *view;      ///< the animation canvas / document owner (myview.h)
    Widget *central_w; ///< the toolbar + side panels (widget.h)

    /**
     * @brief Wires the signals this window listens to (view->AutoSaveTrigger → AutoSave).
     *        Called once during startup, after `view` exists.
     */
    void DoConnections();

    /**
     * @brief Renders every frame to a numbered image via a background ImageExport thread.
     * @param path output path template for the image sequence.
     * @param option NONE / AVI / FFMPEG / MFVIDEO — what ImageRenderOver() does when done.
     * @param video_file MFVIDEO only: destination .mp4.
     * @param video_fps MFVIDEO only: frame rate of the encoded video.
     *
     * Refuses to start a second export while one is running. On completion
     * ImageRenderOver() either stops (NONE) or forwards the frames to the AVI/ffmpeg path.
     */
    void ExportImageSequence(QString path, int option, QString video_file = "", int video_fps = 24);

    /**
     * @brief Opens the ffmpeg export options dialog (single instance) and, on accept,
     *        runs the image sequence + ffmpeg pipeline via StartFFMPEGRender().
     */
    void ExportUsingFFMPEG();

    /**
     * @brief Feeds the already-rendered image sequence to the bundled image2Avi.exe
     *        helper to produce an .avi. Called from ImageRenderOver() for the AVI path.
     */
    void ExportAvi();
public slots:
    /**
     * @brief Applies the values from the Animation Options dialog (fps, W/H, handle size,
     *        autosave interval) and resizes the scene via MyView::SizeChange().
     * @param option_val reposition mode forwarded to SizeChange (REPO/REPOSCALE/NOREPO).
     */
    void GetAnimationParams(int option_val);

    /**
     * @brief Kicks off the ffmpeg render: stores the chosen options, then exports the
     *        image sequence that ImageRenderOver() will hand to ffmpeg.
     * @param options ffmpeg command-line arguments from the export dialog.
     */
    void StartFFMPEGRender(QStringList options);

    /**
     * @brief Kicks off the mp4 render: exports the image sequence that the ImageExport
     *        thread then encodes with the Windows H.264 encoder (no ffmpeg involved).
     * @param file destination .mp4 path chosen in the export dialog.
     * @param fps frame rate chosen in the export dialog.
     */
    void StartMediaFoundationRender(QString file, int fps);

    /**
     * @brief Autosave slot (bound to view->AutoSaveTrigger): saves to the current path.
     */
    void AutoSave();

    /**
     * @brief Shows the "newer version available" notice (bound to CheckUpdate).
     * @param ver the available version string.
     */
    void UpdatePresent(QString ver);

    /**
     * @brief Called when the ImageExport thread finishes: frees it and dispatches to the
     *        AVI/ffmpeg encoder or stops, per the option passed to ExportImageSequence().
     * @param option NONE / AVI / FFMPEG / MFVIDEO.
     */
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

    void on_actionExport_as_mp4_triggered();

    void on_actionAbout_triggered();

    /**
     * @brief Shows Qt's own about box (version, LGPLv3 notice) via QMessageBox::aboutQt().
     *        Required credit for the LGPLv3-licensed Qt libraries this app links against.
     */
    void on_actionAboutQt_triggered();

    void on_actionTerms_and_Conditions_triggered();

private:
    Ui::MyMainWindow *ui;


    // QWidget interface
protected:
    virtual void closeEvent(QCloseEvent *event) override;
};

#endif // MYMAINWINDOW_H
