#include "frame.h"
#include "framepresenter.h"
#include <QHash>
#include <QtAlgorithms>
#include <QListWidget>
#include <QDebug>
#include <QPainter>
#include <QFont>
#include "uiItems.h"
extern QListWidget * my_stick_figure_widget_list;
Frame::Frame()
{
    frame_icon       = new QIcon();
    icon_img         = new QPixmap(50,50);
    current_stick_figure = nullptr;
}
Frame::~Frame(){
    for(StickFigure* S: stick_figures)
    {
        delete S;
    }
    delete icon_img;
    delete frame_icon;
    //delete renderImg;
}
// da Phase 2b questa funzione viene chiamata solo dai percorsi UI vivi (Widget::addStick
// e i paste): la deserializzazione non passa più di qui (vedi PopulateLiveFrame), quindi
// le guardie su libFlag/copyFlag/loadingAnimationFlag sono cadute con i rispettivi flag
StickFigure* Frame::AddStickFigure(QListWidget * my_list_widget, QString name)
{
    QString myName;
    if(name.count()== 0){
        int intName = 0;
        if(!stick_figures.isEmpty()){
            intName = my_stick_figure_widget_list->currentRow()+1;
        }

        myName.asprintf("%d",intName);
    }
    else{
        myName = name;
    }
    //deseleziona graficamente nel viewport

    QListWidgetItem * addedItem = new QListWidgetItem();

    if(current_stick_figure!=nullptr)
    {
        if(!stick_figures.isEmpty() && !current_stick_figure->stick_list.isEmpty())
        {
            current_stick_figure->Highlight(false); //de highlight old selection
        }
    }
    stick_figure_buffer = new StickFigure(addedItem);
    stick_figures.append(stick_figure_buffer);
    //seleziona l'ordine z in base all'ordine di creazione
    stick_figure_buffer->base_z = stick_figures.count();
    //aggiorna lo stickfigure corrente
    current_stick_figure = stick_figure_buffer;
    current_stick_figure->name = myName;
    QVariant newData(QVariant::fromValue(current_stick_figure));
    addedItem->setData(Qt::UserRole,newData);
    addedItem->setData(Qt::DisplayRole,myName);
    addedItem->setIcon(*current_stick_figure->stick_figure_icon);
    my_list_widget->insertItem(my_list_widget->currentRow()+1,addedItem);
    // update current stickfigure
    current_stick_figure->Highlight(false); //de highlight old selection
    current_stick_figure->Highlight(true); //highlight new one
    // update current stick
    scene->clearSelection(); //clear scene selection
    if(!current_stick_figure->stick_list.isEmpty())
    {
        SelectStick(current_stick_figure); // puo essere ridontante in quanto ci sta la callback di sticklayerView_currentItemChanged//update selected stick
    }
    int tempIndex = my_list_widget->currentRow();
    my_list_widget->clearSelection();
    addedItem->setSelected( true);
    my_list_widget->setCurrentRow(tempIndex+1);
    current_stick_figure->scene = scene;

    return stick_figure_buffer;
}
// Frame::addStickFigureToLibrary è stato eliminato (Phase 2b): la libreria contiene
// StickFigureData e viene gestita interamente da MyView::addStickFigureToLibrary
StickFigure* Frame::RemoveStickFigure(StickFigure* toRemove)
{
    if(stick_figures.count() > 0)
    {
        stick_figures.removeAll(toRemove);
        delete toRemove;
        if(!stick_figures.isEmpty())
            current_stick_figure = stick_figures[0];
        else
        {
            qDebug("removed all sticks");
            current_stick_figure = nullptr;
        }
        stick_figure_buffer = current_stick_figure;

    }
    return current_stick_figure;
}
StickData *Frame::SelectStick(StickFigure* S, StickData* s)
{
    int stickIdx = 0;
    if(s!=nullptr){
        if(S->stick_list.contains(s))
            stickIdx = S->stick_list.indexOf(s);
    }
    S->Highlight(true);
    if(S->stick_list[stickIdx]->view)
        S->stick_list[stickIdx]->view->setSelected(true);
    S->current_stick = S->stick_list[stickIdx];
    if(S->current_stick->view)
        S->current_stick->view->selected = true;

    return S->stick_list[stickIdx];
}
void Frame::Refresh(int mode){
    for(StickData*s:total_sticks){
        if(s->view)
            scene->removeItem(s->view);
    }
    for(StickData*s:total_sticks){
        if(s->view)
            scene->addItem(s->view);
    }
    for(StickFigure*S:stick_figures){
        S->Refresh(mode);
    }
}
StickData *Frame::SelectStick(QPointF point)
{
    float distBuffer;
    float minDist;
    int idxBuffer;
    int selectedIdx = -1;
    StickFigure *selectedStickFigure;
    for(StickFigure * S: stick_figures)
    {
        // su un criterio di distanza dall'handle utile, scegli l'item stick più vicino per ogni stickfigure
            idxBuffer = S->SelectStick( &point);
            distBuffer = QLineF(S->stick_list[idxBuffer]->my_line.p2(),point).length();
            //scegli il più vicino tra tutti gli stickfigure
            if(selectedIdx == -1 || (distBuffer<minDist))
            {
                selectedIdx = idxBuffer;
                minDist = distBuffer;
                selectedStickFigure = S;
            }
    }
    if(selectedStickFigure->stick_list[selectedIdx]->view)
        selectedStickFigure->stick_list[selectedIdx]->view->setSelected(true); //for scene
    if(selectedStickFigure->current_stick->view)
        selectedStickFigure->current_stick->view->selected = true; //for rendering purpose
    return selectedStickFigure->stick_list[selectedIdx];
}
/**
 * @details Snapshot copy of a live frame into pure data: every figure is extracted,
 * then the frame-wide z-order list and the current-figure pointer are remapped onto
 * the data copies. Replaces the serialize-to-QBuffer idiom for undo snapshots and
 * frame copy/paste — no stream, no flags.
 */
FrameData *ExtractFrameData(const Frame &source_frame)
{
    FrameData *data = new FrameData();
    data->frame_number = source_frame.frame_number;
    data->num_of_items = source_frame.num_of_items;
    QHash<const StickData*, StickData*> live_to_data;
    for (StickFigure *figure : source_frame.stick_figures) {
        StickFigureData *figure_data = ExtractStickFigureData(*figure);
        data->stick_figures.append(figure_data);
        for (int i = 0; i < figure->stick_list.count(); i++)
            live_to_data.insert(figure->stick_list[i], figure_data->stick_list[i]);
        if (figure == source_frame.current_stick_figure)
            data->current_stick_figure = figure_data;
    }
    for (StickData *ordered_stick : source_frame.total_sticks) {
        StickData *mapped = live_to_data.value(ordered_stick, nullptr);
        if (mapped) // una voce stantia in total_sticks non deve propagarsi allo snapshot
            data->total_sticks.append(mapped);
    }
    return data;
}

/**
 * @details Rebuilds a live frame's content from data, replicating what the legacy
 * operator>> did per configuration: one live figure per data figure (created directly,
 * NOT via AddStickFigure — no list-widget or selection side effects, moveToFrame()
 * rebuilds those for the displayed frame), total_sticks rebuilt from each stick's order
 * with the legacy out-of-range guard, current selection restored from the data record
 * with the legacy bounds checks.
 */
void PopulateLiveFrame(Frame *target_frame, const FrameData &source_data,
                       bool attach_to_scene, bool update_icons)
{
    target_frame->num_of_items = source_data.num_of_items;
    target_frame->frame_number = source_data.frame_number;
    target_frame->total_sticks.clear();
    for (StickFigureData *figure_data : source_data.stick_figures) {
        QListWidgetItem *added_item = new QListWidgetItem();
        StickFigure *figure = new StickFigure(added_item);
        target_frame->stick_figures.append(figure);
        figure->base_z = target_frame->stick_figures.count();
        figure->scene = target_frame->scene;
        target_frame->current_stick_figure = figure;
        PopulateLiveStickFigure(figure, *figure_data, attach_to_scene, update_icons);
        added_item->setData(Qt::DisplayRole, figure->name);
        for (StickData *s : figure->stick_list)
            target_frame->total_sticks.append(s);
    }
    for (StickFigure *figure : target_frame->stick_figures) {
        for (StickData *s : figure->stick_list) {
            // s->order presuppone di formare una permutazione perfetta 0..count-1: se per qualche
            // motivo non lo è (es. mai passato da updateFrameOrder()), senza questo controllo si
            // scrive fuori dai limiti di total_sticks (corruzione dell'heap, crash non immediato)
            if (s->order >= 0 && s->order < target_frame->total_sticks.count())
                target_frame->total_sticks[s->order] = s;
        }
    }
    // ripristina la selezione corrente mappando i puntatori del record dati sugli indici
    // delle figure vive appena create (il target parte senza figure, quindi 1:1)
    target_frame->current_stick_figure = nullptr;
    int figure_index = source_data.stick_figures.indexOf(source_data.current_stick_figure);
    if (source_data.current_stick_figure && figure_index >= 0 && figure_index < target_frame->stick_figures.length()) {
        int stick_index = source_data.current_stick_figure->stick_list.indexOf(source_data.current_stick_figure->current_stick);
        if (stick_index >= 0 && stick_index < target_frame->stick_figures[figure_index]->stick_list.length()) {
            target_frame->current_stick_figure = target_frame->stick_figures[figure_index];
            target_frame->current_stick_figure->current_stick = target_frame->current_stick_figure->stick_list[stick_index];
        }
    }
}

/**
 * @details Wrapper kept for the whole-animation file save path (Animation's
 * operator<< streams every frame through here). Byte layout unchanged.
 */
QDataStream & operator<< (QDataStream& stream, const Frame& my_frame){
    FrameData *data = ExtractFrameData(my_frame);
    stream << *data;
    delete data;
    return stream;
}

/**
 * @details Wrapper kept for the whole-animation file load path: sticks stay detached
 * and icons unrendered (the historical loadingAnimationFlag behavior) because
 * moveToFrame() attaches and refreshes the frame that actually gets displayed.
 */
QDataStream & operator>> (QDataStream& stream, Frame& my_frame){
    FrameData data;
    stream >> data;
    PopulateLiveFrame(&my_frame, data, false, false);
    return stream;
}
float Frame::MinMaxZ(int option){
    if(total_sticks.isEmpty())
        return 0;
    float peak = total_sticks[0]->z;
    for(StickData*s:total_sticks){
        if(option){
            if(s->z>peak)
                peak = s->z;
        }
        else{
            if(s->z<peak)
                peak = s->z;
        }
    }
    return peak;
}
void Frame::StickfigureToBottom(StickFigure* S){
    S->UpdateZ(MinMaxZ(0));
    int min = -1;
    for(int j = total_sticks.length()-1;j>min;j--){
        if(S->stick_list.contains(total_sticks[j])){
            total_sticks.move(j,0);
            min++;
            j++;
        }
    }

}
void Frame::StickfigureToTop(StickFigure* S){
    S->UpdateZ(MinMaxZ(1));
    int max = total_sticks.length()-1;
    for(int j = 0; j<max;j++){
        if(S->stick_list.contains(total_sticks[j])){
            total_sticks.move(total_sticks.indexOf(total_sticks[j]),max);
            j--;
            max--;
        }
    }

}

void Frame::StickfigureAboveBelow(StickFigure* toMove, StickFigure *previous, int mode){

    mode == 1?toMove->UpdateZ(previous->MaxZ()): toMove->UpdateZ(previous->MinZ());
    int peakIdx = 0;
    QList<StickData*> sticksToMove;
    if(mode == 1){
        for(int j = total_sticks.length()-1; j >-1 ;j--){
            if(previous->stick_list.contains(total_sticks[j])){
                peakIdx = j; // il più alto che deve superare toMove
                break;
            }
        }
        for(StickData*s:toMove->stick_list){
            if(total_sticks.indexOf(s)<peakIdx)
                sticksToMove.append(s);
        }
        for(StickData*s:sticksToMove){
            total_sticks.move(total_sticks.indexOf(s),peakIdx);
        }
    }
    else{
        for(int j = 0; j<total_sticks.length();j++){
            if(previous->stick_list.contains(total_sticks[j])){
                peakIdx = j; // il piu basso, sotto al quale si deve abbassare il toMove
                break;
            }
        }
        for(StickData*s:toMove->stick_list){
            if(total_sticks.indexOf(s)>peakIdx)
                sticksToMove.prepend(s);
        }
        for(StickData*s:sticksToMove){
            total_sticks.move(total_sticks.indexOf(s),peakIdx);
        }
    }


}
/**
 * @details Since Phase 2c the render clones need their own StickData (see
 * StickFigure::UpdateIcon() for why) — cloned, wrapped, rendered, then the views are
 * freed via icoScene.clear() BEFORE the cloned data (~stick reads data->view).
 */
void Frame::UpdateIcon()
{
    //crea una scena fittizia, falla bianca e delle stesse dimensioni di quella principale
    QGraphicsScene icoScene;
    icon_img->fill(Qt::white);
    icoScene.setBackgroundBrush(QBrush(QColor(Qt::white)));
    icoScene.setSceneRect(scene->sceneRect());
    QList<StickData*> renderClones;
    for(StickFigure *S:stick_figures)
    {
        for(StickData* s: S->stick_list){
            StickData* clone = s->Clone();
            renderClones.append(clone);
            icoScene.addItem(new StickGraphicsItem(clone));
        }
    }
    // crea un painter che disegni sull'immagine
    QPainter painter(icon_img);
    // renderizza dalla scena sull'immagine mediante il painter
    icoScene.render(&painter);
    // disegna il numero del frame direttamente sull'iconea (fascia scura semi-trasparente in basso,
    // testo bianco sopra): affidarsi all'etichetta nativa della QListWidgetItem in IconMode non
    // mostrava il numero in modo affidabile, quindi lo "cuociamo" nell'immagine stessa
    QRectF numberBand(0, icon_img->height()-14, icon_img->width(), 14);
    painter.fillRect(numberBand, QColor(0,0,0,160));
    painter.setPen(Qt::white);
    QFont numberFont = painter.font();
    numberFont.setPointSize(8);
    painter.setFont(numberFont);
    painter.drawText(numberBand, Qt::AlignCenter, QString::number(frame_number));
    // imposta l'iconea
    delete frame_icon;
    frame_icon = new QIcon(*icon_img);
    linked_item->setIcon(*frame_icon);
    linked_item->setData(Qt::DisplayRole, QString()); // il numero è già nell'iconea: evita di mostrarlo due volte

    painter.end();
    icoScene.clear();
    qDeleteAll(renderClones);
}

void Frame::UpdateRender(){
    /* obsoleto, vedi storico pre-Phase-2c */
}
/**
 * @details Since Phase 4 removed undoFlag, this is a plain qDeleteAll: ~StickFigure()
 * is now the single, unconditional owner of stick cleanup (scene removal,
 * img_list_widget entries, the sticks themselves), so there is nothing left for this
 * function to duplicate.
 */
void Frame::ClearFrame(){
    qDeleteAll(stick_figures);
    stick_figures.clear();

}

void Frame::NormalizeDepth()
{


}
void Frame::CleanTotalSticks(void){
    QList<StickData*> temp;
    for(StickFigure* S:stick_figures){
        for(StickData *s:S->stick_list){
            if(!total_sticks.contains(s)){
                total_sticks.insert(s->order,s);
                temp.append(s);

            }
        }
    }
    for(StickData*s:total_sticks){
        if(!temp.contains(s)){
            total_sticks.removeAll(s);
        }
    }
}
