#include "StickFigure.h"
#include "framepresenter.h"

#include <QtAlgorithms>
#include <QDebug>
#include <QFile>
#include <QVector2D>
// StickFigure work in the one active scene
extern QGraphicsRectItem* myRect;
/**
 * @details Unconditional since Phase 4's undoFlag removal: every StickFigure that
 * exists is guaranteed live (scratch/buffer figures are StickFigureData since Phase
 * 2b, never a StickFigure), so this destructor is the single, sole owner of its
 * sticks' cleanup. Since Phase 2c a stick's view and data are separate objects —
 * DetachView() frees the view (if any; sticks straight out of PopulateLiveStickFigure()
 * with attach_to_scene=false have none), then `delete s` frees the data.
 */
StickFigure::~StickFigure()
{
    for(StickData* s: stickList)
    {
        if(s->view){
            s->view->RemoveImagesFromListWidget();
            FramePresenter::DetachView(s,scene);
        }
        delete s;
    }
    delete iconImg;
    delete stickFigureIcon;
}
// questa funzione aggiorna l'iconea del livello: da Phase 2b è chiamata solo su figure
// vive (le figure di libreria sono StickFigureData e hanno il loro render in myview.cpp)
/**
 * @details Since Phase 2c the render clones need their own StickData (a stick view is
 * never allowed to share a live stick's data — it would steal its `view` back-pointer
 * via the constructor's wiring), so this clones each stick's data first. The cloned
 * data is freed explicitly with `icoScene.clear()` BEFORE `qDeleteAll(clones)` — the
 * clone stick views' destructor reads `data->view`, so the data must outlive them.
 */
void StickFigure::updateIcon()
{
    //crea una scena fittizia, falla bianca e delle stesse dimensioni di quella principale
    QGraphicsScene icoScene;
    iconImg->fill(Qt::white);

    icoScene.setSceneRect(scene->sceneRect());
    icoScene.setBackgroundBrush(QBrush(QColor(Qt::white)));
    // ora clona tutti gli stick nello stickfigure e aggiungili alla scena fittizia
    // (addItem rende la scena proprietaria dei cloni: non vanno cancellati a mano,
    // altrimenti verrebbero distrutti due volte quando icoScene esce di scope)
    QList<StickData*> renderClones;
    for(StickData* S: stickList){
        StickData* clone = S->Clone();
        renderClones.append(clone);
        icoScene.addItem(new StickGraphicsItem(clone));
    }
    // crea un painter che disegni sull'immagine
    QPainter painter(iconImg);
    // renderizza dalla scena sull'immagine mediante il painter
    icoScene.render(&painter);
    // imposta l'iconea
    delete stickFigureIcon;
    stickFigureIcon = new QIcon(*iconImg);
    linkedItem->setIcon(*stickFigureIcon);
    // libera prima le view (icoScene le possiede) poi i dati clonati (view non owning)
    icoScene.clear();
    qDeleteAll(renderClones);
}
StickFigure::StickFigure(QListWidgetItem* item)
{
    p0          =  QPointF(0,0);
    p1          =  QPointF(0,0);
    lineBuffer  =  QLineF(p0,p1);
    drawCount   =  0;
    masterStick =   nullptr;
    currentStick = nullptr;
    linkedItem  = item;
    stickFigureIcon = new QIcon();
    iconImg         = new QPixmap(50,50);
    masterStick = nullptr;
}
StickFigure::StickFigure()
{
    p0          =  QPointF(0,0);
    p1          =  QPointF(0,0);
    lineBuffer  =  QLineF(p0,p1);
    drawCount   =  0;
    masterStick =   nullptr;
    currentStick = nullptr;
    stickFigureIcon = new QIcon();
    iconImg         = new QPixmap(50,50);
    masterStick = nullptr;
}
/**
 * @details Since Phase 2c only builds the StickData for the stick being drawn — no
 * view exists yet. The caller (MyView::drawCmd()) attaches one via
 * FramePresenter::AttachView() right after this returns, then paints the master
 * handle green if this turned out to be the figure's first stick (moved out of here:
 * doing it required a view, which didn't exist yet at this point).
 */
void StickFigure::startDrawing(QPointF *point, QPen pen, QBrush brush)
{
    if(stickList.isEmpty()) // se è il primo stick dello stickfigure
    {
        p0 = *point;
    }
    else //se ci sono altri stick in questo stickfigure i dalla estremità più vicinaz al click
    {
        parentBuffer = stickList[selectOrigin(point)]; //seleziona genitore (potremmo già farlo al rigo precedente visto che la funzione viene chiamata)
        if(selectingOrigin)
            p0 = parentBuffer->myLine.p1();
        else
            p0 = parentBuffer->myLine.p2();
    }

    //crea e alloca l'oggetto stick associato
    lineBuffer      = QLineF(p0,p0); // all'inizio la linea ha lunghezza zero
    StickData *stk  = new StickData();
    stk->myLine     = lineBuffer;
    //assegna l'ordine di profondità
    stk->Z = baseZ+stickList.count()/100;
    stickBuffer     = stk;
    currentStick    = stk;
    // seè il primo stick ad essere disegnato
    if(this->masterStick == nullptr){
        masterStick = currentStick;
        masterStick->parent = nullptr;
    }
    drawCount       = 1; // segnala che un disegno è in atto
    stickBuffer->Pen = pen;
    stickBuffer->Brush = brush;
}
// aggiorna uno stick il cui disegno è già avviato
void StickFigure::previewDrawing(QPointF *point)
{
    setLineFromPoint(point);
    if(stickBuffer->view)
        stickBuffer->view->refresh(1);
    if(stickBuffer->stickType == StickData::IMAGE ||
            stickBuffer->stickType == StickData::RECT||
            (stickBuffer->stickType == StickData::TRAPEZOID)||
            (stickBuffer->stickType == StickData::CIRCLE)||
            (stickBuffer->stickType == StickData::TAPER)){
        stickBuffer->imgAngle= -atan2(stickBuffer->myLine.dx(),stickBuffer->myLine.dy())*180/M_PI;
    }
}
void StickFigure::refresh(int mode)
{
    for(StickData* s:stickList){
        if(s->view)
            s->view->refresh(mode);
    }
}


void StickFigure::setLineFromPoint(QPointF *point)
{
    p1              = *point;
    lineBuffer      = QLineF(p0,p1);
    // lo stick si fonda sulla linea che connette starting point e endpoint
    stickBuffer->myLine = lineBuffer;
}

void StickFigure::highlight(bool setting)
{
    for(StickData* s:this->stickList)
    {
        if(!s->view)
            continue;
        s->view->highlight = setting;

        if(!stickList.isEmpty() && setting == false){
            s->view->selected = false;
        }
    }

}
void StickFigure::updateZ(float value){

    for(StickData* S : stickList){
        float newVal =  S->Z-baseZ+value;
        if(newVal >MAXDEPTH)
            newVal = MAXDEPTH;
        else if(newVal<MINDEPTH)
            newVal = MINDEPTH;
        S->Z = newVal;
        if(S->view)
            S->view->setZValue(S->Z);
    }
    baseZ = value;
}

void StickFigure::endDrawing(QPointF *point)
{
    setLineFromPoint(point);
    drawCount = 0;                  //segnala la fine
    stickList.append(stickBuffer);  //disegno finito, aggiungilo alla lista
    // assegna l'elemento padre
    stickBuffer->parent = parentBuffer;
    if(stickBuffer->view)
        stickBuffer->view->refresh(0);
    // INSERISCILO COME FIGLIO DI TUTTA LA GERARCHIA PRECEDENTE
    while(parentBuffer){
        parentBuffer->children.append(stickBuffer);
        if(selectingOrigin || parentBuffer->stepchild){ // children of a stepchild is also a stepchild!
            stickBuffer->stepchild = true;
        }
        parentBuffer = parentBuffer->parent;
    }
    if(stickBuffer->stickType == StickData::IMAGE && stickBuffer->view){
        stickBuffer->view->imgRect = calcImgRect(stickBuffer->myLine,stickBuffer->stickImg->size());
    }
    scene->clearSelection();
    if(stickBuffer->view)
        stickBuffer->view->setSelected(true);
}
void StickFigure::cancelDrawing()
{
    scene->clearSelection();
    if(stickBuffer==masterStick){
        masterStick = nullptr;
        currentStick = nullptr;
    }
    else{
        currentStick = parentBuffer;
    }
    if(stickBuffer->view){
        stickBuffer->view->RemoveImagesFromListWidget(); // stick vivo (già mostrato durante il disegno)
        FramePresenter::DetachView(stickBuffer,scene);
    }
    delete stickBuffer;
    lineBuffer  = QLineF();
    drawCount   = 0;
    if(!stickList.isEmpty() && currentStick){
        currentStick->view->selected = true; // qui currentStick era già sulla scena (non è quello appena scartato)
        currentStick->view->refresh(0);
    }
}

//scegli come origine lo stick più vicino al mouse fra gli stick dello stickfigure
int StickFigure::selectOrigin( QPointF * point)
{
    //seleziona lo stick più vicino
    int idx = selectStick(point);
        //controlla quale estremità è la più vicina e aseconda di quale essa sia restituisci il punto
        if(measureDistanceFromLine(*point,stickList[idx]->myLine) >
                QLineF(stickList[idx]->myLine.p1(),*point).length())
        {
            // il p1 del masterstick è un eccezione e va considerata come possibile origine
            selectingOrigin = true;
            idx = stickList.indexOf(masterStick);

        }
        else{
            selectingOrigin = false;
        }
     return idx;
}
float StickFigure::measureDistanceFromLine(QPointF p, QLineF l){
    float dist = QLineF(l.p2(),p).length();
    return dist;

}
int StickFigure::selectStick( QPointF * point)
{
    int idx         = 0;
    float  minDist  = 0;
    float currentDist = 0;
    int chosenIdx = stickList.indexOf(masterStick);
    //inizializziamo a partire dall'origine del masterStick
    minDist = measureDistanceFromLine(*point,stickList[0]->myLine);
    while(idx< stickList.length())
    {
        // i punti inizio-fine dello stick corrente
        currentDist = measureDistanceFromLine(*point,stickList[idx]->myLine);
        // se la distanza è più bassa di quella minima registrata dal luogo del mouse
        if(currentDist<= minDist)
        {

            minDist         = currentDist;
            chosenIdx       = idx;
            selectingOrigin = false;
        }
        if(stickList[idx] == masterStick){
            currentDist =QLineF(stickList[idx]->myLine.p1(),*point).length();
            if(currentDist<= minDist)
            {

                minDist         = currentDist;
                chosenIdx       = idx;
                selectingOrigin = true;
            }
        }
        idx++;
    }
    return chosenIdx;
}
// delete selected stick and all its children, if this is the master stick delete everything
void StickFigure::deleteStick(int idx)
{
    StickData *selectedStick = stickList[idx];
    if(selectedStick->view){
        selectedStick->view->RemoveImagesFromListWidget(); // stick vivo: aggiorna la UI prima di distruggerlo
        FramePresenter::DetachView(selectedStick,scene);
    }
    if(selectedStick == masterStick)
        this->masterStick = nullptr;
    //per ogni figlio dell'oggetto, rimuovi gli oggetti dalla scena e cancellali
    for(StickData * s : selectedStick->children){
        int childIndex          = stickList.indexOf(s);
        StickData *childStick   = stickList[childIndex];
        if(childStick->view)
            FramePresenter::DetachView(childStick,scene);
        stickList.removeAt(childIndex);
        StickData * parent      = selectedStick->parent;
        // rimuovi a ritroso ciascuno stick figlio di quello selezionato dalle listte figli di tutti gli altri sitck presenti genitori di esso
        while(parent != nullptr )
        {
            int removefromparentIdx = parent->children.indexOf(childStick);
            parent->children.removeAt(removefromparentIdx);
            parent = parent->parent; // update parent pointer
        }
        if(childStick->view)
            childStick->view->RemoveImagesFromListWidget(); // stick vivo: aggiorna la UI prima di distruggerlo
        delete childStick;
    }
    // epura lo stick dalla lista sticklist
    stickList.removeAll(selectedStick);
    // rimuovi lo stick selezionato dalla lista dei suoi genitori
    StickData * parent = selectedStick->parent;
    while(parent != nullptr)
    {
        int removefromparentIdx = parent->children.indexOf(selectedStick);
        parent->children.removeAt(removefromparentIdx);
        parent                  = parent->parent;
    }
    if(selectedStick->view)
        selectedStick->view->RemoveImagesFromListWidget(); // stick vivo: aggiorna la UI prima di distruggerlo
    delete selectedStick; // distruggi l'oggetto
    lineBuffer = QLineF(); // svuota linebuffer

}
// i serializzatori della classe stick sono stati spostati in stickdata.cpp (Phase 1a),
// quelli a livello figura in stickfiguredata.cpp (Phase 2b): operano solo su dati puri.
// Qui restano i wrapper per i percorsi file .stck e le funzioni ponte vivo<->dati.

/**
 * @details Snapshot copy of a live figure into pure data: sticks are cloned
 * field-by-field and the tree pointers, master and current selection are remapped by
 * index. This replaces the serialize-to-QBuffer idiom for every in-memory copy (undo,
 * copy/paste, library) — no stream, no flags.
 */
StickFigureData *ExtractStickFigureData(const StickFigure &source_figure)
{
    StickFigureData *data = new StickFigureData();
    data->version = source_figure.version;
    data->name = source_figure.name;
    for (StickData *live_stick : source_figure.stickList)
        data->stickList.append(live_stick->Clone());
    for (int i = 0; i < source_figure.stickList.count(); i++) {
        StickData *live_stick = source_figure.stickList[i];
        StickData *copied_stick = data->stickList[i];
        int parent_index = source_figure.stickList.indexOf(live_stick->parent);
        if (parent_index >= 0)
            copied_stick->parent = data->stickList[parent_index];
        for (StickData *child : live_stick->children) {
            int child_index = source_figure.stickList.indexOf(child);
            if (child_index >= 0)
                copied_stick->children.append(data->stickList[child_index]);
        }
    }
    int master_index = source_figure.stickList.indexOf(source_figure.masterStick);
    if (master_index >= 0)
        data->masterStick = data->stickList[master_index];
    int current_index = source_figure.stickList.indexOf(source_figure.currentStick);
    if (current_index >= 0)
        data->currentStick = data->stickList[current_index];
    return data;
}

/**
 * @details Rebuilds a live figure from data, replicating what the legacy operator>>
 * did in its flags-clear (live) configuration: fresh StickData hydrated via CopyFrom,
 * the tree rebuilt canonically by walking each stick's ancestor chain (children end up
 * in the flattened all-descendants form, exactly like a stream load), the master's
 * origin handle painted green (once it has a view), currentStick defaulting to the
 * first stick. Scene attach and icon rendering are explicit parameters instead of
 * global flags.
 */
void PopulateLiveStickFigure(StickFigure *target_figure, const StickFigureData &source_data,
                             bool attach_to_scene, bool update_icon)
{
    // difesa in profondità come nel vecchio operator>>: il target è di regola appena
    // costruito, ma un riuso non deve far trapelare gli stick precedenti
    qDeleteAll(target_figure->stickList);
    target_figure->stickList.clear();
    target_figure->masterStick = nullptr;
    target_figure->currentStick = nullptr;
    target_figure->version = source_data.version;
    target_figure->name = source_data.name;
    for (StickData *source_stick : source_data.stickList) {
        StickData *live_stick = source_stick->Clone();
        target_figure->stickList.append(live_stick);
    }
    for (int i = 0; i < source_data.stickList.count(); i++) {
        StickData *source_stick = source_data.stickList[i];
        StickData *live_stick = target_figure->stickList[i];
        if (source_stick->parent != nullptr) {
            // ricostruzione canonica: appendi lo stick a TUTTI i suoi antenati
            int parent_index = source_data.stickList.indexOf(source_stick->parent);
            live_stick->parent = target_figure->stickList[parent_index];
            StickData *ancestor = source_stick->parent;
            int ancestor_index = parent_index;
            while (true) {
                target_figure->stickList[ancestor_index]->children.append(live_stick);
                if (ancestor->parent == nullptr)
                    break;
                ancestor_index = source_data.stickList.indexOf(ancestor->parent);
                ancestor = ancestor->parent;
            }
        } else { // se stiamo considerando lo stickmaster
            live_stick->parent = nullptr;
            target_figure->masterStick = live_stick;
        }
        if (attach_to_scene){
            StickGraphicsItem *view = FramePresenter::AttachView(live_stick, target_figure->scene);
            if(live_stick == target_figure->masterStick){
                view->stickHandle[0]->setBrush(QBrush(Qt::green));
                view->stickHandle[0]->setZValue(101);
            }
        }
    }
    if (target_figure->stickList.count() > 0)
        target_figure->currentStick = target_figure->stickList[0];
    if (update_icon)
        target_figure->updateIcon();
}

/**
 * @details Wrapper kept for the .stck single-figure save path (saveStickFigure):
 * extract to data, stream the data. Byte layout unchanged.
 */
QDataStream & operator<< (QDataStream& stream, const StickFigure& myStickFigure){
    StickFigureData *data = ExtractStickFigureData(myStickFigure);
    stream << *data;
    delete data;
    return stream;
}

/**
 * @details Wrapper kept for the .stck single-figure load path (loadStickFigure): the
 * target is a live, on-scene figure, so the rebuilt sticks are attached and the icon
 * rendered — the behavior the legacy operator had with all flags clear.
 */
QDataStream & operator>> (QDataStream& stream,StickFigure& myStickFigure){
    StickFigureData data;
    stream >> data;
    PopulateLiveStickFigure(&myStickFigure, data, true, true);
    return stream;
}
// salva sul percorso impostato i dati serializzati dello stickfigure usando il datastream
void StickFigure::saveStickFigure(QString name)
{

    QFile file(name);
    if(!file.open(QIODevice::WriteOnly))
        return;
    QDataStream out(&file);
    out << *this;
    file.close();
}
void StickFigure::updateBoundingRects(){
    for(StickData*s:stickList){
        if(s->view)
            s->view->updateBr(0);
    }
}
// cloneStickFigure è stato eliminato (Phase 2b): i chiamanti passano da
// ExtractStickFigureData()/PopulateLiveStickFigure()
//this function join parent stick with its children
QList<StickData*> StickFigure::weld(StickData* master){
    QList<StickData*> toWeld;
    if(stickList.count()<= 1)
    {
        return toWeld;
    }
    //list of items to be deleted

    //populate the list with the immediate children of the selected stick
    for(StickData* s:master->children)
    {
        if(s->parent == master && !(s->stepchild && master==masterStick) ){
            toWeld.append(s);
        }
    }
    if(toWeld.isEmpty())
    {
        return toWeld;
    }
    // the master stick will now extend to the old extent of the welded element
    QPointF newP2(toWeld[0]->myLine.p2());
    master->myLine.setP2(newP2);
    if(master->stickType == StickData::TAPER || master->stickType == StickData::RECT || master->stickType == StickData::TRAPEZOID)
    {
        master->imgAngle= -atan2(master->myLine.dx(),master->myLine.dy())*180/M_PI;
    }
    for(StickData*s: toWeld)
    {
        //remove parentage of the welded sticks and put the new master in place
        for(StickData* c:s->children)
        {
            if(c->parent == s)
            {
                c->myLine.setP1(newP2); //also update children p1 position
                if(c->stickType == StickData::TAPER || c->stickType == StickData::RECT || c->stickType == StickData::TRAPEZOID)
                {
                    c->imgAngle= -atan2(c->myLine.dx(),c->myLine.dy())*180/M_PI;
                }
                c->parent = master;
            }
        }
        StickData* recursive = master;
        while(recursive!=nullptr)
        { //remove the deleted elements from the list of children of the hierarchy
            recursive->children.removeAll(s);
            recursive = recursive->parent;
        }
    }//delete and remove from scenevivi la vitas
    for(StickData*s: toWeld)
    {
        stickList.removeAll(s);
        if(s->view){
            s->view->RemoveImagesFromListWidget(); // stick vivo: aggiorna la UI prima di distruggerlo
            FramePresenter::DetachView(s,scene);
        }
        delete s;
    }
    refresh(0);
    return toWeld;
}
// splitta uno stick in due parti, quella originale h il ruolo di genitore e quella splittata si inserisce nella catena dei figli
StickData*  StickFigure::chop(StickData* origin){
    // l'end point era quello dell'originale e lo starting point è il centro dell'originale
    QPointF oldP2 = origin->myLine.p2();
    QPointF newP2(0.5*(origin->myLine.p2().x()+origin->myLine.p1().x()),
                 0.5*(origin->myLine.p2().y()+origin->myLine.p1().y() ));
    origin->myLine.setP2(newP2);
    StickData* toAdd = new StickData();
    cloneStick(toAdd,origin);
    toAdd->myLine = QLineF(newP2,oldP2);
    toAdd->parent = origin;
    // nel caso sia un master considera solo i figli diretti e non gli stepchild
    if(origin == masterStick){
        for(StickData*s: stickList){
            if(!s->stepchild && s!=origin){
                toAdd->children.append(s);
                if(s->parent == origin){
                    s->parent = toAdd;
                }
            }
        }
    }
    else{
        toAdd->children = origin->children;
        for(StickData*s:origin->children)
           {
            if(s->parent==origin)
                s->parent = toAdd;
        }
    }
    // inserisci lo stick aggiunto come children lungo la catena genitoriale
    StickData* recoursive = origin;
    while(recoursive != nullptr){
        recoursive->children.append(toAdd);
        recoursive = recoursive->parent;
    }
    FramePresenter::AttachView(toAdd,scene);
    stickList.append(toAdd);

    refresh(0);
    return toAdd;
}

// attraverso il percorso file, carica un data stream su cui caricare i dati salvati
// chiama il deserializzatore per stickfigures per interpretare lo stream
void StickFigure::loadStickFigure(QString name)
{

    QFile file(name);
    if(!file.open(QIODevice::ReadOnly))
        return;
    QDataStream out(&file);
    out >> *this;

    file.close();
    qDebug("load finished");

}
// rotates the whole stickfigure
extern bool startStickFigureRotation;
void StickFigure::rotateStickFigure(QPointF *coord, QList<StickGraphicsItem*> sticks, bool completeRotation){
    // traslation-only
    if(currentStick == masterStick
            &&QLineF(*coord,masterStick->myLine.p1()).length()<QLineF(*coord,masterStick->myLine.p2()).length()){
       traslate(-masterStick->myLine.p1().x()+coord->x(),-masterStick->myLine.p1().y()+coord->y());
       return;
    }
    // this records the angle between the mouse when it is first pressed and the master node of the stickfigure
    static float startingAngle = 0;
    //step 1 calcola l'angolo fra la posizione del mouse e il master node dello stickfigure
    QLineF connect;
    if(completeRotation)
        connect = QLineF(masterStick->myLine.p1(),*coord); //crea la linea connettendo i punti
    else
        connect = QLineF(currentStick->myLine.p1(),*coord); //crea la linea connettendo i punti
    float angle = connect.angle(); //dicci l'angolo
    // se la rotazione è appena iniziata sbbass ail flag e registra lo starting angle in modo che il primo valore inviato sia 0
    if(startStickFigureRotation){
        startStickFigureRotation = false;
        startingAngle = angle;
    }
    // sottrai l'angolo di inizio, la prima iterazione da sempre 0 ovviamente
    angle -= startingAngle;
    for(StickGraphicsItem *s:sticks){

        s->rotate(angle);
    }

}
extern bool startStickFigureScale;
void StickFigure::scale(QPointF *coord){
    float scale = 0;
    static float startingDist = 0;
    static QPointF startingPoint;
    if(startStickFigureScale){

        startingPoint = *coord;
        startingDist = QLineF(startingPoint,masterStick->myLine.p1()).length();
        if(startingDist<=1)
            return;
        startStickFigureScale = false;
    }
    //step 1 calcola la distanza tra la coordinata iniziale e quella corrente, se si allontana dal master stick ingrandisci, senno rimpicicolisci
    QLineF connect(masterStick->myLine.p1(),*coord); //crea la linea connettendo i punti
    float actualDistance = connect.length();
    scale = actualDistance/startingDist;
    for(StickData* s:stickList){
        if(s->view)
            s->view->scale(scale);
    }
    qDebug("scale = %f \r\n",scale);
}
void StickFigure::traslate(qreal dx, qreal dy){
    for(StickData*s:stickList){
        s->myLine.translate(dx,dy);
        if(s->view)
            s->view->refresh(1);
    }
}
void mergeStickFigures(StickFigure* mainStickFigure, StickData* mainStick,StickFigure* toJoin){
    QList<StickData*> matchIndexes;
    // first add an invisible branch that links the two
    StickData* linker = new StickData();
    linker->myLine = QLineF(mainStick->myLine.p2(),toJoin->masterStick->myLine.p1());

    mainStickFigure->stickList.append(linker);
    linker->parent = mainStick;

    linker->stickType = StickData::LINE;
    linker->Pen= Qt::NoPen;
    FramePresenter::AttachView(linker,mainStickFigure->scene);
    if(mainStick->stepchild)
        linker->stepchild = true;

    for(StickData* s: toJoin->stickList){
        StickData* toAdd = s->Clone();
        mainStickFigure->stickList.append(toAdd);
        toAdd->stepchild = false;
        matchIndexes.append(toAdd);
        StickGraphicsItem *toAddView = FramePresenter::AttachView(toAdd,mainStickFigure->scene);

        toAddView->refresh(0);
    }
    int i = 0;
    for(StickData* s: toJoin->stickList){

        StickData* toAdd = matchIndexes[i];
        if(mainStick->stepchild)
            toAdd->stepchild = true;
        if(s == toJoin->masterStick){
            toAdd->parent = linker;

        }
        else if(s->stepchild && s->parent==toJoin->masterStick)
        {

            toAdd->parent = linker;
        }
        else{
            toAdd->parent = matchIndexes[toJoin->stickList.indexOf(s->parent)];
        }
        for(StickData* c: s->children){

                toAdd->children.append(matchIndexes[toJoin->stickList.indexOf(c)]);
        }
        i++;
    }
    linker->children.append(matchIndexes[toJoin->stickList.indexOf(toJoin->masterStick)]->children);
    linker->children.append(matchIndexes[toJoin->stickList.indexOf(toJoin->masterStick)]);

    StickData * recursive;
    recursive = mainStick;
    while(1){
        if(recursive == nullptr)
            break;
        recursive->children.append(matchIndexes[toJoin->stickList.indexOf(toJoin->masterStick)]->children);
        recursive->children.append(matchIndexes[toJoin->stickList.indexOf(toJoin->masterStick)]);
        recursive->children.append(linker);
        recursive = recursive->parent;
    }
    QList<StickData*> stepChildToRemove;
    for(StickData* child:toJoin->masterStick->children){
        // solo i figli DIRETTI del vecchio master vengono davvero riparentati a linker (vedi sopra
        // "s->stepchild && s->parent->isMaster()"): un nipote stepchild più in profondità mantiene il
        // suo parent originale, quindi resta un discendente legittimo del master clonato e non va tolto
        if(child->stepchild && child->parent == toJoin->masterStick)
            stepChildToRemove.append(matchIndexes[toJoin->stickList.indexOf(child)]);

    }
    for(StickData* child:stepChildToRemove){
         matchIndexes[toJoin->stickList.indexOf(toJoin->masterStick)]->children.removeAll(child);
    }

}
// i genitori dello stick sono stepchild e invertono direzione fino al vecchio
// master stick che mantiene la stessa direzione
// la catena diretta diviene stepchild, vecchio master incluso
// mantiene le parentele e la direzione
// gli stepchild fratelli  rimangono stechild ma non cambiano direzione e mantengono la gerarchia
// gli stepchild zii diventano, per via dell'inversione
// diventano figli del figlio del vecchio genitore, per il resto mantengono la gerarchia
// i figli del nuovo master stick mantengono la gerarchia
// gli stick possono suddividersi in
//      Gerarchia sugli stick della catena che collega nuovo e vecchio master, da invertire in direzione, eccetto il vecchio master
//      Gerarchia sugli stick degli zii, che originano dalla catena genitoriale ma non ne fanno parte
//      Gerarchia sugli stick dei fratelli, che originano nello stesso punto del prossimo master
//      Figli del prossimo master che cambieranno il loro status da stepchild a non stepchild
void StickFigure::setStepChildAsMaster(StickData* toMaster){
    QList<StickData*> zii;
    QList<StickData*> catenaGenitori;
    QList<StickData*> fratelli;
    QList<StickData*> oldStepChild;
    QList<StickData*> oldDirectChain;
    QList<StickData*> newDirectChain;
    StickData* oldMaster = masterStick;
    StickData* oldParent = toMaster->parent;
    QPointF p1Buf;
    QPointF p2Buf;

    // determina la catena genitoriale del nuovo masterstick
    newDirectChain = toMaster->children;
    for(StickData* s: stickList){
        // se condivide lo stesso genitore, non è lo stesso stick da fare master e condivideva lo stesso status (stepchild o non stepchild) è fratello
        if(s->parent == toMaster->parent && s!=toMaster && s->stepchild == toMaster->stepchild){
                fratelli.append(s);
        }// se invece  contiene fra i suoi figli il nuovo master è nella catena genitoriale che dovrà essere invertita
        if(s->children.contains(toMaster)){
            catenaGenitori.append(s);
        }
        if(s->stepchild){
            oldStepChild.append(s);
        }
        else{ // tutti i non stepchild di prima
            oldDirectChain.append(s);
        }
    }
    // gli zii, sono gli stick che emergono dalla vecchia catena genitoriale (figli di uno di essi)
    for(StickData* s: stickList){
        if(catenaGenitori.contains(s->parent) && // il genitore di questo stick appartiene alla catena genitoriale vecchia
                s!=toMaster && // non deve essere lo stick che renderemo amster
                 oldStepChild.contains(s) && // deve condividere lo stesso status di stepchild dello stick da rendere master
                !fratelli.contains(s)&& // non deve essere già stato messo nella lista dei fratelli
                !catenaGenitori.contains(s)){ // non deve far parte della catena genitoriale, deve essere un figlio di essa
            zii.append(s);
        }
    }
    // forma le catene dei fratelli, ovvero la gerarchia rimane la stessa, importala nel nuovo master
    for(StickData* s:fratelli){
        s->parent = toMaster;
        toMaster->children.append(s);
        toMaster->children.append(s->children);
    }
    // inverti i genitori, prima mettili in una lista ordinata,
    // dove l'ultimo item sarà il vecchio genitore dello stick da e master
    // e il primo item sarà il vecchio master
    QList<StickData*> chainToInvert;
    StickData* recursive = toMaster->parent;
    for(StickData*s:catenaGenitori){
       Q_UNUSED(s);
       chainToInvert.insert(0,recursive);
       if(recursive!= oldMaster)
        recursive = recursive->parent;
    }// ora dal genitore del prossimo master al primo figlio del vecchio master riassegna il parent
    for(StickData*s:chainToInvert){
       if(chainToInvert.indexOf(s)+1 == chainToInvert.length())
           break; // fermati al master
       s->parent = chainToInvert[chainToInvert.indexOf(s)+1]; // quello che era prima era il figlio diretto diviene il nuovo genitore
    }
    // il genitore del genitore del nuovo master, sarà il nuovo master stesso
    toMaster->parent->parent = toMaster;
    toMaster->parent = nullptr; // il nuovo master non ha genitori
    // pulisci la gerarchia dellacatena dei genitori da invertire
    for(StickData* s: catenaGenitori){
        s->children.clear();
    }
    // nella catena da invertire scambia p1 con p2 e ricostruisci la lista dei figli
    for(StickData* s:catenaGenitori){
        if(s!=oldMaster){ // il vecchio master mantiene la stessa direzione di prima
            p2Buf = s->myLine.p2();
            s->myLine.setP2(s->myLine.p1());
            s->myLine.setP1(p2Buf);
            if(s->stickType == StickData::CIRCLE||s->stickType == StickData::RECT || s->stickType == StickData::TAPER || s->stickType == StickData::TRAPEZOID){
                if(s->stickType != StickData::CIRCLE && s->stickType != StickData::IMAGE ){
                    float buf = s->imgHScale;
                    s->imgHScale = s->imgWScale;
                    s->imgWScale= buf;
                }

                s->imgAngle = s->imgAngle-180;
            }
        }
        StickData* recursive = s->parent; // segna lo stick corrente come figlio di tutti i suoi genitori
        int n = 0;
        while(recursive != nullptr && recursive != oldMaster){
            recursive->children.append(s);
            recursive = recursive->parent;
            n++;
            if(n>stickList.count()){
                qDebug("error while inverting");
                return;
            }
        }
    }
    // importa il vecchio albero genialogico diretto fra i figli della catena genitoriale del nuovo master inverita e aggiornata
    oldDirectChain.removeAll(oldMaster);
    // togli anche il resto della catena genitoriale stessa: senza questo ogni suo membro finisce per
    // elencare se stesso e i propri (ex) antenati come propri discendenti (voci auto-referenziali/cicliche
    // in children), dato che catenaGenitori è già interamente contenuta in oldDirectChain
    for(StickData* s: catenaGenitori){
        oldDirectChain.removeAll(s);
    }
    for(StickData* s:catenaGenitori){
        s->children.append(oldDirectChain);
    }
    // al vecchio master imposta come figli solo quelli che prima erano il suo albero genialogico diretto (non stepchild)
    oldMaster->children.clear();
    oldMaster->children = oldDirectChain;
    // importa il vecchio albero genialogico diretto fra i figli del nuovo master
    toMaster->children.append(oldMaster->children);

    // aggiungi alla catena degli zii al ramo catena genitori invertita
    // ma ora che la catena è invertita, il genitore dello zio sarà quello che prima era il "nonno"
    for(StickData* s:zii){
            s->parent = s->parent->parent;
            StickData* recursive = s->parent;
            while(recursive){ // trasporta la nuova parentela a ritroso lungo la catena
                recursive->children.append(s);
                recursive->children.append(s->children);
                recursive = recursive->parent;

            }
    }
    // tutto è stepchild fuorchè i figli diretti del nuovo master
    for(StickData*s:stickList){
        if(s==toMaster || newDirectChain.contains(s)){
            s->stepchild = false;
        }
        else{
            s->stepchild = true;
        }
    }
    //riassegna il ruolo del master
    masterStick = toMaster;

    refresh(0);
}
// rendi uno stick, che non era uno stepchild, nuovo master dello stickfigure
// gli stick possono raggrupparsi in:
// Fratelli del nuovo master che ne condividevano il genitore
// appartenenti alla catena che collega il nuovo master col vecchio, la cui direzione va invertita
// stick che nascevano nella catena sopracitata ma che generavano gerarchie parallele
// stick che prima erano stepchild, la cui gerarchia verrà unita con la catena genitoriale una volta invertita
void StickFigure::setDirectChainAsMaster(StickData* toMaster){
    QList<StickData*> zii;
    QList<StickData*> catenaGenitori;
    QList<StickData*> fratelli;
    QList<StickData*> oldStepChild;
    StickData* oldMaster = masterStick;
    QPointF p2Buf;
    for(StickData* s: stickList){
        // la catena genitoriale sono quegli stick che collegano il prossimo master al vecchio master (incluso)
        if(s->children.contains(toMaster)){
            catenaGenitori.append(s);
        }
        // i fratelli sono gli stick che condividono la paternità col prossimo master
        else if(s->parent == toMaster->parent && s!=toMaster){
            if(!(s->parent == oldMaster && s->stepchild != toMaster->stepchild))
                fratelli.append(s);
        }
    }
    for(StickData* s:stickList){
        if(s->stepchild){
            oldStepChild.append(s);
            s->stepchild = false;
        }
    }
    // gli zii, sono stick che derivano dalla catena genitoriale prima calcolata ma non ne fanno parte, non devono essere stepchild
    for(StickData* s: stickList){
        if(catenaGenitori.contains(s->parent) &&
                s!=toMaster &&
                !catenaGenitori.contains(s) &&
                !fratelli.contains(s)&&
                !(oldStepChild.contains(s))){
            zii.append(s);
        }
    }
    // aggiorna lo status
    toMaster->stepchild = false;

    // la catena genitoriale va invertita, incluso il vecchio master si scambiano p1 con p2
    // e il primo figlio di ciascuno stick diventa il genitore di quello che prima era suo genitore
    QList<StickData*> catenaDaInvertire;
    StickData* iterator = toMaster->parent;
    for(StickData*s: catenaGenitori) {
        Q_UNUSED(s);
        catenaDaInvertire.insert(0,iterator);
        if(iterator != oldMaster)
            iterator = iterator->parent;
    }
    for(StickData*s: catenaGenitori) {
        p2Buf = s->myLine.p2();
        s->myLine.setP2(s->myLine.p1());
        s->myLine.setP1(p2Buf);
        if( s->stickType == StickData::CIRCLE||s->stickType == StickData::RECT || s->stickType == StickData::TAPER || s->stickType == StickData::TRAPEZOID){
            s->imgAngle = s->imgAngle-180;
            if(s->stickType != StickData::CIRCLE && s->stickType != StickData::IMAGE ){
                float buf = s->imgHScale;
                s->imgHScale = s->imgWScale;
                s->imgWScale= buf;
            }

        }
        // pulisci i figli dello stick, la catena va ricostruita
        s->children.clear(); // attenzione il master così si perde anch gli stepchild che non vengono ricostruiti al for successivo
    }
    toMaster->parent->parent = toMaster;
    for(StickData*s:catenaDaInvertire){
       if(catenaDaInvertire.indexOf(s)+1==catenaDaInvertire.length())
           break; // fermati al master
       s->parent = catenaDaInvertire[catenaDaInvertire.indexOf(s)+1]; // quello che era prima era il figlio diretto diviene il nuovo genitore
    }

    toMaster->parent = nullptr;
    // ricostruisci le liste dei figli ora che la catena è stata invertita
    for(StickData*s: catenaGenitori) {
        StickData * recursive = s->parent;
        s->stepchild = true;
        if(oldStepChild.contains(s))
                oldStepChild.removeAll(s);
        int n = 0;
        while(recursive){ // trasporta verso le gerarchie più alte, che puntano al nuovo master lo stick corrente come figlio
            recursive->children.append(s);
            recursive = recursive->parent;
            n++;
            if(n>stickList.count()){
                qDebug("error while inverting");
                return;
            }
        }
    }
    // i fratelli diventano stepchild ma la gerarchia non cambia
    for(StickData* s:fratelli){
        if(oldStepChild.contains(s))
                oldStepChild.removeAll(s);
        s->stepchild = true;
        if(s != oldMaster){
            for(StickData* c:s->children){
                if(oldStepChild.contains(c))
                        oldStepChild.removeAll(c);
                c->stepchild = true;

            }
            toMaster->children.append(s->children);
        }
        toMaster->children.append(s);
        s->parent = toMaster;

    }
    // gli zii ereditano la gerarchia, ma il loro pdre diviene quello che prima era il padre del padre per via dell'inversione
    // della catena genitoriale
    for(StickData* s:zii){
        if(oldStepChild.contains(s))
                oldStepChild.removeAll(s);
        s->stepchild = true;
        StickData* recursive;
        s->parent = s->parent->parent; // occhio conseguenza dell'inversione della catena genitoriale
        recursive = s->parent;
        while(recursive){ // trasporta la gerarchia verso il nuovo master lungo la catena
            recursive->children.append(s->children);
            recursive->children.append(s);
            recursive = recursive->parent;
        }
        for(StickData* c:s->children){
            if(oldStepChild.contains(c))
                    oldStepChild.removeAll(c);
            c->stepchild = true;
        }
    }
    // quelli che prima erano stepchild, rimangono tali, ma con l'inversione della catena genitoriale,
    // la catena dei vecchi stepchild è contigua con la catena del vecchio master ora
    // trasporta la gerarchia a ritroso per quella strada verso il nuovo master partendo dal vecchio master
    for(StickData*s:oldStepChild){
        s->stepchild = true;
        StickData* recursive = oldMaster;
        while(recursive && !recursive->children.contains(s)){
            recursive->children.append(s);
            recursive = recursive->parent;
        }
    }
    toMaster->stepchild = false;
    masterStick = toMaster;
    refresh(0);
}
// questo algoritmo serve a cambiare l'estremità in cui vi è il punto di origine di un master con un estremo libero
//  - lo stick master non cambia, ma p1 e p2 si scambiano tra loro
//  - quelli che prima erano stepchild diventano catena diretta
//  - altrimenti se vi era soloc atena diretta, origine verso l'esterno, spostaimo l'origine verso l'interno e tutto diventa stecphildren
void StickFigure::invertMaster(StickData* toMaster){
    QPointF p2Buf;

    // inverti p1 con p2 per cambiare la direzione
    p2Buf = toMaster->myLine.p2();
    toMaster->myLine.setP2(toMaster->myLine.p1());
    toMaster->myLine.setP1(p2Buf);
    if(toMaster->stickType == StickData::RECT ||toMaster->stickType == StickData::CIRCLE|| toMaster->stickType == StickData::TAPER || toMaster->stickType == StickData::TRAPEZOID)
      {  toMaster->imgAngle = toMaster->imgAngle-180;
         if(toMaster->stickType != StickData::CIRCLE && toMaster->stickType != StickData::IMAGE ){
             float buf = toMaster->imgHScale;
             toMaster->imgHScale = toMaster->imgWScale;
             toMaster->imgWScale= buf;
         }

    }
    // qui si capisce quale inversione occorre compiere,
    // si parte con l'estremo di origine senza stepchild
    // o si parte con un estremo di origine da cui partono stepchild?
    QList<StickData*> figli;
    QList<StickData*> figliastri;
    for(StickData*s:stickList){
        if(s->parent == masterStick && s->stepchild){
            figliastri.append(s);
        }
        else if(s->parent == masterStick && !s->stepchild)
            figli.append(s);
    }
    // se avevamo solo stepchildren, allora l'inversione consiste nel rendere tutto catena diretta e l'origine si sposta verso l' esterno
    if(figli.isEmpty()){
        for(StickData*s: stickList){
            s->stepchild = false;
        }
    }
    else{ // altrimenti l'origine si sposta verso l'interno, le catene dirette diventano stepchildren
        for(StickData*s: stickList){
            s->stepchild = true;
        }
        toMaster->stepchild = false;
    }
    // aggiorna lo status di master..ma non serve neanche
    masterStick = toMaster;
    toMaster->stepchild = false;
    refresh(0);
}
// questo algoritmo semplificato rende master uno stick che correntemente è
// uno stepchild diretto del master attuale, in pratica
//  - La catena diretta dle master vecchio viene preservata, il nuovo master diventa genitore del master vecchio
//  - i fratelli del master vecchio acquisiscono come genitore il nuovo master e perdono lo status di stepchild
//  - il nuovo master acquisice stato di master, perde quello di stepchild,, il genitore diventa il nullptr
//      inoltre si invertono p1 e p2
void StickFigure::stepDownMaster(StickData* toMaster){
    StickData* oldMaster = masterStick;

    for(StickData*s: stickList){
        if(s->stepchild){
            oldMaster->children.removeAll(s);
        }
    }
    // tutti gli stick direttamente figli del vecchio master, se erano stepchild (quindi fratelli del vecchio master) cambiano parent
    // e perdono lo status id stepchild (anche i loro figli)
    for(StickData* s: stickList){
        if(s->parent == oldMaster){
            if(s->stepchild && s!=toMaster){
                s->stepchild = false;
                for(StickData* c:s->children){
                    c->stepchild = false;
                }
                s->parent = toMaster;
            }
        }
    }
    oldMaster->parent = toMaster;
    // inverti la direzione del nuovo master
    QPointF p2Buf = toMaster->myLine.p2();
    toMaster->myLine.setP2(toMaster->myLine.p1());
    toMaster->myLine.setP1(p2Buf);
    if(toMaster->stickType == StickData::RECT ||toMaster->stickType == StickData::CIRCLE||toMaster->stickType == StickData::TAPER || toMaster->stickType == StickData::TRAPEZOID)
        if(toMaster->stickType != StickData::CIRCLE && toMaster->stickType != StickData::IMAGE ){
            float buf = toMaster->imgHScale;
            toMaster->imgHScale = toMaster->imgWScale;
            toMaster->imgWScale= buf;
        }
    // il nuovo master ha come children tutti gli stick tranne se stesso
    toMaster->children.clear();
    for(StickData* s: stickList)
        toMaster->children.append(s);
    toMaster->children.removeAll(toMaster);

    masterStick = toMaster;
    toMaster->stepchild = false;
    toMaster->parent = nullptr;

    refresh(0);
}
// sets new endpoint of the stick and traslates all its children
void StickFigure::elongate(QPointF newEndPoint, StickData* myStick){
    if(!stickList.contains(myStick))
        return;
    float dx = newEndPoint.x()-myStick->myLine.p2().x();
    float dy = newEndPoint.y()-myStick->myLine.p2().y();
    myStick->myLine.setP2(newEndPoint);
    for(StickData*s: myStick->children){
        if(!(myStick==masterStick && s->stepchild)){
            s->myLine.translate(dx,dy);

        }
    }
    if(myStick->stickType == StickData::IMAGE || myStick->stickType  == StickData::RECT
            || myStick->stickType  == StickData::TRAPEZOID || myStick->stickType  == StickData::TAPER){
        myStick->imgAngle= -atan2(myStick->myLine.dx(),myStick->myLine.dy())*180/M_PI;
    }
    refresh(0);

}
// seleziona quale algoritmo usare a seconda dello stick che deve diventare master
void StickFigure::setMaster(StickData* toMaster){
    QList<StickData*> figli;
    QList<StickData*> figliastri;
    if(masterStick->view){
        masterStick->view->stickHandle[0]->setBrush(QBrush(Qt::red));
        masterStick->view->stickHandle[0]->setZValue(100);
    }
    //prima, considerando il master, vedi quanti sick nascono dal suo punto di origine (figliastri) e quanti nascono dal suo estremo libero (figli)
    for(StickData*s:stickList){
        if(s->parent == masterStick && s->stepchild){
            figliastri.append(s);
        }
        else if(s->parent == masterStick && !s->stepchild)
            figli.append(s);
    }
    // se siamo nella condizione di avere un estremo senza appendici e lo stick è già master, effettua una inversione
    if( toMaster == masterStick && (figli.isEmpty() || figliastri.isEmpty())){
        invertMaster(toMaster);
    }
    // se lo stick era uno stepchild che nasce direttamente dall'origine, allora sposta tutta la gerarchia un passo verso il basso
    else if(figliastri.contains(toMaster)){
        stepDownMaster(toMaster);
    }
    else if(toMaster == masterStick)
        return;
    else if(toMaster->stepchild) // master scelto fa parte dei non stepchild
        setStepChildAsMaster(toMaster);
    else
        setDirectChainAsMaster(toMaster); // masterscelto fa parte dei vecchi stepchild
    if(masterStick->view){
        masterStick->view->stickHandle[0]->setBrush(QBrush(Qt::green));
        masterStick->view->stickHandle[0]->setZValue(101);
    }
}
void splitStickFigures(StickFigure* split, StickData* origin,StickFigure* branch){
    // crea una lista parallela con gli stick corrispondenti nella stickfigure originale
   QList<StickData*> matchingIndex;
   // clonare tutti gli stick figli dello stick selezionato
    matchingIndex.append(origin);
    StickData* toAdd = origin->Clone();
    branch->stickList.append(toAdd);

    for(StickData* s: origin->children)
    {
        if(s->parent != origin->parent)
        {
            matchingIndex.append(s);
            toAdd = s->Clone();
            branch->stickList.append(toAdd);
        }
    }

    branch->masterStick = branch->stickList[matchingIndex.indexOf(origin)];

    int i = 0;
    // in un altro ciclo for, per ogni stick clonato aggiungere gli stessi parent e gli stessi children sfruttando l'indicizzazione
    for(StickData* s: branch->stickList){
        int parentIdx = matchingIndex.indexOf(matchingIndex[i]->parent);
        if(matchingIndex[i]->parent != origin->parent && parentIdx != -1)
            s->parent = branch->stickList[parentIdx];
        for(StickData*c: matchingIndex[i]->children){
            int childIdx = matchingIndex.indexOf(c);
            if(childIdx != -1) // c non fa parte del branch (es. voce spuria da una gerarchia children non pulita)
                s->children.append(branch->stickList[childIdx]);
        }
        i++;
        StickGraphicsItem *view = FramePresenter::AttachView(s,split->scene);
        view->refresh(0);
    }

    split->deleteStick( split->stickList.indexOf(origin));
}
float StickFigure::minZ(){
    if(stickList.isEmpty()){
        return 0;
    }
    float minVal = stickList[0]->Z;
    for(StickData*s:stickList){
        if(s->Z<minVal)
            minVal = s->Z;
    }
    return minVal;
}
float StickFigure::maxZ(){
    if(stickList.isEmpty()){
        return 0;
    }
    float maxVal = stickList[0]->Z;
    for(StickData*s:stickList){
        if(s->Z>maxVal)
            maxVal = s->Z;
    }
    return maxVal;
}
/**
 * @details Used by chop() to duplicate a stick being split in two. Historically this
 * serialized to a QBuffer and deserialized back; since Phase 1a it is a plain deep
 * field copy via StickData::CopyFrom().
 */
void cloneStick(StickData* dest, StickData*src){
    dest->CopyFrom(*src);
}
