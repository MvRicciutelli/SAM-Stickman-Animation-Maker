#include "animation.h"

#include <QFile>
#include <QSpinBox>
#include "advancedtab.h"
#include "advancedlinewidget.h"
#include "advancedcirclewidget.h"
#include "advancedtaperwidget.h"
#include <QCheckBox>
#include "uiItems.h"

extern qint32 W;
extern qint32 H;
extern QPointF onion_offset;
extern QGraphicsRectItem *my_rect;
extern QGraphicsRectItem *limit_rect;
extern QGraphicsScene* scene;

// clearing_animation è diventato un membro privato di Animation (vedi animation.h);
// change_type_flag era un residuo mai letto da nessuno ed è stato eliminato
namespace Ui { class Widget; }
extern Ui::Widget *my_ui;

Animation::Animation()
{

}

Frame *Animation::AddFrame(QListWidgetItem* item, int pos)
{

    frame_buffer = new Frame();
    frame_buffer->scene = scene;
    //frame_buffer->render_img = new QImage(scene->sceneRect().width(),scene->sceneRect().height(),QImage::Format_ARGB32);

    if(frame_list.isEmpty())
    {
        current_frame = frame_buffer;
    }
    frame_list.insert(pos,frame_buffer);
    frame_buffer->linked_item = item;
    return frame_buffer;
}
Frame *Animation::DeleteFrame(Frame *frame){
    int frameNum = frame_list.indexOf(frame);
    if(frame_list.count()>=1){
        frame_list.removeAll(frame);
        if(frame == current_frame){
            for(StickFigure * S:frame->stick_figures){
                // toglilo dalla lista prima di cancellarlo, altrimenti resta un
                // puntatore pendente che viene cancellato di nuovo al prossimo clear()
                my_stick_figure_widget_list->takeItem(my_stick_figure_widget_list->row(S->linked_item));
                delete S->linked_item;
            }
        }
        delete frame->linked_item;
        delete frame;
        //delete my_rect;
        /*if(!clearing_animation)
            scene->clear(); // risky*/
        SceneRemover(scene);
        //my_rect = new QGraphicsRectItem(0,0,W,H); // RENEW IT
        if(!frame_list.isEmpty() && !clearing_animation){
            if(frameNum>=frame_list.count())
            {
                current_frame = frame_list[frame_list.count()-1];
            }
            else{
                current_frame = frame_list[frameNum];
            }
        }
        else{
            current_frame = nullptr;
            frame_buffer = nullptr;
        }
    }
    return current_frame;
}
void Animation::UpdateSelection(QPointF point)
{
    float distBuffer;
    float minDist;
    int idxBuffer;
    int selectedIdx = -1;
    if(current_frame->stick_figures.isEmpty())
        return;
    scene->clearSelection();
    StickFigure *selectedStickFigure = nullptr;
    if(current_frame->CurrentStickFigure() != nullptr){
        current_frame->CurrentStickFigure()->Highlight(false);
        current_frame->CurrentStickFigure()->Refresh(0);
    }

    for(StickFigure * S: current_frame->stick_figures)
    {
        if(S->stick_list.count()>=1) {
            idxBuffer = S->SelectStick( &point);
            // aggiorna il buffer col punto 1 o 2 a seconda se stiamo selezionando lo stick master
            if(S->selecting_origin )
                distBuffer = QLineF(S->stick_list[idxBuffer]->my_line.p1(),point).length();
            else{
                distBuffer = S->MeasureDistanceFromLine(point,S->stick_list[idxBuffer]->my_line);
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
    // current_stick_figure, che può essere nullptr dopo un ripristino di undo/redo il cui
    // "stickfigure corrente" salvato era un layer vuoto (operator>> lo lascia nullptr in quel caso)
    if(current_frame->CurrentStickFigure() && current_frame->CurrentStick()){
        if(selectedStickFigure->stick_list[selectedIdx]->stick_type != current_frame->CurrentStick()->stick_type){
            UpdateTab(selectedStickFigure->stick_list[selectedIdx]->stick_type );
        }
    }

    current_frame->CurrentStickFigure() = selectedStickFigure;
    current_frame->CurrentStick() = current_frame->CurrentStickFigure()->stick_list[selectedIdx];
    current_frame->CurrentStickFigure()->Highlight(true);
    if(current_frame->CurrentStick()->view){
        current_frame->CurrentStick()->view->setSelected(true);
        current_frame->CurrentStick()->view->selected = true;
    }


    UpdateSliders();



}
void Animation::UpdateTab(int t){
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
void Animation::UpdateSelection(StickData* s)
{
    //update current frame

    // update current stickfigure
    scene->clearSelection();
    // dopo un ripristino undo/redo lo stickfigure corrente può essere legittimamente nullo
    // (vedi il commento nell'overload UpdateSelection(QPointF)): niente Highlight in quel caso
    if(current_frame->CurrentStickFigure() != nullptr)
        current_frame->CurrentStickFigure()->Highlight(false);

    for(StickFigure *S : current_frame->stick_figures)
    {
        if(S->stick_list.contains(s))
        {
            current_frame->CurrentStickFigure() = S;
            S->Highlight(true);
        }
    }
    // update current stick (se s non appartiene a nessuna figura del frame, il corrente resta invariato e può essere nullo)
    if(current_frame->CurrentStickFigure() != nullptr && !current_frame->CurrentStickFigure()->stick_list.isEmpty())
    {
     current_frame->CurrentStick() = s;
     if(s->view){
         s->view->setSelected(true);
         s->view->selected = true;
     }
    }
    UpdateSliders();
    //int idx = frame_buffer->current_stick_figure->itemList.indexOf(item);

}
void Animation::UpdateSelection(StickFigure* S)
{
    //update current frame
    if(S==current_frame->CurrentStickFigure())
        return;
    // update current stickfigure
    if(current_frame->CurrentStickFigure() !=nullptr)
    {
        current_frame->CurrentStickFigure()->Highlight(false); //de highlight old selection
    }

    current_frame->CurrentStickFigure() = S; //update current_stick_figure buffer

    S->Highlight(true); //highlight new one

    // update current stick
    scene->clearSelection(); //clear scene selection
    if(!S->stick_list.isEmpty()){
        current_frame->SelectStick(S); //update selected stick
    }


}

void Animation::UpdateSliders(){
    StickData *cs = current_frame->CurrentStick();
    if(current_frame->CurrentStick() == nullptr)
        return;
    if(cs->stick_type == StickData::IMAGE){
        //aggiorna gli slider di modifica alla scala e all'offset
        if(cs->img_w_scale>=1){
            img_width_slider->setValue(10*(cs->img_w_scale-1));
        }
        else{
            img_width_slider->setValue(-10*(1/(cs->img_w_scale)-1));
        }
        if(cs->img_h_scale>=1){
            img_height_slider->setValue(10*(cs->img_h_scale-1));
        }
        else{
            img_height_slider->setValue(-10*(1/(cs->img_h_scale)-1));

        }
        img_v_offset_slider->setValue(cs->img_offset.y());
        img_h_offset_slider->setValue(cs->img_offset.x());
        img_rotation_slider->setValue(cs->img_angle_offset);
    }
    else if(cs->stick_type == StickData::RECT){
        advanced_rect_slider->setValue(cs->img_w_scale);
    }
    else if(cs->stick_type == StickData::CIRCLE){
        if(cs->img_w_scale<1){
            circle_squash_slider->setValue(-(1-cs->img_w_scale)*100);
        }
        else{
            circle_squash_slider->setValue((cs->img_w_scale-1)*20);
        }
    }
    else if(cs->stick_type == StickData::TAPER||cs->stick_type == StickData::TRAPEZOID){
        taper_w_slider->setValue(cs->img_w_scale);
        taper_h_slider->setValue(cs->img_h_scale);
        hard_top_check->setChecked(cs->hard_top);
        hard_bottom_check->setChecked(cs->hard_bottom);

    }
    edit_line_thickness_slider->setValue(cs->pen.width());
    elongate_spinbox->setValue(  current_frame->CurrentStick()->my_line.length());
}
Frame* Animation::SetupFrame(int pos){
    // QString::asprintf è statica: chiamarla su un'istanza ne scartava il risultato e
    // il nome restava vuoto (UpdateIcon() lo sovrascrive comunque, ma l'intento era questo)
    QString name = QString::number(pos);

    // prepara il widget associato
    QListWidgetItem * addedItem = new QListWidgetItem();

    for(Frame* f: frame_list){
        if(f->frame_number>=pos){
            f->frame_number++;
            f->linked_item->setData(Qt::DisplayRole,QString::number(f->frame_number));
        }
    }
    my_frame_widget_list->insertItem(pos,addedItem);
    Frame * addedFrame = AddFrame(addedItem, pos);
    addedFrame->frame_number = pos;
    addedFrame->linked_item = addedItem;
    // popola l'entry QListWidget con puntatore ed iconea del frame
    QVariant newData(QVariant::fromValue(addedFrame));
    addedItem->setData(Qt::UserRole,newData);
    addedItem->setData(Qt::DisplayRole,name);
    addedItem->setIcon(*addedFrame->frame_icon);
    // update other frames
    return addedFrame;
}
// Animation::cloneFrame è stato eliminato (Phase 2b): il ripristino di undo/redo passa
// da PopulateLiveFrame() e le copie in-memory da ExtractFrameData()
QDataStream &operator<<(QDataStream& stream, const Animation& my_animation){
    qint32 frameNum = (qint32)my_animation.frame_list.count();
    stream<<my_animation.version;
    stream<<frameNum;
    stream<<W;
    stream<<H;
    for(Frame* f: my_animation.frame_list){
        stream<<*f;
    }
    return stream;
}
QDataStream &operator>>(QDataStream& stream,Animation& my_animation){
    qint32 frameNum = 0;
    stream>>my_animation.version;
    stream>>frameNum;
    stream>>my_animation.width;
    stream>>my_animation.height;
    // applica subito le dimensioni appena lette: prima non veniva fatto, quindi caricare un file con
    // una canvas di dimensioni diverse da quella corrente lasciava my_rect/limit_rect/onion_offset legati
    // alla vecchia canvas (nessuno li ricalcolava finché l'utente non ridimensionava manualmente)
    W = my_animation.width;
    H = my_animation.height;
    my_rect->setRect(0,0,W,H);
    SceneRemover(scene);
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
        Frame* addedFrame = my_animation.SetupFrame(i);
        stream>>*addedFrame;
        if(stream.status() != QDataStream::Ok){
            // file troncato a metà frame: fermati qui, i frame già letti restano validi
            break;
        }
        addedFrame->UpdateIcon();
    }
    return stream;
}
void Animation::SaveAnimation(QString file_name){
    QFile file(file_name);
    if(!file.open(QIODevice::WriteOnly))
    {
        return;
    }
    QDataStream out(&file);
    out << *this;
    file.close();
}
void Animation::LoadAnimation(QString name)
{

    QFile file(name);
    if(!file.open(QIODevice::ReadOnly))
    {
        return;
    }
    QDataStream out(&file);
    ClearAnimation();
    // niente più loadingAnimationFlag: l'operator>> di Frame non attacca alla scena né
    // renderizza icone per costruzione (vedi PopulateLiveFrame), non serve una guardia
    out >> *this;
    file.close();
    qDebug("load finished");

}
void Animation::ClearAnimation(){
    clearing_animation = true;
    QList <Frame*> clonedList = frame_list;
    for(Frame* f: clonedList){

        DeleteFrame(f);
    }
    my_frame_widget_list->clear();
    //my_frame_widget_list->takeItem(my_frame_widget_list->row(f->linked_item));
    SceneRemover(scene);
    //scene->clear();
    clearing_animation = false;

}
