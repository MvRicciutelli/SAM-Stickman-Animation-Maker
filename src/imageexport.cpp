#include "imageexport.h"
#include "mfvideo.h"
#include <frame.h>
#include <QImage>
#include <QPainter>
#include <QtMath>

extern qint32 W;
extern qint32 H;
extern QGraphicsRectItem* my_rect;
/**
 * @details Passing the base-class parent through is deliberate: without it the caller's
 * parent is dropped and Qt can't reap this thread when the parent dies. That is only
 * safe because MyMainWindow::closeEvent() waits for a running export to finish before
 * the window (this thread's parent) is destroyed.
 *
 * The snapshot is the reason this runs on the UI thread at all: nothing stops the user
 * from deleting a frame, undoing or dragging a stick while run() works, so run() must
 * never touch V->my_animation. Clone() is cheap here — the images inside are shared
 * copy-on-write, not copied (see CLAUDE.md "Images are shared"). The clones are never
 * attached to anything live, so wrapping them in render views steals no back-pointer.
 */
ImageExport::ImageExport(QObject *parent, QString path, int option,
                         QString video_file, int video_fps)
    // il costruttore base va chiamato esplicitamente, altrimenti il parent passato dal chiamante
    // viene ignorato e Qt non ha modo di ripulire questo thread quando il parent viene distrutto
    // (sicuro solo perché MyMainWindow::closeEvent aspetta che il thread finisca prima di chiudere)
    : QThread(parent)
{
    export_path = path;
    render_option = option;
    this->video_file = video_file;
    this->video_fps = video_fps;
    canvas_rect = my_rect->rect();
    image_size = QSize(W, H);
    if(option == PREVIEW){
        // l'anteprima si vede allo zoom della view: da zoomati indietro, renderizzarla alla
        // risoluzione della tela teneva in memoria pixel che lo schermo non mostra (8 MB a
        // frame a 1080p). Il devicePixelRatio la tiene nitida sugli schermi HiDPI.
        // ponytail: risoluzione fissata all'avvio, zoomare DURANTE la riproduzione la sgrana
        preview_scale = qMin(1.0, qSqrt(qAbs(V->transform().determinant())) * V->devicePixelRatioF());
        image_size = QSize(qMax(1, qRound(W * preview_scale)), qMax(1, qRound(H * preview_scale)));
    }
    for(Frame* f : V->my_animation->frame_list){
        QList<StickData*> clones;
        for(StickData* s : f->total_sticks)
            clones.append(s->Clone());
        frames.append(clones);
        frame_numbers.append(f->frame_number);
    }
}

ImageExport::~ImageExport()
{
    for(const QList<StickData*> &clones : frames)
        qDeleteAll(clones);
}

/**
 * @details The render views wrap the snapshot's clones and die with the render scene when
 * this returns, while the clones live on until ~ImageExport() — so no view outlives its
 * data (~StickGraphicsItem reads data->view). The scene rect maps onto the whole image, so
 * a smaller image_size (the PREVIEW scale) renders the same canvas at a lower resolution.
 */
QImage ImageExport::RenderFrame(const QList<StickData*> &sticks) const
{
    QGraphicsScene render_scene; //scena fittizia
    render_scene.setSceneRect(canvas_rect);
    for(StickData* s : sticks)
        render_scene.addItem(new StickGraphicsItem(s));
    // premoltiplicato: è il formato in cui il raster engine dipinge (su ARGB32 semplice ogni
    // span passava da una conversione). Il PNG salvato resta identico: è tutto opaco
    QImage render_img(image_size, QImage::Format_ARGB32_Premultiplied);
    render_img.fill(Qt::white);
    QPainter painter(&render_img); // painter che esegue il drawing sull immagine
    if(render_option != PREVIEW) // l'anteprima resta com'era, senza antialiasing come la tela
        painter.setRenderHints(QPainter::Antialiasing|QPainter::SmoothPixmapTransform);
    render_scene.render(&painter);
    painter.end();
    return render_img; // la scena distrugge qui le sue view; i cloni li libera ~ImageExport()
}

/**
 * @details The QThread entry point. Renders every snapshotted frame: into preview_frames
 * for PREVIEW, otherwise to its own numbered file, with the frame number spliced into the
 * file name before the extension. The PNGs of the video modes are scratch files read back
 * once, so they are written at zlib level 1 (quality 80) rather than the default; the
 * user-facing image sequence (NONE) keeps the default. Then announces completion with
 * ImageExportFinished() and schedules its own destruction (deleteLater()).
 */
void ImageExport::run()
{
    for(int i = 0; i < frames.count(); i++){
        QImage frame = RenderFrame(frames[i]);
        if(render_option == PREVIEW){
            preview_frames.append(frame);
            continue;
        }
        QString item_path = export_path;
        item_path.insert(export_path.lastIndexOf("."),"_"+QString::number(frame_numbers[i]));
        frame.save(item_path, nullptr, render_option == NONE ? -1 : 80);
        rendered_files.append(item_path); // l'encoder MFVIDEO li rilegge in quest'ordine
    }
    // l'encoding va fatto qui e non in MyMainWindow::ImageRenderOver(): quello e' uno slot
    // sul thread della UI, e encodare un'animazione intera la congelerebbe
    if(render_option == MFVIDEO)
        WriteMp4WithMediaFoundation(video_file, rendered_files, video_fps, &video_error);
    emit ImageExportFinished(render_option);
    quit();
    deleteLater();
}
