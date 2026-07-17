#include "imageexport.h"
#include <frame.h>
#include <QImage>
#include <QPainter>

extern qint32 W;
extern qint32 H;
extern QGraphicsRectItem* my_rect;
QString export_path;          ///< output path template shared with Render()
int render_option = NONE;     ///< render mode captured at construction
/**
 * @details Passing the base-class parent through is deliberate: without it the caller's
 * parent is dropped and Qt can't reap this thread when the parent dies. That is only
 * safe because MyMainWindow::closeEvent() waits for a running export to finish before
 * the window (this thread's parent) is destroyed.
 */
ImageExport::ImageExport(QObject *parent, QString path, int option)
    // il costruttore base va chiamato esplicitamente, altrimenti il parent passato dal chiamante
    // viene ignorato e Qt non ha modo di ripulire questo thread quando il parent viene distrutto
    // (sicuro solo perché MyMainWindow::closeEvent aspetta che il thread finisca prima di chiudere)
    : QThread(parent)
{
    export_path = path;
    render_option = option;
}
extern bool render_flag;
/**
 * @details Flattens each frame onto its own QImage and writes it, with the frame number
 * spliced into the file name before the extension. Every stick on screen is LIVE, so it
 * is Clone()d before being wrapped in a throwaway StickGraphicsItem — otherwise the
 * constructor would steal the real view's data->view back-pointer (the same rule as
 * MyView::UpdateOnionSkins()/PreparePreview(), see CLAUDE.md "Data model"). The render
 * scene is cleared BEFORE the clones are freed so no view outlives its data.
 */
void ImageExport::Render(int option)
{
    Q_UNUSED(option); // la modalità in uso è già in render_option (impostata dal costruttore)
    for(Frame* f :V->my_animation->frame_list){
        QGraphicsScene render_scene; //scena fittizia
        QImage *render_img = new QImage(W,H,QImage::Format_ARGB32); //immagine temporanea
        QPainter painter(render_img); // painter che esegue il drawing sull immagine
        painter.setRenderHints(QPainter::Antialiasing|QPainter::SmoothPixmapTransform);
        render_scene.setSceneRect(my_rect->rect());
        // questi stick sono VIVI: clona i dati prima di wrapparli, altrimenti il costruttore
        // di stick ruberebbe il back-pointer data->view alla view reale sullo schermo
        QList<StickData*> render_clones;

        for(StickData* s: f->total_sticks){
            StickData* clone = s->Clone();
            render_clones.append(clone);
            render_scene.addItem(new StickGraphicsItem(clone));
        }

        //disegna sull' immagine e salvala
        render_img->fill(Qt::white);
        painter.setBackground(QBrush(QColor(Qt::white)));
        render_scene.render(&painter);
        QString item_path = export_path;
        item_path.insert(export_path.lastIndexOf("."),"_"+QString::number(f->frame_number));
        render_img->save(item_path);
        painter.end();
        delete render_img;
        render_scene.clear(); // distruggi le view di rendering PRIMA di liberare i dati clonati
        qDeleteAll(render_clones);
        //...add all other video frames here
    }


    //emit ImageExportFinished(render_option);

}

/**
 * @details The QThread entry point. For a real export it flips `render_flag` around
 * Render() (so code that must behave differently during a render can tell), for PREVIEW
 * it fills MyView::preview_list instead. Either way it announces completion with
 * ImageExportFinished() and then schedules its own destruction (deleteLater()).
 */
void ImageExport::run()
{
    if(render_option != PREVIEW){
        render_flag = true;
        Render(render_option);
        render_flag = false;
    }else{
        V->PreparePreview();

    }
    emit ImageExportFinished(render_option);
    quit();
    deleteLater();
}
