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
extern QPointF onionOffset;
extern QGraphicsRectItem *myRect;
extern QGraphicsRectItem *limitRect;
extern QGraphicsScene* scene;

bool clearingAnimation = false;
bool changeTypeFlag = false;
namespace Ui { class Widget; }
extern Ui::Widget *myUi;

#define CS currentFrame->currentStickFigure->currentStick
#define CURRENTSTICKFIGURE currentFrame->currentStickFigure


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
                myStickFigureWidgetList->takeItem(myStickFigureWidgetList->row(S->linkedItem));
                delete S->linkedItem;
            }
        }
        delete frame->linkedItem;
        delete frame;
        //delete myRect;
        /*if(!clearingAnimation)
            scene->clear(); // risky*/
        sceneRemover(scene);
        //myRect = new QGraphicsRectItem(0,0,W,H); // RENEW IT
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
    if(CURRENTSTICKFIGURE != nullptr){
        CURRENTSTICKFIGURE->highlight(false);
        CURRENTSTICKFIGURE->refresh(0);
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
    // CURRENTSTICKFIGURE va controllato PRIMA: espandere CS da solo deferenzia già
    // currentStickFigure, che può essere nullptr dopo un ripristino di undo/redo il cui
    // "stickfigure corrente" salvato era un layer vuoto (operator>> lo lascia nullptr in quel caso)
    if(CURRENTSTICKFIGURE && CS){
        if(selectedStickFigure->stickList[selectedIdx]->stickType != CS->stickType){
            updateTab(selectedStickFigure->stickList[selectedIdx]->stickType );
        }
    }

    CURRENTSTICKFIGURE = selectedStickFigure;
    CS = CURRENTSTICKFIGURE->stickList[selectedIdx];
    CURRENTSTICKFIGURE->highlight(true);
    CS->setSelected(true);
    CS->selected = true;


    updateSliders();



}
void Animation::updateTab(int t){
    int idx = myTabWidget->currentIndex();
    myTabWidget->removeTab(1);
    if(t == stick::IMAGE){
        myTabWidget->insertTab(1,advancedImgTab,"Image Editing");
    }
    else if(t == stick::RECT){
        myTabWidget->insertTab(1,advancedRectTab,"Rect Editing");
    }
    else if(t == stick::LINE){
        myTabWidget->insertTab(1,advancedLineTab,"Line Editing");
    }
    else if(t == stick::CIRCLE){
        myTabWidget->insertTab(1,advancedCircleTab,"Circle Editing");
    }
    else if(t == stick::TAPER){
        myTabWidget->insertTab(1,advancedTaperTab,"Taper Editing");
        hardTopCheck->show();
        hardBottomCheck->show();
    }
    else if(t == stick::TRAPEZOID){
        myTabWidget->insertTab(1,advancedTaperTab,"Trapezoid Editing");
        hardTopCheck->hide();
        hardBottomCheck->hide();
    }
    myTabWidget->setCurrentIndex(idx);
}
void Animation::updateSelection(stick* s)
{
    //update current frame

    // update current stickfigure
    scene->clearSelection();
    CURRENTSTICKFIGURE->highlight(false);

    for(StickFigure *S : currentFrame->stickFigures)
    {
        if(S->stickList.contains(s))
        {
            CURRENTSTICKFIGURE = S;
            S->highlight(true);
        }
    }
    // update current stick
    if(!CURRENTSTICKFIGURE->stickList.isEmpty())
    {
     CS = s;
     s->setSelected(true);
     s->selected = true;
    }
    updateSliders();
    //int idx = frameBuffer->currentStickFigure->itemList.indexOf(item);

}
void Animation::updateSelection(StickFigure* S)
{
    //update current frame
    if(S==CURRENTSTICKFIGURE)
        return;
    // update current stickfigure
    if(CURRENTSTICKFIGURE !=nullptr)
    {
        CURRENTSTICKFIGURE->highlight(false); //de highlight old selection
    }

    CURRENTSTICKFIGURE = S; //update currentStickFigure buffer

    S->highlight(true); //highlight new one

    // update current stick
    scene->clearSelection(); //clear scene selection
    if(!S->stickList.isEmpty()){
        currentFrame->selectStick(S); //update selected stick
    }


}

void Animation::updateSliders(){
    stick *cs = CS;
    if(CS == nullptr)
        return;
    if(cs->stickType == stick::IMAGE){
        //aggiorna gli slider di modifica alla scala e all'offset
        if(cs->imgWScale>=1){
            imgWidthSlider->setValue(10*(cs->imgWScale-1));
        }
        else{
            imgWidthSlider->setValue(-10*(1/(cs->imgWScale)-1));
        }
        if(cs->imgHScale>=1){
            imgHeightSlider->setValue(10*(cs->imgHScale-1));
        }
        else{
            imgHeightSlider->setValue(-10*(1/(cs->imgHScale)-1));

        }
        imgVOffsetSlider->setValue(cs->imgOffset.y());
        imgHOffsetSlider->setValue(cs->imgOffset.x());
        imgRotationSlider->setValue(cs->imgAngleOffset);
    }
    else if(cs->stickType == stick::RECT){
        advancedRectSlider->setValue(cs->imgWScale);
    }
    else if(cs->stickType == stick::CIRCLE){
        if(cs->imgWScale<1){
            circleSquashSlider->setValue(-(1-cs->imgWScale)*100);
        }
        else{
            circleSquashSlider->setValue((cs->imgWScale-1)*20);
        }
    }
    else if(cs->stickType == stick::TAPER||cs->stickType == stick::TRAPEZOID){
        taperWSlider->setValue(cs->imgWScale);
        taperHSlider->setValue(cs->imgHScale);
        hardTopCheck->setChecked(cs->hardTop);
        hardBottomCheck->setChecked(cs->hardBottom);

    }
    editLineThicknessSlider->setValue(cs->Pen.width());
    elongateSpinbox->setValue(  CS->myLine.length());
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
    myFrameWidgetList->insertItem(pos,addedItem);
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
    // una canvas di dimensioni diverse da quella corrente lasciava myRect/limitRect/onionOffset legati
    // alla vecchia canvas (nessuno li ricalcolava finché l'utente non ridimensionava manualmente)
    W = myAnimation.width;
    H = myAnimation.height;
    myRect->setRect(0,0,W,H);
    sceneRemover(scene);
    limitRect->setRect(-myRect->rect().width()/10, -myRect->rect().height()/10, myRect->rect().width()*1.1, myRect->rect().height()*1.1);

    scene->addItem(myRect);
    scene->setSceneRect(limitRect->rect());
    onionOffset = QPointF(-myRect->rect().width()/10,-myRect->rect().height()/10);
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
    myFrameWidgetList->clear();
    //myFrameWidgetList->takeItem(myFrameWidgetList->row(f->linkedItem));
    sceneRemover(scene);
    //scene->clear();
    clearingAnimation = false;

}
