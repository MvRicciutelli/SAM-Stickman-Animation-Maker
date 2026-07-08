#include "frame.h"
#include <QHash>
#include <QListWidget>
#include <QDebug>
#include <QBuffer>
#include <QPainter>
#include <QFont>
#include "uiItems.h"
#define CS currentStickFigure->currentStick
extern QListWidget * myStickFigureWidgetList;
extern bool undoFlag;
extern bool switchFrameFlag;
Frame::Frame()
{
    frameIcon       = new QIcon();
    iconImg         = new QPixmap(50,50);
    currentStickFigure = nullptr;
}
Frame::~Frame(){
    for(StickFigure* S: stickFigures)
    {
        delete S;
    }
    delete iconImg;
    delete frameIcon;
    //delete renderImg;
}
// da Phase 2b questa funzione viene chiamata solo dai percorsi UI vivi (Widget::addStick
// e i paste): la deserializzazione non passa più di qui (vedi PopulateLiveFrame), quindi
// le guardie su libFlag/copyFlag/loadingAnimationFlag sono cadute con i rispettivi flag
StickFigure* Frame::addStickFigure(QListWidget * myListWidget, QString name)
{
    QString myName;
    if(name.count()== 0){
        int intName = 0;
        if(!stickFigures.isEmpty()){
            intName = myStickFigureWidgetList->currentRow()+1;
        }

        myName.asprintf("%d",intName);
    }
    else{
        myName = name;
    }
    //deseleziona graficamente nel viewport

    QListWidgetItem * addedItem = new QListWidgetItem();

    if(currentStickFigure!=nullptr)
    {
        if(!stickFigures.isEmpty() && !currentStickFigure->stickList.isEmpty())
        {
            currentStickFigure->highlight(false); //de highlight old selection
        }
    }
    stickFigureBuffer = new StickFigure(addedItem);
    stickFigures.append(stickFigureBuffer);
    //seleziona l'ordine z in base all'ordine di creazione
    stickFigureBuffer->baseZ = stickFigures.count();
    //aggiorna lo stickfigure corrente
    currentStickFigure = stickFigureBuffer;
    currentStickFigure->name = myName;
    // switchFrameFlag mancava: senza di esso, duplicare un frame (on_copyFrame_clicked) inseriva
    // ogni suo stickfigure in myStickFigureWidgetList durante il deserializzo, per poi vederselo
    // subito buttare via dal clear()+rebuild che moveToFrame() fa comunque un attimo dopo
    if(!undoFlag && !switchFrameFlag){
        QVariant newData(QVariant::fromValue(currentStickFigure));
        addedItem->setData(Qt::UserRole,newData);
        addedItem->setData(Qt::DisplayRole,myName);
        addedItem->setIcon(*currentStickFigure->stickFigureIcon);
        myListWidget->insertItem(myListWidget->currentRow()+1,addedItem);
    }
    // update current stickfigure
    currentStickFigure->highlight(false); //de highlight old selection
    currentStickFigure->highlight(true); //highlight new one
    if(!undoFlag && !switchFrameFlag){
        // update current stick
        scene->clearSelection(); //clear scene selection
        if(!currentStickFigure->stickList.isEmpty())
        {
            selectStick(currentStickFigure); // puo essere ridontante in quanto ci sta la callback di sticklayerView_currentItemChanged//update selected stick
        }
        int tempIndex = myListWidget->currentRow();
        myListWidget->clearSelection();
        addedItem->setSelected( true);
        myListWidget->setCurrentRow(tempIndex+1);
    }
    currentStickFigure->scene = scene;

    return stickFigureBuffer;
}
// Frame::addStickFigureToLibrary è stato eliminato (Phase 2b): la libreria contiene
// StickFigureData e viene gestita interamente da myView::addStickFigureToLibrary
StickFigure* Frame::removeStickFigure(StickFigure* toRemove)
{
    if(stickFigures.count() > 0)
    {
        stickFigures.removeAll(toRemove);
        delete toRemove;
        if(!stickFigures.isEmpty())
            currentStickFigure = stickFigures[0];
        else
        {
            qDebug("removed all sticks");
            currentStickFigure = nullptr;
        }
        stickFigureBuffer = currentStickFigure;

    }
    return currentStickFigure;
}
stick *Frame::selectStick(StickFigure* S, stick* s)
{
    int stickIdx = 0;
    if(s!=nullptr){
        if(S->stickList.contains(s))
            stickIdx = S->stickList.indexOf(s);
    }
    S->highlight(true);
    S->stickList[stickIdx]->setSelected(true);
    S->currentStick = S->stickList[stickIdx];
    S->currentStick->selected = true;

    return S->stickList[stickIdx];
}
void Frame::refresh(int mode){
    for(stick*s:totalSticks){
        scene->removeItem(s);
    }
    for(stick*s:totalSticks){
        scene->addItem(s);
    }
    for(StickFigure*S:stickFigures){
        S->refresh(mode);
    }
}
stick *Frame::selectStick(QPointF point)
{
    float distBuffer;
    float minDist;
    int idxBuffer;
    int selectedIdx = -1;
    StickFigure *selectedStickFigure;
    for(StickFigure * S: stickFigures)
    {
        // su un criterio di distanza dall'handle utile, scegli l'item stick più vicino per ogni stickfigure
            idxBuffer = S->selectStick( &point);
            distBuffer = QLineF(S->stickList[idxBuffer]->myLine.p2(),point).length();
            //scegli il più vicino tra tutti gli stickfigure
            if(selectedIdx == -1 || (distBuffer<minDist))
            {
                selectedIdx = idxBuffer;
                minDist = distBuffer;
                selectedStickFigure = S;
            }
    }
    selectedStickFigure->stickList[selectedIdx]->setSelected(true); //for scene
    selectedStickFigure->currentStick->selected = true; //for rendering purpose
    return selectedStickFigure->stickList[selectedIdx];
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
    data->frameNumber = source_frame.frameNumber;
    data->numOfItems = source_frame.numOfItems;
    QHash<const stick*, StickData*> live_to_data;
    for (StickFigure *figure : source_frame.stickFigures) {
        StickFigureData *figure_data = ExtractStickFigureData(*figure);
        data->stickFigures.append(figure_data);
        for (int i = 0; i < figure->stickList.count(); i++)
            live_to_data.insert(figure->stickList[i], figure_data->stickList[i]);
        if (figure == source_frame.currentStickFigure)
            data->currentStickFigure = figure_data;
    }
    for (stick *ordered_stick : source_frame.totalSticks) {
        StickData *mapped = live_to_data.value(ordered_stick, nullptr);
        if (mapped) // una voce stantia in totalSticks non deve propagarsi allo snapshot
            data->totalSticks.append(mapped);
    }
    return data;
}

/**
 * @details Rebuilds a live frame's content from data, replicating what the legacy
 * operator>> did per configuration: one live figure per data figure (created directly,
 * NOT via addStickFigure — no list-widget or selection side effects, moveToFrame()
 * rebuilds those for the displayed frame), totalSticks rebuilt from each stick's order
 * with the legacy out-of-range guard, current selection restored from the data record
 * with the legacy bounds checks.
 */
void PopulateLiveFrame(Frame *target_frame, const FrameData &source_data,
                       bool attach_to_scene, bool update_icons)
{
    target_frame->numOfItems = source_data.numOfItems;
    target_frame->frameNumber = source_data.frameNumber;
    target_frame->totalSticks.clear();
    for (StickFigureData *figure_data : source_data.stickFigures) {
        QListWidgetItem *added_item = new QListWidgetItem();
        StickFigure *figure = new StickFigure(added_item);
        target_frame->stickFigures.append(figure);
        figure->baseZ = target_frame->stickFigures.count();
        figure->scene = target_frame->scene;
        target_frame->currentStickFigure = figure;
        PopulateLiveStickFigure(figure, *figure_data, attach_to_scene, update_icons);
        added_item->setData(Qt::DisplayRole, figure->name);
        for (stick *s : figure->stickList)
            target_frame->totalSticks.append(s);
    }
    for (StickFigure *figure : target_frame->stickFigures) {
        for (stick *s : figure->stickList) {
            // s->order presuppone di formare una permutazione perfetta 0..count-1: se per qualche
            // motivo non lo è (es. mai passato da updateFrameOrder()), senza questo controllo si
            // scrive fuori dai limiti di totalSticks (corruzione dell'heap, crash non immediato)
            if (s->order >= 0 && s->order < target_frame->totalSticks.count())
                target_frame->totalSticks[s->order] = s;
        }
    }
    // ripristina la selezione corrente mappando i puntatori del record dati sugli indici
    // delle figure vive appena create (il target parte senza figure, quindi 1:1)
    target_frame->currentStickFigure = nullptr;
    int figure_index = source_data.stickFigures.indexOf(source_data.currentStickFigure);
    if (source_data.currentStickFigure && figure_index >= 0 && figure_index < target_frame->stickFigures.length()) {
        int stick_index = source_data.currentStickFigure->stickList.indexOf(source_data.currentStickFigure->currentStick);
        if (stick_index >= 0 && stick_index < target_frame->stickFigures[figure_index]->stickList.length()) {
            target_frame->currentStickFigure = target_frame->stickFigures[figure_index];
            target_frame->currentStickFigure->currentStick = target_frame->currentStickFigure->stickList[stick_index];
        }
    }
}

/**
 * @details Wrapper kept for the whole-animation file save path (Animation's
 * operator<< streams every frame through here). Byte layout unchanged.
 */
QDataStream & operator<< (QDataStream& stream, const Frame& myFrame){
    FrameData *data = ExtractFrameData(myFrame);
    stream << *data;
    delete data;
    return stream;
}

/**
 * @details Wrapper kept for the whole-animation file load path: sticks stay detached
 * and icons unrendered (the historical loadingAnimationFlag behavior) because
 * moveToFrame() attaches and refreshes the frame that actually gets displayed.
 */
QDataStream & operator>> (QDataStream& stream, Frame& myFrame){
    FrameData data;
    stream >> data;
    PopulateLiveFrame(&myFrame, data, false, false);
    return stream;
}
float Frame::minMaxZ(int option){
    if(totalSticks.isEmpty())
        return 0;
    float peak = totalSticks[0]->Z;
    for(stick*s:totalSticks){
        if(option){
            if(s->Z>peak)
                peak = s->Z;
        }
        else{
            if(s->Z<peak)
                peak = s->Z;
        }
    }
    return peak;
}
void Frame::stickfigureToBottom(StickFigure* S){
    S->updateZ(minMaxZ(0));
    int min = -1;
    for(int j = totalSticks.length()-1;j>min;j--){
        if(S->stickList.contains(totalSticks[j])){
            totalSticks.move(j,0);
            min++;
            j++;
        }
    }

}
void Frame::stickfigureToTop(StickFigure* S){
    S->updateZ(minMaxZ(1));
    int max = totalSticks.length()-1;
    for(int j = 0; j<max;j++){
        if(S->stickList.contains(totalSticks[j])){
            totalSticks.move(totalSticks.indexOf(totalSticks[j]),max);
            j--;
            max--;
        }
    }

}

void Frame::stickfigureAboveBelow(StickFigure* toMove, StickFigure *previous, int mode){

    mode == 1?toMove->updateZ(previous->maxZ()): toMove->updateZ(previous->minZ());
    int peakIdx = 0;
    QList<stick*> sticksToMove;
    if(mode == 1){
        for(int j = totalSticks.length()-1; j >-1 ;j--){
            if(previous->stickList.contains(totalSticks[j])){
                peakIdx = j; // il più alto che deve superare toMove
                break;
            }
        }
        for(stick*s:toMove->stickList){
            if(totalSticks.indexOf(s)<peakIdx)
                sticksToMove.append(s);
        }
        for(stick*s:sticksToMove){
            totalSticks.move(totalSticks.indexOf(s),peakIdx);
        }
    }
    else{
        for(int j = 0; j<totalSticks.length();j++){
            if(previous->stickList.contains(totalSticks[j])){
                peakIdx = j; // il piu basso, sotto al quale si deve abbassare il toMove
                break;
            }
        }
        for(stick*s:toMove->stickList){
            if(totalSticks.indexOf(s)>peakIdx)
                sticksToMove.prepend(s);
        }
        for(stick*s:sticksToMove){
            totalSticks.move(totalSticks.indexOf(s),peakIdx);
        }
    }


}
// questa funzione aggiorna l'iconea del livello
void Frame::updateIcon()
{
    //crea una scena fittizia, falla bianca e delle stesse dimensioni di quella principale
    QGraphicsScene icoScene;
    iconImg->fill(Qt::white);
    icoScene.setBackgroundBrush(QBrush(QColor(Qt::white)));
    icoScene.setSceneRect(scene->sceneRect());
    // ora clona tutti gli stick nello stickfigure e aggiungili alla scena fittizia
    // (addItem rende la scena proprietaria dei cloni: non vanno cancellati a mano,
    // altrimenti verrebbero distrutti due volte quando icoScene esce di scope)
    for(StickFigure *S:stickFigures)
    {
        for(stick* s: S->stickList){
            icoScene.addItem(new stick(s));
        }
    }
    // crea un painter che disegni sull'immagine
    QPainter painter(iconImg);
    // renderizza dalla scena sull'immagine mediante il painter
    icoScene.render(&painter);
    // disegna il numero del frame direttamente sull'iconea (fascia scura semi-trasparente in basso,
    // testo bianco sopra): affidarsi all'etichetta nativa della QListWidgetItem in IconMode non
    // mostrava il numero in modo affidabile, quindi lo "cuociamo" nell'immagine stessa
    QRectF numberBand(0, iconImg->height()-14, iconImg->width(), 14);
    painter.fillRect(numberBand, QColor(0,0,0,160));
    painter.setPen(Qt::white);
    QFont numberFont = painter.font();
    numberFont.setPointSize(8);
    painter.setFont(numberFont);
    painter.drawText(numberBand, Qt::AlignCenter, QString::number(frameNumber));
    // imposta l'iconea
    delete frameIcon;
    frameIcon = new QIcon(*iconImg);
    linkedItem->setIcon(*frameIcon);
    linkedItem->setData(Qt::DisplayRole, QString()); // il numero è già nell'iconea: evita di mostrarlo due volte

    // i cloni vengono cancellati automaticamente dal distruttore di icoScene
    painter.end();
}

void Frame::updateRender(){
    /*QGraphicsScene renderScene;
    renderScene.setSceneRect(scene->sceneRect());
    // ora clona tutti gli stick nello stickfigure in una tempList, aggiungili alla scena fittizia
    QList<stick*> tempList;
    for(StickFigure *S:stickFigures)
    {
        for(stick* s: S->stickList){
            stick* clone = new stick(s);
            tempList.append(clone);
            renderScene.addItem(clone);
        }
    }
    QPainter painter(renderImg);
    renderImg->fill(Qt::transparent);
    painter.setBackground(QBrush(QColor(Qt::transparent)));
    renderScene.render(&painter);
    renderScene.clear();*/
}
void Frame::clearFrame(){
    for(StickFigure *S:stickFigures){
        // il frame viene svuotato per sempre qui: libera gli stick esplicitamente invece di
        // affidarsi al distruttore di StickFigure, che salta la pulizia quando undoFlag/copyFlag/
        // libFlag/clearUndoFlag sono attivi (pensati per gli oggetti "di passaggio" della
        // (de)serializzazione, non per una cancellazione definitiva come questa - senza questo,
        // ogni undo/redo che passa da qui perdeva la memoria del frame sovrascritto)
        for(stick* s: S->stickList){
            scene->removeItem(s);
            delete s;
        }
        delete S;

    }
    stickFigures.clear();

}

void Frame::normalizeDepth()
{


}
void Frame::cleanTotalSticks(void){
    QList<stick*> temp;
    for(StickFigure* S:stickFigures){
        for(stick *s:S->stickList){
            if(!totalSticks.contains(s)){
                totalSticks.insert(s->order,s);
                temp.append(s);

            }
        }
    }
    for(stick*s:totalSticks){
        if(!temp.contains(s)){
            totalSticks.removeAll(s);
        }
    }
}
