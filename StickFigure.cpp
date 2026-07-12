#include "StickFigure.h"
#include "framepresenter.h"

#include <QtAlgorithms>
#include <QDebug>
#include <QFile>
#include <QVector2D>
// StickFigure work in the one active scene
extern QGraphicsRectItem* my_rect;
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
    for(StickData* s: stick_list)
    {
        if(s->view){
            s->view->RemoveImagesFromListWidget();
            FramePresenter::DetachView(s,scene);
        }
        delete s;
    }
    delete icon_img;
    delete stick_figure_icon;
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
void StickFigure::UpdateIcon()
{
    //crea una scena fittizia, falla bianca e delle stesse dimensioni di quella principale
    QGraphicsScene icoScene;
    icon_img->fill(Qt::white);

    icoScene.setSceneRect(scene->sceneRect());
    icoScene.setBackgroundBrush(QBrush(QColor(Qt::white)));
    // ora clona tutti gli stick nello stickfigure e aggiungili alla scena fittizia
    // (addItem rende la scena proprietaria dei cloni: non vanno cancellati a mano,
    // altrimenti verrebbero distrutti due volte quando icoScene esce di scope)
    QList<StickData*> renderClones;
    for(StickData* S: stick_list){
        StickData* clone = S->Clone();
        renderClones.append(clone);
        icoScene.addItem(new StickGraphicsItem(clone));
    }
    // crea un painter che disegni sull'immagine
    QPainter painter(icon_img);
    // renderizza dalla scena sull'immagine mediante il painter
    icoScene.render(&painter);
    // imposta l'iconea
    delete stick_figure_icon;
    stick_figure_icon = new QIcon(*icon_img);
    linked_item->setIcon(*stick_figure_icon);
    // libera prima le view (icoScene le possiede) poi i dati clonati (view non owning)
    icoScene.clear();
    qDeleteAll(renderClones);
}
StickFigure::StickFigure(QListWidgetItem* item)
{
    p0          =  QPointF(0,0);
    p1          =  QPointF(0,0);
    line_buffer  =  QLineF(p0,p1);
    draw_count   =  0;
    master_stick =   nullptr;
    current_stick = nullptr;
    linked_item  = item;
    stick_figure_icon = new QIcon();
    icon_img         = new QPixmap(50,50);
    master_stick = nullptr;
}
StickFigure::StickFigure()
{
    p0          =  QPointF(0,0);
    p1          =  QPointF(0,0);
    line_buffer  =  QLineF(p0,p1);
    draw_count   =  0;
    master_stick =   nullptr;
    current_stick = nullptr;
    stick_figure_icon = new QIcon();
    icon_img         = new QPixmap(50,50);
    master_stick = nullptr;
}
/**
 * @details Since Phase 2c only builds the StickData for the stick being drawn — no
 * view exists yet. The caller (MyView::DrawCmd()) attaches one via
 * FramePresenter::AttachView() right after this returns, then paints the master
 * handle green if this turned out to be the figure's first stick (moved out of here:
 * doing it required a view, which didn't exist yet at this point).
 */
void StickFigure::StartDrawing(QPointF *point, QPen pen, QBrush brush)
{
    if(stick_list.isEmpty()) // se è il primo stick dello stickfigure
    {
        p0 = *point;
    }
    else //se ci sono altri stick in questo stickfigure i dalla estremità più vicinaz al click
    {
        parent_buffer = stick_list[SelectOrigin(point)]; //seleziona genitore (potremmo già farlo al rigo precedente visto che la funzione viene chiamata)
        if(selecting_origin)
            p0 = parent_buffer->my_line.p1();
        else
            p0 = parent_buffer->my_line.p2();
    }

    //crea e alloca l'oggetto stick associato
    line_buffer      = QLineF(p0,p0); // all'inizio la linea ha lunghezza zero
    StickData *stk  = new StickData();
    stk->my_line     = line_buffer;
    //assegna l'ordine di profondità
    stk->z = base_z+stick_list.count()/100;
    stick_buffer     = stk;
    current_stick    = stk;
    // seè il primo stick ad essere disegnato
    if(this->master_stick == nullptr){
        master_stick = current_stick;
        master_stick->parent = nullptr;
    }
    draw_count       = 1; // segnala che un disegno è in atto
    stick_buffer->pen = pen;
    stick_buffer->brush = brush;
}
// aggiorna uno stick il cui disegno è già avviato
void StickFigure::PreviewDrawing(QPointF *point)
{
    SetLineFromPoint(point);
    if(stick_buffer->view)
        stick_buffer->view->Refresh(1);
    if(stick_buffer->stick_type == StickData::IMAGE ||
            stick_buffer->stick_type == StickData::RECT||
            (stick_buffer->stick_type == StickData::TRAPEZOID)||
            (stick_buffer->stick_type == StickData::CIRCLE)||
            (stick_buffer->stick_type == StickData::TAPER)){
        stick_buffer->img_angle= -atan2(stick_buffer->my_line.dx(),stick_buffer->my_line.dy())*180/M_PI;
    }
}
void StickFigure::Refresh(int mode)
{
    for(StickData* s:stick_list){
        if(s->view)
            s->view->Refresh(mode);
    }
}


void StickFigure::SetLineFromPoint(QPointF *point)
{
    p1              = *point;
    line_buffer      = QLineF(p0,p1);
    // lo stick si fonda sulla linea che connette starting point e endpoint
    stick_buffer->my_line = line_buffer;
}

void StickFigure::Highlight(bool setting)
{
    for(StickData* s:this->stick_list)
    {
        if(!s->view)
            continue;
        s->view->highlight = setting;

        if(!stick_list.isEmpty() && setting == false){
            s->view->selected = false;
        }
    }

}
void StickFigure::UpdateZ(float value){

    for(StickData* S : stick_list){
        float newVal =  S->z-base_z+value;
        if(newVal >MAXDEPTH)
            newVal = MAXDEPTH;
        else if(newVal<MINDEPTH)
            newVal = MINDEPTH;
        S->z = newVal;
        if(S->view)
            S->view->setZValue(S->z);
    }
    base_z = value;
}

void StickFigure::EndDrawing(QPointF *point)
{
    SetLineFromPoint(point);
    draw_count = 0;                  //segnala la fine
    stick_list.append(stick_buffer);  //disegno finito, aggiungilo alla lista
    // assegna l'elemento padre
    stick_buffer->parent = parent_buffer;
    if(stick_buffer->view)
        stick_buffer->view->Refresh(0);
    // INSERISCILO COME FIGLIO DI TUTTA LA GERARCHIA PRECEDENTE
    while(parent_buffer){
        parent_buffer->children.append(stick_buffer);
        if(selecting_origin || parent_buffer->stepchild){ // children of a stepchild is also a stepchild!
            stick_buffer->stepchild = true;
        }
        parent_buffer = parent_buffer->parent;
    }
    if(stick_buffer->stick_type == StickData::IMAGE && stick_buffer->view){
        stick_buffer->view->img_rect = CalcImgRect(stick_buffer->my_line,stick_buffer->stick_img->size());
    }
    scene->clearSelection();
    if(stick_buffer->view)
        stick_buffer->view->setSelected(true);
}
void StickFigure::CancelDrawing()
{
    scene->clearSelection();
    if(stick_buffer==master_stick){
        master_stick = nullptr;
        current_stick = nullptr;
    }
    else{
        current_stick = parent_buffer;
    }
    if(stick_buffer->view){
        stick_buffer->view->RemoveImagesFromListWidget(); // stick vivo (già mostrato durante il disegno)
        FramePresenter::DetachView(stick_buffer,scene);
    }
    delete stick_buffer;
    line_buffer  = QLineF();
    draw_count   = 0;
    if(!stick_list.isEmpty() && current_stick){
        current_stick->view->selected = true; // qui current_stick era già sulla scena (non è quello appena scartato)
        current_stick->view->Refresh(0);
    }
}

//scegli come origine lo stick più vicino al mouse fra gli stick dello stickfigure
int StickFigure::SelectOrigin( QPointF * point)
{
    //seleziona lo stick più vicino
    int idx = SelectStick(point);
        //controlla quale estremità è la più vicina e aseconda di quale essa sia restituisci il punto
        if(MeasureDistanceFromLine(*point,stick_list[idx]->my_line) >
                QLineF(stick_list[idx]->my_line.p1(),*point).length())
        {
            // il p1 del masterstick è un eccezione e va considerata come possibile origine
            selecting_origin = true;
            idx = stick_list.indexOf(master_stick);

        }
        else{
            selecting_origin = false;
        }
     return idx;
}
float StickFigure::MeasureDistanceFromLine(QPointF p, QLineF l){
    float dist = QLineF(l.p2(),p).length();
    return dist;

}
int StickFigure::SelectStick( QPointF * point)
{
    int idx         = 0;
    float  minDist  = 0;
    float currentDist = 0;
    int chosenIdx = stick_list.indexOf(master_stick);
    //inizializziamo a partire dall'origine del master_stick
    minDist = MeasureDistanceFromLine(*point,stick_list[0]->my_line);
    while(idx< stick_list.length())
    {
        // i punti inizio-fine dello stick corrente
        currentDist = MeasureDistanceFromLine(*point,stick_list[idx]->my_line);
        // se la distanza è più bassa di quella minima registrata dal luogo del mouse
        if(currentDist<= minDist)
        {

            minDist         = currentDist;
            chosenIdx       = idx;
            selecting_origin = false;
        }
        if(stick_list[idx] == master_stick){
            currentDist =QLineF(stick_list[idx]->my_line.p1(),*point).length();
            if(currentDist<= minDist)
            {

                minDist         = currentDist;
                chosenIdx       = idx;
                selecting_origin = true;
            }
        }
        idx++;
    }
    return chosenIdx;
}
// delete selected stick and all its children, if this is the master stick delete everything
void StickFigure::DeleteStick(int idx)
{
    StickData *selectedStick = stick_list[idx];
    if(selectedStick->view){
        selectedStick->view->RemoveImagesFromListWidget(); // stick vivo: aggiorna la UI prima di distruggerlo
        FramePresenter::DetachView(selectedStick,scene);
    }
    if(selectedStick == master_stick)
        this->master_stick = nullptr;
    //per ogni figlio dell'oggetto, rimuovi gli oggetti dalla scena e cancellali
    for(StickData * s : selectedStick->children){
        int childIndex          = stick_list.indexOf(s);
        StickData *childStick   = stick_list[childIndex];
        if(childStick->view)
            FramePresenter::DetachView(childStick,scene);
        stick_list.removeAt(childIndex);
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
    stick_list.removeAll(selectedStick);
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
    line_buffer = QLineF(); // svuota linebuffer

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
    for (StickData *live_stick : source_figure.stick_list)
        data->stick_list.append(live_stick->Clone());
    for (int i = 0; i < source_figure.stick_list.count(); i++) {
        StickData *live_stick = source_figure.stick_list[i];
        StickData *copied_stick = data->stick_list[i];
        int parent_index = source_figure.stick_list.indexOf(live_stick->parent);
        if (parent_index >= 0)
            copied_stick->parent = data->stick_list[parent_index];
        for (StickData *child : live_stick->children) {
            int child_index = source_figure.stick_list.indexOf(child);
            if (child_index >= 0)
                copied_stick->children.append(data->stick_list[child_index]);
        }
    }
    int master_index = source_figure.stick_list.indexOf(source_figure.master_stick);
    if (master_index >= 0)
        data->master_stick = data->stick_list[master_index];
    int current_index = source_figure.stick_list.indexOf(source_figure.current_stick);
    if (current_index >= 0)
        data->current_stick = data->stick_list[current_index];
    return data;
}

/**
 * @details Rebuilds a live figure from data, replicating what the legacy operator>>
 * did in its flags-clear (live) configuration: fresh StickData hydrated via CopyFrom,
 * the tree rebuilt canonically by walking each stick's ancestor chain (children end up
 * in the flattened all-descendants form, exactly like a stream load), the master's
 * origin handle painted green (once it has a view), current_stick defaulting to the
 * first stick. Scene attach and icon rendering are explicit parameters instead of
 * global flags.
 */
void PopulateLiveStickFigure(StickFigure *target_figure, const StickFigureData &source_data,
                             bool attach_to_scene, bool update_icon)
{
    // difesa in profondità come nel vecchio operator>>: il target è di regola appena
    // costruito, ma un riuso non deve far trapelare gli stick precedenti
    qDeleteAll(target_figure->stick_list);
    target_figure->stick_list.clear();
    target_figure->master_stick = nullptr;
    target_figure->current_stick = nullptr;
    target_figure->version = source_data.version;
    target_figure->name = source_data.name;
    for (StickData *source_stick : source_data.stick_list) {
        StickData *live_stick = source_stick->Clone();
        target_figure->stick_list.append(live_stick);
    }
    for (int i = 0; i < source_data.stick_list.count(); i++) {
        StickData *source_stick = source_data.stick_list[i];
        StickData *live_stick = target_figure->stick_list[i];
        if (source_stick->parent != nullptr) {
            // ricostruzione canonica: appendi lo stick a TUTTI i suoi antenati
            int parent_index = source_data.stick_list.indexOf(source_stick->parent);
            live_stick->parent = target_figure->stick_list[parent_index];
            StickData *ancestor = source_stick->parent;
            int ancestor_index = parent_index;
            while (true) {
                target_figure->stick_list[ancestor_index]->children.append(live_stick);
                if (ancestor->parent == nullptr)
                    break;
                ancestor_index = source_data.stick_list.indexOf(ancestor->parent);
                ancestor = ancestor->parent;
            }
        } else { // se stiamo considerando lo stickmaster
            live_stick->parent = nullptr;
            target_figure->master_stick = live_stick;
        }
        if (attach_to_scene){
            StickGraphicsItem *view = FramePresenter::AttachView(live_stick, target_figure->scene);
            if(live_stick == target_figure->master_stick){
                view->stick_handle[0]->setBrush(QBrush(Qt::green));
                view->stick_handle[0]->setZValue(101);
            }
        }
    }
    if (target_figure->stick_list.count() > 0)
        target_figure->current_stick = target_figure->stick_list[0];
    if (update_icon)
        target_figure->UpdateIcon();
}

/**
 * @details Wrapper kept for the .stck single-figure save path (SaveStickFigure):
 * extract to data, stream the data. Byte layout unchanged.
 */
QDataStream & operator<< (QDataStream& stream, const StickFigure& my_stick_figure){
    StickFigureData *data = ExtractStickFigureData(my_stick_figure);
    stream << *data;
    delete data;
    return stream;
}

/**
 * @details Wrapper kept for the .stck single-figure load path (LoadStickFigure): the
 * target is a live, on-scene figure, so the rebuilt sticks are attached and the icon
 * rendered — the behavior the legacy operator had with all flags clear.
 */
QDataStream & operator>> (QDataStream& stream,StickFigure& my_stick_figure){
    StickFigureData data;
    stream >> data;
    PopulateLiveStickFigure(&my_stick_figure, data, true, true);
    return stream;
}
// salva sul percorso impostato i dati serializzati dello stickfigure usando il datastream
void StickFigure::SaveStickFigure(QString name)
{

    QFile file(name);
    if(!file.open(QIODevice::WriteOnly))
        return;
    QDataStream out(&file);
    out << *this;
    file.close();
}
void StickFigure::UpdateBoundingRects(){
    for(StickData*s:stick_list){
        if(s->view)
            s->view->UpdateBr(0);
    }
}
// cloneStickFigure è stato eliminato (Phase 2b): i chiamanti passano da
// ExtractStickFigureData()/PopulateLiveStickFigure()
//this function join parent stick with its children
QList<StickData*> StickFigure::Weld(StickData* master){
    QList<StickData*> toWeld;
    if(stick_list.count()<= 1)
    {
        return toWeld;
    }
    //list of items to be deleted

    //populate the list with the immediate children of the selected stick
    for(StickData* s:master->children)
    {
        if(s->parent == master && !(s->stepchild && master==master_stick) ){
            toWeld.append(s);
        }
    }
    if(toWeld.isEmpty())
    {
        return toWeld;
    }
    // the master stick will now extend to the old extent of the welded element
    QPointF newP2(toWeld[0]->my_line.p2());
    master->my_line.setP2(newP2);
    if(master->stick_type == StickData::TAPER || master->stick_type == StickData::RECT || master->stick_type == StickData::TRAPEZOID)
    {
        master->img_angle= -atan2(master->my_line.dx(),master->my_line.dy())*180/M_PI;
    }
    for(StickData*s: toWeld)
    {
        //remove parentage of the welded sticks and put the new master in place
        for(StickData* c:s->children)
        {
            if(c->parent == s)
            {
                c->my_line.setP1(newP2); //also update children p1 position
                if(c->stick_type == StickData::TAPER || c->stick_type == StickData::RECT || c->stick_type == StickData::TRAPEZOID)
                {
                    c->img_angle= -atan2(c->my_line.dx(),c->my_line.dy())*180/M_PI;
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
        stick_list.removeAll(s);
        if(s->view){
            s->view->RemoveImagesFromListWidget(); // stick vivo: aggiorna la UI prima di distruggerlo
            FramePresenter::DetachView(s,scene);
        }
        delete s;
    }
    Refresh(0);
    return toWeld;
}
// splitta uno stick in due parti, quella originale h il ruolo di genitore e quella splittata si inserisce nella catena dei figli
StickData*  StickFigure::Chop(StickData* origin){
    // l'end point era quello dell'originale e lo starting point è il centro dell'originale
    QPointF oldP2 = origin->my_line.p2();
    QPointF newP2(0.5*(origin->my_line.p2().x()+origin->my_line.p1().x()),
                 0.5*(origin->my_line.p2().y()+origin->my_line.p1().y() ));
    origin->my_line.setP2(newP2);
    StickData* toAdd = new StickData();
    CloneStick(toAdd,origin);
    toAdd->my_line = QLineF(newP2,oldP2);
    toAdd->parent = origin;
    // nel caso sia un master considera solo i figli diretti e non gli stepchild
    if(origin == master_stick){
        for(StickData*s: stick_list){
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
    stick_list.append(toAdd);

    Refresh(0);
    return toAdd;
}

// attraverso il percorso file, carica un data stream su cui caricare i dati salvati
// chiama il deserializzatore per stickfigures per interpretare lo stream
void StickFigure::LoadStickFigure(QString name)
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
extern bool start_stick_figure_rotation;
void StickFigure::RotateStickFigure(QPointF *coord, QList<StickGraphicsItem*> sticks, bool complete_rotation){
    // traslation-only
    if(current_stick == master_stick
            &&QLineF(*coord,master_stick->my_line.p1()).length()<QLineF(*coord,master_stick->my_line.p2()).length()){
       Traslate(-master_stick->my_line.p1().x()+coord->x(),-master_stick->my_line.p1().y()+coord->y());
       return;
    }
    // this records the angle between the mouse when it is first pressed and the master node of the stickfigure
    static float startingAngle = 0;
    //step 1 calcola l'angolo fra la posizione del mouse e il master node dello stickfigure
    QLineF connect;
    if(complete_rotation)
        connect = QLineF(master_stick->my_line.p1(),*coord); //crea la linea connettendo i punti
    else
        connect = QLineF(current_stick->my_line.p1(),*coord); //crea la linea connettendo i punti
    float angle = connect.angle(); //dicci l'angolo
    // se la rotazione è appena iniziata sbbass ail flag e registra lo starting angle in modo che il primo valore inviato sia 0
    if(start_stick_figure_rotation){
        start_stick_figure_rotation = false;
        startingAngle = angle;
    }
    // sottrai l'angolo di inizio, la prima iterazione da sempre 0 ovviamente
    angle -= startingAngle;
    for(StickGraphicsItem *s:sticks){

        s->Rotate(angle);
    }

}
extern bool start_stick_figure_scale;
void StickFigure::Scale(QPointF *coord){
    float scale = 0;
    static float startingDist = 0;
    static QPointF starting_point;
    if(start_stick_figure_scale){

        starting_point = *coord;
        startingDist = QLineF(starting_point,master_stick->my_line.p1()).length();
        if(startingDist<=1)
            return;
        start_stick_figure_scale = false;
    }
    //step 1 calcola la distanza tra la coordinata iniziale e quella corrente, se si allontana dal master stick ingrandisci, senno rimpicicolisci
    QLineF connect(master_stick->my_line.p1(),*coord); //crea la linea connettendo i punti
    float actualDistance = connect.length();
    scale = actualDistance/startingDist;
    for(StickData* s:stick_list){
        if(s->view)
            s->view->Scale(scale);
    }
    qDebug("scale = %f \r\n",scale);
}
void StickFigure::Traslate(qreal dx, qreal dy){
    for(StickData*s:stick_list){
        s->my_line.translate(dx,dy);
        if(s->view)
            s->view->Refresh(1);
    }
}
void MergeStickFigures(StickFigure* main_stick_figure, StickData* main_stick,StickFigure* to_join){
    QList<StickData*> matchIndexes;
    // first add an invisible branch that links the two
    StickData* linker = new StickData();
    linker->my_line = QLineF(main_stick->my_line.p2(),to_join->master_stick->my_line.p1());

    main_stick_figure->stick_list.append(linker);
    linker->parent = main_stick;

    linker->stick_type = StickData::LINE;
    linker->pen= Qt::NoPen;
    linker->is_linker = true;
    FramePresenter::AttachView(linker,main_stick_figure->scene);
    if(main_stick->stepchild)
        linker->stepchild = true;

    for(StickData* s: to_join->stick_list){
        StickData* toAdd = s->Clone();
        main_stick_figure->stick_list.append(toAdd);
        toAdd->stepchild = false;
        matchIndexes.append(toAdd);
        StickGraphicsItem *toAddView = FramePresenter::AttachView(toAdd,main_stick_figure->scene);

        toAddView->Refresh(0);
    }
    int i = 0;
    for(StickData* s: to_join->stick_list){

        StickData* toAdd = matchIndexes[i];
        if(main_stick->stepchild)
            toAdd->stepchild = true;
        if(s == to_join->master_stick){
            toAdd->parent = linker;

        }
        else if(s->stepchild && s->parent==to_join->master_stick)
        {

            toAdd->parent = linker;
        }
        else{
            toAdd->parent = matchIndexes[to_join->stick_list.indexOf(s->parent)];
        }
        for(StickData* c: s->children){

                toAdd->children.append(matchIndexes[to_join->stick_list.indexOf(c)]);
        }
        i++;
    }
    linker->children.append(matchIndexes[to_join->stick_list.indexOf(to_join->master_stick)]->children);
    linker->children.append(matchIndexes[to_join->stick_list.indexOf(to_join->master_stick)]);

    StickData * recursive;
    recursive = main_stick;
    while(1){
        if(recursive == nullptr)
            break;
        recursive->children.append(matchIndexes[to_join->stick_list.indexOf(to_join->master_stick)]->children);
        recursive->children.append(matchIndexes[to_join->stick_list.indexOf(to_join->master_stick)]);
        recursive->children.append(linker);
        recursive = recursive->parent;
    }
    QList<StickData*> stepChildToRemove;
    for(StickData* child:to_join->master_stick->children){
        // solo i figli DIRETTI del vecchio master vengono davvero riparentati a linker (vedi sopra
        // "s->stepchild && s->parent->IsMaster()"): un nipote stepchild più in profondità mantiene il
        // suo parent originale, quindi resta un discendente legittimo del master clonato e non va tolto
        if(child->stepchild && child->parent == to_join->master_stick)
            stepChildToRemove.append(matchIndexes[to_join->stick_list.indexOf(child)]);

    }
    for(StickData* child:stepChildToRemove){
         matchIndexes[to_join->stick_list.indexOf(to_join->master_stick)]->children.removeAll(child);
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
void StickFigure::SetStepChildAsMaster(StickData* to_master){
    QList<StickData*> zii;
    QList<StickData*> catenaGenitori;
    QList<StickData*> fratelli;
    QList<StickData*> oldStepChild;
    QList<StickData*> oldDirectChain;
    QList<StickData*> newDirectChain;
    StickData* oldMaster = master_stick;
    StickData* oldParent = to_master->parent;
    QPointF p1Buf;
    QPointF p2Buf;

    // determina la catena genitoriale del nuovo masterstick
    newDirectChain = to_master->children;
    for(StickData* s: stick_list){
        // se condivide lo stesso genitore, non è lo stesso stick da fare master e condivideva lo stesso status (stepchild o non stepchild) è fratello
        if(s->parent == to_master->parent && s!=to_master && s->stepchild == to_master->stepchild){
                fratelli.append(s);
        }// se invece  contiene fra i suoi figli il nuovo master è nella catena genitoriale che dovrà essere invertita
        if(s->children.contains(to_master)){
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
    for(StickData* s: stick_list){
        if(catenaGenitori.contains(s->parent) && // il genitore di questo stick appartiene alla catena genitoriale vecchia
                s!=to_master && // non deve essere lo stick che renderemo amster
                 oldStepChild.contains(s) && // deve condividere lo stesso status di stepchild dello stick da rendere master
                !fratelli.contains(s)&& // non deve essere già stato messo nella lista dei fratelli
                !catenaGenitori.contains(s)){ // non deve far parte della catena genitoriale, deve essere un figlio di essa
            zii.append(s);
        }
    }
    // forma le catene dei fratelli, ovvero la gerarchia rimane la stessa, importala nel nuovo master
    for(StickData* s:fratelli){
        s->parent = to_master;
        to_master->children.append(s);
        to_master->children.append(s->children);
    }
    // inverti i genitori, prima mettili in una lista ordinata,
    // dove l'ultimo item sarà il vecchio genitore dello stick da e master
    // e il primo item sarà il vecchio master
    QList<StickData*> chainToInvert;
    StickData* recursive = to_master->parent;
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
    to_master->parent->parent = to_master;
    to_master->parent = nullptr; // il nuovo master non ha genitori
    // pulisci la gerarchia dellacatena dei genitori da invertire
    for(StickData* s: catenaGenitori){
        s->children.clear();
    }
    // nella catena da invertire scambia p1 con p2 e ricostruisci la lista dei figli
    for(StickData* s:catenaGenitori){
        if(s!=oldMaster){ // il vecchio master mantiene la stessa direzione di prima
            p2Buf = s->my_line.p2();
            s->my_line.setP2(s->my_line.p1());
            s->my_line.setP1(p2Buf);
            if(s->stick_type == StickData::CIRCLE||s->stick_type == StickData::RECT || s->stick_type == StickData::TAPER || s->stick_type == StickData::TRAPEZOID){
                if(s->stick_type != StickData::CIRCLE && s->stick_type != StickData::IMAGE ){
                    float buf = s->img_h_scale;
                    s->img_h_scale = s->img_w_scale;
                    s->img_w_scale= buf;
                }

                s->img_angle = s->img_angle-180;
            }
        }
        StickData* recursive = s->parent; // segna lo stick corrente come figlio di tutti i suoi genitori
        int n = 0;
        while(recursive != nullptr && recursive != oldMaster){
            recursive->children.append(s);
            recursive = recursive->parent;
            n++;
            if(n>stick_list.count()){
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
    to_master->children.append(oldMaster->children);

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
    for(StickData*s:stick_list){
        if(s==to_master || newDirectChain.contains(s)){
            s->stepchild = false;
        }
        else{
            s->stepchild = true;
        }
    }
    //riassegna il ruolo del master
    master_stick = to_master;

    Refresh(0);
}
// rendi uno stick, che non era uno stepchild, nuovo master dello stickfigure
// gli stick possono raggrupparsi in:
// Fratelli del nuovo master che ne condividevano il genitore
// appartenenti alla catena che collega il nuovo master col vecchio, la cui direzione va invertita
// stick che nascevano nella catena sopracitata ma che generavano gerarchie parallele
// stick che prima erano stepchild, la cui gerarchia verrà unita con la catena genitoriale una volta invertita
void StickFigure::SetDirectChainAsMaster(StickData* to_master){
    QList<StickData*> zii;
    QList<StickData*> catenaGenitori;
    QList<StickData*> fratelli;
    QList<StickData*> oldStepChild;
    StickData* oldMaster = master_stick;
    QPointF p2Buf;
    for(StickData* s: stick_list){
        // la catena genitoriale sono quegli stick che collegano il prossimo master al vecchio master (incluso)
        if(s->children.contains(to_master)){
            catenaGenitori.append(s);
        }
        // i fratelli sono gli stick che condividono la paternità col prossimo master
        else if(s->parent == to_master->parent && s!=to_master){
            if(!(s->parent == oldMaster && s->stepchild != to_master->stepchild))
                fratelli.append(s);
        }
    }
    for(StickData* s:stick_list){
        if(s->stepchild){
            oldStepChild.append(s);
            s->stepchild = false;
        }
    }
    // gli zii, sono stick che derivano dalla catena genitoriale prima calcolata ma non ne fanno parte, non devono essere stepchild
    for(StickData* s: stick_list){
        if(catenaGenitori.contains(s->parent) &&
                s!=to_master &&
                !catenaGenitori.contains(s) &&
                !fratelli.contains(s)&&
                !(oldStepChild.contains(s))){
            zii.append(s);
        }
    }
    // aggiorna lo status
    to_master->stepchild = false;

    // la catena genitoriale va invertita, incluso il vecchio master si scambiano p1 con p2
    // e il primo figlio di ciascuno stick diventa il genitore di quello che prima era suo genitore
    QList<StickData*> catenaDaInvertire;
    StickData* iterator = to_master->parent;
    for(StickData*s: catenaGenitori) {
        Q_UNUSED(s);
        catenaDaInvertire.insert(0,iterator);
        if(iterator != oldMaster)
            iterator = iterator->parent;
    }
    for(StickData*s: catenaGenitori) {
        p2Buf = s->my_line.p2();
        s->my_line.setP2(s->my_line.p1());
        s->my_line.setP1(p2Buf);
        if( s->stick_type == StickData::CIRCLE||s->stick_type == StickData::RECT || s->stick_type == StickData::TAPER || s->stick_type == StickData::TRAPEZOID){
            s->img_angle = s->img_angle-180;
            if(s->stick_type != StickData::CIRCLE && s->stick_type != StickData::IMAGE ){
                float buf = s->img_h_scale;
                s->img_h_scale = s->img_w_scale;
                s->img_w_scale= buf;
            }

        }
        // pulisci i figli dello stick, la catena va ricostruita
        s->children.clear(); // attenzione il master così si perde anch gli stepchild che non vengono ricostruiti al for successivo
    }
    to_master->parent->parent = to_master;
    for(StickData*s:catenaDaInvertire){
       if(catenaDaInvertire.indexOf(s)+1==catenaDaInvertire.length())
           break; // fermati al master
       s->parent = catenaDaInvertire[catenaDaInvertire.indexOf(s)+1]; // quello che era prima era il figlio diretto diviene il nuovo genitore
    }

    to_master->parent = nullptr;
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
            if(n>stick_list.count()){
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
            to_master->children.append(s->children);
        }
        to_master->children.append(s);
        s->parent = to_master;

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
    to_master->stepchild = false;
    master_stick = to_master;
    Refresh(0);
}
// questo algoritmo serve a cambiare l'estremità in cui vi è il punto di origine di un master con un estremo libero
//  - lo stick master non cambia, ma p1 e p2 si scambiano tra loro
//  - quelli che prima erano stepchild diventano catena diretta
//  - altrimenti se vi era soloc atena diretta, origine verso l'esterno, spostaimo l'origine verso l'interno e tutto diventa stecphildren
void StickFigure::InvertMaster(StickData* to_master){
    QPointF p2Buf;

    // inverti p1 con p2 per cambiare la direzione
    p2Buf = to_master->my_line.p2();
    to_master->my_line.setP2(to_master->my_line.p1());
    to_master->my_line.setP1(p2Buf);
    if(to_master->stick_type == StickData::RECT ||to_master->stick_type == StickData::CIRCLE|| to_master->stick_type == StickData::TAPER || to_master->stick_type == StickData::TRAPEZOID)
      {  to_master->img_angle = to_master->img_angle-180;
         if(to_master->stick_type != StickData::CIRCLE && to_master->stick_type != StickData::IMAGE ){
             float buf = to_master->img_h_scale;
             to_master->img_h_scale = to_master->img_w_scale;
             to_master->img_w_scale= buf;
         }

    }
    // qui si capisce quale inversione occorre compiere,
    // si parte con l'estremo di origine senza stepchild
    // o si parte con un estremo di origine da cui partono stepchild?
    QList<StickData*> figli;
    QList<StickData*> figliastri;
    for(StickData*s:stick_list){
        if(s->parent == master_stick && s->stepchild){
            figliastri.append(s);
        }
        else if(s->parent == master_stick && !s->stepchild)
            figli.append(s);
    }
    // se avevamo solo stepchildren, allora l'inversione consiste nel rendere tutto catena diretta e l'origine si sposta verso l' esterno
    if(figli.isEmpty()){
        for(StickData*s: stick_list){
            s->stepchild = false;
        }
    }
    else{ // altrimenti l'origine si sposta verso l'interno, le catene dirette diventano stepchildren
        for(StickData*s: stick_list){
            s->stepchild = true;
        }
        to_master->stepchild = false;
    }
    // aggiorna lo status di master..ma non serve neanche
    master_stick = to_master;
    to_master->stepchild = false;
    Refresh(0);
}
// questo algoritmo semplificato rende master uno stick che correntemente è
// uno stepchild diretto del master attuale, in pratica
//  - La catena diretta dle master vecchio viene preservata, il nuovo master diventa genitore del master vecchio
//  - i fratelli del master vecchio acquisiscono come genitore il nuovo master e perdono lo status di stepchild
//  - il nuovo master acquisice stato di master, perde quello di stepchild,, il genitore diventa il nullptr
//      inoltre si invertono p1 e p2
void StickFigure::StepDownMaster(StickData* to_master){
    StickData* oldMaster = master_stick;

    for(StickData*s: stick_list){
        if(s->stepchild){
            oldMaster->children.removeAll(s);
        }
    }
    // tutti gli stick direttamente figli del vecchio master, se erano stepchild (quindi fratelli del vecchio master) cambiano parent
    // e perdono lo status id stepchild (anche i loro figli)
    for(StickData* s: stick_list){
        if(s->parent == oldMaster){
            if(s->stepchild && s!=to_master){
                s->stepchild = false;
                for(StickData* c:s->children){
                    c->stepchild = false;
                }
                s->parent = to_master;
            }
        }
    }
    oldMaster->parent = to_master;
    // inverti la direzione del nuovo master
    QPointF p2Buf = to_master->my_line.p2();
    to_master->my_line.setP2(to_master->my_line.p1());
    to_master->my_line.setP1(p2Buf);
    if(to_master->stick_type == StickData::RECT ||to_master->stick_type == StickData::CIRCLE||to_master->stick_type == StickData::TAPER || to_master->stick_type == StickData::TRAPEZOID)
        if(to_master->stick_type != StickData::CIRCLE && to_master->stick_type != StickData::IMAGE ){
            float buf = to_master->img_h_scale;
            to_master->img_h_scale = to_master->img_w_scale;
            to_master->img_w_scale= buf;
        }
    // il nuovo master ha come children tutti gli stick tranne se stesso
    to_master->children.clear();
    for(StickData* s: stick_list)
        to_master->children.append(s);
    to_master->children.removeAll(to_master);

    master_stick = to_master;
    to_master->stepchild = false;
    to_master->parent = nullptr;

    Refresh(0);
}
// sets new endpoint of the stick and traslates all its children
void StickFigure::Elongate(QPointF new_end_point, StickData* my_stick){
    if(!stick_list.contains(my_stick))
        return;
    float dx = new_end_point.x()-my_stick->my_line.p2().x();
    float dy = new_end_point.y()-my_stick->my_line.p2().y();
    my_stick->my_line.setP2(new_end_point);
    for(StickData*s: my_stick->children){
        if(!(my_stick==master_stick && s->stepchild)){
            s->my_line.translate(dx,dy);

        }
    }
    if(my_stick->stick_type == StickData::IMAGE || my_stick->stick_type  == StickData::RECT
            || my_stick->stick_type  == StickData::TRAPEZOID || my_stick->stick_type  == StickData::TAPER){
        my_stick->img_angle= -atan2(my_stick->my_line.dx(),my_stick->my_line.dy())*180/M_PI;
    }
    Refresh(0);

}
// seleziona quale algoritmo usare a seconda dello stick che deve diventare master
void StickFigure::SetMaster(StickData* to_master){
    QList<StickData*> figli;
    QList<StickData*> figliastri;
    if(master_stick->view){
        master_stick->view->stick_handle[0]->setBrush(QBrush(Qt::red));
        master_stick->view->stick_handle[0]->setZValue(100);
    }
    //prima, considerando il master, vedi quanti sick nascono dal suo punto di origine (figliastri) e quanti nascono dal suo estremo libero (figli)
    for(StickData*s:stick_list){
        if(s->parent == master_stick && s->stepchild){
            figliastri.append(s);
        }
        else if(s->parent == master_stick && !s->stepchild)
            figli.append(s);
    }
    // se siamo nella condizione di avere un estremo senza appendici e lo stick è già master, effettua una inversione
    if( to_master == master_stick && (figli.isEmpty() || figliastri.isEmpty())){
        InvertMaster(to_master);
    }
    // se lo stick era uno stepchild che nasce direttamente dall'origine, allora sposta tutta la gerarchia un passo verso il basso
    else if(figliastri.contains(to_master)){
        StepDownMaster(to_master);
    }
    else if(to_master == master_stick)
        return;
    else if(to_master->stepchild) // master scelto fa parte dei non stepchild
        SetStepChildAsMaster(to_master);
    else
        SetDirectChainAsMaster(to_master); // masterscelto fa parte dei vecchi stepchild
    if(master_stick->view){
        master_stick->view->stick_handle[0]->setBrush(QBrush(Qt::green));
        master_stick->view->stick_handle[0]->setZValue(101);
    }
}
void SplitStickFigures(StickFigure* split, StickData* origin,StickFigure* branch){
    // crea una lista parallela con gli stick corrispondenti nella stickfigure originale
   QList<StickData*> matchingIndex;
   // clonare tutti gli stick figli dello stick selezionato
    matchingIndex.append(origin);
    StickData* toAdd = origin->Clone();
    branch->stick_list.append(toAdd);

    for(StickData* s: origin->children)
    {
        if(s->parent != origin->parent)
        {
            matchingIndex.append(s);
            toAdd = s->Clone();
            branch->stick_list.append(toAdd);
        }
    }

    branch->master_stick = branch->stick_list[matchingIndex.indexOf(origin)];

    int i = 0;
    // in un altro ciclo for, per ogni stick clonato aggiungere gli stessi parent e gli stessi children sfruttando l'indicizzazione
    for(StickData* s: branch->stick_list){
        int parent_idx = matchingIndex.indexOf(matchingIndex[i]->parent);
        if(matchingIndex[i]->parent != origin->parent && parent_idx != -1)
            s->parent = branch->stick_list[parent_idx];
        for(StickData*c: matchingIndex[i]->children){
            int childIdx = matchingIndex.indexOf(c);
            if(childIdx != -1) // c non fa parte del branch (es. voce spuria da una gerarchia children non pulita)
                s->children.append(branch->stick_list[childIdx]);
        }
        i++;
        StickGraphicsItem *view = FramePresenter::AttachView(s,split->scene);
        view->Refresh(0);
    }

    StickData* origin_parent = origin->parent;
    split->DeleteStick( split->stick_list.indexOf(origin));
    // un merge precedente puo' aver lasciato un linker invisibile (StickData::is_linker) come
    // genitore di origin: se questo split l'ha appena svuotato di tutti i suoi discendenti va
    // rimosso anche lui, altrimenti resta per sempre nella figura ospite con le sue maniglie
    // visibili (bug: la maniglia del master della figura appena scollegata resta attaccata
    // all'altra figura). Risale la catena in caso di merge concatenati.
    while(origin_parent && origin_parent->is_linker && origin_parent->children.isEmpty()){
        StickData* next = origin_parent->parent;
        split->DeleteStick(split->stick_list.indexOf(origin_parent));
        origin_parent = next;
    }
}
float StickFigure::MinZ(){
    if(stick_list.isEmpty()){
        return 0;
    }
    float minVal = stick_list[0]->z;
    for(StickData*s:stick_list){
        if(s->z<minVal)
            minVal = s->z;
    }
    return minVal;
}
float StickFigure::MaxZ(){
    if(stick_list.isEmpty()){
        return 0;
    }
    float maxVal = stick_list[0]->z;
    for(StickData*s:stick_list){
        if(s->z>maxVal)
            maxVal = s->z;
    }
    return maxVal;
}
/**
 * @details Used by Chop() to duplicate a stick being split in two. Historically this
 * serialized to a QBuffer and deserialized back; since Phase 1a it is a plain deep
 * field copy via StickData::CopyFrom().
 */
void CloneStick(StickData* dest, StickData*src){
    dest->CopyFrom(*src);
}
