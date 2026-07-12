#include "animation.h"

#include <QFile>
#include <QSpinBox>
#include "advancedtab.h"
#include "advancedlinewidget.h"
#include "advancedcirclewidget.h"
#include "advancedtaperwidget.h"
#include <QCheckBox>
#include "uiItems.h"

extern int W;
extern int H;
extern QPointF onion_offset;
extern QGraphicsRectItem *my_rect;
extern QGraphicsRectItem *limit_rect;
extern QGraphicsScene* scene;

bool clearingAnimation = false;
bool changeTypeFlag = false;
namespace Ui { class Widget; }
extern Ui::Widget *myUi;

Animation::Animation()
{

}

Frame *Animation::addFrame(QListWidgetItem* item, int pos)
{

    frameBuffer = new Frame();
    frameBuffer->scene = scene;
    //frameBuffer->renderImg = new QImage(scene->sceneRect().width(),scene->sceneRect().height(),QImage::Format_ARGB32);

    if(frameList.isEmpty())
    {
        currentFrame = frameBuffer;
    }
    frameList.insert(pos,frameBuffer);
    frameBuffer->linkedItem = item;
    return frameBuffer;
}
Frame *Animation::deleteFrame(Frame *frame){
    int frameNum = frameList.indexOf(frame);
    if(frameList.count()>=1){
        frameList.removeAll(frame);
        if(frame == currentFrame){
            for(StickFigure * S:frame->stickFigures){
                // toglilo dalla lista prima di cancellarlo, altrimenti resta un
                // puntatore pendente che viene cancellato di nuovo al prossimo clear()
                my_stick_figure_widget_list->takeItem(my_stick_figure_widget_list->row(S->linkedItem));
                delete S->linkedItem;
            }
        }
        delete frame->linkedItem;
        delete frame;
        //delete my_rect;
        /*if(!clearingAnimation)
            scene->clear(); // risky*/
        sceneRemover(scene);
        //my_rect = new QGraphicsRectItem(0,0,W,H); // RENEW IT
        if(!frameList.isEmpty() && !clearingAnimation){
            if(frameNum>=frameList.count())
            {
                currentFrame = frameList[frameList.count()-1];
            }
            else{
                currentFrame = frameList[frameNum];
            }
        }
        else{
            currentFrame = nullptr;
            frameBuffer = nullptr;
        }
    }
    return currentFrame;
}
void Animation::updateSelection(QPointF point)
{
    float distBuffer;
    float minDist;
    int idxBuffer;
    int selectedIdx = -1;
    if(currentFrame->stickFigures.isEmpty())
        return;
    scene->clearSelection();
    StickFigure *selectedStickFigure = nullptr;
    if(currentFrame->CurrentStickFigure() != nullptr){
        currentFrame->CurrentStickFigure()->highlight(false);
        currentFrame->CurrentStickFigure()->refresh(0);
    }

    for(StickFigure * S: currentFrame->stickFigures)
    {
        if(S->stickList.count()>=1) {
            idxBuffer = S->selectStick( &point);
            // aggiorna il buffer col punto 1 o 2 a seconda se stiamo selezionando lo stick master
            if(S->selectingOrigin )
                distBuffer = QLineF(S->stickList[idxBuffer]->myLine.p1(),point).length();
            else{
                distBuffer = S->measureDistanceFromLine(point,S->stickList[idxBuffer]->myLine);
            }
            if(selectedIdx == -1 || (distBuffer<minDist))
            {
                selectedIdx = idxBuffer;
                minDist = distBuffer;
                selectedStickFigure = S;
            }
        }
    }
    if(selectedStickFigure == nullptr) // nessuno stickfigure nel frame ha almeno uno stick
        return;
    // CurrentStickFigure() va controllato PRIMA: CurrentStick() da solo deferenzia già
    // currentStickFigure, che può essere nullptr dopo un ripristino di undo/redo il cui
    // "stickfigure corrente" salvato era un layer vuoto (operator>> lo lascia nullptr in quel caso)
    if(currentFrame->CurrentStickFigure() && currentFrame->CurrentStick()){
        if(selectedStickFigure->stickList[selectedIdx]->stickType != currentFrame->CurrentStick()->stickType){
            updateTab(selectedStickFigure->stickList[selectedIdx]->stickType );
        }
    }

    currentFrame->CurrentStickFigure() = selectedStickFigure;
    currentFrame->CurrentStick() = currentFrame->CurrentStickFigure()->stickList[selectedIdx];
    currentFrame->CurrentStickFigure()->highlight(true);
    if(currentFrame->CurrentStick()->view){
        currentFrame->CurrentStick()->view->setSelected(true);
        currentFrame->CurrentStick()->view->selected = true;
    }


    updateSliders();



}
void Animation::updateTab(int t){
    int idx = my_tab_widget->currentIndex();
    my_tab_widget->removeTab(1);
    if(t == StickData::IMAGE){
        my_tab_widget->insertTab(1,advanced_img_tab,"Image Editing");
    }
    else if(t == StickData::RECT){
        my_tab_widget->insertTab(1,advanced_rect_tab,"Rect Editing");
    }
    else if(t == StickData::LINE){
        my_tab_widget->insertTab(1,advanced_line_tab,"Line Editing");
    }
    else if(t == StickData::CIRCLE){
        my_tab_widget->insertTab(1,advanced_circle_tab,"Circle Editing");
    }
    else if(t == StickData::TAPER){
        my_tab_widget->insertTab(1,advanced_taper_tab,"Taper Editing");
        hard_top_check->show();
        hard_bottom_check->show();
    }
    else if(t == StickData::TRAPEZOID){
        my_tab_widget->insertTab(1,advanced_taper_tab,"Trapezoid Editing");
        hard_top_check->hide();
        hard_bottom_check->hide();
    }
    my_tab_widget->setCurrentIndex(idx);
}
void Animation::updateSelection(StickData* s)
{
    //update current frame

    // update current stickfigure
    scene->clearSelection();
    currentFrame->CurrentStickFigure()->highlight(false);

    for(StickFigure *S : currentFrame->stickFigures)
    {
        if(S->stickList.contains(s))
        {
            currentFrame->CurrentStickFigure() = S;
            S->highlight(true);
        }
    }
    // update current stick
    if(!currentFrame->CurrentStickFigure()->stickList.isEmpty())
    {
     currentFrame->CurrentStick() = s;
     if(s->view){
         s->view->setSelected(true);
         s->view->selected = true;
     }
    }
    updateSliders();
    //int idx = frameBuffer->currentStickFigure->itemList.indexOf(item);

}
void Animation::updateSelection(StickFigure* S)
{
    //update current frame
    if(S==currentFrame->CurrentStickFigure())
        return;
    // update current stickfigure
    if(currentFrame->CurrentStickFigure() !=nullptr)
    {
        currentFrame->CurrentStickFigure()->highlight(false); //de highlight old selection
    }

    currentFrame->CurrentStickFigure() = S; //update currentStickFigure buffer

    S->highlight(true); //highlight new one

    // update current stick
    scene->clearSelection(); //clear scene selection
    if(!S->stickList.isEmpty()){
        currentFrame->selectStick(S); //update selected stick
    }


}

void Animation::updateSliders(){
    StickData *cs = currentFrame->CurrentStick();
    if(currentFrame->CurrentStick() == nullptr)
        return;
    if(cs->stickType == StickData::IMAGE){
        //aggiorna gli slider di modifica alla scala e all'offset
        if(cs->imgWScale>=1){
            img_width_slider->setValue(10*(cs->imgWScale-1));
        }
        else{
            img_width_slider->setValue(-10*(1/(cs->imgWScale)-1));
        }
        if(cs->imgHScale>=1){
            img_height_slider->setValue(10*(cs->imgHScale-1));
        }
        else{
            img_height_slider->setValue(-10*(1/(cs->imgHScale)-1));

        }
        img_v_offset_slider->setValue(cs->imgOffset.y());
        img_h_offset_slider->setValue(cs->imgOffset.x());
        img_rotation_slider->setValue(cs->imgAngleOffset);
    }
    else if(cs->stickType == StickData::RECT){
        advanced_rect_slider->setValue(cs->imgWScale);
    }
    else if(cs->stickType == StickData::CIRCLE){
        if(cs->imgWScale<1){
            circle_squash_slider->setValue(-(1-cs->imgWScale)*100);
        }
        else{
            circle_squash_slider->setValue((cs->imgWScale-1)*20);
        }
    }
    else if(cs->stickType == StickData::TAPER||cs->stickType == StickData::TRAPEZOID){
        taper_w_slider->setValue(cs->imgWScale);
        taper_h_slider->setValue(cs->imgHScale);
        hard_top_check->setChecked(cs->hardTop);
        hard_bottom_check->setChecked(cs->hardBottom);

    }
    edit_line_thickness_slider->setValue(cs->Pen.width());
    elongate_spinbox->setValue(  currentFrame->CurrentStick()->myLine.length());
}
Frame* Animation::setupFrame(int pos){
    QString name;
    name.asprintf("%d",pos);

    // prepara il widget associato
    QListWidgetItem * addedItem = new QListWidgetItem();

    for(Frame* f: frameList){
        if(f->frameNumber>=pos){
            f->frameNumber++;
            f->linkedItem->setData(Qt::DisplayRole,QString::number(f->frameNumber));
        }
    }
    my_frame_widget_list->insertItem(pos,addedItem);
    Frame * addedFrame = addFrame(addedItem, pos);
    addedFrame->frameNumber = pos;
    addedFrame->linkedItem = addedItem;
    // popola l'entry QListWidget con puntatore ed iconea del frame
    QVariant newData(QVariant::fromValue(addedFrame));
    addedItem->setData(Qt::UserRole,newData);
    addedItem->setData(Qt::DisplayRole,name);
    addedItem->setIcon(*addedFrame->frameIcon);
    // update other frames
    return addedFrame;
}
// Animation::cloneFrame è stato eliminato (Phase 2b): il ripristino di undo/redo passa
// da PopulateLiveFrame() e le copie in-memory da ExtractFrameData()
QDataStream &operator<<(QDataStream& stream, const Animation& myAnimation){
    int frameNum = myAnimation.frameList.count();
    stream<<myAnimation.version;
    stream<<frameNum;
    stream<<W;
    stream<<H;
    for(Frame* f: myAnimation.frameList){
        stream<<*f;
    }
    return stream;
}
QDataStream &operator>>(QDataStream& stream,Animation& myAnimation){
    int frameNum = 0;
    stream>>myAnimation.version;
    stream>>frameNum;
    stream>>myAnimation.width;
    stream>>myAnimation.height;
    // applica subito le dimensioni appena lette: prima non veniva fatto, quindi caricare un file con
    // una canvas di dimensioni diverse da quella corrente lasciava my_rect/limit_rect/onion_offset legati
    // alla vecchia canvas (nessuno li ricalcolava finché l'utente non ridimensionava manualmente)
    W = myAnimation.width;
    H = myAnimation.height;
    my_rect->setRect(0,0,W,H);
    sceneRemover(scene);
    limit_rect->setRect(-my_rect->rect().width()/10, -my_rect->rect().height()/10, my_rect->rect().width()*1.1, my_rect->rect().height()*1.1);

    scene->addItem(my_rect);
    scene->setSceneRect(limit_rect->rect());
    onion_offset = QPointF(-my_rect->rect().width()/10,-my_rect->rect().height()/10);
    // un file valido non ha comunque un numero enorme di frame: senza questo controllo, un file
    // troncato o corrotto (es. un int con qualche bit invertito) farebbe partire un ciclo che
    // alloca centinaia di migliaia di Frame/QListWidgetItem prima di accorgersi che i dati non ci sono
    if(stream.status() != QDataStream::Ok || frameNum < 0 || frameNum > 100000){
        return stream;
    }
    for(int i = 0;i<frameNum;i++){
        Frame* addedFrame = myAnimation.setupFrame(i);
        stream>>*addedFrame;
        if(stream.status() != QDataStream::Ok){
            // file troncato a metà frame: fermati qui, i frame già letti restano validi
            break;
        }
        addedFrame->updateIcon();
    }
    return stream;
}
void Animation::saveAnimation(QString fileName){
    QFile file(fileName);
    if(!file.open(QIODevice::WriteOnly))
    {
        return;
    }
    QDataStream out(&file);
    out << *this;
    file.close();
}
void Animation::loadAnimation(QString name)
{

    QFile file(name);
    if(!file.open(QIODevice::ReadOnly))
    {
        return;
    }
    QDataStream out(&file);
    clearAnimation();
    // niente più loadingAnimationFlag: l'operator>> di Frame non attacca alla scena né
    // renderizza icone per costruzione (vedi PopulateLiveFrame), non serve una guardia
    out >> *this;
    file.close();
    qDebug("load finished");

}
void Animation::clearAnimation(){
    clearingAnimation = true;
    QList <Frame*> clonedList = frameList;
    for(Frame* f: clonedList){

        deleteFrame(f);
    }
    my_frame_widget_list->clear();
    //my_frame_widget_list->takeItem(my_frame_widget_list->row(f->linkedItem));
    sceneRemover(scene);
    //scene->clear();
    clearingAnimation = false;

}
