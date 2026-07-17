#ifndef IMAGEEXPORT_H
#define IMAGEEXPORT_H

#include "myview.h"

#include <QObject>
#include <QThread>

// --- What ImageExport should do, both as the render mode and as the value carried by
// --- ImageExportFinished so MyMainWindow::ImageRenderOver() knows the next step.
#define FFMPEG 1   ///< render an image sequence, then feed it to ffmpeg
#define AVI 2      ///< render an image sequence, then feed it to the image2Avi.exe helper
#define PREVIEW 3  ///< don't write files: pre-render frames into MyView::preview_list for playback
#define NONE 0     ///< render an image sequence and stop (plain export)

extern MyView *V; ///< the single canvas instance; the source of the frames to render

/**
 * @brief Background worker that renders the animation's frames to images off the UI
 *        thread, so exporting doesn't freeze the app.
 *
 * Created and started by MyMainWindow::ExportImageSequence(); on completion it emits
 * ImageExportFinished(option), which MyMainWindow::ImageRenderOver() turns into the
 * matching next step (encode to AVI/ffmpeg, or nothing). PREVIEW is the odd one out:
 * it renders into memory (MyView::preview_list) instead of to disk. The thread
 * self-destructs (deleteLater()) once run() returns.
 */
class ImageExport : public QThread
{
    Q_OBJECT
public:
    /**
     * @brief Stores the output path and render mode.
     * @param parent parent QObject (passed to QThread so Qt can clean the thread up).
     * @param path output path template; the frame number is spliced in before the
     *        extension for each frame.
     * @param option one of NONE / FFMPEG / AVI / PREVIEW.
     */
    explicit ImageExport(QObject *parent = nullptr, QString path = "", int option = NONE);

    /**
     * @brief Renders every frame to its own numbered image file.
     * @param option render mode (kept for symmetry; the path/mode also live in globals).
     */
    void Render(int option);
signals:
    /**
     * @brief Emitted from run() when rendering ends; carries the mode so the receiver
     *        (MyMainWindow::ImageRenderOver()) knows what to do next.
     * @param option the render mode that just finished.
     */
    void ImageExportFinished(int option);
public slots:


    // QThread interface
protected:
    virtual void run() override;
};

#endif // IMAGEEXPORT_H
