#include "imageexport.h"
#include <frame.h>
#include <QImage>
#include <QPainter>

extern int W;
extern int H;
extern QGraphicsRectItem* my_rect;
QString exportPath;
int renderOption = NONE;
ImageExport::ImageExport(QObject *parent, QString path, int option)
    // il costruttore base va chiamato esplicitamente, altrimenti il parent passato dal chiamante
    // viene ignorato e Qt non ha modo di ripulire questo thread quando il parent viene distrutto
    // (sicuro solo perché MyMainWindow::closeEvent aspetta che il thread finisca prima di chiudere)
    : QThread(parent)
{
    exportPath = path;
    renderOption = option;
}
extern bool render_flag;
void ImageExport::Render(int option)
{
    for(Frame* f :V->my_animation->frame_list){
        QGraphicsScene renderScene; //scena fittizia
        QImage *render_img = new QImage(W,H,QImage::Format_ARGB32); //immagine temporanea
        QPainter painter(render_img); // painter che esegue il drawing sull immagine
        painter.setRenderHints(QPainter::Antialiasing|QPainter::SmoothPixmapTransform);
        renderScene.setSceneRect(my_rect->rect());
        // questi stick sono VIVI: clona i dati prima di wrapparli, altrimenti il costruttore
        // di stick ruberebbe il back-pointer data->view alla view reale sullo schermo
        QList<StickData*> renderClones;

        for(StickData* s: f->total_sticks){
            StickData* clone = s->Clone();
            renderClones.append(clone);
            renderScene.addItem(new StickGraphicsItem(clone));
        }

        //disegna sull' immagine e salvala
        render_img->fill(Qt::white);
        painter.setBackground(QBrush(QColor(Qt::white)));
        renderScene.render(&painter);
        QString itemPath = exportPath;
        itemPath.insert(exportPath.lastIndexOf("."),"_"+QString::number(f->frame_number));
        render_img->save(itemPath);
        painter.end();
        delete render_img;
        renderScene.clear(); // distruggi le view di rendering PRIMA di liberare i dati clonati
        qDeleteAll(renderClones);
        //...add all other video frames here
    }


    //emit ImageExportFinished(renderOption);

}

void ImageExport::run()
{
    if(renderOption != PREVIEW){
        render_flag = true;
        Render(renderOption);
        render_flag = false;
    }else{
        V->PreparePreview();

    }
    emit ImageExportFinished(renderOption);
    quit();
    deleteLater();
}
