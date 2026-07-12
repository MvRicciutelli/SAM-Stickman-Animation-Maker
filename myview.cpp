#include "myview.h"
#include <QtAlgorithms>
#include <math.h>
#include <QDebug>
#include <QGraphicsRotation>
#include <frame.h>
#include <QMessageBox>
#include <QSpinBox>
#include <QCheckBox>
#include <animationoptions.h>
#include <QMessageBox>
#include "uiItems.h"
#include "framepresenter.h"


bool    isRightPressed = false;
bool    isMiddlePressed = false;
bool    isPressed = false;
extern float zoomLvl;
QPointF startingCoord; //mouse click coordinateBuffer
QPointF coord; //current mouse pos
float   rotationBuffer; //selected item Rotation buffer
float   lengthBuffer;
float zoomLvlBuf;
FrameData* stickLibraryBuffer; ///< libreria corrente: solo dati da Phase 2b
extern QSpinBox * onion_skin_sb;
bool startStickFigureRotation = false;
bool start_stick_figure_scale = false;
bool translateScene = false;
bool rotateScene = false;
bool scaleScene = false;
StickFigure* joinToStickFigure;
StickFigure* splitToStickFigure;;
StickData* joinToStick;
StickData* splitToStick;
extern QString image_name_buffer;
extern int W;
extern int H;
extern QGraphicsRectItem* my_rect;
extern QGraphicsRectItem* limit_rect;
extern bool playBack ;
extern bool loadFile;
extern QPointF onion_offset;
extern QImage * image_draw_buffer ;
extern QString animation_path;
extern int autoSaveInterval;
extern int opCount;

#define MAXUNDO 50 ///< cap on undo_stack depth, enforced via QUndoStack::setUndoLimit()

MyView::MyView(QWidget *parent) : QGraphicsView(parent)
{
    onionRender = false;
    myAnimation = new Animation();
    myAnimation->scene = scene();
    setMouseTracking(true);
    myPen.setWidth(10);
    myBrush.setStyle(Qt::SolidPattern);
    myBrush.setColor(Qt::gray);
    myPen.setCapStyle(Qt::RoundCap);
    stickLibraryBuffer = new FrameData();
    undo_stack.setUndoLimit(MAXUNDO);
    // autosave triggering moved here from storeUndo() (Phase 4): indexChanged fires
    // on every push/undo/redo, which is a more correct autosave trigger than the old
    // "only on a fresh edit" behavior (undo/redo are edits too, worth autosaving)
    connect(&undo_stack, &QUndoStack::indexChanged, this, [this](int){
        opCount++;
        if(QFile::exists(animation_path) && opCount % autoSaveInterval == 0){
            emit autoSaveTrigger();
        }
    });
}

MyView::~MyView()
{
    for (StickFigure* S : myAnimation->CurrentFrame()->stickFigures)
    {
        removeHandles(S);
    }
    for ( Frame* f : myAnimation->frameList)
    {
        delete f;
    }
    delete myAnimation;
    delete copyStickFigureBuffer;
}

void MyView::setTool(int Tool)
{
    changeTool();

    tool = Tool;
}

void MyView::deleteStickFigure()
{
    bool updateCurrentStickFigure =  false;
    if(my_stick_figure_widget_list->selectedItems().count() == 1 && myAnimation->currentFrame->stickFigures.count()>1)
    {
        //scegli lo stickfigure selzionato dalla lista grafica
        QListWidgetItem *current = my_stick_figure_widget_list->selectedItems()[0];
        // estrai il dato utente salvato in esso, puntatore ad uno stickfigure
        QVariant  retrievedData = (current->data(Qt::UserRole));
        //riconverti il dato
        StickFigure* cs = qvariant_cast<StickFigure*>(retrievedData);
        updateCurrentStickFigure = (cs == myAnimation->CurrentFrame()->CurrentStickFigure());

        //distruggi l'oggetto della lista
        my_stick_figure_widget_list->takeItem(my_stick_figure_widget_list->row(current));
        delete current;
        // delete from the total list
        for(StickData*s:cs->stickList){
            myAnimation->CurrentFrame()->totalSticks.removeAll(s);
        }
        //rimuovilo dal frame e distruggilo
        StickFigure* ns = myAnimation->currentFrame->removeStickFigure(cs);
        // Ora controlliamo la nuova selezione non sia nulla e aggiorniamo la selezione
        // dell'elenco del widget
        if(ns!=NULL && ns!=nullptr)
        {
            myAnimation->updateSelection(ns);
            my_stick_figure_widget_list->clearSelection();
            ns->linkedItem->setSelected(true);
            // senza questo, currentRow() poteva restare sulla riga che Qt seleziona di default dopo
            // takeItem() invece che su quella di ns: un successivo Move Layer Up/Down avrebbe quindi
            // spostato una riga diversa da quella su cui viene applicata la Z (myAnimation->CurrentFrame()->CurrentStickFigure())
            my_stick_figure_widget_list->setCurrentRow(my_stick_figure_widget_list->row(ns->linkedItem));
        }
        myAnimation->currentFrame->updateIcon();
        if(updateCurrentStickFigure){
             arrowSelection();
        }
    }
    else{
        QMessageBox::about(this,"error","There must be at least one stickfigure layer in the scene");
    }

}
//cambia l'ordine di profondità dello stickfigure selezionato
void MyView::moveStickFigureZ(int increment, int mode)
{
    if(my_stick_figure_widget_list->count() <= 1 )
        return;
    int currentIndex = my_stick_figure_widget_list->currentRow();
    if((currentIndex == 0 && increment<0)||(currentIndex == my_stick_figure_widget_list->count()-1 && increment>0))
        return;
    if(mode == MODE_TOP){
        increment = my_stick_figure_widget_list->count()-1-currentIndex;
        if(increment == 0 )
            return;
    }
    else if(mode == MODE_BOTTOM){
        increment = -currentIndex;
        if(increment == 0)
            return;
    }
    int newIdx = currentIndex+increment;
    QListWidgetItem *stickFigureAtNewIdxItem = my_stick_figure_widget_list->item(newIdx);
    // estrai il dato utente salvato in esso, puntatore ad uno stickfigure
    QVariant  retrievedData = (stickFigureAtNewIdxItem->data(Qt::UserRole));
    //riconverti il dato
    StickFigure* stickFigureAtNewIdx = qvariant_cast<StickFigure*>(retrievedData);

    if(newIdx<my_stick_figure_widget_list->count() && newIdx>=0 && newIdx != currentIndex){
        if(mode == MODE_TOP){

            myAnimation->CurrentFrame()->stickfigureToTop(myAnimation->CurrentFrame()->CurrentStickFigure());
        }
        else if(mode == MODE_BOTTOM){
            myAnimation->CurrentFrame()->stickfigureToBottom(myAnimation->CurrentFrame()->CurrentStickFigure());

        }
        else if(increment>0){

            myAnimation->CurrentFrame()->stickfigureAboveBelow(myAnimation->CurrentFrame()->CurrentStickFigure(),stickFigureAtNewIdx,1);
        }
        else{
            myAnimation->CurrentFrame()->stickfigureAboveBelow(myAnimation->CurrentFrame()->CurrentStickFigure(),stickFigureAtNewIdx,0);

        }
        //sposta nella lista grafica il layer dello stickfigure
        QListWidgetItem * item = my_stick_figure_widget_list->takeItem(currentIndex);
        my_stick_figure_widget_list->insertItem(newIdx,item);
        item->setSelected(true);
        my_stick_figure_widget_list->setCurrentRow(newIdx);

        myAnimation->currentFrame->updateIcon();
        myAnimation->CurrentFrame()->refresh(0);
    }

/*
    if(my_stick_figure_widget_list->count()>0)
    {
        my_stick_figure_widget_list->clearSelection();
        int currentIndex = my_stick_figure_widget_list->currentRow();
        if(!(currentIndex+1 == my_stick_figure_widget_list->count() && increment>0)||
                !(currentIndex == 0 && increment<0))
        {
            myAnimation->CurrentFrame()->CurrentStickFigure()->baseZ+=increment;
            myAnimation->CurrentFrame()->CurrentStickFigure()->updateZ(0.1);
            if(!(increment>0 && currentIndex == my_stick_figure_widget_list->count()-1) && !(increment<0 && currentIndex == 0))
            {
                //sposta nella lista grafica il layer dello stickfigure
                QListWidgetItem * item = my_stick_figure_widget_list->takeItem(currentIndex);
                my_stick_figure_widget_list->insertItem(currentIndex+increment,item);
                my_stick_figure_widget_list->setSelected(item,true);
                my_stick_figure_widget_list->setCurrentRow(currentIndex+increment);
            }
            myAnimation->currentFrame->updateIcon();
        }
    }
*/
}
void MyView::mapMyCoords(QPointF& myCoord, QPointF startingPoint){
    coord = mapToScene(startingPoint.x(),startingPoint.y());
    if(myCoord.x()<-my_rect->rect().width()/10)
        myCoord.setX(-my_rect->rect().width()/10);
    if(myCoord.y()<-my_rect->rect().height()/10)
        myCoord.setY(-my_rect->rect().height()/10);
    if(myCoord.y()>my_rect->rect().height()+my_rect->rect().height()/10)
        myCoord.setY(my_rect->rect().height()+my_rect->rect().height()/10);
    if(myCoord.x()>my_rect->rect().width()+my_rect->rect().width()/10)
        myCoord.setX(my_rect->rect().width()+my_rect->rect().width()/10);
}
void MyView::mousePressEvent(QMouseEvent *event)
{
    if(event->button()==Qt::LeftButton){
        isPressed = true;
    }
    else if(event->button()==Qt::RightButton){
        isRightPressed = true;
    }else if(event->button()==Qt::MiddleButton){
        isMiddlePressed = true;
    }
    // converti le coordinate del mouse in coordinate di scena
    mapMyCoords(coord,event->pos());


    startingCoord   = coord; //buffer per il dragghing del mouse premuto
    if(!isRightPressed && !isMiddlePressed){
    switch(tool)
    {
            case CLAMP:
            case NOTOOL:
            {
                storeUndo();
                arrowSelection();
                startStickFigureRotation = true;
                for(StickData*s:myAnimation->CurrentFrame()->CurrentStickFigure()->stickList){
                    if(s->view) s->view->angleBuffer2 = s->myLine.angle();
                }
                break;
            }
            case DRAW:
            case DRAWCIRCLE:
            case DRAWRECT:
            case DRAWTRAPEZOID:
            case DRAWTAPER:
            case DRAWIMG:
            {
                auto currentScene = scene();
                auto num = currentScene->items();
                drawCmd(&coord,(int)tool);

                num = currentScene->items();
                break;
            }
            case(ROTATE):
            {
                storeUndo();
                startStickFigureRotation = true;
                for(StickData*s:myAnimation->CurrentFrame()->CurrentStickFigure()->stickList){
                    if(s->view) s->view->angleBuffer2 = s->myLine.angle();
                }
                break;
            }
            case(SCALE):
            {
                storeUndo();
                start_stick_figure_scale = true;
                for(StickData*s:myAnimation->CurrentFrame()->CurrentStickFigure()->stickList){
                    if(s->view){
                        s->view->scaleBuffer = s->myLine.length();
                        s->view->widthBuffer = s->Pen.width();
                    }
                }
                break;
            }
            case(MOVESCENE):
            {
                storeUndo();
                break;
            }
            case(ROTATESCENE):
            {
                storeUndo();
                for(StickFigure* S: myAnimation->currentFrame->stickFigures){
                    //check it is not an empty layer
                    if(S->masterStick && S->stickList.count()>0){
                        S->originPosBuffer = S->masterStick->myLine.p1();
                        S->sceneRotationBuffer = 0;
                        for(StickData*s:S->stickList){
                            if(s->view) s->view->angleBuffer2 = s->myLine.angle();
                        }
                    }
                }
                break;
            }
            case(SCALESCENE):
            {
                for(StickFigure* S: myAnimation->currentFrame->stickFigures){
                    //check it is not an empty layer
                    if(S->masterStick && S->stickList.count()>0){
                        S->originPosBuffer = S->masterStick->myLine.p1();
                        for(StickData*s:S->stickList){
                            if(s->view){
                                s->view->scaleBuffer = s->myLine.length();
                                s->view->widthBuffer = s->Pen.width();
                            }
                        }
                    }
                }
                storeUndo();
                break;
            }
            case(JOIN):{
                if(myAnimation->CurrentFrame()->CurrentStickFigure()!= nullptr){
                    //check layer is not empty
                    if(myAnimation->CurrentFrame()->CurrentStickFigure()->stickList.count()>0){
                        joinToStickFigure = myAnimation->CurrentFrame()->CurrentStickFigure();
                        joinToStick = joinToStickFigure->currentStick;
                        storeUndo();
                        arrowSelection();

                        if(myAnimation->CurrentFrame()->CurrentStickFigure()!=joinToStickFigure && myAnimation->CurrentFrame()->CurrentStickFigure()->stickList.count()>0){
                            removeHandles(joinToStickFigure);
                            mergeStickFigures(joinToStickFigure,joinToStick,myAnimation->CurrentFrame()->CurrentStickFigure());
                            addHandles(joinToStickFigure);
                            deleteStickFigure();
                            for(StickData*s:myAnimation->CurrentFrame()->CurrentStickFigure()->stickList){
                                if(!myAnimation->CurrentFrame()->totalSticks.contains(s))
                                    myAnimation->CurrentFrame()->totalSticks.append(s);
                            }
                            qDebug("merge ok\r\n");
                        }
                    }
                }
                break;
            }
            case(SPLIT):{

                break;
            }
            case(ELONGATE):
            {
                storeUndo();
                arrowSelection();
                if(myAnimation->CurrentFrame()->CurrentStick()==nullptr) break; // niente da selezionare (nessuno stick nel frame)
                lengthBuffer = myAnimation->CurrentFrame()->CurrentStick()->myLine.length();
                break;
            }
            case(MANIPULATE):
            {
                storeUndo();
                arrowSelection();
                if(myAnimation->CurrentFrame()->CurrentStick()==nullptr) break; // niente da selezionare (nessuno stick nel frame)
                lengthBuffer = myAnimation->CurrentFrame()->CurrentStick()->myLine.length();

                break;
            }
    }
    }
    //QGraphicsView::mousePressEvent(event);
}

void MyView::keyPressEvent(QKeyEvent *event)
{
    switch(event->key())
    {
        case(Qt::Key_Cancel):
        case(Qt::Key_Delete):
        {
            if(scene()->selectedItems().count() == 1 && myAnimation->CurrentFrame()->CurrentStickFigure()->drawCount==0)
            {
                storeUndo();

                StickGraphicsItem *item = (StickGraphicsItem*)scene()->selectedItems()[0];//oggetto selezionato nella scnea
                StickData *item_data = item->data;
                int idx = myAnimation->CurrentFrame()->CurrentStickFigure()->stickList.indexOf(item_data);        // indice corrispondente nella lista ordinata degli oggetti
                // se si tratta dello stick master e ci sono altri stickfigure, cancella l'intero stickfigure
                if(myAnimation->CurrentFrame()->CurrentStickFigure()->stickList[idx] == myAnimation->CurrentFrame()->CurrentStickFigure()->masterStick &&
                        myAnimation->currentFrame->stickFigures.count()>1)
                {
                    //estrai dalla lista grafica l'elemento selezionato collegato allo stickfigure
                    QListWidgetItem * associatedItem = myAnimation->CurrentFrame()->CurrentStickFigure()->linkedItem;
                    my_stick_figure_widget_list->clearSelection();
                    associatedItem->setSelected(true);
                    //cancella lo stickfigure
                    deleteStickFigure();
                }
                //altrimenti cancella il singolo stick e aggiorna l'iconea del livello
                else if(my_stick_figure_widget_list->selectedItems().count()==1)
                {
                    StickData* nextCurrentStick = myAnimation->CurrentFrame()->CurrentStick()->parent;
                    // questa lista serve per rimuovere dalla totalStick tutti gli stick cancellati
                    QList<StickData*> oldStickList = myAnimation->CurrentFrame()->CurrentStickFigure()->stickList;
                    myAnimation->CurrentFrame()->CurrentStickFigure()->deleteStick( idx);
                    QList<StickData*> survivors = myAnimation->CurrentFrame()->CurrentStickFigure()->stickList;
                    for(StickData*s:oldStickList){
                        if(!survivors.contains(s)){
                            myAnimation->CurrentFrame()->totalSticks.removeAll(s);
                        }
                    }
                    myAnimation->CurrentFrame()->numOfItems = myAnimation->CurrentFrame()->totalSticks.length();
                    myAnimation->CurrentFrame()->CurrentStickFigure()->updateIcon();
                    my_stick_figure_widget_list->selectedItems()[0]->setIcon(*myAnimation->CurrentFrame()->CurrentStickFigure()->stickFigureIcon);
                    myAnimation->CurrentFrame()->CurrentStick() = nextCurrentStick;
                }
                myAnimation->currentFrame->updateIcon();
                // distruggi l'oggetto
                qDebug("item deleted");
            }
            break;
        }
        case(Qt::Key_Escape):
        {
            switch(tool)
            {
                case(DRAW):
                case DRAWCIRCLE:
                {
                    if(myAnimation->CurrentFrame()->CurrentStickFigure()->drawCount==1)
                    {
                        myAnimation->CurrentFrame()->CurrentStickFigure()->cancelDrawing();
                    }
                    break;
                }
            }
            break;
        }
    }

    QGraphicsView::keyPressEvent(event);
}
void MyView::arrowSelection(){
    if(myAnimation->CurrentFrame()->stickFigures.isEmpty())
        return;
    if(myAnimation->CurrentFrame()->totalSticks.empty())
        return;
    myAnimation->updateSelection(coord);
    my_stick_figure_widget_list->clearSelection();
    myAnimation->CurrentFrame()->CurrentStickFigure()->linkedItem->setSelected(true);
    my_stick_figure_widget_list->setCurrentRow(my_stick_figure_widget_list->row(myAnimation->CurrentFrame()->CurrentStickFigure()->linkedItem));
    if(myAnimation->CurrentFrame()->CurrentStick()!=nullptr && myAnimation->CurrentFrame()->CurrentStickFigure()->stickList.count()>0
            && myAnimation->CurrentFrame()->CurrentStick()->view)
        myAnimation->CurrentFrame()->CurrentStick()->view->populateImageListWidget();
    //dbg_lbl->setText(QString::number(myAnimation->CurrentFrame()->CurrentStick()->order));
}
// il disegno di una linea conta 2 click
// il primo determina lo starting point
// il secondo l'end point
void MyView::drawCmd(QPointF* point, int mode)
{
    if(!myAnimation->currentFrame->stickFigures.isEmpty() && myAnimation->CurrentFrame()->CurrentStickFigure() != nullptr)
    {

        if(myAnimation->CurrentFrame()->CurrentStickFigure()->drawCount == 0){
            storeUndo();

            myAnimation->CurrentFrame()->CurrentStickFigure()->startDrawing(point, myPen, myBrush);
            StickData *new_stick = myAnimation->CurrentFrame()->CurrentStick();
            // il costruttore di stick imposta già new_stick->view: la scena vera e propria
            // riceve l'elemento dentro AttachView(), senza una addItem() separata
            FramePresenter::AttachView(new_stick, scene());
            if(!myAnimation->CurrentFrame()->CurrentStickFigure()->stickList.isEmpty()){
               new_stick->view->selected = false;
               new_stick->view->refresh(0);
            }
            new_stick->view->selected = true;
            new_stick->hardTop = hard_top_check->isChecked();
            new_stick->hardBottom = hard_bottom_check->isChecked();
            new_stick->Pen = myPen;
            // se stiamo usando il tool per generare cerchi cambiamo il tipo dello stick
            if(mode == DRAWCIRCLE){
                new_stick->stickType = StickData::CIRCLE;
            }
            else if(mode == DRAWRECT){
                new_stick->stickType = StickData::RECT;
                new_stick->imgWScale = advanced_rect_slider->value();
                new_stick->Pen.setJoinStyle(Qt::MiterJoin);
            }
            else if(mode == DRAWTAPER){
                new_stick->stickType = StickData::TAPER;
                new_stick->imgHScale = taper_h_slider->value();
                new_stick->imgWScale = taper_w_slider->value();
            }
            else if(mode == DRAWTRAPEZOID){
                new_stick->stickType = StickData::TRAPEZOID;
                new_stick->imgHScale = taper_h_slider->value();
                new_stick->imgWScale = taper_w_slider->value();
                new_stick->Pen.setJoinStyle(Qt::MiterJoin);
            }
            else if(mode == DRAWIMG){
                new_stick->stickType = StickData::IMAGE;
                QImage* img = new QImage(*image_draw_buffer);

                new_stick->view->addImage(img, image_name_buffer);

            }

           qDebug("Draw 1 = %f, %f",point->x(),point->y());
        }
        else if(myAnimation->CurrentFrame()->CurrentStickFigure()->drawCount >0)
        {
            myAnimation->CurrentFrame()->CurrentStickFigure()->endDrawing(point);
            scene()->addItem(myAnimation->CurrentFrame()->CurrentStick()->view->stickHandle[0]);
            scene()->addItem(myAnimation->CurrentFrame()->CurrentStick()->view->stickHandle[1]);
            qDebug("Draw 2 = %f, %f",point->x(),point->y());
            myAnimation->currentFrame->updateIcon();
            myAnimation->updateTab(myAnimation->CurrentFrame()->CurrentStick()->stickType);
            // add it to the total scene list
            myAnimation->currentFrame->totalSticks.append(myAnimation->CurrentFrame()->CurrentStick());
            myAnimation->updateSelection(myAnimation->CurrentFrame()->CurrentStick());

        }
    }
    else
    {
        QMessageBox::about(this,"error","no stickfigure layer present, please add a stickfigure");
    }


}
//Esci dallo strumento che stai usando
void MyView::changeTool()
{
    switch(tool)
    {
        case(NOTOOL):
        {
            break;
        }
        case(DRAW):
        case(DRAWCIRCLE):
        case(DRAWRECT):
        case(DRAWTAPER):
        case(DRAWTRAPEZOID):
        case(DRAWIMG):
        {
            myAnimation->updateTab(tool); // updates ui panels
            if(!myAnimation->currentFrame->stickFigures.isEmpty())
            {
                // se stavi disegnando uno stick e cambi prima di averlo finito, cancellalo!
                if(myAnimation->CurrentFrame()->CurrentStickFigure()->drawCount==1)
                {
                    scene()->removeItem((QGraphicsItem*)myAnimation->CurrentFrame()->CurrentStick());
                    myAnimation->CurrentFrame()->CurrentStickFigure()->cancelDrawing();
                }
            }
            else{

            }
            if(tool == DRAWIMG){
                delete image_draw_buffer;
                image_draw_buffer = nullptr;
            }
            break;
        }
        case(MANIPULATE):
        {
            break;
        }
        case(ROTATE):
        {
            break;
        }
        case(SCALE):
        {
            break;
        }
        case(ELONGATE):
        {
            break;
        }
        case(MOVESCENE):
        {
            break;
        }
        case(ROTATESCENE):
        {
            break;
        }
        case(SCALESCENE):
        {
            break;
        }
        case(JOIN):
        {
            break;
        }
        case(SPLIT):
        {
            break;
        }
    }
}

void MyView::wheelEvent(QWheelEvent *event)
{
    if(myAnimation->CurrentFrame()->CurrentStickFigure() !=nullptr){
        if(!myAnimation->CurrentFrame()->CurrentStickFigure()->stickList.isEmpty() && myAnimation->CurrentFrame()->CurrentStickFigure()->currentStick!=nullptr){
            int idx = myAnimation->CurrentFrame()->totalSticks.indexOf(myAnimation->CurrentFrame()->CurrentStickFigure()->currentStick);
            if(event->angleDelta().y()>0){
                for(int j = idx+1;j<myAnimation->CurrentFrame()->totalSticks.length();j++){
                    if(myAnimation->CurrentFrame()->CurrentStickFigure()->stickList.contains(myAnimation->CurrentFrame()->totalSticks[j])){
                        myAnimation->CurrentFrame()->CurrentStickFigure()->highlight(false);

                        myAnimation->CurrentFrame()->selectStick(myAnimation->CurrentFrame()->CurrentStickFigure(),myAnimation->CurrentFrame()->totalSticks[j]);
                        break;
                    }
                }
            }
            else{
                for(int j = idx-1;j>-1;j--){
                    if(myAnimation->CurrentFrame()->CurrentStickFigure()->stickList.contains(myAnimation->CurrentFrame()->totalSticks[j])){
                        myAnimation->CurrentFrame()->CurrentStickFigure()->highlight(false);
                        myAnimation->CurrentFrame()->selectStick(myAnimation->CurrentFrame()->CurrentStickFigure(),myAnimation->CurrentFrame()->totalSticks[j]);
                        break;
                    }
                }
            }
            myAnimation->CurrentFrame()->CurrentStickFigure()->refresh(0);
        }
    }
}



// mouse rilasciato
void MyView::mouseReleaseEvent(QMouseEvent *event)
{
    if(event->button()==Qt::LeftButton){
        isPressed = false;
        if(myAnimation->currentFrame->stickFigures.count()>0)
        {
            if(myAnimation->CurrentFrame()->CurrentStickFigure()->stickList.count()>0
                     && my_stick_figure_widget_list->selectedItems().count() == 1)
            {
                //termina una rotazione di stick al rilascio
                if(myAnimation->CurrentFrame()->CurrentStick()->view)
                    myAnimation->CurrentFrame()->CurrentStick()->view->endRotation();
                myAnimation->CurrentFrame()->CurrentStickFigure()->updateIcon();
                my_stick_figure_widget_list->selectedItems()[0]->setIcon(*myAnimation->CurrentFrame()->CurrentStickFigure()->stickFigureIcon);
                myAnimation->currentFrame->updateIcon();
            }
            if(tool == MOVESCENE || tool == ROTATESCENE || tool == SCALESCENE){
                // questi strumenti modificano ogni stickfigure del frame: aggiorna l'icona di ognuno,
                // non solo di quello selezionato, altrimenti le altre anteprime restano quelle pre-trascinamento
                for(StickFigure* S: myAnimation->currentFrame->stickFigures){
                    S->updateIcon();
                    S->linkedItem->setIcon(*S->stickFigureIcon);
                }
            }
        }
    }else if(event->button()==Qt::RightButton){
        isRightPressed = false;
        setTransformationAnchor(QGraphicsView::AnchorViewCenter);
    }else if(event->button()==Qt::MiddleButton){
        isMiddlePressed = false;
        //zoomLvl = zoomLvlBuf/zoomLvl;
        setTransformationAnchor(QGraphicsView::AnchorViewCenter);
    }

    QGraphicsView::mouseReleaseEvent(event);
}

// Se il mouse viene mosso durante il disegno di una linea continua ad aggiornarne il disegno
// Se viene mosso durante una pressione con stick selezionato, agisci sul relativo puntatore all'elemento linea
// per ruotarla seguendo il mouse
void MyView::mouseMoveEvent(QMouseEvent *event)
{
    static QPointF prevCoord(0,0);
    mapMyCoords(coord, event->pos());
    switch(tool)
    {
        case CLAMP:
        case NOTOOL:
        {
            if(isPressed)
            {
                if(scene()->selectedItems().count() == 1)
                {
                    //trova l'indice dello stick selezionato dentro la lista nella scena cercando il puntatore nelle liste
                    StickGraphicsItem* item = (StickGraphicsItem*)scene()->selectedItems()[0];
                    myAnimation->updateSelection(item->data);

                    //qDebug("selected item %d", idx);
                    // Ora ruota lo stick
                    if(myAnimation->CurrentFrame()->CurrentStickFigure()->stickList.count()>0){
                        if(tool == CLAMP){
                            if(myAnimation->CurrentFrame()->CurrentStick()->view)
                                myAnimation->CurrentFrame()->CurrentStick()->view->rotate(&coord);
                        }
                        else{
                            QList<StickGraphicsItem*> toRotate;
                            if(myAnimation->CurrentFrame()->CurrentStick() == myAnimation->CurrentFrame()->CurrentStickFigure()->masterStick){
                                for(StickData*s:myAnimation->CurrentFrame()->CurrentStickFigure()->stickList){
                                    if(!s->stepchild && s->view){
                                        toRotate.append(s->view);
                                    }
                                }
                            }else{
                                for(StickData* s:myAnimation->CurrentFrame()->CurrentStick()->children)
                                    if(s->view) toRotate.append(s->view);
                            }
                            if(myAnimation->CurrentFrame()->CurrentStick()->view)
                                toRotate.append(myAnimation->CurrentFrame()->CurrentStick()->view);
                            myAnimation->CurrentFrame()->CurrentStickFigure()->rotateStickFigure(&coord,toRotate,false);
                        }
                            //
                    }
                }
            }
            break;
        }
        case DRAW:
        case DRAWCIRCLE:
        case DRAWRECT:
        case DRAWTAPER:
        case DRAWTRAPEZOID:
        case DRAWIMG:
        {
        // durante il disegno,l'estremo libero della linea segue il mouse
            if(myAnimation->CurrentFrame()->CurrentStickFigure() != nullptr && myAnimation->CurrentFrame()->CurrentStickFigure() != nullptr)
            {
                if(myAnimation->CurrentFrame()->CurrentStickFigure()->drawCount > 0)
                {
                    myAnimation->CurrentFrame()->CurrentStickFigure()->previewDrawing(&coord);
                    qDebug("coord = %f, %f",coord.x(),coord.y());
                }
            }
            else
            {
            }
            break;
        }
        case MANIPULATE:
        {
            if(myAnimation->CurrentFrame()->CurrentStickFigure() != nullptr && myAnimation->CurrentFrame()->CurrentStick()!=nullptr)
            {
                if(isPressed){
                    if(myAnimation->CurrentFrame()->CurrentStick()->view)
                        myAnimation->CurrentFrame()->CurrentStick()->view->manipulate(&coord);
                    elongate_spinbox->setValue(myAnimation->CurrentFrame()->CurrentStick()->myLine.length());
                }
            }
            break;
        }
        case ROTATE:
        {
            if(myAnimation->CurrentFrame()->CurrentStickFigure() != nullptr)
            {
                if(isPressed){
                    QList<StickGraphicsItem*> views;
                    for(StickData* s: myAnimation->CurrentFrame()->CurrentStickFigure()->stickList){
                        if(s->view) views.append(s->view);
                    }
                    myAnimation->CurrentFrame()->CurrentStickFigure()->rotateStickFigure(&coord,views,true);
                }
            }
            break;
        }
        case(SCALE):
        {
            if(myAnimation->CurrentFrame()->CurrentStickFigure() != nullptr && isPressed){
               myAnimation->CurrentFrame()->CurrentStickFigure()->scale(&coord);
            }
            break;
        }
        case(MOVESCENE):
        {
            if(isPressed && !myAnimation->currentFrame->stickFigures.isEmpty()){
                int dx = -startingCoord.x()+coord.x();
                int dy = -startingCoord.y()+coord.y();
                startingCoord = coord;
                for(StickFigure*S:myAnimation->currentFrame->stickFigures){
                    //check it is not an empty layer
                    if( S->stickList.count()>0 && S->masterStick){
                        S->traslate(dx,dy);
                    }
                }
            }
            break;
        }
        case(ROTATESCENE):
        {
            if(isPressed && !myAnimation->currentFrame->stickFigures.isEmpty()){
                QPointF center(W/2,H/2); // centro del sistema
                QLineF mouseToCenterNow(coord,center); // linea che connette il centro al mouse
                QLineF mouseToCenterStart(startingCoord,center); // linea che connette il centro alla posizione del mouse precedente
                float startingAngle = mouseToCenterStart.angle(); // angolo assoluto del mouse prima rispetto al centro

                float actualAngle = mouseToCenterNow.angle();// angolo assoluto del mouse ora rispetto al centro
                float angle = actualAngle-startingAngle; // incremento in angolo rispetto a prima
                // normalizza l'incremento: angle() torna un valore in [0,360), quindi attraversare lo
                // 0/360 senza questo produce uno scatto di quasi un giro intero per un singolo frame
                if(angle>180) angle -= 360;
                else if(angle<-180) angle += 360;
                startingCoord = coord; // le coordinate attuali vengono memorizzate per la prossima iterazione
                for(StickFigure*S:myAnimation->currentFrame->stickFigures){
                    //check it is not an empty layer
                    if( S->stickList.count()>0 && S->masterStick){
                        QLineF connectCenter(center,S->masterStick->myLine.p1()); // linea che connette il master dello stick al centro

                        connectCenter.setAngle(connectCenter.angle()+angle); // ruotiamola dell'angolo di incremento
                        // ruota ogni singolo stick
                        for(StickData *s:S->stickList){

                            if(s->view){
                                s->view->rotate(angle);
                                s->view->angleBuffer2+=angle; // aggiorna il buffer interno dell'angolo dello stick
                            }
                        }

                        QLineF Delta(S->originPosBuffer,connectCenter.p2()); // traslazione della punta della lineacentro-master ruotata
                        //trasla lo stickfigure dell'ammontare
                        S->traslate(Delta.dx(),Delta.dy());
                        S->originPosBuffer = S->masterStick->myLine.p1(); // agiorna il buffer di posizione dello stickfigure
                    }
                    }
            }
            break;
        }
        case(SCALESCENE):
        {


            if(isPressed && !myAnimation->currentFrame->stickFigures.isEmpty()){
                float scaleAmount = 1;
                float dist = 0;
                QPointF center(W/2,H/2);
                float startDist = QLineF(startingCoord,center).length();
                if(startDist<1) // evita una divisione per zero (o quasi) se si preme esattamente sul centro
                    break;
                dist = QLineF(coord,center).length();
                scaleAmount = dist/startDist;
                for(StickFigure * S:myAnimation->currentFrame->stickFigures){
                    //check it is not an empty layer
                        if( S->stickList.count()>0 && S->masterStick){
                        // per traslare lo stick verso il centro [W/2,H/2] proporzionalmente
                        // serve calcolare la distanza del masterstick da essa
                        // moltiplicare dx e dy per lo scale factor
                        // usarli come traslazione
                        float dx = 0;
                        float dy = 0;
                        // il dx e dy deve essere una quantità tale da non far eccedere ad actual dist ilv alore  di originaldist
                        QLineF actualDistFromCenter(S->masterStick->myLine.p1(),center);
                        QLineF originalDistFromCenter(S->originPosBuffer,center);
                        // quindi fai la differenza fra il valore di distaza originale (max) e la distanza attuale modificata dal fattore di scala per dx e dy
                        dx = -originalDistFromCenter.dx()*scaleAmount+actualDistFromCenter.dx();
                        dy = -originalDistFromCenter.dy()*scaleAmount+actualDistFromCenter.dy();
                        S->traslate(dx,dy);
                        for(StickData* s:S->stickList){
                            if(s->view) s->view->scale(scaleAmount);

                        }
                    }
                }
            }
            break;
        }
        case(ELONGATE):
        {
            if(myAnimation->CurrentFrame()->CurrentStickFigure()==nullptr)
                break;
            if(myAnimation->CurrentFrame()->CurrentStick() == nullptr)
                break;
            if(isPressed){
                QLineF newEndPoint(myAnimation->CurrentFrame()->CurrentStick()->myLine.p1(),coord);
                lengthBuffer = newEndPoint.length();
                myAnimation->CurrentFrame()->CurrentStickFigure()->elongate(coord,myAnimation->CurrentFrame()->CurrentStick());
                elongate_spinbox->setValue(lengthBuffer);
            }
            break;
        }

    }
    if(isRightPressed){
        setTransformationAnchor(QGraphicsView::AnchorUnderMouse);
        QLineF travel(prevCoord,coord);
        translate(travel.dx(),travel.dy());
    }else if(isMiddlePressed){

        //calcola lo scostamento dal centro all'inizio della pressione e quella attuale
        float distFromCenter0 = QLineF(prevCoord,QPointF(W/2,H/2)).length();
        float distFromCenter1 = QLineF(coord,QPointF(W/2,H/2)).length();

        zoomLvlBuf = zoomLvl+(distFromCenter1-distFromCenter0)/(0.5*sqrtf(W*W+H*H)); // zoombuf è incrementato dal valore precedente di un valore sempre minore di 1

        scale(zoomLvlBuf/zoomLvl,zoomLvlBuf/zoomLvl); // la divisione serve a riportare allo zoom 0, lo zoom applicato è zombuf
        zoomLvl = zoomLvlBuf;
    }
    prevCoord = coord;
    QGraphicsView::mouseMoveEvent(event);
}
//funzione usata per riscalare interamente il frame quando si riscala l'animazione intera
void MyView::scaleFrame(Frame* F, float scaleAmount, QRectF R){
    QPointF center(R.width()/2,R.height()/2);
    if(F->stickFigures.isEmpty())
        return;
    for(StickFigure * S:F->stickFigures){
        if(!S->stickList.isEmpty()){
            QLineF actualDistFromCenter(S->masterStick->myLine.p1(),center);
            float dx =-(scaleAmount-1)*actualDistFromCenter.dx();
            float dy =-(scaleAmount-1)*actualDistFromCenter.dy();
            S->traslate(dx,dy);
            for(StickData* s:S->stickList){
                if(s->view){
                    s->view->scaleBuffer = s->myLine.length();
                    s->view->widthBuffer = s->Pen.widthF();
                    s->view->scale(scaleAmount);
                }
            }
        }

    }
}
Frame* MyView::setUpFrame(int pos){
    //decidi il nome
    //int intName = 0;
    /*if(myAnimation->frameList.count()>=1){
        intName = myAnimation->currentFrame->frameNumber+1;

    }*/
    return myAnimation->setupFrame(pos);
}
void MyView::deleteFrame(Frame* frame){
    int frameNum = frame->frameNumber;
    my_frame_widget_list->takeItem(my_frame_widget_list->row(frame->linkedItem));
    myAnimation->deleteFrame(frame);
    my_frame_widget_list->clearSelection();
    for(Frame *f: myAnimation->frameList){
        if(f->frameNumber>frameNum){
            f->frameNumber--;
            f->linkedItem->setData(Qt::DisplayRole, QString::number(f->frameNumber));
        }
    }
    myAnimation->currentFrame->linkedItem->setSelected(true);
}
// routine per passare da un frame ad un altro, svuota la scena, svuota le widget list, ripopola entrambi, aggiorna onion skins e selezioni
void MyView::moveToFrame(Frame* frame){
    // rimuovi tutti gli stick dalla scena
    auto currentScene = scene();
    auto num = currentScene->items();
    if(myAnimation->frameList.count()>=1){
        for(StickFigure *S:myAnimation->currentFrame->stickFigures){
            removeHandles(S);
            for(StickData * s: S->stickList){
               if(s->view && scene()->items().contains(s->view)){
                    currentScene->removeItem(s->view);

               }
            }


        }
    }
    num = currentScene->items();
    // rimuovigli elementi dalla lista widget
    my_stick_figure_widget_list->clear();

    // assicura che ogni stick del nuovo frame abbia una view PRIMA di addHandles() qui sotto:
    // un frame appena caricato da file (attach_to_scene=false) non ne ha ancora una, un frame
    // già visitato in precedenza sì, basta ri-attaccarla alla scena
    for(int j = 0;j<frame->numOfItems;j++)
    {
        StickData *s = frame->totalSticks[j];
        if(s->view)
            currentScene->addItem(s->view);
        else
            FramePresenter::AttachView(s, currentScene);
    }

    // aggiungi tutta gli stickfigure del nuovo frame alla lista del widget
    for(StickFigure *S:frame->stickFigures)
    {
        QListWidgetItem * addedItem = new QListWidgetItem(my_stick_figure_widget_list);
        QVariant newData(QVariant::fromValue(S));
        addedItem->setData(Qt::UserRole,newData);
        addedItem->setData(Qt::DisplayRole,S->name);
        addedItem->setIcon(*S->stickFigureIcon);
        S->linkedItem = addedItem;
        my_stick_figure_widget_list->addItem(S->linkedItem);
        // aggiungi alla scena
        if(!playBack){
            S->updateBoundingRects();
            S->updateIcon();
            addHandles(S);
        }
    }
    currentScene->clearSelection();
    myAnimation->currentFrame = frame;

    // aggiorna il current frame, se si parte d auna situazione di inizio forza il primo stickfigure
    if(frame->currentStickFigure !=nullptr){
        if(frame->currentStickFigure->currentStick!=nullptr){
            frame->selectStick(frame->currentStickFigure,frame->currentStickFigure->currentStick);
        }
        else{
            arrowSelection();
        }
    }
    else if(frame->stickFigures.count() == 1 && frame->stickFigures[0]->stickList.isEmpty()){
        myAnimation->CurrentFrame()->CurrentStickFigure() = frame->stickFigures[0];
        myAnimation->updateSelection(myAnimation->CurrentFrame()->CurrentStickFigure());
    }else{
        arrowSelection();
    }

    if(!playBack)
    {
        updateOnionSkins();//aggiorna gli onion skins
    }
    num = currentScene->items();
    //scene()->update(myR);
    // se non siamo in modalità playback aggiorna le selezioni nelle widgetlists e delle iconee
    if(playBack == false){
        myAnimation->currentFrame->linkedItem->setSelected(true);
        my_frame_widget_list->setCurrentItem(myAnimation->currentFrame->linkedItem);
        // niente guardia su stickFigures.isEmpty(): updateIcon() gestisce già correttamente un
        // frame vuoto (lo riempie di bianco), senza di essa un frame appena creato restava senza
        // anteprima invece di mostrare una tela bianca
        if(!myAnimation->frameList.isEmpty()){
            myAnimation->currentFrame->updateIcon();
        }
        /*
        if(my_rect == nullptr){
            my_rect = new QGraphicsRectItem(0,0,W,H);
            my_rect->setPen(Qt::NoPen);
            my_rect->setZValue(-100);
            if(!currentScene->items().contains(my_rect))
            {
                currentScene->addItem(my_rect);
            }
        }
        */

    }
    num = currentScene->items();
    myAnimation->CurrentFrame()->refresh(0);
}
// routine per calcolare gli onion skins, prima cancellali, ridisegnali su un immagine, e aggiungi l'immagine alla scena
void MyView::updateOnionSkins(){
    // svuota i buffer di immagine vecchi (presneti nella scena) immagazzinati nel ciclo for precedente
    for(QGraphicsPixmapItem* P:onionSkins){
        scene()->removeItem(P);
        delete P;
    }
    onionSkins.clear();
    if(onion_skin_sb->value()==0 || myAnimation->currentFrame->frameNumber == 0)
        return;
    // imposta la flag per routine speciali
    onionRender = true;
    QGraphicsScene renderScene; // scena fittizia su cui renderizzare gli onionskins
    // immagine fittizia su cui registrare gli onion skins
    QImage renderImg =  QImage(scene()->sceneRect().width(),scene()->sceneRect().height(),QImage::Format_ARGB32);

    renderScene.setSceneRect(scene()->sceneRect());
    // dallo spinbox trai il valore del numero di onionskins servono
    int skinNum = onion_skin_sb->value();
    float opacityVal = 0;
    if(myAnimation->currentFrame->frameNumber<skinNum)
    {
        skinNum = myAnimation->currentFrame->frameNumber;
    }
    // disegna sulla scena fittizia gli stickfigures dei frame da onionskinnare che partono dal frame corrente- numero di skin calcolabili
    for(int i = myAnimation->currentFrame->frameNumber-skinNum; i<myAnimation->currentFrame->frameNumber; i++){
        // todo anzichè copiare stickfigure per stickfigure, copiare l'intera scena
        // questi stick sono VIVI (hanno già una view sul frame i): clonarne i dati prima di
        // wrapparli evita che il costruttore di stick rubi il back-pointer data->view alla
        // view reale (vedi StickFigure::updateIcon per lo stesso pattern)
        QList<StickData*> renderClones;
        for(StickData *s:myAnimation->frameList[i]->totalSticks)
        {
            StickData* clone = s->Clone();
            renderClones.append(clone);
            renderScene.addItem(new StickGraphicsItem(clone));
        }
        QPainter painter(&renderImg);
        renderImg.fill(Qt::transparent);
        painter.setBackground(QBrush(QColor(Qt::transparent)));
        renderScene.render(&painter);
        // più è vecchio l'onion skin più è trasparente, più è vicino al frame corrente più è opaco
        opacityVal += 0.5/skinNum;
        // trasferisci la scena renderizzata
        QGraphicsPixmapItem* item = new QGraphicsPixmapItem(QPixmap::fromImage((renderImg)));
        item->setOffset(onion_offset);
        item->setOpacity(opacityVal);
        item->setZValue(0);
        onionSkins.append(item);
        scene()->addItem(item);
        renderScene.clear(); // distruggi le view di rendering PRIMA di liberare i dati clonati
        qDeleteAll(renderClones);
        painter.end();

    }

    onionRender = false;
}
/**
 * @brief Single QUndoCommand shape covering every undoable edit (REFACTOR_PLAN.md
 *        Phase 4's permitted simplification: one generic snapshot command instead of
 *        one subclass per user action).
 *
 * @details The mutation itself is never re-run through this class — it already
 * happened inline in the caller, exactly as before Phase 4 (a mouse drag, a button
 * handler, a slider callback). This command only *records* that edit so it can be
 * reverted/reapplied later:
 * - The constructor captures `before_` (the live frame, right before the caller's
 *   mutation runs) — this is what `storeUndo()` used to push directly.
 * - `redo()`'s FIRST call (made automatically by `QUndoStack::push()`, before the
 *   caller's mutation has even run yet) is a deliberate no-op — there is nothing to
 *   reapply on first push, the caller's own code performs the edit right after.
 * - `undo()`'s FIRST call captures `after_` lazily (the live frame now holds
 *   whatever the edit produced, since it ran normally) before restoring `before_`.
 *   This mirrors the pre-Phase-4 design exactly: the old code only ever captured a
 *   "redo" snapshot at the moment `undoRedo()` actually needed one, never eagerly.
 * - Every subsequent `redo()`/`undo()` call is a pure, symmetric snapshot restore.
 */
class FrameUndoCommand : public QUndoCommand
{
public:
    FrameUndoCommand(MyView *view, int command_tag)
        : view_(view), command_tag_(command_tag)
    {
        before_ = ExtractFrameData(*view_->myAnimation->currentFrame);
    }
    ~FrameUndoCommand() override
    {
        delete before_;
        delete after_;
    }
    void redo() override
    {
        if(is_first_redo_){
            is_first_redo_ = false;
            return;
        }
        view_->RestoreFrameSnapshot(command_tag_, MODE_REDO, after_);
    }
    void undo() override
    {
        if(after_ == nullptr)
            after_ = ExtractFrameData(*view_->myAnimation->currentFrame);
        view_->RestoreFrameSnapshot(command_tag_, MODE_UNDO, before_);
    }
private:
    MyView *view_;
    int command_tag_;
    FrameData *before_ = nullptr;
    FrameData *after_ = nullptr;
    bool is_first_redo_ = true;
};

/**
 * @details Replaces the pre-Phase-4 `undoRedo()`'s dispatch body verbatim, just
 * driven by a QUndoCommand's before/after snapshot instead of a popped buffer entry.
 * No flag needed here anymore: `Frame::clearFrame()` and `~StickFigure()` each own
 * exactly the cleanup they perform, so nothing needs to skip work for the other.
 */
void MyView::RestoreFrameSnapshot(int command_tag, int mode, FrameData *snapshot)
{
    // se è un undo su un addframe, o un redo su un deleteframe ci sta da rimuovere un frame
    if((command_tag == CMD_ADDFRAME && mode == MODE_UNDO)||
            (command_tag == CMD_DELETEFRAME && mode == MODE_REDO)){
        // routine di cancellazione del frame
        deleteFrame(myAnimation->currentFrame);
        // svuota il frame da popolare con il buffer e ripopolalo dallo snapshot,
        // attaccando davvero gli stick alla scena (il target è il frame VERO)
        myAnimation->frameList[snapshot->frameNumber]->clearFrame();
        PopulateLiveFrame(myAnimation->frameList[snapshot->frameNumber],*snapshot,true,true);
        // spostati verso tale frame
        moveToFrame(myAnimation->frameList[snapshot->frameNumber]);
    }
    // se è un undo su uno spostamento
    else if(command_tag == CMD_MOVETOFRAME){
        //svuota il frame di destinazione e popolalo con lo snapshot
        myAnimation->frameList[snapshot->frameNumber]->clearFrame();
        PopulateLiveFrame(myAnimation->frameList[snapshot->frameNumber],*snapshot,true,true);
        // spostati sul frame di destinazione
        moveToFrame(myAnimation->frameList[snapshot->frameNumber]);
    }
    // se è un undo su un deleteframe o un redo su un adddframe bisogna aggiungere un frame
    else if((command_tag == CMD_DELETEFRAME && mode == MODE_UNDO)||
            (command_tag == CMD_ADDFRAME && mode == MODE_REDO)){
        // routine di aggiunta del frame
        Frame* addedFrame = setUpFrame(snapshot->frameNumber);
        PopulateLiveFrame(addedFrame,*snapshot,true,true);
        // spostati sulla destinazione
        moveToFrame(addedFrame);
    }
    else{
        myAnimation->currentFrame->clearFrame();
        sceneRemover(scene());
        PopulateLiveFrame(myAnimation->currentFrame,*snapshot,true,true);
        moveToFrame(myAnimation->currentFrame);
    }
}

/**
 * @details Captures the pre-edit snapshot and pushes it: `QUndoStack::push()` also
 * calls `redo()` once (a deliberate no-op here, see FrameUndoCommand) and — if the
 * stack's current index wasn't at the top (the user had undone something and then
 * made a fresh edit instead of redoing) — automatically discards the now-stale redo
 * history. `QUndoStack` does that as part of `push()`.
 */
void MyView::storeUndo(int command){
    if(myAnimation->frameList.isEmpty())
    {
        return;
    }
    updateFrameOrder(myAnimation->CurrentFrame());
    undo_stack.push(new FrameUndoCommand(this, command));
}
void MyView::updateFrameOrder(Frame* f){
    int j = 0;
    for(StickData*s:myAnimation->CurrentFrame()->totalSticks)
    {
        s->order = j;
        j++;
    }
    myAnimation->CurrentFrame()->numOfItems = myAnimation->CurrentFrame()->totalSticks.length();
    /*
    int j = 0;
    QList<QGraphicsItem*> orderedList = f->scene->items(f->scene->itemsBoundingRect(),Qt::IntersectsItemBoundingRect,Qt::DescendingOrder,QTransform());
    for(QGraphicsItem* item:orderedList){
        if(item->type() != STKTYPE){
            orderedList.removeAll(item);
        }
    }
    for(QGraphicsItem* item:orderedList){
        if(item->type() == STKTYPE){
            static_cast<StickGraphicsItem*>(item)->order = orderedList.indexOf(item);
            j++;
        }
    }
    f->numOfItems = j;
    */
}

void MyView::addHandles(StickFigure *S)
{
    for(StickData*s:S->stickList){
        if(!s->view) continue;
        scene()->addItem(s->view->stickHandle[0]);
        scene()->addItem(s->view->stickHandle[1]);
        if(s == S->masterStick){
            s->view->stickHandle[0]->setBrush(QBrush(Qt::green));
            s->view->stickHandle[0]->setZValue(101);
        }
    }
}

void MyView::removeHandles(StickFigure *S)
{

    for(StickData*s:S->stickList)
    {
        if(!s->view) continue;
        scene()->removeItem(s->view->stickHandle[0]);
        scene()->removeItem(s->view->stickHandle[1]);
    }
}
// cancella la storia di undo/redo, da chiamare quando si carica un nuovo file
/**
 * @details QUndoStack::clear() deletes every remaining FrameUndoCommand (each
 * frees its own before_/after_ FrameData in its destructor) — the old manual
 * per-entry delete loop and the clearUndoFlag choreography around it are both gone.
 */
void MyView::clearUndo(){
    undo_stack.clear();
}
/**
 * @details Renders the 50x50 preview icon of a library entry from its data record:
 * temporary stick view objects are built from the StickData, rendered on a throwaway
 * scene, then freed by that scene's destructor. This mirrors StickFigure::updateIcon()
 * for figures that no longer have a live view (the library holds pure data now).
 */
static QIcon RenderLibraryIcon(const StickFigureData &figure_data){
    QGraphicsScene icon_scene;
    QPixmap icon_pixmap(50,50);
    icon_pixmap.fill(Qt::white);
    icon_scene.setSceneRect(my_rect->rect());
    icon_scene.setBackgroundBrush(QBrush(QColor(Qt::white)));
    for(StickData *stick_data : figure_data.stickList){
        // la libreria non è mai "viva": stick_data non ha e non avrà mai un'altra view,
        // quindi wrapparlo direttamente (senza clonarlo) non ruba il back-pointer a nessuno
        StickGraphicsItem *render_view = new StickGraphicsItem(stick_data);
        render_view->setZValue(stick_data->Z);
        render_view->br = render_view->updateBr(0);
        icon_scene.addItem(render_view); // la scena diventa proprietaria della view
    }
    QPainter painter(&icon_pixmap);
    icon_scene.render(&painter);
    return QIcon(icon_pixmap);
}
/**
 * @details Straight file write of the library data — the libFlag that used to select
 * the "library" behavior inside the shared operators is gone; FrameData's operator<<
 * has exactly one behavior.
 */
void MyView::saveLibrary(QString fileName){
    QFile file(fileName);
    if(!file.open(QIODevice::WriteOnly)){
        return;
    }
    QDataStream out(&file);
    out << *stickLibraryBuffer;
    file.close();
}
/**
 * @details The library buffer owns plain data: replacing it is delete + new, no
 * per-figure cleanup choreography (freeLibraryFrame is gone).
 */
void MyView::clearCurrentLib(){
    my_current_library_widget->clear();
    delete stickLibraryBuffer;
    stickLibraryBuffer = new FrameData();
}
/**
 * @details Reads the library file into the data buffer, then builds the list-widget
 * entries: each item stores its StickFigureData pointer and an icon rendered from the
 * data. mode!=0 keeps the widget untouched (legacy semantics, currently unused).
 */
void MyView::loadLibrary(QString fileName, int mode){
    QFile file(fileName);
    if(!file.open(QIODevice::ReadOnly)){
        return;
    }
    QDataStream in(&file);
    my_current_library_widget->clear();
    delete stickLibraryBuffer;
    stickLibraryBuffer = new FrameData();
    in >> *stickLibraryBuffer;
    file.close();
    if(mode)
        return;
    // aggiungi tutti gli stickfigure della libreria alla lista del widget
    for(StickFigureData *figure_data:stickLibraryBuffer->stickFigures){
        QListWidgetItem * addedItem = new QListWidgetItem();
        addedItem->setData(Qt::UserRole,QVariant::fromValue(figure_data));
        addedItem->setData(Qt::DisplayRole,figure_data->name);
        addedItem->setIcon(RenderLibraryIcon(*figure_data));
        my_current_library_widget->addItem(addedItem);
    }
}
/**
 * @details Extracts the current figure into a data record, appends it to the library
 * buffer and creates its widget entry (insert-after-current + select, as before).
 * @return the live source figure, or nullptr if it has no sticks.
 */
StickFigure* MyView::addStickFigureToLibrary(){
    if(myAnimation->CurrentFrame()->CurrentStickFigure()->stickList.count()>0)
    {
        StickFigureData *figure_data = ExtractStickFigureData(*myAnimation->CurrentFrame()->CurrentStickFigure());
        stickLibraryBuffer->stickFigures.append(figure_data);
        QListWidgetItem * addedItem = new QListWidgetItem();
        addedItem->setData(Qt::UserRole,QVariant::fromValue(figure_data));
        addedItem->setData(Qt::DisplayRole,figure_data->name);
        addedItem->setIcon(RenderLibraryIcon(*figure_data));
        int newIdx = my_current_library_widget->currentRow()+1;
        my_current_library_widget->insertItem(newIdx,addedItem);
        my_current_library_widget->clearSelection();
        addedItem->setSelected(true);
        my_current_library_widget->setCurrentRow(newIdx);
        return myAnimation->CurrentFrame()->CurrentStickFigure();
    }else
    {
        return nullptr;
    }

}
/**
 * @details Removes the selected entry from both the widget and the data buffer; the
 * record is freed with a plain delete (it owns its sticks, nothing else to detach).
 */
void MyView::removeFromLibrary(){
    // get selected item
    int idx = my_current_library_widget->currentRow();
    if(idx<0 || idx>=my_current_library_widget->count()) // niente selezionato: senza questo, takeItem(-1) torna nullptr e va in crash sotto
        return;
    QListWidgetItem *itemToRemove = my_current_library_widget->takeItem(idx);
    StickFigureData* toRemove = qvariant_cast<StickFigureData*>(itemToRemove->data(Qt::UserRole));
    delete itemToRemove;
    stickLibraryBuffer->stickFigures.removeAll(toRemove);
    delete toRemove;
}
void MyView::setGraphics(bool all, int attribute){
    storeUndo(CMD_SIMPLE);
    if(!all){
        if(attribute == ATTRIBUTE_PENCOLOR)
        {
            myAnimation->CurrentFrame()->CurrentStick()->Pen.setColor(myPen.color());
        }
        else if(attribute== ATTRIBUTE_PENWIDTH)
        {
            myAnimation->CurrentFrame()->CurrentStick()->Pen.setWidth(myPen.width());
        }
        else if(attribute== ATTRIBUTE_BRUSHCOLOR)
        {
            myAnimation->CurrentFrame()->CurrentStick()->Brush.setColor(myBrush.color());
        }
        if(myAnimation->CurrentFrame()->CurrentStick()->view)
            myAnimation->CurrentFrame()->CurrentStick()->view->refresh(0);
    }
    else{
        for(StickData * s:myAnimation->CurrentFrame()->CurrentStickFigure()->stickList){
            if(attribute == ATTRIBUTE_PENCOLOR)
                s->Pen.setColor(myPen.color());
            else if(attribute== ATTRIBUTE_PENWIDTH)
                s->Pen.setWidth(myPen.width());
            else if(attribute== ATTRIBUTE_BRUSHCOLOR)
                s->Brush.setColor(myBrush.color());
            if(s->view)
                s->view->refresh(0);
        }
    }

    myAnimation->CurrentFrame()->CurrentStickFigure()->updateIcon();
}
/**
 * @brief MyView::splitStickFigure
 */
void MyView::splitStickFigure(){
    if(myAnimation->CurrentFrame()->CurrentStickFigure()== nullptr || myAnimation->CurrentFrame()->CurrentStick() == nullptr)
    {
        QMessageBox::about(this,"error","No stick item selected");
        return;
    }
    if(myAnimation->CurrentFrame()->CurrentStick() == myAnimation->CurrentFrame()->CurrentStickFigure()->masterStick)
    {
        QMessageBox::about(this,"error","cannot split a stickfigure from his master node");
        return;
    }
    storeUndo();
    splitToStickFigure = myAnimation->CurrentFrame()->CurrentStickFigure();
    removeHandles(splitToStickFigure);
    splitToStick = splitToStickFigure->currentStick;
    QList<StickData*> oldStickList = myAnimation->CurrentFrame()->CurrentStickFigure()->stickList;

    StickFigure * branch = myAnimation->currentFrame->addStickFigure(my_stick_figure_widget_list, splitToStickFigure->name+"_split");
    splitStickFigures(splitToStickFigure,splitToStick,branch);
    splitToStickFigure->refresh(0);
    // update the total list
    QList<StickData*> survivorList = splitToStickFigure->stickList;
    for(StickData*s:oldStickList){
        if(!survivorList.contains(s)){
            myAnimation->CurrentFrame()->totalSticks.removeAll(s);
        }
    }
    myAnimation->CurrentFrame()->totalSticks.append(branch->stickList);
    addHandles(splitToStickFigure);
    addHandles(branch);
    //
    branch->refresh(0);
    myAnimation->CurrentFrame()->CurrentStickFigure() = branch;
    myAnimation->CurrentFrame()->CurrentStick() = branch->stickList[0];
    myAnimation->CurrentFrame()->CurrentStickFigure()->updateIcon();

}
void MyView::sizeChange(int option){

    QRectF oldRect = my_rect->rect();
    QPointF oldCenter = my_rect->rect().center();
    float amount =  W/my_rect->rect().width();
    my_rect->setPen(Qt::NoPen);
    my_rect->setBrush(QBrush(QColor(Qt::white)));
    scene()->setBackgroundBrush(QBrush(QColor(Qt::white)));
    my_rect->setRect(0,0,W,H);
    onion_offset = QPointF(-my_rect->rect().width()/10,-my_rect->rect().height()/10);
    QPointF newCenter = my_rect->rect().center();
    limit_rect->setRect(-my_rect->rect().width()/10, -my_rect->rect().height()/10, my_rect->rect().width()*1.1, my_rect->rect().height()*1.1);
    scene()->setSceneRect(limit_rect->rect());
    float dx = newCenter.x()-oldCenter.x();
    float dy = newCenter.y()-oldCenter.y();
    switch(option)
    {

        case REPOSCALE:
        {
            for(Frame*F:myAnimation->frameList){
                scaleFrame(F,amount,oldRect);
                for(StickFigure* S: F->stickFigures){
                    S->traslate(dx,dy);
                }

            }
            break;
        }
        case REPO:
        {
            for(Frame*F:myAnimation->frameList){
                for(StickFigure* S: F->stickFigures){
                    S->traslate(dx,dy);
                }
            }
            break;
        }
        case NOREPO:
            break;
    }

    //()->addItem(my_rect);
   // scene()->update(0,0,1000,1000);
}
void MyView::preparePreview(){

    playBack = true;
    for(Frame* f :myAnimation->frameList){
        QGraphicsScene renderScene; //scena fittizia
        QImage *renderImg = new QImage(W,H,QImage::Format_ARGB32); //immagine temporanea


        QPainter painter(renderImg); // painter che esegue il drawing sull immagine
        renderScene.setSceneRect(my_rect->rect());
        // questi stick sono VIVI: clona i dati prima di wrapparli, altrimenti il costruttore
        // di stick ruberebbe il back-pointer data->view alla view reale sullo schermo
        QList<StickData*> renderClones;
        for(StickFigure *S:f->stickFigures)
        {
            for(StickData* s: S->stickList){
                StickData* clone = s->Clone();
                renderClones.append(clone);
                renderScene.addItem(new StickGraphicsItem(clone));
            }
        }
        //disegna sull' immagine e salvala
        renderImg->fill(Qt::white);
        painter.setBackground(QBrush(QColor(Qt::white)));
        renderScene.render(&painter);
        painter.end();
        QGraphicsPixmapItem* item = new QGraphicsPixmapItem(QPixmap::fromImage(*(renderImg)));
        previewList.append(item);
        delete renderImg;
        renderScene.clear(); // distruggi le view di rendering PRIMA di liberare i dati clonati
        qDeleteAll(renderClones);
        //...add all other video frames here
    }
    playBack = false;
}
void MyView::displayPreview(int idx){
    // rimuovi il pixmap del frame precedente: altrimenti si accumula uno per frame per tutta
    // la riproduzione, invece di essere ripulito solo (tutto insieme) quando la riproduzione finisce
    if(idx>0)
        scene()->removeItem(previewList[idx-1]);
    QGraphicsPixmapItem* renderImg = previewList[idx];

    //item->setOffset(onion_offset);
    scene()->addItem(renderImg);
    //item->setZValue(0);
}
