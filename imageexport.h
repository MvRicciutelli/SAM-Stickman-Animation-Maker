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
#define MFVIDEO 4  ///< render an image sequence, then encode it to .mp4 with Windows' own encoder

extern MyView *V; ///< the single canvas instance; the source of the frames to render

/**
 * @brief Background worker that renders the animation's frames to images off the UI
 *        thread, so exporting doesn't freeze the app.
 *
 * Created and started by MyMainWindow::ExportImageSequence(); on completion it emits
 * ImageExportFinished(option), which MyMainWindow::ImageRenderOver() turns into the
 * matching next step (encode to AVI/ffmpeg, or nothing). PREVIEW is the odd one out:
 * it renders into memory (preview_frames, which MySleeper turns into pixmaps) instead
 * of to disk. The thread self-destructs (deleteLater()) once run() returns.
 *
 * run() NEVER reads the live animation: the user keeps editing while it works, so the
 * constructor (UI thread) clones every frame's sticks and the canvas geometry first, and
 * run() renders only that snapshot.
 */
class ImageExport : public QThread
{
    Q_OBJECT
public:
    /**
     * @brief Snapshots the animation to render and stores the output path and mode.
     * @param parent parent QObject (passed to QThread so Qt can clean the thread up).
     * @param path output path template; the frame number is spliced in before the
     *        extension for each frame.
     * @param option one of NONE / FFMPEG / AVI / PREVIEW / MFVIDEO.
     * @param video_file MFVIDEO only: destination .mp4.
     * @param video_fps MFVIDEO only: frame rate of the encoded video.
     */
    explicit ImageExport(QObject *parent = nullptr, QString path = "", int option = NONE,
                         QString video_file = "", int video_fps = 24);

    /**
     * @brief Frees the snapshot taken by the constructor.
     */
    ~ImageExport();

    QString video_file;         ///< MFVIDEO only: destination .mp4
    int video_fps;              ///< MFVIDEO only: frame rate of the encoded video
    QString video_error;        ///< MFVIDEO only: empty on success, else why encoding failed
    QStringList rendered_files; ///< the frame images run() wrote, in playback order
    QList<QImage> preview_frames; ///< PREVIEW only: one image per frame, rendered at preview_scale
    qreal preview_scale = 1;    ///< PREVIEW only: preview_frames' size relative to the canvas
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

private:
    // erano globali a livello di file: un'anteprima avviata durante un export li
    // sovrascriveva, e l'export finiva senza mai encodare il video
    QString export_path;                  ///< output path template
    int render_option = NONE;             ///< render mode
    QList<QList<StickData*>> frames;      ///< per frame, Clone()s of its total_sticks (z order)
    QList<int> frame_numbers;             ///< frame_number of each entry of `frames`
    QRectF canvas_rect;                   ///< my_rect at construction: the area rendered
    QSize image_size;                     ///< pixel size of each rendered image

    /**
     * @brief Flattens one snapshotted frame onto a white image of image_size.
     * @param sticks the frame's cloned sticks, in z order.
     * @return the rendered frame.
     */
    QImage RenderFrame(const QList<StickData*> &sticks) const;
};

#endif // IMAGEEXPORT_H
