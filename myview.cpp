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
    my_animation = new Animation();
    my_animation->scene = scene();
    setMouseTracking(true);
    my_pen.setWidth(10);
    my_brush.setStyle(Qt::SolidPattern);
    my_brush.setColor(Qt::gray);
    my_pen.setCapStyle(Qt::RoundCap);
    stickLibraryBuffer = new FrameData();
    undo_stack.setUndoLimit(MAXUNDO);
    // autosave triggering moved here from StoreUndo() (Phase 4): indexChanged fires
    // on every push/undo/redo, which is a more correct autosave trigger than the old
    // "only on a fresh edit" behavior (undo/redo are edits too, worth autosaving)
    connect(&undo_stack, &QUndoStack::indexChanged, this, [this](int){
        opCount++;
        if(QFile::exists(animation_path) && opCount % autoSaveInterval == 0){
            emit AutoSaveTrigger();
        }
    });
}

MyView::~MyView()
{
    for (StickFigure* S : my_animation->CurrentFrame()->stick_figures)
    {
        RemoveHandles(S);
    }
    for ( Frame* f : my_animation->frame_list)
    {
        delete f;
    }
    delete my_animation;
    delete copy_stick_figure_buffer;
}

void MyView::SetTool(int new_tool)
{
    ChangeTool();

    tool = new_tool;
}

void MyView::DeleteStickFigure()
{
    bool updateCurrentStickFigure =  false;
    if(my_stick_figure_widget_list->selectedItems().count() == 1 && my_animation->current_frame->stick_figures.count()>1)
    {
        //scegli lo stickfigure selzionato dalla lista grafica
        QListWidgetItem *current = my_stick_figure_widget_list->selectedItems()[0];
        // estrai il dato utente salvato in esso, puntatore ad uno stickfigure
        QVariant  retrievedData = (current->data(Qt::UserRole));
        //riconverti il dato
        StickFigure* cs = qvariant_cast<StickFigure*>(retrievedData);
        updateCurrentStickFigure = (cs == my_animation->CurrentFrame()->CurrentStickFigure());

        //distruggi l'oggetto della lista
        my_stick_figure_widget_list->takeItem(my_stick_figure_widget_list->row(current));
        delete current;
        // delete from the total list
        for(StickData*s:cs->stick_list){
            my_animation->CurrentFrame()->total_sticks.removeAll(s);
        }
        //rimuovilo dal frame e distruggilo
        StickFigure* ns = my_animation->current_frame->RemoveStickFigure(cs);
        // Ora controlliamo la nuova selezione non sia nulla e aggiorniamo la selezione
        // dell'elenco del widget
        if(ns!=NULL && ns!=nullptr)
        {
            my_animation->UpdateSelection(ns);
            my_stick_figure_widget_list->clearSelection();
            ns->linked_item->setSelected(true);
            // senza questo, currentRow() poteva restare sulla riga che Qt seleziona di default dopo
            // takeItem() invece che su quella di ns: un successivo Move Layer Up/Down avrebbe quindi
            // spostato una riga diversa da quella su cui viene applicata la Z (my_animation->CurrentFrame()->CurrentStickFigure())
            my_stick_figure_widget_list->setCurrentRow(my_stick_figure_widget_list->row(ns->linked_item));
        }
        my_animation->current_frame->UpdateIcon();
        if(updateCurrentStickFigure){
             ArrowSelection();
        }
    }
    else{
        QMessageBox::about(this,"error","There must be at least one stickfigure layer in the scene");
    }

}
//cambia l'ordine di profondità dello stickfigure selezionato
void MyView::MoveStickFigureZ(int increment, int mode)
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

            my_animation->CurrentFrame()->StickfigureToTop(my_animation->CurrentFrame()->CurrentStickFigure());
        }
        else if(mode == MODE_BOTTOM){
            my_animation->CurrentFrame()->StickfigureToBottom(my_animation->CurrentFrame()->CurrentStickFigure());

        }
        else if(increment>0){

            my_animation->CurrentFrame()->StickfigureAboveBelow(my_animation->CurrentFrame()->CurrentStickFigure(),stickFigureAtNewIdx,1);
        }
        else{
            my_animation->CurrentFrame()->StickfigureAboveBelow(my_animation->CurrentFrame()->CurrentStickFigure(),stickFigureAtNewIdx,0);

        }
        //sposta nella lista grafica il layer dello stickfigure
        QListWidgetItem * item = my_stick_figure_widget_list->takeItem(currentIndex);
        my_stick_figure_widget_list->insertItem(newIdx,item);
        item->setSelected(true);
        my_stick_figure_widget_list->setCurrentRow(newIdx);

        my_animation->current_frame->UpdateIcon();
        my_animation->CurrentFrame()->Refresh(0);
    }

/*
    if(my_stick_figure_widget_list->count()>0)
    {
        my_stick_figure_widget_list->clearSelection();
        int currentIndex = my_stick_figure_widget_list->currentRow();
        if(!(currentIndex+1 == my_stick_figure_widget_list->count() && increment>0)||
                !(currentIndex == 0 && increment<0))
        {
            my_animation->CurrentFrame()->CurrentStickFigure()->base_z+=increment;
            my_animation->CurrentFrame()->CurrentStickFigure()->UpdateZ(0.1);
            if(!(increment>0 && currentIndex == my_stick_figure_widget_list->count()-1) && !(increment<0 && currentIndex == 0))
            {
                //sposta nella lista grafica il layer dello stickfigure
                QListWidgetItem * item = my_stick_figure_widget_list->takeItem(currentIndex);
                my_stick_figure_widget_list->insertItem(currentIndex+increment,item);
                my_stick_figure_widget_list->setSelected(item,true);
                my_stick_figure_widget_list->setCurrentRow(currentIndex+increment);
            }
            my_animation->current_frame->UpdateIcon();
        }
    }
*/
}
void MyView::MapMyCoords(QPointF& myCoord, QPointF startingPoint){
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
    MapMyCoords(coord,event->pos());


    startingCoord   = coord; //buffer per il dragghing del mouse premuto
    if(!isRightPressed && !isMiddlePressed){
    switch(tool)
    {
            case CLAMP:
            case NOTOOL:
            {
                StoreUndo();
                ArrowSelection();
                startStickFigureRotation = true;
                for(StickData*s:my_animation->CurrentFrame()->CurrentStickFigure()->stick_list){
                    if(s->view) s->view->angle_buffer2 = s->my_line.angle();
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
                DrawCmd(&coord,(int)tool);

                num = currentScene->items();
                break;
            }
            case(ROTATE):
            {
                StoreUndo();
                startStickFigureRotation = true;
                for(StickData*s:my_animation->CurrentFrame()->CurrentStickFigure()->stick_list){
                    if(s->view) s->view->angle_buffer2 = s->my_line.angle();
                }
                break;
            }
            case(SCALE):
            {
                StoreUndo();
                start_stick_figure_scale = true;
                for(StickData*s:my_animation->CurrentFrame()->CurrentStickFigure()->stick_list){
                    if(s->view){
                        s->view->scale_buffer = s->my_line.length();
                        s->view->width_buffer = s->pen.width();
                    }
                }
                break;
            }
            case(MOVESCENE):
            {
                StoreUndo();
                break;
            }
            case(ROTATESCENE):
            {
                StoreUndo();
                for(StickFigure* S: my_animation->current_frame->stick_figures){
                    //check it is not an empty layer
                    if(S->master_stick && S->stick_list.count()>0){
                        S->origin_pos_buffer = S->master_stick->my_line.p1();
                        S->scene_rotation_buffer = 0;
                        for(StickData*s:S->stick_list){
                            if(s->view) s->view->angle_buffer2 = s->my_line.angle();
                        }
                    }
                }
                break;
            }
            case(SCALESCENE):
            {
                for(StickFigure* S: my_animation->current_frame->stick_figures){
                    //check it is not an empty layer
                    if(S->master_stick && S->stick_list.count()>0){
                        S->origin_pos_buffer = S->master_stick->my_line.p1();
                        for(StickData*s:S->stick_list){
                            if(s->view){
                                s->view->scale_buffer = s->my_line.length();
                                s->view->width_buffer = s->pen.width();
                            }
                        }
                    }
                }
                StoreUndo();
                break;
            }
            case(JOIN):{
                if(my_animation->CurrentFrame()->CurrentStickFigure()!= nullptr){
                    //check layer is not empty
                    if(my_animation->CurrentFrame()->CurrentStickFigure()->stick_list.count()>0){
                        joinToStickFigure = my_animation->CurrentFrame()->CurrentStickFigure();
                        joinToStick = joinToStickFigure->current_stick;
                        StoreUndo();
                        ArrowSelection();

                        if(my_animation->CurrentFrame()->CurrentStickFigure()!=joinToStickFigure && my_animation->CurrentFrame()->CurrentStickFigure()->stick_list.count()>0){
                            RemoveHandles(joinToStickFigure);
                            MergeStickFigures(joinToStickFigure,joinToStick,my_animation->CurrentFrame()->CurrentStickFigure());
                            AddHandles(joinToStickFigure);
                            DeleteStickFigure();
                            for(StickData*s:my_animation->CurrentFrame()->CurrentStickFigure()->stick_list){
                                if(!my_animation->CurrentFrame()->total_sticks.contains(s))
                                    my_animation->CurrentFrame()->total_sticks.append(s);
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
                StoreUndo();
                ArrowSelection();
                if(my_animation->CurrentFrame()->CurrentStick()==nullptr) break; // niente da selezionare (nessuno stick nel frame)
                lengthBuffer = my_animation->CurrentFrame()->CurrentStick()->my_line.length();
                break;
            }
            case(MANIPULATE):
            {
                StoreUndo();
                ArrowSelection();
                if(my_animation->CurrentFrame()->CurrentStick()==nullptr) break; // niente da selezionare (nessuno stick nel frame)
                lengthBuffer = my_animation->CurrentFrame()->CurrentStick()->my_line.length();

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
            if(scene()->selectedItems().count() == 1 && my_animation->CurrentFrame()->CurrentStickFigure()->draw_count==0)
            {
                StoreUndo();

                StickGraphicsItem *item = (StickGraphicsItem*)scene()->selectedItems()[0];//oggetto selezionato nella scnea
                StickData *item_data = item->data;
                int idx = my_animation->CurrentFrame()->CurrentStickFigure()->stick_list.indexOf(item_data);        // indice corrispondente nella lista ordinata degli oggetti
                // se si tratta dello stick master e ci sono altri stickfigure, cancella l'intero stickfigure
                if(my_animation->CurrentFrame()->CurrentStickFigure()->stick_list[idx] == my_animation->CurrentFrame()->CurrentStickFigure()->master_stick &&
                        my_animation->current_frame->stick_figures.count()>1)
                {
                    //estrai dalla lista grafica l'elemento selezionato collegato allo stickfigure
                    QListWidgetItem * associatedItem = my_animation->CurrentFrame()->CurrentStickFigure()->linked_item;
                    my_stick_figure_widget_list->clearSelection();
                    associatedItem->setSelected(true);
                    //cancella lo stickfigure
                    DeleteStickFigure();
                }
                //altrimenti cancella il singolo stick e aggiorna l'iconea del livello
                else if(my_stick_figure_widget_list->selectedItems().count()==1)
                {
                    StickData* nextCurrentStick = my_animation->CurrentFrame()->CurrentStick()->parent;
                    // questa lista serve per rimuovere dalla totalStick tutti gli stick cancellati
                    QList<StickData*> oldStickList = my_animation->CurrentFrame()->CurrentStickFigure()->stick_list;
                    my_animation->CurrentFrame()->CurrentStickFigure()->DeleteStick( idx);
                    QList<StickData*> survivors = my_animation->CurrentFrame()->CurrentStickFigure()->stick_list;
                    for(StickData*s:oldStickList){
                        if(!survivors.contains(s)){
                            my_animation->CurrentFrame()->total_sticks.removeAll(s);
                        }
                    }
                    my_animation->CurrentFrame()->num_of_items = my_animation->CurrentFrame()->total_sticks.length();
                    my_animation->CurrentFrame()->CurrentStickFigure()->UpdateIcon();
                    my_stick_figure_widget_list->selectedItems()[0]->setIcon(*my_animation->CurrentFrame()->CurrentStickFigure()->stick_figure_icon);
                    my_animation->CurrentFrame()->CurrentStick() = nextCurrentStick;
                }
                my_animation->current_frame->UpdateIcon();
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
                    if(my_animation->CurrentFrame()->CurrentStickFigure()->draw_count==1)
                    {
                        my_animation->CurrentFrame()->CurrentStickFigure()->CancelDrawing();
                    }
                    break;
                }
            }
            break;
        }
    }

    QGraphicsView::keyPressEvent(event);
}
void MyView::ArrowSelection(){
    if(my_animation->CurrentFrame()->stick_figures.isEmpty())
        return;
    if(my_animation->CurrentFrame()->total_sticks.empty())
        return;
    my_animation->UpdateSelection(coord);
    my_stick_figure_widget_list->clearSelection();
    my_animation->CurrentFrame()->CurrentStickFigure()->linked_item->setSelected(true);
    my_stick_figure_widget_list->setCurrentRow(my_stick_figure_widget_list->row(my_animation->CurrentFrame()->CurrentStickFigure()->linked_item));
    if(my_animation->CurrentFrame()->CurrentStick()!=nullptr && my_animation->CurrentFrame()->CurrentStickFigure()->stick_list.count()>0
            && my_animation->CurrentFrame()->CurrentStick()->view)
        my_animation->CurrentFrame()->CurrentStick()->view->PopulateImageListWidget();
    //dbg_lbl->setText(QString::number(my_animation->CurrentFrame()->CurrentStick()->order));
}
// il disegno di una linea conta 2 click
// il primo determina lo starting point
// il secondo l'end point
void MyView::DrawCmd(QPointF* point, int mode)
{
    if(!my_animation->current_frame->stick_figures.isEmpty() && my_animation->CurrentFrame()->CurrentStickFigure() != nullptr)
    {

        if(my_animation->CurrentFrame()->CurrentStickFigure()->draw_count == 0){
            StoreUndo();

            my_animation->CurrentFrame()->CurrentStickFigure()->StartDrawing(point, my_pen, my_brush);
            StickData *new_stick = my_animation->CurrentFrame()->CurrentStick();
            // il costruttore di stick imposta già new_stick->view: la scena vera e propria
            // riceve l'elemento dentro AttachView(), senza una addItem() separata
            FramePresenter::AttachView(new_stick, scene());
            if(!my_animation->CurrentFrame()->CurrentStickFigure()->stick_list.isEmpty()){
               new_stick->view->selected = false;
               new_stick->view->Refresh(0);
            }
            new_stick->view->selected = true;
            new_stick->hard_top = hard_top_check->isChecked();
            new_stick->hard_bottom = hard_bottom_check->isChecked();
            new_stick->pen = my_pen;
            // se stiamo usando il tool per generare cerchi cambiamo il tipo dello stick
            if(mode == DRAWCIRCLE){
                new_stick->stick_type = StickData::CIRCLE;
            }
            else if(mode == DRAWRECT){
                new_stick->stick_type = StickData::RECT;
                new_stick->img_w_scale = advanced_rect_slider->value();
                new_stick->pen.setJoinStyle(Qt::MiterJoin);
            }
            else if(mode == DRAWTAPER){
                new_stick->stick_type = StickData::TAPER;
                new_stick->img_h_scale = taper_h_slider->value();
                new_stick->img_w_scale = taper_w_slider->value();
            }
            else if(mode == DRAWTRAPEZOID){
                new_stick->stick_type = StickData::TRAPEZOID;
                new_stick->img_h_scale = taper_h_slider->value();
                new_stick->img_w_scale = taper_w_slider->value();
                new_stick->pen.setJoinStyle(Qt::MiterJoin);
            }
            else if(mode == DRAWIMG){
                new_stick->stick_type = StickData::IMAGE;
                QImage* img = new QImage(*image_draw_buffer);

                new_stick->view->AddImage(img, image_name_buffer);

            }

           qDebug("Draw 1 = %f, %f",point->x(),point->y());
        }
        else if(my_animation->CurrentFrame()->CurrentStickFigure()->draw_count >0)
        {
            my_animation->CurrentFrame()->CurrentStickFigure()->EndDrawing(point);
            scene()->addItem(my_animation->CurrentFrame()->CurrentStick()->view->stick_handle[0]);
            scene()->addItem(my_animation->CurrentFrame()->CurrentStick()->view->stick_handle[1]);
            qDebug("Draw 2 = %f, %f",point->x(),point->y());
            my_animation->current_frame->UpdateIcon();
            my_animation->UpdateTab(my_animation->CurrentFrame()->CurrentStick()->stick_type);
            // add it to the total scene list
            my_animation->current_frame->total_sticks.append(my_animation->CurrentFrame()->CurrentStick());
            my_animation->UpdateSelection(my_animation->CurrentFrame()->CurrentStick());

        }
    }
    else
    {
        QMessageBox::about(this,"error","no stickfigure layer present, please add a stickfigure");
    }


}
//Esci dallo strumento che stai usando
void MyView::ChangeTool()
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
            my_animation->UpdateTab(tool); // updates ui panels
            if(!my_animation->current_frame->stick_figures.isEmpty())
            {
                // se stavi disegnando uno stick e cambi prima di averlo finito, cancellalo!
                if(my_animation->CurrentFrame()->CurrentStickFigure()->draw_count==1)
                {
                    scene()->removeItem((QGraphicsItem*)my_animation->CurrentFrame()->CurrentStick());
                    my_animation->CurrentFrame()->CurrentStickFigure()->CancelDrawing();
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
    if(my_animation->CurrentFrame()->CurrentStickFigure() !=nullptr){
        if(!my_animation->CurrentFrame()->CurrentStickFigure()->stick_list.isEmpty() && my_animation->CurrentFrame()->CurrentStickFigure()->current_stick!=nullptr){
            int idx = my_animation->CurrentFrame()->total_sticks.indexOf(my_animation->CurrentFrame()->CurrentStickFigure()->current_stick);
            if(event->angleDelta().y()>0){
                for(int j = idx+1;j<my_animation->CurrentFrame()->total_sticks.length();j++){
                    if(my_animation->CurrentFrame()->CurrentStickFigure()->stick_list.contains(my_animation->CurrentFrame()->total_sticks[j])){
                        my_animation->CurrentFrame()->CurrentStickFigure()->Highlight(false);

                        my_animation->CurrentFrame()->SelectStick(my_animation->CurrentFrame()->CurrentStickFigure(),my_animation->CurrentFrame()->total_sticks[j]);
                        break;
                    }
                }
            }
            else{
                for(int j = idx-1;j>-1;j--){
                    if(my_animation->CurrentFrame()->CurrentStickFigure()->stick_list.contains(my_animation->CurrentFrame()->total_sticks[j])){
                        my_animation->CurrentFrame()->CurrentStickFigure()->Highlight(false);
                        my_animation->CurrentFrame()->SelectStick(my_animation->CurrentFrame()->CurrentStickFigure(),my_animation->CurrentFrame()->total_sticks[j]);
                        break;
                    }
                }
            }
            my_animation->CurrentFrame()->CurrentStickFigure()->Refresh(0);
        }
    }
}



// mouse rilasciato
void MyView::mouseReleaseEvent(QMouseEvent *event)
{
    if(event->button()==Qt::LeftButton){
        isPressed = false;
        if(my_animation->current_frame->stick_figures.count()>0)
        {
            if(my_animation->CurrentFrame()->CurrentStickFigure()->stick_list.count()>0
                     && my_stick_figure_widget_list->selectedItems().count() == 1)
            {
                //termina una rotazione di stick al rilascio
                if(my_animation->CurrentFrame()->CurrentStick()->view)
                    my_animation->CurrentFrame()->CurrentStick()->view->EndRotation();
                my_animation->CurrentFrame()->CurrentStickFigure()->UpdateIcon();
                my_stick_figure_widget_list->selectedItems()[0]->setIcon(*my_animation->CurrentFrame()->CurrentStickFigure()->stick_figure_icon);
                my_animation->current_frame->UpdateIcon();
            }
            if(tool == MOVESCENE || tool == ROTATESCENE || tool == SCALESCENE){
                // questi strumenti modificano ogni stickfigure del frame: aggiorna l'icona di ognuno,
                // non solo di quello selezionato, altrimenti le altre anteprime restano quelle pre-trascinamento
                for(StickFigure* S: my_animation->current_frame->stick_figures){
                    S->UpdateIcon();
                    S->linked_item->setIcon(*S->stick_figure_icon);
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
    MapMyCoords(coord, event->pos());
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
                    my_animation->UpdateSelection(item->data);

                    //qDebug("selected item %d", idx);
                    // Ora ruota lo stick
                    if(my_animation->CurrentFrame()->CurrentStickFigure()->stick_list.count()>0){
                        if(tool == CLAMP){
                            if(my_animation->CurrentFrame()->CurrentStick()->view)
                                my_animation->CurrentFrame()->CurrentStick()->view->Rotate(&coord);
                        }
                        else{
                            QList<StickGraphicsItem*> toRotate;
                            if(my_animation->CurrentFrame()->CurrentStick() == my_animation->CurrentFrame()->CurrentStickFigure()->master_stick){
                                for(StickData*s:my_animation->CurrentFrame()->CurrentStickFigure()->stick_list){
                                    if(!s->stepchild && s->view){
                                        toRotate.append(s->view);
                                    }
                                }
                            }else{
                                for(StickData* s:my_animation->CurrentFrame()->CurrentStick()->children)
                                    if(s->view) toRotate.append(s->view);
                            }
                            if(my_animation->CurrentFrame()->CurrentStick()->view)
                                toRotate.append(my_animation->CurrentFrame()->CurrentStick()->view);
                            my_animation->CurrentFrame()->CurrentStickFigure()->RotateStickFigure(&coord,toRotate,false);
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
            if(my_animation->CurrentFrame()->CurrentStickFigure() != nullptr && my_animation->CurrentFrame()->CurrentStickFigure() != nullptr)
            {
                if(my_animation->CurrentFrame()->CurrentStickFigure()->draw_count > 0)
                {
                    my_animation->CurrentFrame()->CurrentStickFigure()->PreviewDrawing(&coord);
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
            if(my_animation->CurrentFrame()->CurrentStickFigure() != nullptr && my_animation->CurrentFrame()->CurrentStick()!=nullptr)
            {
                if(isPressed){
                    if(my_animation->CurrentFrame()->CurrentStick()->view)
                        my_animation->CurrentFrame()->CurrentStick()->view->Manipulate(&coord);
                    elongate_spinbox->setValue(my_animation->CurrentFrame()->CurrentStick()->my_line.length());
                }
            }
            break;
        }
        case ROTATE:
        {
            if(my_animation->CurrentFrame()->CurrentStickFigure() != nullptr)
            {
                if(isPressed){
                    QList<StickGraphicsItem*> views;
                    for(StickData* s: my_animation->CurrentFrame()->CurrentStickFigure()->stick_list){
                        if(s->view) views.append(s->view);
                    }
                    my_animation->CurrentFrame()->CurrentStickFigure()->RotateStickFigure(&coord,views,true);
                }
            }
            break;
        }
        case(SCALE):
        {
            if(my_animation->CurrentFrame()->CurrentStickFigure() != nullptr && isPressed){
               my_animation->CurrentFrame()->CurrentStickFigure()->Scale(&coord);
            }
            break;
        }
        case(MOVESCENE):
        {
            if(isPressed && !my_animation->current_frame->stick_figures.isEmpty()){
                int dx = -startingCoord.x()+coord.x();
                int dy = -startingCoord.y()+coord.y();
                startingCoord = coord;
                for(StickFigure*S:my_animation->current_frame->stick_figures){
                    //check it is not an empty layer
                    if( S->stick_list.count()>0 && S->master_stick){
                        S->Traslate(dx,dy);
                    }
                }
            }
            break;
        }
        case(ROTATESCENE):
        {
            if(isPressed && !my_animation->current_frame->stick_figures.isEmpty()){
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
                for(StickFigure*S:my_animation->current_frame->stick_figures){
                    //check it is not an empty layer
                    if( S->stick_list.count()>0 && S->master_stick){
                        QLineF connectCenter(center,S->master_stick->my_line.p1()); // linea che connette il master dello stick al centro

                        connectCenter.setAngle(connectCenter.angle()+angle); // ruotiamola dell'angolo di incremento
                        // ruota ogni singolo stick
                        for(StickData *s:S->stick_list){

                            if(s->view){
                                s->view->Rotate(angle);
                                s->view->angle_buffer2+=angle; // aggiorna il buffer interno dell'angolo dello stick
                            }
                        }

                        QLineF Delta(S->origin_pos_buffer,connectCenter.p2()); // traslazione della punta della lineacentro-master ruotata
                        //trasla lo stickfigure dell'ammontare
                        S->Traslate(Delta.dx(),Delta.dy());
                        S->origin_pos_buffer = S->master_stick->my_line.p1(); // agiorna il buffer di posizione dello stickfigure
                    }
                    }
            }
            break;
        }
        case(SCALESCENE):
        {


            if(isPressed && !my_animation->current_frame->stick_figures.isEmpty()){
                float scaleAmount = 1;
                float dist = 0;
                QPointF center(W/2,H/2);
                float startDist = QLineF(startingCoord,center).length();
                if(startDist<1) // evita una divisione per zero (o quasi) se si preme esattamente sul centro
                    break;
                dist = QLineF(coord,center).length();
                scaleAmount = dist/startDist;
                for(StickFigure * S:my_animation->current_frame->stick_figures){
                    //check it is not an empty layer
                        if( S->stick_list.count()>0 && S->master_stick){
                        // per traslare lo stick verso il centro [W/2,H/2] proporzionalmente
                        // serve calcolare la distanza del masterstick da essa
                        // moltiplicare dx e dy per lo scale factor
                        // usarli come traslazione
                        float dx = 0;
                        float dy = 0;
                        // il dx e dy deve essere una quantità tale da non far eccedere ad actual dist ilv alore  di originaldist
                        QLineF actualDistFromCenter(S->master_stick->my_line.p1(),center);
                        QLineF originalDistFromCenter(S->origin_pos_buffer,center);
                        // quindi fai la differenza fra il valore di distaza originale (max) e la distanza attuale modificata dal fattore di scala per dx e dy
                        dx = -originalDistFromCenter.dx()*scaleAmount+actualDistFromCenter.dx();
                        dy = -originalDistFromCenter.dy()*scaleAmount+actualDistFromCenter.dy();
                        S->Traslate(dx,dy);
                        for(StickData* s:S->stick_list){
                            if(s->view) s->view->Scale(scaleAmount);

                        }
                    }
                }
            }
            break;
        }
        case(ELONGATE):
        {
            if(my_animation->CurrentFrame()->CurrentStickFigure()==nullptr)
                break;
            if(my_animation->CurrentFrame()->CurrentStick() == nullptr)
                break;
            if(isPressed){
                QLineF newEndPoint(my_animation->CurrentFrame()->CurrentStick()->my_line.p1(),coord);
                lengthBuffer = newEndPoint.length();
                my_animation->CurrentFrame()->CurrentStickFigure()->Elongate(coord,my_animation->CurrentFrame()->CurrentStick());
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
void MyView::ScaleFrame(Frame* F, float scaleAmount, QRectF R){
    QPointF center(R.width()/2,R.height()/2);
    if(F->stick_figures.isEmpty())
        return;
    for(StickFigure * S:F->stick_figures){
        if(!S->stick_list.isEmpty()){
            QLineF actualDistFromCenter(S->master_stick->my_line.p1(),center);
            float dx =-(scaleAmount-1)*actualDistFromCenter.dx();
            float dy =-(scaleAmount-1)*actualDistFromCenter.dy();
            S->Traslate(dx,dy);
            for(StickData* s:S->stick_list){
                if(s->view){
                    s->view->scale_buffer = s->my_line.length();
                    s->view->width_buffer = s->pen.widthF();
                    s->view->Scale(scaleAmount);
                }
            }
        }

    }
}
Frame* MyView::SetUpFrame(int pos){
    //decidi il nome
    //int intName = 0;
    /*if(my_animation->frame_list.count()>=1){
        intName = my_animation->current_frame->frame_number+1;

    }*/
    return my_animation->SetupFrame(pos);
}
void MyView::DeleteFrame(Frame* frame){
    int frameNum = frame->frame_number;
    my_frame_widget_list->takeItem(my_frame_widget_list->row(frame->linked_item));
    my_animation->DeleteFrame(frame);
    my_frame_widget_list->clearSelection();
    for(Frame *f: my_animation->frame_list){
        if(f->frame_number>frameNum){
            f->frame_number--;
            f->linked_item->setData(Qt::DisplayRole, QString::number(f->frame_number));
        }
    }
    my_animation->current_frame->linked_item->setSelected(true);
}
// routine per passare da un frame ad un altro, svuota la scena, svuota le widget list, ripopola entrambi, aggiorna onion skins e selezioni
void MyView::MoveToFrame(Frame* frame){
    // rimuovi tutti gli stick dalla scena
    auto currentScene = scene();
    auto num = currentScene->items();
    if(my_animation->frame_list.count()>=1){
        for(StickFigure *S:my_animation->current_frame->stick_figures){
            RemoveHandles(S);
            for(StickData * s: S->stick_list){
               if(s->view && scene()->items().contains(s->view)){
                    currentScene->removeItem(s->view);

               }
            }


        }
    }
    num = currentScene->items();
    // rimuovigli elementi dalla lista widget
    my_stick_figure_widget_list->clear();

    // assicura che ogni stick del nuovo frame abbia una view PRIMA di AddHandles() qui sotto:
    // un frame appena caricato da file (attach_to_scene=false) non ne ha ancora una, un frame
    // già visitato in precedenza sì, basta ri-attaccarla alla scena
    for(int j = 0;j<frame->num_of_items;j++)
    {
        StickData *s = frame->total_sticks[j];
        if(s->view)
            currentScene->addItem(s->view);
        else
            FramePresenter::AttachView(s, currentScene);
    }

    // aggiungi tutta gli stickfigure del nuovo frame alla lista del widget
    for(StickFigure *S:frame->stick_figures)
    {
        QListWidgetItem * addedItem = new QListWidgetItem(my_stick_figure_widget_list);
        QVariant newData(QVariant::fromValue(S));
        addedItem->setData(Qt::UserRole,newData);
        addedItem->setData(Qt::DisplayRole,S->name);
        addedItem->setIcon(*S->stick_figure_icon);
        S->linked_item = addedItem;
        my_stick_figure_widget_list->addItem(S->linked_item);
        // aggiungi alla scena
        if(!playBack){
            S->UpdateBoundingRects();
            S->UpdateIcon();
            AddHandles(S);
        }
    }
    currentScene->clearSelection();
    my_animation->current_frame = frame;

    // aggiorna il current frame, se si parte d auna situazione di inizio forza il primo stickfigure
    if(frame->current_stick_figure !=nullptr){
        if(frame->current_stick_figure->current_stick!=nullptr){
            frame->SelectStick(frame->current_stick_figure,frame->current_stick_figure->current_stick);
        }
        else{
            ArrowSelection();
        }
    }
    else if(frame->stick_figures.count() == 1 && frame->stick_figures[0]->stick_list.isEmpty()){
        my_animation->CurrentFrame()->CurrentStickFigure() = frame->stick_figures[0];
        my_animation->UpdateSelection(my_animation->CurrentFrame()->CurrentStickFigure());
    }else{
        ArrowSelection();
    }

    if(!playBack)
    {
        UpdateOnionSkins();//aggiorna gli onion skins
    }
    num = currentScene->items();
    //scene()->update(myR);
    // se non siamo in modalità playback aggiorna le selezioni nelle widgetlists e delle iconee
    if(playBack == false){
        my_animation->current_frame->linked_item->setSelected(true);
        my_frame_widget_list->setCurrentItem(my_animation->current_frame->linked_item);
        // niente guardia su stick_figures.isEmpty(): UpdateIcon() gestisce già correttamente un
        // frame vuoto (lo riempie di bianco), senza di essa un frame appena creato restava senza
        // anteprima invece di mostrare una tela bianca
        if(!my_animation->frame_list.isEmpty()){
            my_animation->current_frame->UpdateIcon();
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
    my_animation->CurrentFrame()->Refresh(0);
}
// routine per calcolare gli onion skins, prima cancellali, ridisegnali su un immagine, e aggiungi l'immagine alla scena
void MyView::UpdateOnionSkins(){
    // svuota i buffer di immagine vecchi (presneti nella scena) immagazzinati nel ciclo for precedente
    for(QGraphicsPixmapItem* P:onion_skins){
        scene()->removeItem(P);
        delete P;
    }
    onion_skins.clear();
    if(onion_skin_sb->value()==0 || my_animation->current_frame->frame_number == 0)
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
    if(my_animation->current_frame->frame_number<skinNum)
    {
        skinNum = my_animation->current_frame->frame_number;
    }
    // disegna sulla scena fittizia gli stickfigures dei frame da onionskinnare che partono dal frame corrente- numero di skin calcolabili
    for(int i = my_animation->current_frame->frame_number-skinNum; i<my_animation->current_frame->frame_number; i++){
        // todo anzichè copiare stickfigure per stickfigure, copiare l'intera scena
        // questi stick sono VIVI (hanno già una view sul frame i): clonarne i dati prima di
        // wrapparli evita che il costruttore di stick rubi il back-pointer data->view alla
        // view reale (vedi StickFigure::UpdateIcon per lo stesso pattern)
        QList<StickData*> renderClones;
        for(StickData *s:my_animation->frame_list[i]->total_sticks)
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
        onion_skins.append(item);
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
 *   mutation runs) — this is what `StoreUndo()` used to push directly.
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
        before_ = ExtractFrameData(*view_->my_animation->current_frame);
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
            after_ = ExtractFrameData(*view_->my_animation->current_frame);
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
 * No flag needed here anymore: `Frame::ClearFrame()` and `~StickFigure()` each own
 * exactly the cleanup they perform, so nothing needs to skip work for the other.
 */
void MyView::RestoreFrameSnapshot(int command_tag, int mode, FrameData *snapshot)
{
    // se è un undo su un addframe, o un redo su un deleteframe ci sta da rimuovere un frame
    if((command_tag == CMD_ADDFRAME && mode == MODE_UNDO)||
            (command_tag == CMD_DELETEFRAME && mode == MODE_REDO)){
        // routine di cancellazione del frame
        DeleteFrame(my_animation->current_frame);
        // svuota il frame da popolare con il buffer e ripopolalo dallo snapshot,
        // attaccando davvero gli stick alla scena (il target è il frame VERO)
        my_animation->frame_list[snapshot->frame_number]->ClearFrame();
        PopulateLiveFrame(my_animation->frame_list[snapshot->frame_number],*snapshot,true,true);
        // spostati verso tale frame
        MoveToFrame(my_animation->frame_list[snapshot->frame_number]);
    }
    // se è un undo su uno spostamento
    else if(command_tag == CMD_MOVETOFRAME){
        //svuota il frame di destinazione e popolalo con lo snapshot
        my_animation->frame_list[snapshot->frame_number]->ClearFrame();
        PopulateLiveFrame(my_animation->frame_list[snapshot->frame_number],*snapshot,true,true);
        // spostati sul frame di destinazione
        MoveToFrame(my_animation->frame_list[snapshot->frame_number]);
    }
    // se è un undo su un deleteframe o un redo su un adddframe bisogna aggiungere un frame
    else if((command_tag == CMD_DELETEFRAME && mode == MODE_UNDO)||
            (command_tag == CMD_ADDFRAME && mode == MODE_REDO)){
        // routine di aggiunta del frame
        Frame* addedFrame = SetUpFrame(snapshot->frame_number);
        PopulateLiveFrame(addedFrame,*snapshot,true,true);
        // spostati sulla destinazione
        MoveToFrame(addedFrame);
    }
    else{
        my_animation->current_frame->ClearFrame();
        SceneRemover(scene());
        PopulateLiveFrame(my_animation->current_frame,*snapshot,true,true);
        MoveToFrame(my_animation->current_frame);
    }
}

/**
 * @details Captures the pre-edit snapshot and pushes it: `QUndoStack::push()` also
 * calls `redo()` once (a deliberate no-op here, see FrameUndoCommand) and — if the
 * stack's current index wasn't at the top (the user had undone something and then
 * made a fresh edit instead of redoing) — automatically discards the now-stale redo
 * history. `QUndoStack` does that as part of `push()`.
 */
void MyView::StoreUndo(int command){
    if(my_animation->frame_list.isEmpty())
    {
        return;
    }
    UpdateFrameOrder(my_animation->CurrentFrame());
    undo_stack.push(new FrameUndoCommand(this, command));
}
void MyView::UpdateFrameOrder(Frame* f){
    int j = 0;
    for(StickData*s:my_animation->CurrentFrame()->total_sticks)
    {
        s->order = j;
        j++;
    }
    my_animation->CurrentFrame()->num_of_items = my_animation->CurrentFrame()->total_sticks.length();
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
    f->num_of_items = j;
    */
}

void MyView::AddHandles(StickFigure *S)
{
    for(StickData*s:S->stick_list){
        if(!s->view) continue;
        scene()->addItem(s->view->stick_handle[0]);
        scene()->addItem(s->view->stick_handle[1]);
        if(s == S->master_stick){
            s->view->stick_handle[0]->setBrush(QBrush(Qt::green));
            s->view->stick_handle[0]->setZValue(101);
        }
    }
}

void MyView::RemoveHandles(StickFigure *S)
{

    for(StickData*s:S->stick_list)
    {
        if(!s->view) continue;
        scene()->removeItem(s->view->stick_handle[0]);
        scene()->removeItem(s->view->stick_handle[1]);
    }
}
// cancella la storia di undo/redo, da chiamare quando si carica un nuovo file
/**
 * @details QUndoStack::clear() deletes every remaining FrameUndoCommand (each
 * frees its own before_/after_ FrameData in its destructor) — the old manual
 * per-entry delete loop and the clearUndoFlag choreography around it are both gone.
 */
void MyView::ClearUndo(){
    undo_stack.clear();
}
/**
 * @details Renders the 50x50 preview icon of a library entry from its data record:
 * temporary stick view objects are built from the StickData, rendered on a throwaway
 * scene, then freed by that scene's destructor. This mirrors StickFigure::UpdateIcon()
 * for figures that no longer have a live view (the library holds pure data now).
 */
static QIcon RenderLibraryIcon(const StickFigureData &figure_data){
    QGraphicsScene icon_scene;
    QPixmap icon_pixmap(50,50);
    icon_pixmap.fill(Qt::white);
    icon_scene.setSceneRect(my_rect->rect());
    icon_scene.setBackgroundBrush(QBrush(QColor(Qt::white)));
    for(StickData *stick_data : figure_data.stick_list){
        // la libreria non è mai "viva": stick_data non ha e non avrà mai un'altra view,
        // quindi wrapparlo direttamente (senza clonarlo) non ruba il back-pointer a nessuno
        StickGraphicsItem *render_view = new StickGraphicsItem(stick_data);
        render_view->setZValue(stick_data->z);
        render_view->br = render_view->UpdateBr(0);
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
void MyView::SaveLibrary(QString fileName){
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
void MyView::ClearCurrentLib(){
    my_current_library_widget->clear();
    delete stickLibraryBuffer;
    stickLibraryBuffer = new FrameData();
}
/**
 * @details Reads the library file into the data buffer, then builds the list-widget
 * entries: each item stores its StickFigureData pointer and an icon rendered from the
 * data. mode!=0 keeps the widget untouched (legacy semantics, currently unused).
 */
void MyView::LoadLibrary(QString fileName, int mode){
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
    for(StickFigureData *figure_data:stickLibraryBuffer->stick_figures){
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
StickFigure* MyView::AddStickFigureToLibrary(){
    if(my_animation->CurrentFrame()->CurrentStickFigure()->stick_list.count()>0)
    {
        StickFigureData *figure_data = ExtractStickFigureData(*my_animation->CurrentFrame()->CurrentStickFigure());
        stickLibraryBuffer->stick_figures.append(figure_data);
        QListWidgetItem * addedItem = new QListWidgetItem();
        addedItem->setData(Qt::UserRole,QVariant::fromValue(figure_data));
        addedItem->setData(Qt::DisplayRole,figure_data->name);
        addedItem->setIcon(RenderLibraryIcon(*figure_data));
        int newIdx = my_current_library_widget->currentRow()+1;
        my_current_library_widget->insertItem(newIdx,addedItem);
        my_current_library_widget->clearSelection();
        addedItem->setSelected(true);
        my_current_library_widget->setCurrentRow(newIdx);
        return my_animation->CurrentFrame()->CurrentStickFigure();
    }else
    {
        return nullptr;
    }

}
/**
 * @details Removes the selected entry from both the widget and the data buffer; the
 * record is freed with a plain delete (it owns its sticks, nothing else to detach).
 */
void MyView::RemoveFromLibrary(){
    // get selected item
    int idx = my_current_library_widget->currentRow();
    if(idx<0 || idx>=my_current_library_widget->count()) // niente selezionato: senza questo, takeItem(-1) torna nullptr e va in crash sotto
        return;
    QListWidgetItem *itemToRemove = my_current_library_widget->takeItem(idx);
    StickFigureData* toRemove = qvariant_cast<StickFigureData*>(itemToRemove->data(Qt::UserRole));
    delete itemToRemove;
    stickLibraryBuffer->stick_figures.removeAll(toRemove);
    delete toRemove;
}
void MyView::SetGraphics(bool all, int attribute){
    StoreUndo(CMD_SIMPLE);
    if(!all){
        if(attribute == ATTRIBUTE_PENCOLOR)
        {
            my_animation->CurrentFrame()->CurrentStick()->pen.setColor(my_pen.color());
        }
        else if(attribute== ATTRIBUTE_PENWIDTH)
        {
            my_animation->CurrentFrame()->CurrentStick()->pen.setWidth(my_pen.width());
        }
        else if(attribute== ATTRIBUTE_BRUSHCOLOR)
        {
            my_animation->CurrentFrame()->CurrentStick()->brush.setColor(my_brush.color());
        }
        if(my_animation->CurrentFrame()->CurrentStick()->view)
            my_animation->CurrentFrame()->CurrentStick()->view->Refresh(0);
    }
    else{
        for(StickData * s:my_animation->CurrentFrame()->CurrentStickFigure()->stick_list){
            if(attribute == ATTRIBUTE_PENCOLOR)
                s->pen.setColor(my_pen.color());
            else if(attribute== ATTRIBUTE_PENWIDTH)
                s->pen.setWidth(my_pen.width());
            else if(attribute== ATTRIBUTE_BRUSHCOLOR)
                s->brush.setColor(my_brush.color());
            if(s->view)
                s->view->Refresh(0);
        }
    }

    my_animation->CurrentFrame()->CurrentStickFigure()->UpdateIcon();
}
/**
 * @brief MyView::SplitStickFigure
 */
void MyView::SplitStickFigure(){
    if(my_animation->CurrentFrame()->CurrentStickFigure()== nullptr || my_animation->CurrentFrame()->CurrentStick() == nullptr)
    {
        QMessageBox::about(this,"error","No stick item selected");
        return;
    }
    if(my_animation->CurrentFrame()->CurrentStick() == my_animation->CurrentFrame()->CurrentStickFigure()->master_stick)
    {
        QMessageBox::about(this,"error","cannot split a stickfigure from his master node");
        return;
    }
    StoreUndo();
    splitToStickFigure = my_animation->CurrentFrame()->CurrentStickFigure();
    RemoveHandles(splitToStickFigure);
    splitToStick = splitToStickFigure->current_stick;
    QList<StickData*> oldStickList = my_animation->CurrentFrame()->CurrentStickFigure()->stick_list;

    StickFigure * branch = my_animation->current_frame->AddStickFigure(my_stick_figure_widget_list, splitToStickFigure->name+"_split");
    SplitStickFigures(splitToStickFigure,splitToStick,branch);
    splitToStickFigure->Refresh(0);
    // update the total list
    QList<StickData*> survivorList = splitToStickFigure->stick_list;
    for(StickData*s:oldStickList){
        if(!survivorList.contains(s)){
            my_animation->CurrentFrame()->total_sticks.removeAll(s);
        }
    }
    my_animation->CurrentFrame()->total_sticks.append(branch->stick_list);
    AddHandles(splitToStickFigure);
    AddHandles(branch);
    //
    branch->Refresh(0);
    my_animation->CurrentFrame()->CurrentStickFigure() = branch;
    my_animation->CurrentFrame()->CurrentStick() = branch->stick_list[0];
    my_animation->CurrentFrame()->CurrentStickFigure()->UpdateIcon();

}
void MyView::SizeChange(int option){

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
            for(Frame*F:my_animation->frame_list){
                ScaleFrame(F,amount,oldRect);
                for(StickFigure* S: F->stick_figures){
                    S->Traslate(dx,dy);
                }

            }
            break;
        }
        case REPO:
        {
            for(Frame*F:my_animation->frame_list){
                for(StickFigure* S: F->stick_figures){
                    S->Traslate(dx,dy);
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
void MyView::PreparePreview(){

    playBack = true;
    for(Frame* f :my_animation->frame_list){
        QGraphicsScene renderScene; //scena fittizia
        QImage *renderImg = new QImage(W,H,QImage::Format_ARGB32); //immagine temporanea


        QPainter painter(renderImg); // painter che esegue il drawing sull immagine
        renderScene.setSceneRect(my_rect->rect());
        // questi stick sono VIVI: clona i dati prima di wrapparli, altrimenti il costruttore
        // di stick ruberebbe il back-pointer data->view alla view reale sullo schermo
        QList<StickData*> renderClones;
        for(StickFigure *S:f->stick_figures)
        {
            for(StickData* s: S->stick_list){
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
        preview_list.append(item);
        delete renderImg;
        renderScene.clear(); // distruggi le view di rendering PRIMA di liberare i dati clonati
        qDeleteAll(renderClones);
        //...add all other video frames here
    }
    playBack = false;
}
void MyView::DisplayPreview(int idx){
    // rimuovi il pixmap del frame precedente: altrimenti si accumula uno per frame per tutta
    // la riproduzione, invece di essere ripulito solo (tutto insieme) quando la riproduzione finisce
    if(idx>0)
        scene()->removeItem(preview_list[idx-1]);
    QGraphicsPixmapItem* renderImg = preview_list[idx];

    //item->setOffset(onion_offset);
    scene()->addItem(renderImg);
    //item->setZValue(0);
}
