#include "myview.h"
#include <QtAlgorithms>
#include <algorithm>
#include <math.h>
#include <QDebug>
#include <QPainterPath>
#include <QRadialGradient>
#include <QTransform>
#include <QGraphicsRotation>
#include <frame.h>
#include <QMessageBox>
#include <QSpinBox>
#include <QCheckBox>
#include <animationoptions.h>
#include <QInputDialog>
#include <QMessageBox>
#include <QShortcut>
#include <QTabletEvent>
#include <QRandomGenerator>
#include "uiItems.h"
#include "framepresenter.h"


bool    is_right_pressed = false;
bool    is_middle_pressed = false;
bool    is_pressed = false;
extern float zoom_lvl;
QPointF starting_coord; //mouse click coordinateBuffer
QPointF coord; //current mouse pos
float   length_buffer;
float zoom_lvl_buf;
FrameData* stick_library_buffer; ///< libreria corrente: solo dati da Phase 2b
extern QSpinBox * onion_skin_sb;
// start_stick_figure_rotation/start_stick_figure_scale sono diventati membri statici di
// StickFigure (la classe che li consuma) — vedi StickFigure.h
bool translate_scene = false;
bool rotate_scene = false;
bool scale_scene = false;
StickFigure* join_to_stick_figure;
StickFigure* split_to_stick_figure;;
StickData* join_to_stick;
StickData* split_to_stick;
extern QString image_name_buffer;
extern qint32 W;
extern qint32 H;
extern QGraphicsRectItem* my_rect;
extern QGraphicsRectItem* limit_rect;
// play_back è un membro di MyView ora (vedi myview.h);
// load_file: rimosso — veniva solo scritto (mymainwindow.cpp) e mai letto da nessuno
extern QImage * image_draw_buffer ;
extern QString animation_path;
extern int auto_save_interval;
extern int op_count;
extern int handle_size;

#define MAXUNDO 50 ///< cap on undo_stack depth, enforced via QUndoStack::setUndoLimit()
#define MAXDRAWUNDO 20 ///< cap on draw mode's own stroke history (PushDrawUndo()); far
                       ///< shallower than MAXUNDO because one entry is a whole canvas
                       ///< (~19 MB at 1080p), not a frame's worth of stick records

/**
 * @details Builds the empty document the rest of the app edits: a fresh Animation
 * (whose scene is this view's scene), the default gray pen/brush new sticks inherit,
 * and the undo machinery. The autosave hook lives on QUndoStack::indexChanged (fires
 * on push AND on undo/redo, so undo/redo count as edits worth saving) and emits
 * AutoSaveTrigger() every auto_save_interval operations — caught by
 * MyMainWindow::AutoSave() (wired in MyMainWindow::DoConnections()).
 */
MyView::MyView(QWidget *parent) : QGraphicsView(parent)
{
    my_animation = new Animation();
    my_animation->scene = scene();
    setMouseTracking(true);
    my_pen.setWidth(10);
    my_brush.setStyle(Qt::SolidPattern);
    my_brush.setColor(Qt::gray);
    my_pen.setCapStyle(Qt::RoundCap);
    stick_library_buffer = new FrameData();
    undo_stack.setUndoLimit(MAXUNDO);
    // autosave triggering moved here from StoreUndo() (Phase 4): indexChanged fires
    // on every push/undo/redo, which is a more correct autosave trigger than the old
    // "only on a fresh edit" behavior (undo/redo are edits too, worth autosaving)
    connect(&undo_stack, &QUndoStack::indexChanged, this, [this](int){
        op_count++;
        if(QFile::exists(animation_path) && op_count % auto_save_interval == 0){
            emit AutoSaveTrigger();
        }
    });
    // Ctrl+Z / Ctrl+Y (QKeySequence::Undo/Redo — Ctrl+Y is the Redo default on Windows):
    // window-scoped shortcuts so undo/redo work from the keyboard regardless of which
    // canvas child has focus, same targets as the undo/redo toolbar buttons (widget.cpp).
    // In draw mode they drive the session-local image undo chain instead of undo_stack —
    // the shortcut consumes the key before keyPressEvent sees it, so the dispatch lives here.
    connect(new QShortcut(QKeySequence::Undo, this), &QShortcut::activated, this, [this]{ Undo(); });
    connect(new QShortcut(QKeySequence::Redo, this), &QShortcut::activated, this, [this]{ Redo(); });
    draw_brush_stamp = MakeRoundBrush(128, draw_brush_hardness, draw_brush_roundness); // punta tonda integrata
}

/**
 * @details Destruction order matters: handles first (they live on the scene and are
 * read while a StickGraphicsItem is torn down), then every frame — this loop is the
 * sole owner of the frames, since Animation has no destructor that frees them, so
 * there is no double free. The library buffer is deleted here because it is created in
 * the constructor; copy_stick_figure_buffer is the (possibly null) paste buffer.
 */
MyView::~MyView()
{
    if (my_animation->current_frame != nullptr) // una view a cui non è mai stato dato un frame non ha nulla da staccare
    {
        for (StickFigure* S : my_animation->CurrentFrame()->stick_figures)
        {
            RemoveHandles(S);
        }
    }
    for ( Frame* f : my_animation->frame_list)
    {
        delete f;
    }
    delete my_animation;
    delete copy_stick_figure_buffer;
    qDeleteAll(multi_copy_buffer); // buffer copia della selezione multipla: puro dato
    // creato nel costruttore, quindi liberato qui: senza questo la libreria corrente
    // resta allocata fino alla fine del processo
    delete stick_library_buffer;
}

/**
 * @details ChangeTool() runs first so the tool being left cleans up (e.g. a half-drawn
 * stick is cancelled) before `tool` flips to the new id. Called by the toolbar button
 * slots in widget.cpp.
 */
void MyView::SetTool(int new_tool)
{
    // scegliere QUALSIASI tool esce da draw mode: pennello/gomma sono sotto-strumenti del
    // pannello draw (non passano da SetTool), quindi ogni SetTool è un tool "non-draw".
    // Qui perché draw mode può essere stato avviato col tool ancora su NOTOOL (bottone
    // "Edit drawing"), caso che il ramo DRAWFREEHAND di ChangeTool() non coprirebbe
    if(draw_mode)
        ExitDrawMode();

    ChangeTool();

    tool = new_tool;
    // maniglia/bounding box appaiono sui tool di selezione, spariscono sugli altri (spec).
    // NON ricentrare il perno: un cambio tool deve conservare la posizione della maniglia
    // spostata dall'utente con Ctrl-trascina (una NUOVA selezione la ricentra da sé, via
    // UpdateMultiSelectionUI(true)). Prima, passare a ROTATE/SCALE la riportava al centro.
    UpdateSelectionDecorations(false);
}

/**
 * @details Removes the currently selected figure layer from the frame, its list-widget
 * row and the flattened total_sticks z-order list, then hands selection to a surviving
 * layer (Animation::UpdateSelection()) — without that the list's currentRow could stay
 * on Qt's default post-takeItem row and a following Move-Layer would move the wrong one.
 * Refuses to remove the last layer. Also reached from keyPressEvent() (Delete on a
 * master stick) and from the JOIN tool once two figures have been merged.
 */
void MyView::DeleteStickFigure()
{
    bool update_current_stick_figure =  false;
    if(my_stick_figure_widget_list->selectedItems().count() == 1 && my_animation->current_frame->stick_figures.count()>1)
    {
        //scegli lo stickfigure selzionato dalla lista grafica
        QListWidgetItem *current = my_stick_figure_widget_list->selectedItems()[0];
        // estrai il dato utente salvato in esso, puntatore ad uno stickfigure
        QVariant  retrieved_data = (current->data(Qt::UserRole));
        //riconverti il dato
        StickFigure* cs = qvariant_cast<StickFigure*>(retrieved_data);
        update_current_stick_figure = (cs == my_animation->CurrentFrame()->CurrentStickFigure());

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
        if(update_current_stick_figure){
             ArrowSelection();
        }
    }
    else{
        QMessageBox::about(this,"error","There must be at least one stickfigure layer in the scene");
    }

}
//cambia l'ordine di profondità dello stickfigure selezionato
/**
 * @details Keeps two things in step: the model z-order (Frame::StickfigureToTop/
 * ToBottom/AboveBelow(), which restack the sticks) and the layer list widget (the
 * row is physically moved with takeItem()/insertItem()). Driven by the move-layer
 * buttons in widget.cpp; `mode` picks top/bottom/one-step and `increment` gives the
 * step direction for the one-step case.
 */
void MyView::MoveStickFigureZ(int increment, int mode)
{
    if(draw_mode)
        ExitDrawMode(); // riordinare i layer chiude la sessione di disegno
    if(my_stick_figure_widget_list->count() <= 1 )
        return;
    // selezione multipla: applica lo spostamento a ogni figura selezionata, in un
    // ordine che preserva le posizioni relative (senza scavalcamenti tra selezionati)
    static bool multi_reentry = false; // una sola gesture alla volta esiste, come gli altri latch statici
    if(MultiSelectionActive() && !multi_reentry){
        multi_reentry = true;
        QList<StickFigure*> ordered = multi_selection;
        std::sort(ordered.begin(), ordered.end(), [this](StickFigure *a, StickFigure *b){
            return my_stick_figure_widget_list->row(a->linked_item)
                 < my_stick_figure_widget_list->row(b->linked_item);
        });
        // UP e BOTTOM processano dall'alto; DOWN e TOP dal basso (vedi sopra)
        if(mode == MODE_DOWN || mode == MODE_TOP)
            std::reverse(ordered.begin(), ordered.end());
        for(StickFigure *S : ordered){
            if(!my_animation->current_frame->stick_figures.contains(S) || !S->linked_item)
                continue;
            my_stick_figure_widget_list->setCurrentRow(my_stick_figure_widget_list->row(S->linked_item));
            my_animation->UpdateSelection(S);
            MoveStickFigureZ(increment, mode);
        }
        multi_reentry = false;
        UpdateMultiSelectionUI(); // ripristina le righe selezionate nella lista layer
        return;
    }
    int current_index = my_stick_figure_widget_list->currentRow();
    if(current_index < 0) // nessuna riga selezionata: item(new_idx) sotto tornerebbe nullptr
        return;
    if((current_index == 0 && increment<0)||(current_index == my_stick_figure_widget_list->count()-1 && increment>0))
        return;
    if(mode == MODE_TOP){
        increment = my_stick_figure_widget_list->count()-1-current_index;
        if(increment == 0 )
            return;
    }
    else if(mode == MODE_BOTTOM){
        increment = -current_index;
        if(increment == 0)
            return;
    }
    int new_idx = current_index+increment;
    QListWidgetItem *stick_figure_at_new_idx_item = my_stick_figure_widget_list->item(new_idx);
    // estrai il dato utente salvato in esso, puntatore ad uno stickfigure
    QVariant  retrieved_data = (stick_figure_at_new_idx_item->data(Qt::UserRole));
    //riconverti il dato
    StickFigure* stick_figure_at_new_idx = qvariant_cast<StickFigure*>(retrieved_data);

    if(new_idx<my_stick_figure_widget_list->count() && new_idx>=0 && new_idx != current_index){
        if(mode == MODE_TOP){

            my_animation->CurrentFrame()->StickfigureToTop(my_animation->CurrentFrame()->CurrentStickFigure());
        }
        else if(mode == MODE_BOTTOM){
            my_animation->CurrentFrame()->StickfigureToBottom(my_animation->CurrentFrame()->CurrentStickFigure());

        }
        else if(increment>0){

            my_animation->CurrentFrame()->StickfigureAboveBelow(my_animation->CurrentFrame()->CurrentStickFigure(),stick_figure_at_new_idx,1);
        }
        else{
            my_animation->CurrentFrame()->StickfigureAboveBelow(my_animation->CurrentFrame()->CurrentStickFigure(),stick_figure_at_new_idx,0);

        }
        //sposta nella lista grafica il layer dello stickfigure
        QListWidgetItem * item = my_stick_figure_widget_list->takeItem(current_index);
        my_stick_figure_widget_list->insertItem(new_idx,item);
        item->setSelected(true);
        my_stick_figure_widget_list->setCurrentRow(new_idx);

        my_animation->current_frame->UpdateIcon();
        my_animation->CurrentFrame()->Refresh(0);
    }
}
/**
 * @details Clamps an edit position to the canvas (my_rect) plus a 10% margin on each
 * side, so a drag can't fling a stick arbitrarily far off the page. Every mouse handler
 * runs the cursor through here first; it also refreshes the global `coord` used by
 * ArrowSelection() and the tool handlers.
 */
void MyView::MapMyCoords(QPointF& my_coord, QPointF starting_point){
    coord = mapToScene(starting_point.x(),starting_point.y());
    if(my_coord.x()<-my_rect->rect().width()/10)
        my_coord.setX(-my_rect->rect().width()/10);
    if(my_coord.y()<-my_rect->rect().height()/10)
        my_coord.setY(-my_rect->rect().height()/10);
    if(my_coord.y()>my_rect->rect().height()+my_rect->rect().height()/10)
        my_coord.setY(my_rect->rect().height()+my_rect->rect().height()/10);
    if(my_coord.x()>my_rect->rect().width()+my_rect->rect().width()/10)
        my_coord.setX(my_rect->rect().width()+my_rect->rect().width()/10);
}
/**
 * @details Start of every gesture: records which button is down, maps the click to
 * scene coordinates (into `coord`/`starting_coord`) and, for a left click, branches on
 * the active `tool`. Most branches call StoreUndo() up front (the gesture may span many
 * mouseMoveEvent() calls, so the snapshot is taken once at the start) and cache each
 * stick's angle/length/scale in its view's *_buffer fields, the reference the drag
 * deltas in mouseMoveEvent() are applied against. Right/middle buttons are left for
 * pan/zoom, also handled in mouseMoveEvent().
 */
void MyView::mousePressEvent(QMouseEvent *event)
{
    if(event->button()==Qt::LeftButton){
        is_pressed = true;
    }
    else if(event->button()==Qt::RightButton){
        is_right_pressed = true;
    }else if(event->button()==Qt::MiddleButton){
        is_middle_pressed = true;
    }
    // converti le coordinate del mouse in coordinate di scena
    MapMyCoords(coord,event->pos());


    starting_coord   = coord; //buffer per il dragghing del mouse premuto
    // draw mode intercetta ogni input a monte del tool: un click sinistro inizia una
    // pennellata (con snapshot per l'undo di sessione), i tasti destro/centrale sono
    // ignorati (pan/zoom disabilitati durante il disegno)
    if(draw_mode){
        if(event->button()==Qt::LeftButton){
            if(draw_tool == DT_SEL_RECT || draw_tool == DT_SEL_LASSO){
                DrawSelPress(coord, event->modifiers());
            } else {
                draw_stroking = true;
                draw_last_pos = coord;
                PaintStrokeSegment(coord, coord); // dab iniziale (click singolo = un punto)
                // lo snapshot per l'undo si prende al RILASCIO, quando la pennellata è completa
            }
        }
        return;
    }
    if(!is_right_pressed && !is_middle_pressed){
    switch(tool)
    {
            case CLAMP:
            case NOTOOL:
            {
                // Ctrl: selezione dei vertici di un poligono (Shift = multi-selezione,
                // il trascinamento successivo sposta tutti i vertici selezionati)
                if(event->modifiers() & Qt::ControlModifier){
                    int vertex_idx = -1;
                    StickData *hit = FindPolygonVertex(coord, &vertex_idx);
                    if(hit != nullptr){
                        if(event->modifiers() & Qt::ShiftModifier){
                            // Shift: aggiungi/togli il vertice dalla selezione
                            if(hit->view->selected_vertices.contains(vertex_idx))
                                hit->view->selected_vertices.remove(vertex_idx);
                            else
                                hit->view->selected_vertices.insert(vertex_idx);
                        }
                        else if(!hit->view->selected_vertices.contains(vertex_idx)){
                            // click su un vertice non selezionato: selezione singola
                            DeselectAllVertices();
                            hit->view->selected_vertices.insert(vertex_idx);
                        }
                        // (click senza Shift su un vertice già selezionato: la selezione
                        // resta com'è, così si può trascinare una multi-selezione)
                        my_animation->UpdateSelection(hit);
                        StoreUndo(); // il drag che può seguire è un'unica modifica
                        poly_vertex_drag = true;
                        hit->view->Refresh(0);
                    }
                    else{
                        DeselectAllVertices(); // Ctrl+click a vuoto: deseleziona tutto
                        // Ctrl+click/trascina sulla tela vuota riposiziona il perno della
                        // selezione al cursore (sostituisce il vecchio Spazio, spec)
                        if(HasActiveSelection()){
                            multi_pivot = coord;
                            pivot_dragging = true;
                            // singola figura: registra subito l'offset persistente, altrimenti
                            // UpdateSelectionDecorations ricalcolerebbe il perno dal vecchio
                            // offset e la maniglia rimbalzerebbe fino al primo movimento (bug 2)
                            if(!MultiSelectionActive()){
                                QList<StickFigure*> sel = SelectedFigures();
                                StickFigure *S = sel.isEmpty() ? nullptr : sel.first();
                                if(S != nullptr && S->master_stick != nullptr)
                                    S->pivot_offset = coord - S->master_stick->my_line.p1();
                            }
                            UpdateSelectionDecorations();
                        }
                    }
                    break;
                }
                DeselectAllVertices(); // click normale altrove: la selezione vertici decade
                // ---- multi-selezione di stickfigure ----
                {
                    // click (senza modificatori) sulla maniglia: trascina il PERNO delle
                    // trasformazioni, non la selezione
                    if(MultiSelectionActive() && event->modifiers() == Qt::NoModifier
                            && QLineF(coord, multi_pivot).length() <= handle_size * 1.5f){
                        pivot_dragging = true;
                        break;
                    }
                    // maniglie della transformation box: pomello = rotazione, spigoli/lati =
                    // scala (uniforme), attorno alla maniglia mobile. Solo senza modificatori
                    // (Shift/Alt restano add/sub della selezione). Va PRIMA di FigureAtPoint:
                    // le maniglie stanno sul bordo del box, non su uno stick, quindi altrimenti
                    // partirebbe il rubber band.
                    if(SelectionHandlesActive() && event->modifiers() == Qt::NoModifier){
                        int bh = HitSelectionHandle(coord);
                        if(bh == 8){ BeginSelectionRotate(); sel_box_gesture = 2; break; }
                        if(bh >= 0){ BeginSelectionScale();  sel_box_gesture = 1; break; }
                    }
                    // frecce della texture: la priorità PIÙ BASSA (spec), quindi dopo le
                    // maniglie del box — HitTextureArrow() rinuncia da sé anche su una
                    // maniglia di stick, così la rotazione normale resta intoccata
                    if(event->modifiers() == Qt::NoModifier){
                        bool arrow_fill = true, arrow_tip = true;
                        if(HitTextureArrow(coord, &arrow_fill, &arrow_tip)){
                            StoreUndo(); // un solo passo di undo per tutto il trascinamento
                            texture_arrow_is_fill = arrow_fill;
                            texture_arrow_gesture = arrow_tip ? 2 : 1;
                            QPointF tail, tip;
                            if(TextureArrowPoints(arrow_fill, &tail, &tip))
                                texture_arrow_tail = tail;
                            break;
                        }
                    }
                    StickFigure *hit_figure = FigureAtPoint(coord);
                    if((event->modifiers() & Qt::ShiftModifier) && hit_figure != nullptr){
                        AddToMultiSelection(hit_figure); // Shift+click: aggiungi alla selezione
                        break;
                    }
                    if((event->modifiers() & Qt::AltModifier) && hit_figure != nullptr
                            && multi_selection.contains(hit_figure)){
                        RemoveFromMultiSelection(hit_figure); // Alt+click: togli dalla selezione
                        break;
                    }
                    if(hit_figure == nullptr){
                        // click sulla tela vuota: la selezione multipla decade e parte il
                        // rubber band. La forma (rettangolo o lazo) dipende dal checkbox del
                        // menu (stickfig_lasso); il lazo è disegnato in drawForeground.
                        ClearMultiSelection();
                        if(stickfig_lasso){
                            band_is_lasso = true;
                            band_poly.clear();
                            band_poly << coord;
                        } else {
                            band_is_lasso = false;
                            selection_band = new QGraphicsRectItem(QRectF(coord, coord));
                            QPen band_pen(QColor(200, 200, 200), 0);
                            band_pen.setStyle(Qt::DotLine);
                            selection_band->setPen(band_pen);
                            selection_band->setBrush(Qt::NoBrush);
                            selection_band->setZValue(199);
                            scene()->addItem(selection_band);
                        }
                        band_dragging = true;
                        break;
                    }
                    if(MultiSelectionActive() && multi_selection.contains(hit_figure)){
                        // trascinare una figura selezionata sposta TUTTA la selezione
                        StoreUndo();
                        multi_dragging = true;
                        break;
                    }
                    if(MultiSelectionActive())
                        ClearMultiSelection(); // click su una figura fuori dalla selezione
                }
                StoreUndo();
                ArrowSelection();
                StickFigure::start_stick_figure_rotation = true;
                // dopo un ripristino di undo/redo lo stickfigure corrente può essere nullo
                // (ArrowSelection esce presto su un frame vuoto senza impostarlo)
                if(my_animation->CurrentFrame()->CurrentStickFigure() != nullptr){
                    for(StickData*s:my_animation->CurrentFrame()->CurrentStickFigure()->stick_list){
                        if(s->view) s->view->angle_buffer2 = s->my_line.angle();
                    }
                }
                // selezione singola: maniglia (sul master) + bounding box (spec)
                UpdateSelectionDecorations(true);
                break;
            }
            case DRAW:
            case DRAWCIRCLE:
            case DRAWRECT:
            case DRAWTRAPEZOID:
            case DRAWTAPER:
            case DRAWIMG:
            case DRAWFREEHAND:
            case DRAWTEXT:
            {
                DrawCmd(&coord,(int)tool);
                break;
            }
            case(ROTATE):
            {
                BeginSelectionRotate(); // snapshot + buffer (condiviso col pomello della transformation box)
                break;
            }
            case(SCALE):
            {
                BeginSelectionScale(); // snapshot + buffer (condiviso con le maniglie della transformation box)
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
                        join_to_stick_figure = my_animation->CurrentFrame()->CurrentStickFigure();
                        join_to_stick = join_to_stick_figure->current_stick;
                        StoreUndo();
                        ArrowSelection();

                        if(my_animation->CurrentFrame()->CurrentStickFigure()!=join_to_stick_figure && my_animation->CurrentFrame()->CurrentStickFigure()->stick_list.count()>0){
                            RemoveHandles(join_to_stick_figure);
                            MergeStickFigures(join_to_stick_figure,join_to_stick,my_animation->CurrentFrame()->CurrentStickFigure());
                            AddHandles(join_to_stick_figure);
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
                length_buffer = my_animation->CurrentFrame()->CurrentStick()->my_line.length();
                break;
            }
            case(MANIPULATE):
            {
                StoreUndo();
                ArrowSelection();
                if(my_animation->CurrentFrame()->CurrentStick()==nullptr) break; // niente da selezionare (nessuno stick nel frame)
                length_buffer = my_animation->CurrentFrame()->CurrentStick()->my_line.length();

                break;
            }
            case(DRAWPOLY):
            {
                StickFigure *fig = my_animation->current_frame->stick_figures.isEmpty()
                        ? nullptr : my_animation->CurrentFrame()->CurrentStickFigure();
                float radius = PolyPickRadius();
                // secondo click della base: chiudi la linea e apri la sessione di vertici
                if(fig != nullptr && fig->draw_count == 1){
                    DrawCmd(&coord,(int)tool);
                    poly_session_active = true;
                    poly_session_extends = false;
                    break;
                }
                StickData *session = PolySessionStick(StickData::POLYGON);
                if(poly_session_active && session != nullptr){
                    // Alt: cancella il vertice cliccato invece di aggiungerne uno
                    if(event->modifiers() & Qt::AltModifier){
                        float dist = 0;
                        int vertex_idx = session->view->SelectPolygonPoint(coord, &dist);
                        if(vertex_idx >= 0 && dist <= radius){
                            StoreUndo();
                            session->view->DeletePolygonPoint(vertex_idx);
                        }
                        break;
                    }
                    // click sul punto iniziale della base = chiusura del poligono
                    if(QLineF(coord, session->my_line.p1()).length() <= radius){
                        FinishPolygonSession();
                        break;
                    }
                    StoreUndo(); // granularità per-vertice: ogni click si annulla da solo
                    int insert_idx = poly_session_extends
                            ? session->view->NearestEdgeInsertIndex(coord)
                            : session->polygon_points.length();
                    session->view->ClearPolygonPreview();
                    session->view->AddPolygonPoint(coord, insert_idx);
                    break;
                }
                poly_session_active = false; // sessione orfana (undo/cambio frame): riparti pulito
                // vertice esistente: Alt = cancella, altrimenti inizia il trascinamento
                int vertex_idx = -1;
                StickData *hit = FindPolygonVertex(coord, &vertex_idx, StickData::POLYGON);
                if(hit != nullptr){
                    StoreUndo();
                    my_animation->UpdateSelection(hit);
                    if(event->modifiers() & Qt::AltModifier)
                        hit->view->DeletePolygonPoint(vertex_idx);
                    else
                        poly_drag_idx = vertex_idx;
                    break;
                }
                if(event->modifiers() & Qt::ControlModifier)
                    break; // Ctrl: il tool può solo spostare vertici esistenti
                // click sul bordo di un poligono esistente: inserisci UN vertice e
                // consenti di trascinarlo subito, esattamente come la multilinea (sotto).
                // Un tempo apriva una sessione persistente (poly_session_extends): il
                // click SUCCESSIVO dell'utente, spesso non inteso come "aggiungi vertice",
                // ne inseriva un secondo di nascosto -> "due vertici per click" solo sui
                // poligoni (Group D). Niente sessione = un vertice per click, come le multilinee.
                int insert_idx = -1;
                StickData *edge_hit = FindPolygonEdge(coord, &insert_idx, StickData::POLYGON);
                if(edge_hit != nullptr){
                    StoreUndo();
                    my_animation->UpdateSelection(edge_hit);
                    edge_hit->view->AddPolygonPoint(coord, insert_idx);
                    poly_drag_idx = insert_idx; // il trascinamento aggiusta subito la posizione
                    break;
                }
                // altrimenti: primo click di un nuovo poligono
                DrawCmd(&coord,(int)tool);
                break;
            }
            case(DRAWMULTI):
            {
                StickFigure *fig = my_animation->current_frame->stick_figures.isEmpty()
                        ? nullptr : my_animation->CurrentFrame()->CurrentStickFigure();
                float radius = PolyPickRadius();
                // secondo click: chiude la base (2 vertici) e apre la sessione di estensione
                if(fig != nullptr && fig->draw_count == 1){
                    DrawCmd(&coord,(int)tool);
                    poly_session_active = true;
                    poly_session_extends = false;
                    break;
                }
                StickData *session = PolySessionStick(StickData::MULTILINE);
                if(poly_session_active && session != nullptr){
                    session->view->ClearPolygonPreview();
                    StoreUndo(); // granularità per-vertice, come per il poligono
                    session->view->AppendMultilineVertex(coord);
                    break;
                }
                poly_session_active = false; // sessione orfana: riparti pulito
                // vertice esistente: Alt = cancella (sotto i 2 vertici sparisce lo stick
                // intero), altrimenti trascina
                int vertex_idx = -1;
                StickData *hit = FindPolygonVertex(coord, &vertex_idx, StickData::MULTILINE);
                if(hit != nullptr){
                    StoreUndo();
                    my_animation->UpdateSelection(hit);
                    if(event->modifiers() & Qt::AltModifier){
                        if(hit->view->SceneVertices().length() <= 2)
                            DeleteWholeStick(hit); // resterebbe 1 vertice: via tutto lo stick
                        else
                            hit->view->DeletePolygonPoint(vertex_idx);
                    }
                    else{
                        poly_drag_idx = vertex_idx;
                    }
                    break;
                }
                // vicino a un segmento esistente (o con Ctrl): nuovo punto al CENTRO del
                // segmento più vicino, poi il drag lo posiziona
                int insert_idx = -1;
                StickData *edge_hit = FindPolygonEdge(coord, &insert_idx, StickData::MULTILINE);
                if(edge_hit != nullptr){
                    StoreUndo();
                    my_animation->UpdateSelection(edge_hit);
                    QPolygonF verts = edge_hit->view->SceneVertices();
                    QPointF mid = (verts[insert_idx - 1] + verts[insert_idx % verts.length()]) / 2.0;
                    edge_hit->view->AddPolygonPoint(mid, insert_idx);
                    poly_drag_idx = insert_idx; // il trascinamento aggiusta subito la posizione
                    break;
                }
                if(event->modifiers() & Qt::ControlModifier)
                    break; // Ctrl: solo editing di multilinee esistenti, niente nuovo disegno
                // altrimenti: primo click di una nuova multilinea
                DrawCmd(&coord,(int)tool);
                break;
            }
            case(DRAW_IK):
            {
                StickFigure *fig = my_animation->current_frame->stick_figures.isEmpty()
                        ? nullptr : my_animation->CurrentFrame()->CurrentStickFigure();
                if(fig == nullptr) break;

                StickData *pending = IkPendingStart();
                if(ik_drawing && pending != nullptr){
                    // secondo click: valida il target e RI-DERIVA lo start dal ramo del
                    // target (FindIkTargetCandidate/ResolveIkStartForTarget). Il primo
                    // click sceglie uno start provvisorio, ma se piu' arti condividono
                    // l'ancora non puo' sapere quale intende l'utente: lo decide il target.
                    StickData *target = FindIkTargetCandidate(fig, pending, coord);
                    StickData *start  = target != nullptr
                            ? ResolveIkStartForTarget(target, pending->my_line.p1(), pending)
                            : nullptr;
                    if(target != nullptr && start != nullptr){
                        // NIENTE StoreUndo qui: la creazione catena è UN gesto a due click
                        // il cui snapshot pre-gesto è già stato preso al primo click (sotto,
                        // prima di IK_START). Un secondo snapshot spezzava l'annullamento in
                        // due: un solo Undo disfaceva il commit ma lasciava lo start bloccato
                        // su IK_START (e il target su un ruolo orfano), così ne' lo start ne'
                        // il target si potevano riselezionare per una nuova catena. Con un
                        // unico comando, Undo riporta tutto a IK_NONE in un colpo.
                        if(start != pending){
                            // lo start giusto è su un ramo diverso che condivide l'ancora:
                            // libera il provvisorio (ramo sbagliato) e promuovi quello reale
                            pending->ik_role = StickData::IK_NONE;
                            if(pending->view) pending->view->Refresh(0);
                            start->ik_role = StickData::IK_START;
                            if(start->view) start->view->Refresh(0);
                        }
                        CommitIkChain(start, target);
                        ik_drawing = false;
                        ClearIkPreviewItems(); // NON CancelIkDrawing(): il ruolo appena
                                               // commesso non va disfatto
                    }
                    // target non valido: nessuna mutazione, resta in attesa di un altro click
                    break;
                }

                // non in sessione: click su un target esistente lo accende/spegne (Alt =
                // cancella l'intera catena), altrimenti click su uno start idoneo apre
                // una nuova sessione di creazione
                StickData *existing_target = FindIkTargetHandle(fig, coord);
                if(existing_target != nullptr){
                    StoreUndo();
                    if(event->modifiers() & Qt::AltModifier)
                        ClearIkChain(existing_target->ik_start);
                    else
                        ToggleIkChain(existing_target);
                    break;
                }
                StickData *candidate = FindIkStartCandidate(fig, coord);
                if(candidate != nullptr){
                    StoreUndo();
                    candidate->ik_role = StickData::IK_START; // spec: il primo click commette subito il ruolo
                    my_animation->UpdateSelection(candidate);
                    if(candidate->view)
                        candidate->view->Refresh(0);
                    ik_drawing = true;
                    QPen preview_line_pen(Qt::black, 1, Qt::DotLine);
                    preview_line_pen.setCosmetic(true); // spessore costante a ogni zoom
                    ik_preview_line = new QGraphicsLineItem(QLineF(candidate->my_line.p1(), coord));
                    ik_preview_line->setPen(preview_line_pen);
                    ik_preview_line->setZValue(1000);
                    scene()->addItem(ik_preview_line);
                    // stessa croce del marcatore persistente (drawForeground): dimensione in
                    // unità di scena legata a handle_size e penna COSMETICA, così è ben
                    // visibile a ogni zoom. Prima era 10px con penna non-cosmetica da 1 unità:
                    // su una scena grande a zoom-adatta risultava quasi invisibile (bug Group D).
                    float s = handle_size * 1.1f;
                    QPainterPath cross_path;
                    cross_path.moveTo(-s,-s); cross_path.lineTo(s,s);
                    cross_path.moveTo(s,-s); cross_path.lineTo(-s,s);
                    QPen cross_pen(QColor(220, 30, 30), 2.5);
                    cross_pen.setCosmetic(true);
                    ik_preview_cross = new QGraphicsPathItem(cross_path);
                    ik_preview_cross->setPen(cross_pen);
                    ik_preview_cross->setZValue(1000);
                    ik_preview_cross->setPos(coord);
                    scene()->addItem(ik_preview_cross);
                    break;
                }
                // altrimenti: click a vuoto, scartato (spec)
                break;
            }
    }
    }
    UpdateIkOverlay(); // un click DRAW_IK può creare/attivare/cancellare una catena
    //QGraphicsView::mousePressEvent(event);
}

/**
 * @details Two shortcuts. Delete/Cancel removes the selected stick: if it's the figure's
 * master (and other layers exist) the whole figure goes via DeleteStickFigure(),
 * otherwise just that stick via StickFigure::DeleteStick(), pruning the removed sticks
 * from total_sticks and moving selection to the parent. Escape cancels a half-drawn
 * stick (StickFigure::CancelDrawing()). Both are snapshotted with StoreUndo() first.
 */
void MyView::keyPressEvent(QKeyEvent *event)
{
    // draw mode: Esc/Enter escono dalla sessione. Ctrl+Z/Y sono gestiti dai QShortcut del
    // costruttore (che consumano il tasto prima di qui) e pilotano l'undo locale di sessione
    if(draw_mode){
        if((event->modifiers() & Qt::ControlModifier) && event->key()==Qt::Key_D){
            DiscardPixelSelection(); // scarta la selezione pixel (spec)
            return;
        }
        if(event->key()==Qt::Key_Escape || event->key()==Qt::Key_Return || event->key()==Qt::Key_Enter)
            ExitDrawMode();
        return;
    }
    switch(event->key())
    {
        case(Qt::Key_Cancel):
        case(Qt::Key_Delete):
        {
            // con una selezione multipla il tasto Canc elimina tutte le figure selezionate
            if(MultiSelectionActive()){
                DeleteMultiSelection();
                break;
            }
            if(scene()->selectedItems().count() == 1 && my_animation->CurrentFrame()->CurrentStickFigure() != nullptr
                    && my_animation->CurrentFrame()->CurrentStickFigure()->draw_count==0)
            {
                StickGraphicsItem *item = (StickGraphicsItem*)scene()->selectedItems()[0];//oggetto selezionato nella scnea
                StickData *item_data = item->data;
                int idx = my_animation->CurrentFrame()->CurrentStickFigure()->stick_list.indexOf(item_data);        // indice corrispondente nella lista ordinata degli oggetti
                // l'item selezionato nella scena può non appartenere allo stickfigure corrente:
                // senza questo controllo stick_list[-1] legge fuori dai limiti (l'undo va preso
                // DOPO, così un Delete a vuoto non lascia uno snapshot senza modifica sullo stack)
                if(idx < 0)
                    break;
                StoreUndo();
                // se si tratta dello stick master e ci sono altri stickfigure, cancella l'intero stickfigure
                if(my_animation->CurrentFrame()->CurrentStickFigure()->stick_list[idx] == my_animation->CurrentFrame()->CurrentStickFigure()->master_stick &&
                        my_animation->current_frame->stick_figures.count()>1)
                {
                    //estrai dalla lista grafica l'elemento selezionato collegato allo stickfigure
                    QListWidgetItem * associated_item = my_animation->CurrentFrame()->CurrentStickFigure()->linked_item;
                    my_stick_figure_widget_list->clearSelection();
                    associated_item->setSelected(true);
                    //cancella lo stickfigure
                    DeleteStickFigure();
                }
                //altrimenti cancella il singolo stick e aggiorna l'iconea del livello
                else if(my_stick_figure_widget_list->selectedItems().count()==1)
                {
                    StickData* next_current_stick = my_animation->CurrentFrame()->CurrentStick()->parent;
                    // questa lista serve per rimuovere dalla totalStick tutti gli stick cancellati
                    QList<StickData*> old_stick_list = my_animation->CurrentFrame()->CurrentStickFigure()->stick_list;
                    my_animation->CurrentFrame()->CurrentStickFigure()->DeleteStick( idx);
                    QList<StickData*> survivors = my_animation->CurrentFrame()->CurrentStickFigure()->stick_list;
                    for(StickData*s:old_stick_list){
                        if(!survivors.contains(s)){
                            my_animation->CurrentFrame()->total_sticks.removeAll(s);
                        }
                    }
                    my_animation->CurrentFrame()->num_of_items = my_animation->CurrentFrame()->total_sticks.length();
                    my_animation->CurrentFrame()->CurrentStickFigure()->UpdateIcon();
                    my_stick_figure_widget_list->selectedItems()[0]->setIcon(*my_animation->CurrentFrame()->CurrentStickFigure()->stick_figure_icon);
                    my_animation->CurrentFrame()->CurrentStick() = next_current_stick;
                }
                my_animation->current_frame->UpdateIcon();
                // distruggi l'oggetto
                qDebug("item deleted");
            }
            break;
        }
        case(Qt::Key_Escape):
        {
            if(MultiSelectionActive()){
                ClearMultiSelection(); // Esc scioglie la selezione multipla (spec)
                break;
            }
            switch(tool)
            {
                case(DRAW):
                case DRAWCIRCLE:
                case DRAWFREEHAND: // Esc prima del 2° click scarta l'anteprima gialla a metà
                case DRAWTEXT:     // idem per il testo (dopo il 2° click c'è il prompt, non un Esc)
                {
                    if(my_animation->CurrentFrame()->CurrentStickFigure() != nullptr &&
                            my_animation->CurrentFrame()->CurrentStickFigure()->draw_count==1)
                    {
                        my_animation->CurrentFrame()->CurrentStickFigure()->CancelDrawing();
                    }
                    break;
                }
                case(DRAWPOLY):
                {
                    // prima del secondo click della base Esc annulla come per gli altri
                    // DRAW*; a base completata termina soltanto la sessione di vertici
                    if(my_animation->CurrentFrame()->CurrentStickFigure() != nullptr &&
                            my_animation->CurrentFrame()->CurrentStickFigure()->draw_count==1)
                    {
                        my_animation->CurrentFrame()->CurrentStickFigure()->CancelDrawing();
                        poly_session_active = false;
                    }
                    else if(poly_session_active)
                    {
                        FinishPolygonSession();
                    }
                    break;
                }
                case(DRAWMULTI):
                {
                    // per la multilinea Esc ANNULLA il disegno (spec): la base a metà si
                    // scarta come per i DRAW*, uno stick già committato viene eliminato
                    if(my_animation->CurrentFrame()->CurrentStickFigure() != nullptr &&
                            my_animation->CurrentFrame()->CurrentStickFigure()->draw_count==1)
                    {
                        my_animation->CurrentFrame()->CurrentStickFigure()->CancelDrawing();
                        poly_session_active = false;
                    }
                    else if(poly_session_active)
                    {
                        StickData *session = PolySessionStick(StickData::MULTILINE);
                        FinishPolygonSession();
                        if(session != nullptr){
                            StoreUndo();
                            DeleteWholeStick(session);
                        }
                    }
                    break;
                }
                case(DRAW_IK):
                {
                    if(ik_drawing)
                        CancelIkDrawing();
                    break;
                }
            }
            break;
        }
        case(Qt::Key_Return):
        case(Qt::Key_Enter):
        {
            if((tool == DRAWPOLY || tool == DRAWMULTI) && poly_session_active)
                FinishPolygonSession();
            break;
        }
        case(Qt::Key_Space):
        {
            // Spazio tenuto premuto: trascina la selezione (una o più figure) seguendo il
            // cursore, come se la si prendesse dallo stick master (spec). Un'unica voce di
            // undo per il gesto; il movimento avviene in mouseMoveEvent finché è premuto.
            if(ToolUsesSelection() && HasActiveSelection() && !event->isAutoRepeat()){
                StoreUndo();
                space_dragging = true;
                starting_coord = coord; // `coord` è aggiornata da ogni mouseMoveEvent
            }
            break;
        }
    }

    QGraphicsView::keyPressEvent(event);
}

/**
 * @details Only job: releasing Space ends the drag-by-master gesture and refreshes the
 * moved figures' icons and the selection decorations.
 */
void MyView::keyReleaseEvent(QKeyEvent *event)
{
    if(event->key() == Qt::Key_Space && !event->isAutoRepeat() && space_dragging){
        space_dragging = false;
        for(StickFigure *S : SelectedFigures()){
            S->UpdateIcon();
            if(S->linked_item)
                S->linked_item->setIcon(*S->stick_figure_icon);
        }
        if(my_animation->current_frame != nullptr)
            my_animation->current_frame->UpdateIcon();
        UpdateSelectionDecorations();
    }
    QGraphicsView::keyReleaseEvent(event);
}

/**
 * @details Phase 3 pen pressure. During a draw session a stylus generates QTabletEvent
 * *and* (because we don't accept it) a synthesized QMouseEvent that follows and does the
 * actual painting — so this override's only job is to latch the pen's current pressure into
 * draw_pressure_val, which PressureFactor() (via StampBrush()/PaintStrokeSegment(), each gated
 * by its own draw_pressure_size/draw_pressure_opacity checkbox) reads a moment later. Outside
 * draw mode we ignore the tablet entirely and let the synthesized mouse path run as before.
 * A mouse (no tablet events) never touches draw_pressure_val, which the stroke reset keeps
 * at 1.0, so mouse input stays full-pressure.
 */
void MyView::tabletEvent(QTabletEvent *event)
{
    if(draw_mode)
        draw_pressure_val = qBound(0.0, event->pressure(), 1.0);
    event->ignore(); // non accettato di proposito: Qt sintetizza il QMouseEvent che disegna
}

/**
 * @details Only job: a double click ends a DRAWPOLY/DRAWMULTI vertex session. The
 * first press of the pair has already been delivered to mousePressEvent() and has
 * placed the final vertex, so closing the session is all that's left to do.
 */
void MyView::mouseDoubleClickEvent(QMouseEvent *event)
{
    if(event->button() == Qt::LeftButton && (tool == DRAWPOLY || tool == DRAWMULTI)
            && poly_session_active){
        FinishPolygonSession();
        return;
    }
    QGraphicsView::mouseDoubleClickEvent(event);
}
/**
 * @details Picking routine behind the NOTOOL/CLAMP/ELONGATE/MANIPULATE tools: asks the
 * Animation which stick is under `coord` (Animation::UpdateSelection()), then mirrors
 * that choice into the layer list widget and repopulates the image list from the picked
 * stick's view. Bails out on an empty frame so the CurrentStickFigure()/CurrentStick()
 * dereferences below are always safe.
 */
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
/**
 * @details Drawing a stick takes two clicks, tracked by the figure's draw_count.
 * First click: StickFigure::StartDrawing() creates the StickData, FramePresenter::
 * AttachView() gives it a live view on the scene, and `mode` sets the resulting shape
 * (CIRCLE/RECT/TAPER/TRAPEZOID/IMAGE) plus its pen/slider parameters. Second click:
 * StickFigure::EndDrawing() fixes the free end, the endpoint handles go on the scene and
 * the stick is appended to the frame's total_sticks z-order list. In between,
 * mouseMoveEvent() calls StickFigure::PreviewDrawing() so the free end follows the mouse.
 */
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
            new_stick->brush_follows_stick = my_brush_follows_stick; // come pen/brush: si eredita dallo stato corrente
            new_stick->texture_tiled = my_texture_tiled;
            new_stick->stroke_brush = my_stroke_brush;
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
            else if(mode == DRAWPOLY){
                new_stick->stick_type = StickData::POLYGON;
            }
            else if(mode == DRAWMULTI){
                new_stick->stick_type = StickData::MULTILINE;
                // larghezza/curvatura per i due estremi iniziali (indici virtuali 0 e 1)
                new_stick->vertex_widths << my_pen.widthF() << my_pen.widthF();
                new_stick->vertex_curvatures << 0.0f << 0.0f;
            }
            else if(mode == DRAWTEXT){
                // il testo arriva al 3° passo (dopo il 2° click, sotto): finché è vuoto lo
                // stick dipinge la sua spina, che è esattamente l'anteprima del gesto
                new_stick->stick_type = StickData::TEXT;
            }
            else if(mode == DRAWFREEHAND){
                // durante il gesto a 2 click lo stick resta una LINE dipinta in GIALLO: è
                // solo un'anteprima per vedere dove lo si sta piazzando, come un normale
                // stick. Al secondo click diventa un FREEHAND invisibile (vedi sotto) prima
                // di entrare in draw mode; nessun placeholder immagine serve finché è LINE
                QPen preview_pen(Qt::yellow);
                preview_pen.setWidth(qMax(2, my_pen.width()));
                preview_pen.setCapStyle(Qt::RoundCap);
                new_stick->pen = preview_pen;
            }

           qDebug("Draw 1 = %f, %f",point->x(),point->y());
        }
        else if(my_animation->CurrentFrame()->CurrentStickFigure()->draw_count >0)
        {
            my_animation->CurrentFrame()->CurrentStickFigure()->EndDrawing(point);
            scene()->addItem(my_animation->CurrentFrame()->CurrentStick()->view->stick_handle[0]);
            scene()->addItem(my_animation->CurrentFrame()->CurrentStick()->view->stick_handle[1]);
            // se questo è il primo stick della stickfigure (= il suo master), colora la maniglia
            // di verde: StartDrawing() non può farlo da sé (non esiste ancora una view a quel punto,
            // vedi il suo commento), quindi tocca a questa chiamata subito dopo AttachView()
            if(my_animation->CurrentFrame()->CurrentStick() == my_animation->CurrentFrame()->CurrentStickFigure()->master_stick){
                my_animation->CurrentFrame()->CurrentStick()->view->stick_handle[0]->setBrush(QBrush(Qt::green));
                my_animation->CurrentFrame()->CurrentStick()->view->stick_handle[0]->setZValue(101);
            }
            qDebug("Draw 2 = %f, %f",point->x(),point->y());
            // 3° passo del testo: inserimento del contenuto. Va PRIMA di UpdateIcon/
            // UpdateTab/UpdateSelection qui sotto: sono loro a rispecchiare lo stick nel
            // pannello (Animation::UpdateSliders -> refresh_text_panel) e a ridisegnare
            // l'anteprima del frame, quindi il testo deve già esserci. Annullare (o
            // confermare a vuoto) scarta lo stick appena creato, come un Esc a metà
            // disegno: lo StoreUndo() del 1° click copre già l'intera operazione, e
            // DeleteWholeStick() aggiorna da sé le icone di figura e frame
            if(mode == DRAWTEXT){
                StickData *ts = my_animation->CurrentFrame()->CurrentStick();
                bool confirmed = false;
                QString typed = QInputDialog::getMultiLineText(this, tr("Text stick"),
                                                               tr("Text:"), QString(),
                                                               &confirmed);
                if(!confirmed || typed.isEmpty()){
                    DeleteWholeStick(ts);
                    return;
                }
                ts->text = typed;
                if(ts->view) ts->view->Refresh(0);
            }
            my_animation->current_frame->UpdateIcon();
            my_animation->UpdateTab(my_animation->CurrentFrame()->CurrentStick()->stick_type);
            // add it to the total scene list
            my_animation->current_frame->total_sticks.append(my_animation->CurrentFrame()->CurrentStick());
            my_animation->UpdateSelection(my_animation->CurrentFrame()->CurrentStick());

            // il freehand entra in draw mode subito dopo il secondo click (spec). Ora che
            // la posizione è fissata, l'anteprima gialla LINE diventa un FREEHAND invisibile
            // (NoPen); EnterDrawMode() gli assegna il canvas. La creazione ha già fatto
            // StoreUndo() sopra, quindi non ne serve un altro
            if(mode == DRAWFREEHAND){
                StickData *fh = my_animation->CurrentFrame()->CurrentStick();
                fh->stick_type = StickData::FREEHAND;
                fh->pen.setStyle(Qt::NoPen);
                EnterDrawMode(fh, false);
            }
        }
    }
    else
    {
        QMessageBox::about(this,"error","no stickfigure layer present, please add a stickfigure");
    }


}

// ===================== Freehand draw mode ============================================
// A FREEHAND stick (StickData::FREEHAND, stickdata.h) is an IMAGE-like raster stick. Draw
// mode replaces its image with a live, viewport-sized canvas painted in place, then on exit
// crops it to the drawn area and re-anchors the stick. The canvas maps 1 pixel per scene unit
// over the enter-time visible rect (draw_region) — so mouse->pixel is a plain subtraction at
// any pan/zoom.
//
// Lifecycle:   DrawCmd()'s DRAWFREEHAND branch (the Freehand toolbar tool) creates the
//              invisible 2-click stick and calls EnterDrawMode(); ExitDrawMode() (Esc/Enter,
//              any tool switch, or a save/load/render that must finalize the image) crops and
//              re-anchors it. draw_mode gates every mouse/key event in the input overrides.
// Painting:    mousePressEvent/mouseMoveEvent stamp the brush along the drag — StampBrush()
//              (one dab, built from MakeRoundBrush()) via PaintStrokeSegment() (spacing +
//              interpolation). tabletEvent() latches pen pressure into draw_pressure_val
//              (Phase 3); the eraser composites DestinationOut.
// Undo:        a draw-local chain (PushDrawUndo()/DrawStrokeUndo()/DrawStrokeRedo()), driven
//              by MyView::Undo()/Redo() while draw_mode is on — it NEVER touches the frame
//              undo_stack (one frame-undo entry, taken at EnterDrawMode(), reverts the session).
// Beneficiaries: the Freehand toolbar button (widget.cpp on_drawFreehandBtn_clicked) and the draw-mode
//              panel (widget.cpp advanced_draw_tab: brush/eraser/size/opacity/colour/pressure,
//              plus the pixel-selection tools documented in the next section).
//
// ponytail: 1px == 1 scene unit; screen-DPI-independent, and pan/zoom are frozen for the
// session (the canvas is fixed to the viewport captured on entry). Wider-than-viewport
// or re-scaled-resolution painting is out of scope for Phase 1.

/// @details Soft round grayscale-alpha stamp: opaque core, feathered edge. Rebuilt whenever
/// hardness or roundness change (the panel does it) and re-scaled per dab; the color is applied
/// at stamp time, not here. Hardness is where the gradient stops being fully opaque, so 100%
/// is a hard-edged disc and 0% feathers from the very centre. Roundness squashes the whole tip
/// — feather included — via the painter transform, so the mask stays square and the caller's
/// uniform scaling keeps working; the tip's rotation is NOT baked in (see StampBrush()).
QImage MyView::MakeRoundBrush(int size, int hardness, int roundness)
{
    if(size < 1) size = 1;
    QImage mask(size, size, QImage::Format_ARGB32_Premultiplied);
    mask.fill(Qt::transparent);
    QRadialGradient g(size/2.0, size/2.0, size/2.0);
    g.setColorAt(0.0, QColor(0,0,0,255));
    // il core deve stare SOTTO 1.0: setColorAt() sostituisce uno stop alla stessa posizione,
    // quindi a durezza 100 mangerebbe lo stop trasparente del bordo e darebbe la punta più
    // morbida possibile invece della più dura. A 0.99 la sfumatura è larga meno di un pixel.
    g.setColorAt(qMin(0.99, qBound(0, hardness, 100)/100.0), QColor(0,0,0,255));
    g.setColorAt(1.0, QColor(0,0,0,0));
    QPainter p(&mask);
    p.setRenderHint(QPainter::Antialiasing);
    p.setPen(Qt::NoPen);
    p.setBrush(g);
    p.translate(size/2.0, size/2.0);
    p.scale(1.0, qBound(1, roundness, 100)/100.0);
    p.translate(-size/2.0, -size/2.0);
    p.drawEllipse(0, 0, size, size);
    p.end();
    // stessa forma delle punte importate (MaskToStamp): di un timbro conta solo l'alpha,
    // quindi tenerlo a 8 bit invece che a 32 — qui è poca roba (128x128), ma è il formato
    // che ogni consumatore si aspetta di poter ricevere indifferentemente
    return mask.convertToFormat(QImage::Format_Alpha8);
}

/// @details Tight bounding rect of the non-transparent pixels (alpha > 0). Empty rect
/// when the image is fully transparent — the "nothing painted" signal for ExitDrawMode().
QRect MyView::OpaqueBounds(const QImage &img)
{
    if(img.isNull())
        return QRect();
    QImage a = img.convertToFormat(QImage::Format_ARGB32);
    int min_x = a.width(), min_y = a.height(), max_x = -1, max_y = -1;
    for(int y = 0; y < a.height(); y++){
        const QRgb *row = reinterpret_cast<const QRgb*>(a.constScanLine(y));
        for(int x = 0; x < a.width(); x++){
            if(qAlpha(row[x]) != 0){
                if(x < min_x) min_x = x;
                if(x > max_x) max_x = x;
                if(y < min_y) min_y = y;
                if(y > max_y) max_y = y;
            }
        }
    }
    if(max_x < 0)
        return QRect();
    return QRect(QPoint(min_x, min_y), QPoint(max_x, max_y));
}

/// @details Pen pressure (0..1 from tabletEvent) is remapped so a "normal", half-way press
/// (0.5) reproduces the dialled-in size/opacity exactly, instead of always undershooting it
/// (a plain 0..1 multiplier would make even a firm press cut the selected value in half).
/// Harder than normal scales up to 2x at full pressure; softer than normal scales down toward 0.
double MyView::PressureFactor() const
{
    return draw_pressure_val / 0.5;
}

/// @details The loose draw_brush_* fields as the one struct that describes a brush, so draw mode
/// and a stick's brush BORDER stamp through the same BrushDab(). They stay loose fields because
/// each is wired to its own control in the draw-mode panel; this is the assembly point.
BrushStroke MyView::DrawBrush() const
{
    BrushStroke brush;
    brush.stamp = draw_brush_stamp;
    brush.color = draw_brush_color;
    brush.size = draw_brush_size;
    brush.opacity = qBound(0, draw_brush_opacity, 100)/100.0;
    brush.spacing = draw_brush_spacing;
    brush.angle = draw_brush_angle;
    brush.size_jitter = draw_brush_size_jitter;
    brush.angle_jitter = draw_brush_angle_jitter;
    return brush;
}

/// @details One brush dab centered at a canvas-pixel point, built by BrushDab() (abrbrush.cpp)
/// and composited here: the eraser clears instead of painting (DestinationOut). Size and opacity
/// each read pen pressure only when their own checkbox (draw_pressure_size /
/// draw_pressure_opacity) is on — the two are independent, so e.g. pressure can drive size
/// while opacity stays fixed at the dialled-in value, or vice versa.
void MyView::StampBrush(const QPointF &canvas_px)
{
    if(!draw_canvas)
        return;
    qreal dab_opacity = 1.0;
    const QImage dab = BrushDab(DrawBrush(),
                                draw_pressure_size    ? PressureFactor() : 1.0,
                                draw_pressure_opacity ? PressureFactor() : 1.0,
                                &dab_opacity);
    if(dab.isNull())
        return;

    QPainter p(draw_canvas);
    p.setOpacity(dab_opacity);
    if(draw_eraser)
        p.setCompositionMode(QPainter::CompositionMode_DestinationOut);
    // il dab ruotato è più grande di s: centralo sulle sue dimensioni effettive, non su s
    p.drawImage(QPointF(canvas_px.x() - dab.width()/2.0, canvas_px.y() - dab.height()/2.0), dab);
    p.end();
}

/// @details Stamps the brush along a scene-space segment, spaced by draw_brush_spacing (a
/// percentage of the current, pressure-adjusted stamp diameter) so a fast drag still lays
/// down a continuous line rather than visibly separate dabs. Scene points map to canvas
/// pixels through draw_disp_inv (the inverse of the same transform paint() uses), so painting
/// happens in the stick's rotated frame: what you draw on screen lands where the cursor is,
/// and is stored already rotated with the stick. Then repaints the view.
///
/// The pitch is measured along the WHOLE stroke, not per segment: draw_dab_carry remembers how
/// far the cursor travelled past the last dab, so the leftover is consumed by the next segment.
/// Without it every mouse-move stamped a dab at its own start point (moves are only a few pixels
/// apart), which silently capped the effective spacing at "one dab per event" — a spacing above
/// ~100% looked identical to a much lower one.
void MyView::PaintStrokeSegment(const QPointF &from_scene, const QPointF &to_scene)
{
    if(!draw_canvas)
        return;
    QPointF half(draw_canvas->width()/2.0, draw_canvas->height()/2.0);
    QPointF a = draw_disp_inv.map(from_scene) + half;
    QPointF b = draw_disp_inv.map(to_scene)   + half;
    double dist = QLineF(a, b).length();
    double size_factor = draw_pressure_size ? PressureFactor() : 1.0;
    // spacing tracks the actual (pressured) dab size, scaled by the user's spacing percentage
    double spacing = qMax(1.0, draw_brush_size * size_factor * (draw_brush_spacing / 100.0));
    if(dist <= 0.0){
        StampBrush(a);          // inizio pennellata (click singolo): primo dab, la corsa riparte da qui
        draw_dab_carry = 0.0;
    } else {
        for(double d = spacing - draw_dab_carry; d <= dist; d += spacing)
            StampBrush(a + (b - a) * (d / dist));
        draw_dab_carry = fmod(draw_dab_carry + dist, spacing);
    }
    if(draw_target && draw_target->view){
        draw_target->view->Refresh(0);
        draw_target->view->update();
    }
}

/// @details Session-local undo push: drops any redo tail, then snapshots the canvas. This
/// chain lives only while draw_mode is active and never touches undo_stack (spec).
///
/// Capped like undo_stack is (MAXUNDO), and for a much more pressing reason: an entry here is
/// a WHOLE canvas, and the canvas is a square the size of the viewport's diagonal — about
/// 19 MB at 1080p. Uncapped, a long drawing session kept one per stroke and ran into the
/// gigabytes. The oldest snapshot goes first, so the depth is the last MAXDRAWUNDO strokes.
///
/// ponytail: whole-canvas snapshots; per-stroke dirty rectangles if the cap ever feels tight.
void MyView::PushDrawUndo()
{
    if(!draw_canvas)
        return;
    while(draw_undo.count() > draw_undo_idx + 1)
        draw_undo.removeLast();
    draw_undo.append(*draw_canvas);
    while(draw_undo.count() > MAXDRAWUNDO)
        draw_undo.removeFirst(); // scartando dalla testa l'indice corrente scala con la lista
    draw_undo_idx = draw_undo.count() - 1;
}

void MyView::DrawStrokeUndo()
{
    if(!draw_canvas || draw_undo_idx <= 0)
        return;
    draw_undo_idx--;
    *draw_canvas = draw_undo[draw_undo_idx];
    if(draw_target && draw_target->view){
        draw_target->view->Refresh(0);
        draw_target->view->update();
    }
}

void MyView::DrawStrokeRedo()
{
    if(!draw_canvas || draw_undo_idx + 1 >= draw_undo.count())
        return;
    draw_undo_idx++;
    *draw_canvas = draw_undo[draw_undo_idx];
    if(draw_target && draw_target->view){
        draw_target->view->Refresh(0);
        draw_target->view->update();
    }
}

/// @details In draw mode the toolbar Undo/Redo buttons and the Ctrl+Z/Y shortcuts drive
/// the session-local stroke chain (the freehand image state only); outside draw mode they
/// drive the global frame undo_stack. Routing both triggers here keeps them consistent.
void MyView::Undo()
{
    if(draw_mode){ CommitPixelFloat(); DrawStrokeUndo(); } // fondi un'eventuale trasformazione in corso
    else          undo_stack.undo();
}

void MyView::Redo()
{
    if(draw_mode){ CommitPixelFloat(); DrawStrokeRedo(); }
    else          undo_stack.redo();
}

/**
 * @details Bakes the stick's current transformed appearance (offset/scale/rotation) into a
 * fresh square canvas — the spec's "collapse changes into pixels the moment draw mode is
 * entered" — then makes that canvas the stick's live image, displayed in the stick's own
 * rotated frame (img_angle = the line angle) covering the viewport. Because painting then
 * happens in that frame, the drawing is born at the stick's position/rotation and needs no
 * re-rotation on exit. Handles are hidden, the draw panel is shown with the brush selected,
 * and the draw-local undo is seeded. take_undo pushes one frame-undo snapshot so the whole
 * session is a single global-undo step; the freshly-created path passes false (DrawCmd
 * already snapshotted).
 */
void MyView::EnterDrawMode(StickData *stick, bool take_undo)
{
    if(!stick)
        return;
    if(draw_mode)
        ExitDrawMode();
    if(take_undo)
        StoreUndo();

    draw_target = stick;
    draw_orig_line = stick->my_line; // the 2-click skeleton: origin (p1) and the stick's rotation
    if(draw_orig_line.length() < 1.0) // the 2 clicks coincided: give it a fake length/rotation
        draw_orig_line = QLineF(draw_orig_line.p1(), draw_orig_line.p1() + QPointF(0, 100));

    draw_region = mapToScene(viewport()->rect()).boundingRect();
    QPointF vc = draw_region.center(); // viewport centre, in scene coordinates
    // Make the canvas a SQUARE whose side is the viewport's diagonal: at that size, no matter
    // what angle it's rotated to (see theta below), it still covers the entire visible viewport.
    int D = int(ceil(sqrt(draw_region.width()*draw_region.width()
                          + draw_region.height()*draw_region.height())));
    if(D < 1) D = 1;

    // Display rotation = the stick's own line angle (same convention IMAGE sticks use: img_angle
    // follows the stick's direction). The canvas lives and paints in this already-rotated frame,
    // so the drawing is born at the stick's position+rotation directly — no re-rotation is needed
    // on exit — and it keeps rotating with the stick from then on.
    double theta = -atan2(draw_orig_line.dx(), draw_orig_line.dy()) * 180.0 / M_PI;
    double len = draw_orig_line.length();
    QPointF p1 = draw_orig_line.p1();
    draw_theta = theta;

    // Solve for the img_offset that puts the canvas's CENTRE on the viewport's centre. paint()
    // computes scene = p1 + R(theta)*(img_offset + (pixel - D/2)); at the canvas centre pixel
    // (D/2), that reduces to vc = p1 + R(theta)*img_offset, so img_offset = R(-theta)*(vc - p1) —
    // R(-theta), the inverse rotation, is what QTransform::inverted() below builds and applies.
    QTransform rot; rot.rotate(theta);
    QPointF img_offset = rot.inverted().map(vc - p1);

    // The canvas's display transform, built to be IDENTICAL to what paint() uses later:
    // scene = draw_disp.map(pixel - D/2)
    QTransform disp;
    disp.translate(p1.x(), p1.y());
    disp.rotate(theta);
    disp.translate(img_offset.x(), img_offset.y());
    draw_disp = disp;
    draw_disp_inv = disp.inverted();

    // The stick's CURRENT on-screen appearance (needed to re-open an existing drawing for
    // editing) is captured here, before anything below overwrites the stick's own transform.
    QImage old_scaled;
    QTransform old_disp; // old appearance's centred local pixel -> scene transform
    if(stick->view && stick->stick_img && !stick->stick_img->isNull()){
        old_scaled = stick->view->CalcImg();
        old_disp.translate(stick->my_line.p1().x(), stick->my_line.p1().y());
        old_disp.rotate(stick->img_angle + stick->img_angle_offset);
        old_disp.translate(stick->img_offset.x(), stick->img_offset.y());
    }

    QImage *canvas = new QImage(D, D, QImage::Format_ARGB32_Premultiplied);
    canvas->fill(Qt::transparent);
    // Bake the old appearance (positioned in SCENE space by old_disp) into the new canvas.
    // Four QTransform stages chained with operator* into one matrix — for `A * B`, a point is
    // mapped by A first, then B, so reading left-to-right below IS the pipeline order:
    //   old raw pixel -> centred on its own middle -> placed in the scene (old_disp) ->
    //   back into the new canvas's centred pixel space (draw_disp_inv, the exact inverse of
    //   draw_disp above) -> shifted from centred to the canvas's raw top-left addressing.
    if(!old_scaled.isNull()){
        QTransform bake =
              QTransform::fromTranslate(-old_scaled.width()/2.0, -old_scaled.height()/2.0) // raw -> centred
            * old_disp                                                                     // -> scene
            * draw_disp_inv                                                                // -> new canvas, centred
            * QTransform::fromTranslate(D/2.0, D/2.0);                                      // -> new canvas, raw
        QPainter cp(canvas);
        cp.setRenderHint(QPainter::SmoothPixmapTransform);
        cp.setTransform(bake);
        cp.drawImage(0, 0, old_scaled);
        cp.end();
    }

    qDeleteAll(stick->stick_img_list);
    stick->stick_img_list.clear();
    stick->img_name_list.clear();
    stick->stick_img_list.append(canvas);
    stick->img_name_list.append("freehand");
    stick->stick_img = canvas;
    draw_canvas = canvas;

    // lo stick resta nel suo frame reale (scheletro dei 2 click); il canvas DxD copre il
    // viewport ruotato di theta e centrato su vc
    stick->my_line = draw_orig_line;
    stick->img_angle = theta;
    stick->img_angle_offset = 0;
    stick->img_offset = img_offset;
    stick->img_w_scale = float(D / len); // CalcImg riporta il canvas DxD a dimensione naturale
    stick->img_h_scale = float(D / len);

    if(stick->view){
        stick->view->stick_handle[0]->hide();
        stick->view->stick_handle[1]->hide();
        stick->view->Refresh(0);
        stick->view->update();
    }
    draw_undo.clear();
    draw_undo_idx = -1;
    PushDrawUndo(); // stato iniziale della sessione
    draw_stroking = false;
    draw_eraser = false; // pennello selezionato all'ingresso (spec)
    draw_tool = DT_BRUSH;
    draw_selection = QRegion(); draw_sel_bounds = QRect(); // nessuna selezione pixel all'ingresso
    draw_float_active = false; draw_float = QImage(); draw_sel_gesture = 0;
    draw_sel_t = QPointF(); draw_sel_r = 0; draw_sel_sx = draw_sel_sy = 1;
    draw_mode = true;
    // mostra subito il pannello draw mode e rendilo il tab attivo (spec)
    my_animation->UpdateTab(StickData::FREEHAND);
    if(my_tab_widget && advanced_draw_tab)
        my_tab_widget->setCurrentWidget(advanced_draw_tab);
    emit DrawModeChanged(true);
}

/**
 * @details Crops the canvas to the painted pixels (spec: "bbox optimized to actual drawing
 * size") and re-anchors the cropped image on the stick's own skeleton at the stick's
 * rotation (the drawing was painted in that frame, so no re-rotation is needed), then
 * discards the session's undo chain. When nothing was painted the canvas is left a 1x1
 * transparent placeholder
 * (ponytail: keep an invisible degenerate stick rather than tear down a possibly-master
 * stick and its figure; the user can delete it normally). Safe when not in draw mode.
 */
void MyView::ExitDrawMode()
{
    if(!draw_mode)
        return;
    DiscardPixelSelection(); // a floating pixel selection is baked into the canvas before cropping
    StickData *stick = draw_target;
    QImage *canvas = draw_canvas;
    // Clear all draw-mode state BEFORE any callback below runs, so a reentrant SetTool()/
    // ExitDrawMode() call (e.g. triggered by emit DrawModeChanged() further down) sees
    // draw_mode already false and returns immediately instead of recursing.
    draw_mode = false;
    draw_target = nullptr;
    draw_canvas = nullptr;
    draw_stroking = false;
    draw_undo.clear();
    draw_undo_idx = -1;
    emit DrawModeChanged(false); // keeps the panel's toggle button in sync on every exit path

    if(!stick || !canvas)
        return;

    QRect bounds = OpaqueBounds(*canvas);
    // Capture the canvas's dimensions NOW: canvas is the same QImage* as stick->stick_img_list[0],
    // so the qDeleteAll() below frees it — any read of canvas->width()/height() after that point
    // would be a use-after-free.
    double canvas_half_w = canvas->width()/2.0, canvas_half_h = canvas->height()/2.0;
    if(bounds.isEmpty())
        bounds = QRect(int(canvas_half_w), int(canvas_half_h), 1, 1); // nothing was painted: 1x1 invisible placeholder

    QImage *cropped = new QImage(canvas->copy(bounds));
    qDeleteAll(stick->stick_img_list);
    stick->stick_img_list.clear();
    stick->img_name_list.clear();
    stick->stick_img_list.append(cropped);
    stick->img_name_list.append("freehand");
    stick->stick_img = cropped;

    // The drawing is ALREADY sitting in the stick's own rotated frame (that's the whole point of
    // painting in a canvas pre-rotated by EnterDrawMode), so no re-rotation is needed here — just
    // crop it and re-anchor the cropped image on the same skeleton/rotation, exactly like an
    // IMAGE stick, so it stays exactly where it was painted and keeps rotating with the stick.
    double bw = bounds.width(), bh = bounds.height();
    QPointF center_px(bounds.x() + bw/2.0, bounds.y() + bh/2.0);
    // Crop centre, in canvas-pixel space, re-centred (- canvas_half_w/h) then mapped through
    // draw_disp — the same centred-pixel-to-scene transform EnterDrawMode() built — to land in
    // scene coordinates.
    QPointF crop_center_scene = draw_disp.map(center_px - QPointF(canvas_half_w, canvas_half_h));
    double len = draw_orig_line.length();
    QPointF p1 = draw_orig_line.p1();
    QTransform rot; rot.rotate(draw_theta);
    stick->my_line = draw_orig_line;                               // normal skeleton/origin/rotation
    stick->img_angle = draw_theta;                                // follows the line, like an IMAGE
    stick->img_angle_offset = 0;
    // img_offset must place the image so its own centre lands on crop_center_scene; since
    // paint() applies scene = p1 + R(img_angle)*img_offset (+ ...), solving for img_offset means
    // undoing that rotation on the vector from p1 to the target centre — R(-theta), built here as
    // rot.inverted() (Qt QTransform), mapped over (crop_center_scene - p1).
    stick->img_offset = rot.inverted().map(crop_center_scene - p1);
    double max_dim = qMax(bw, bh);
    stick->img_w_scale = float(max_dim / len);                    // CalcImg() -> natural (1:1) size
    stick->img_h_scale = float(max_dim / len);

    if(stick->view){
        stick->view->stick_handle[0]->show();
        stick->view->stick_handle[1]->show();
        stick->view->Refresh(0);
        stick->view->update();
    }
    my_animation->current_frame->UpdateIcon();
    my_animation->UpdateSelection(stick);
}

// ===== Pixel selection & free-transform (draw mode) ==================================
// Draw mode's two marquee sub-tools — rectangle (DT_SEL_RECT) and lasso (DT_SEL_LASSO),
// chosen with SetDrawTool() — select a set of canvas pixels into draw_selection, then wrap
// them in a move/scale/rotate gizmo that LOOKS identical to the stickfigure transformation
// box but is fully separate: it transforms PIXELS (not figures), exists only while draw_mode
// is active, and is discarded on draw-mode exit, on a switch to brush/eraser, or on Ctrl+D
// (all routed through DiscardPixelSelection()). Shift adds to / Alt subtracts from the set.
//
// Coordinates: SceneToCanvas()/CanvasToScene() convert between the viewport-rotated canvas
//              pixels and the scene (draw_disp/draw_disp_inv, the same transform the paint
//              path uses), so selection math lines up with what the brush painted.
// Selection:   DrawSelPress()/DrawSelMove()/DrawSelRelease() build a QRegion and hand it to
//              SetPixelSelection() (replace/add/subtract). PixelGizmo() is the transform about
//              the box centre; GizmoHandles()/HitPixelHandle() give the handle geometry/hit-test.
// Float:       the first move/scale/rotate lifts the pixels into draw_float (LiftPixelFloat(),
//              punching a transparent hole); CommitPixelFloat() bakes them back and records one
//              draw-local undo step (see the Freehand section's undo chain).
// Rendering:   drawForeground() paints the floating pixels, the marching-ants marquee, and the
//              gizmo — the gizmo via the SHARED DrawTransformHandles() with draw_outline=true.
// Beneficiaries: the draw-mode panel's Select/Lasso buttons (widget.cpp advanced_draw_tab tool
//              row). This subsystem is the pixel-space twin of the stickfigure transformation
//              box below; the only shared code is DrawTransformHandles() (the renderer).

/// @details Scene coord -> canvas pixel (measured from the canvas top-left): the inverse of
/// draw_disp used by the selection/hit-test path, paired with CanvasToScene() below. draw_disp
/// maps a canvas pixel measured from the canvas centre into the scene, so this un-maps and
/// re-centres. Identity fallback when there is no canvas (outside draw mode).
QPointF MyView::SceneToCanvas(const QPointF &scene) const
{
    if(!draw_canvas) return scene;
    QPointF half(draw_canvas->width()/2.0, draw_canvas->height()/2.0);
    return draw_disp_inv.map(scene) + half;
}
/// @details Canvas pixel -> scene coord: the exact inverse of SceneToCanvas(), placing a
/// canvas pixel back where paint() draws it. Used to position handles/marquee in the scene.
QPointF MyView::CanvasToScene(const QPointF &canvas) const
{
    if(!draw_canvas) return canvas;
    QPointF half(draw_canvas->width()/2.0, draw_canvas->height()/2.0);
    return draw_disp.map(canvas - half);
}

/// @details The gizmo: translate+rotate+scale about the (base) selection-box centre, so
/// the handles and the lifted pixels share one transform. Identity when t/r=0 and s=1.
QTransform MyView::PixelGizmo() const
{
    QPointF c = QRectF(draw_sel_bounds).center();
    QTransform g;
    g.translate(c.x() + draw_sel_t.x(), c.y() + draw_sel_t.y());
    g.rotate(draw_sel_r);
    g.scale(draw_sel_sx, draw_sel_sy);
    g.translate(-c.x(), -c.y());
    return g;
}

/// @details Handle positions (canvas px, gizmo applied): 0..3 corners TL,TR,BR,BL;
/// 4..7 edge mids top,right,bottom,left; 8 = rotation knob above the top edge.
void MyView::GizmoHandles(QPointF out[9]) const
{
    QRectF r(draw_sel_bounds);
    QPointF base[9] = {
        r.topLeft(), r.topRight(), r.bottomRight(), r.bottomLeft(),
        (r.topLeft()+r.topRight())/2.0, (r.topRight()+r.bottomRight())/2.0,
        (r.bottomRight()+r.bottomLeft())/2.0, (r.bottomLeft()+r.topLeft())/2.0,
        QPointF(r.center().x(), r.top() - 24.0) // 24 canvas px above top-mid (scales with gizmo)
    };
    QTransform g = PixelGizmo();
    for(int i = 0; i < 9; i++)
        out[i] = g.map(base[i]);
}

/// @details Which pixel-gizmo handle a scene click grabs: 0-7 a scale handle, 8 the rotation
/// knob, -1 none. Handles are mapped to the scene (CanvasToScene) and matched within a screen-
/// constant radius (PolyPickRadius), so the pick feels the same at any zoom. Used by DrawSelPress.
int MyView::HitPixelHandle(const QPointF &scene) const
{
    if(draw_selection.isEmpty())
        return -1;
    QPointF handles[9];
    GizmoHandles(handles);
    float radius = const_cast<MyView*>(this)->PolyPickRadius(); // schermo-costante, come le altre maniglie
    for(int i = 0; i < 9; i++){
        if(QLineF(scene, CanvasToScene(handles[i])).length() <= radius)
            return i;
    }
    return -1;
}

/// @details Cut the selected pixels out of the canvas into draw_float (origin =
/// bounds.topLeft()), leaving a transparent hole — the standard "float on first transform"
/// so a move/scale/rotate drags real pixels and reveals what was underneath.
void MyView::LiftPixelFloat()
{
    if(!draw_canvas || draw_selection.isEmpty() || draw_float_active)
        return;
    QRect b = draw_sel_bounds;
    QRegion local = draw_selection.translated(-b.topLeft());
    QImage mask(b.size(), QImage::Format_ARGB32_Premultiplied);
    mask.fill(Qt::transparent);
    // Build a stencil the size of the selection bounds: opaque black inside the selected
    // region, transparent everywhere else. QPainter::setClipRegion() (Qt) restricts the
    // subsequent fillRect() to that region, so painting black over the whole mask rect
    // only actually paints inside the selection shape (which may be a lasso, not a rect).
    { QPainter mp(&mask); mp.setClipRegion(local); mp.fillRect(mask.rect(), Qt::black); }

    draw_float = draw_canvas->copy(b);
    // CompositionMode_DestinationIn (Qt) keeps only the destination pixels where the drawn
    // image (mask) is opaque, using the mask's alpha as a keep/discard test — this erases
    // everything in draw_float outside the (possibly non-rectangular) selection shape.
    { QPainter fp(&draw_float);
      fp.setCompositionMode(QPainter::CompositionMode_DestinationIn);
      fp.drawImage(0, 0, mask); }

    // CompositionMode_DestinationOut (Qt) is the inverse: it erases destination pixels where
    // the drawn image (mask) is opaque, punching the same selection shape as a transparent
    // hole in the live canvas — the pixels now live only in draw_float, ready to be dragged.
    { QPainter cp(draw_canvas);
      cp.setCompositionMode(QPainter::CompositionMode_DestinationOut);
      cp.drawImage(b.topLeft(), mask); }
    draw_float_active = true;
}

/// @details Bake the transformed float back into the canvas and record one draw-undo step.
/// No-op (and no undo push) when nothing is floating.
void MyView::CommitPixelFloat()
{
    if(!draw_canvas || !draw_float_active)
        return;
    // draw_float's pixel (0,0) is draw_sel_bounds.topLeft() in canvas space (that's where
    // LiftPixelFloat() copied it from), so fromTranslate() re-places it there first. For
    // `A * B`, QTransform::operator* transforms a point by A, then by B — the opposite order
    // from chained .translate()/.rotate()/.scale() calls, which apply last-called-first — so
    // PixelGizmo()'s live rotate/scale/move (centred on that same box) is applied afterwards.
    QTransform m = QTransform::fromTranslate(draw_sel_bounds.left(), draw_sel_bounds.top())
                 * PixelGizmo();
    QPainter cp(draw_canvas);
    cp.setRenderHint(QPainter::SmoothPixmapTransform);
    cp.setTransform(m);
    cp.drawImage(0, 0, draw_float);
    cp.end();
    draw_float_active = false;
    draw_float = QImage();
    PushDrawUndo();                                      // la trasformazione è un passo di undo locale
}

/// @details Commit any float, then forget the selection and reset the gizmo. Reached from a
/// tool switch to brush/eraser, Ctrl+D and draw-mode exit.
void MyView::DiscardPixelSelection()
{
    CommitPixelFloat();
    draw_selection = QRegion();
    draw_sel_bounds = QRect();
    draw_sel_t = QPointF(); draw_sel_r = 0; draw_sel_sx = draw_sel_sy = 1;
    draw_sel_gesture = 0;
    draw_lasso.clear();
    if(draw_target && draw_target->view){
        draw_target->view->update();
        if(viewport()) viewport()->update();
    }
}

/// @details Apply a freshly-drawn region with the current op (replace/add/subtract), then
/// recompute the gizmo box and reset the transform. Committing any prior float first keeps
/// the invariant "at most one of {plain mask, floating transform}".
void MyView::SetPixelSelection(const QRegion &r, int op)
{
    CommitPixelFloat();
    if(op == 1)      draw_selection = draw_selection.united(r);
    else if(op == 2) draw_selection = draw_selection.subtracted(r);
    else             draw_selection = r;
    // clampa alla tela: nulla si seleziona fuori dai pixel esistenti
    if(draw_canvas)
        draw_selection = draw_selection.intersected(draw_canvas->rect());
    draw_sel_bounds = draw_selection.boundingRect();
    draw_sel_t = QPointF(); draw_sel_r = 0; draw_sel_sx = draw_sel_sy = 1;
}

/// @details Switch draw sub-tool. Choosing a non-selection tool (brush/eraser) discards the
/// pixel selection (spec); switching between the two selection tools keeps it so add/subtract
/// can continue. draw_eraser stays in sync for the paint path.
void MyView::SetDrawTool(int t)
{
    if((t == DT_BRUSH || t == DT_ERASER) && draw_tool != t)
        DiscardPixelSelection();
    draw_tool = t;
    draw_eraser = (t == DT_ERASER);
    if(viewport())
        viewport()->setCursor((t == DT_SEL_RECT || t == DT_SEL_LASSO) ? Qt::CrossCursor
                                                                      : Qt::ArrowCursor);
}

/// @details Selection-tool mouse press. With Shift/Alt held, or with no existing selection,
/// this starts a new region gesture (rect rubber-band or lasso). Otherwise a click on a
/// handle starts scale/rotate and a click inside the box starts a move (both lift the pixels
/// on first drag); a click outside starts a fresh replace-selection.
void MyView::DrawSelPress(const QPointF &scene, Qt::KeyboardModifiers mods)
{
    draw_sel_op = (mods & Qt::ShiftModifier) ? 1 : (mods & Qt::AltModifier) ? 2 : 0;
    bool forced_new = (draw_sel_op != 0);

    if(!forced_new && !draw_selection.isEmpty()){
        int h = HitPixelHandle(scene);
        if(h == 8){                                     // rotation knob
            LiftPixelFloat();
            draw_sel_gesture = 5;
            draw_sel_r0 = draw_sel_r;
            // QRectF::center() gives the box's centre in canvas pixels (untransformed); mapping
            // it through PixelGizmo() (Qt QTransform::map()) applies the *current* live rotate/
            // scale/move so the pivot tracks the box as the user has already dragged it, not its
            // original position; CanvasToScene() then converts that canvas-pixel point into scene
            // coordinates, since `scene` (the mouse position) and QLineF::angle() below both work
            // in scene space.
            QPointF c = CanvasToScene(PixelGizmo().map(QRectF(draw_sel_bounds).center()));
            draw_sel_ang0 = QLineF(c, scene).angle();     // pointer-to-pivot angle at gesture start
            return;
        }
        if(h >= 0){                                     // scale handle
            LiftPixelFloat();
            draw_sel_gesture = 4;
            draw_sel_handle = h;
            draw_sel_sx0 = draw_sel_sx; draw_sel_sy0 = draw_sel_sy;
            return;
        }
        // dentro il box (post-gizmo) = spostamento
        QPointF cp = SceneToCanvas(scene);
        if(PixelGizmo().map(QRectF(draw_sel_bounds)).containsPoint(cp, Qt::OddEvenFill)){
            LiftPixelFloat();
            draw_sel_gesture = 3;
            draw_sel_t0 = draw_sel_t;
            draw_sel_p0 = cp;                           // ancora in canvas px
            return;
        }
    }

    // nuova selezione (replace, oppure add/subtract con Shift/Alt)
    QPointF cp = SceneToCanvas(scene);
    if(draw_tool == DT_SEL_LASSO){
        draw_sel_gesture = 2;
        draw_lasso.clear();
        draw_lasso << cp;
    } else {
        draw_sel_gesture = 1;
        draw_sel_p0 = cp;
        draw_lasso.clear();
        draw_lasso << cp << cp;                         // p0 e p1 vivi per l'anteprima del rettangolo
    }
}

/// @details Per-gesture drag update (mouseMoveEvent, left button held). Dispatches on
/// draw_sel_gesture: 1/2 grow the rubber-band rectangle / lasso polyline; 3 translates the
/// gizmo; 4 scales it about the box centre (corners = both axes, edge mids = one, recovered in
/// the rotated-but-unscaled frame so the axes stay meaningful); 5 rotates about the box centre.
/// Repaints so drawForeground shows the live selection/float.
void MyView::DrawSelMove(const QPointF &scene)
{
    QPointF cp = SceneToCanvas(scene);
    switch(draw_sel_gesture){
    case 1: draw_lasso.clear(); draw_lasso << draw_sel_p0 << cp; break; // rettangolo: 2 vertici vivi
    case 2: draw_lasso << cp; break;                                    // lazo: accumula i punti
    case 3: // move
        draw_sel_t = draw_sel_t0 + (cp - draw_sel_p0);
        break;
    case 4: { // scale about box centre (corners scale both axes; edge mids scale only one)
        QPointF c = QRectF(draw_sel_bounds).center();
        // Build the box's rotation+move (no scale) as its own QTransform, then invert it: mapping
        // the live cursor position (cp, still in canvas pixels) through that inverse undoes the
        // gizmo's current rotation/translation, landing the cursor back in the box's own rigid
        // (unscaled, unrotated) frame. That keeps the two scale axes meaningful (aligned to the
        // box's own top/side edges) no matter how far the box has already been rotated.
        QTransform rigid; rigid.translate(c.x()+draw_sel_t.x(), c.y()+draw_sel_t.y());
        rigid.rotate(draw_sel_r); rigid.translate(-c.x(), -c.y());
        QPointF u = rigid.inverted().map(cp);           // u - c = scale * (base - c)
        QRectF r(draw_sel_bounds);
        QPointF base[8] = { r.topLeft(), r.topRight(), r.bottomRight(), r.bottomLeft(),
            (r.topLeft()+r.topRight())/2.0, (r.topRight()+r.bottomRight())/2.0,
            (r.bottomRight()+r.bottomLeft())/2.0, (r.bottomLeft()+r.topLeft())/2.0 };
        QPointF h = base[draw_sel_handle];
        bool horiz = (draw_sel_handle != 4 && draw_sel_handle != 6); // top/bottom mid = Y only
        bool vert  = (draw_sel_handle != 5 && draw_sel_handle != 7); // left/right mid = X only
        if(horiz && qAbs(h.x() - c.x()) > 1e-3) draw_sel_sx = (u.x() - c.x()) / (h.x() - c.x());
        if(vert  && qAbs(h.y() - c.y()) > 1e-3) draw_sel_sy = (u.y() - c.y()) / (h.y() - c.y());
        break;
    }
    case 5: { // rotate about box centre
        // Same pivot as DrawSelPress: the box centre, mapped through the live PixelGizmo()
        // transform and converted from canvas pixels to scene coordinates, so it matches the
        // frame of `scene` (the current mouse position) that QLineF() below measures against.
        QPointF c = CanvasToScene(PixelGizmo().map(QRectF(draw_sel_bounds).center()));
        double a = QLineF(c, scene).angle();
        draw_sel_r = draw_sel_r0 - (a - draw_sel_ang0); // QLineF::angle() increases counter-clockwise
        break;
    }
    default: return;
    }
    if(draw_target && draw_target->view){
        draw_target->view->update();
        if(viewport()) viewport()->update();
    }
}

/// @details Ends a gesture (mouseReleaseEvent). A rectangle (1) or lasso (2) is finalized into
/// the selection via SetPixelSelection() with the pending op; the lasso needs >=3 points. A
/// move/scale/rotate (3/4/5) needs nothing here — its gizmo state is already live and the
/// lifted float stays active until it is committed (DiscardPixelSelection/CommitPixelFloat).
void MyView::DrawSelRelease(const QPointF &scene)
{
    QPointF cp = SceneToCanvas(scene);
    if(draw_sel_gesture == 1){                          // rectangle
        // draw_sel_p0 is the first corner clicked, cp the corner under the cursor now; the user
        // may have dragged in any direction, so QRectF(p0,cp) can have a negative width/height.
        // .normalized() (Qt) swaps corners as needed so left<=right and top<=bottom; .toRect()
        // rounds to whole pixels since the canvas is pixel-addressed; .intersected() (Qt) clips
        // the result to the canvas bounds so the selection can never reach outside the image.
        QRect rc = QRectF(draw_sel_p0, cp).normalized().toRect().intersected(draw_canvas->rect());
        if(rc.width() > 0 && rc.height() > 0)
            SetPixelSelection(QRegion(rc), draw_sel_op);
    } else if(draw_sel_gesture == 2){                   // lazo
        draw_lasso << cp;
        if(draw_lasso.size() >= 3)
            SetPixelSelection(QRegion(draw_lasso.toPolygon()), draw_sel_op);
        draw_lasso.clear();
    }
    // gesture 3/4/5 (move/scale/rotate): lo stato gizmo è già aggiornato; il float resta
    // attivo finché non lo si fonde (Ctrl+D, cambio tool, uscita)
    draw_sel_gesture = 0;
    if(draw_target && draw_target->view){
        draw_target->view->update();
        if(viewport()) viewport()->update();
    }
}

/// @details Shared predicate: the two stick types whose vertices the polygon/multiline
/// tools and the Ctrl selection of NOTOOL can edit.
static bool IsVertexStick(const StickData *s)
{
    return s->stick_type == StickData::POLYGON || s->stick_type == StickData::MULTILINE;
}

/**
 * @details The DRAWPOLY/DRAWMULTI session target is deliberately never stored as a
 * pointer: an undo, frame switch or delete mid-session would leave it dangling.
 * CurrentStick() is already kept valid by those paths, so re-deriving it (with a
 * type/view check) each time is both simpler and safe.
 */
StickData *MyView::PolySessionStick(qint32 wanted_type)
{
    if(my_animation->current_frame->stick_figures.isEmpty())
        return nullptr;
    StickData *cs = my_animation->CurrentFrame()->CurrentStick();
    if(cs != nullptr && cs->stick_type == wanted_type && cs->view)
        return cs;
    return nullptr;
}

/**
 * @details Replicates the single-stick half of the Delete-key handler (keyPressEvent):
 * StickFigure::DeleteStick() plus pruning the removed sticks from total_sticks and
 * refreshing the icons. Used where the code, not the user, decides a stick must go:
 * DRAWMULTI's Esc-cancel and the "one vertex left" deletion rule. The caller has
 * already taken the undo snapshot.
 */
void MyView::DeleteWholeStick(StickData *stick)
{
    StickFigure *fig = my_animation->CurrentFrame()->CurrentStickFigure();
    if(fig == nullptr)
        return;
    int idx = fig->stick_list.indexOf(stick);
    if(idx < 0)
        return;
    QList<StickData*> old_stick_list = fig->stick_list;
    fig->DeleteStick(idx);
    for(StickData *s : old_stick_list){
        if(!fig->stick_list.contains(s))
            my_animation->CurrentFrame()->total_sticks.removeAll(s);
    }
    my_animation->CurrentFrame()->num_of_items = my_animation->CurrentFrame()->total_sticks.length();
    my_animation->CurrentFrame()->CurrentStick() = fig->stick_list.isEmpty() ? nullptr
                                                                             : fig->master_stick;
    fig->UpdateIcon();
    if(fig->linked_item)
        fig->linked_item->setIcon(*fig->stick_figure_icon);
    my_animation->current_frame->UpdateIcon();
}

/**
 * @details handle_size è in pixel di schermo: convertito in unità di scena il raggio
 * di aggancio di vertici e spigoli resta percettivamente costante a ogni zoom.
 */
float MyView::PolyPickRadius()
{
    return QLineF(mapToScene(QPoint(0,0)), mapToScene(QPoint(handle_size,0))).length();
}

/**
 * @details Ends the point-adding session (Enter/Esc/double click/close-click/tool
 * change all route here). Clears the flags and every polygon's next-vertex preview in
 * the frame — the preview is view-only state, so nothing here touches the model.
 */
void MyView::FinishPolygonSession()
{
    poly_session_active = false;
    poly_session_extends = false;
    // il null-check serve: ChangeTool() chiama qui a ogni SetTool(), anche prima che
    // esista il primo frame (fixture dei test / avvio)
    if(my_animation->current_frame == nullptr || my_animation->current_frame->stick_figures.isEmpty())
        return;
    for(StickData *s : my_animation->CurrentFrame()->total_sticks){
        if(IsVertexStick(s) && s->view)
            s->view->ClearPolygonPreview();
    }
}

/**
 * @details Clicking anywhere without Ctrl, or leaving the tool, drops the whole vertex
 * multi-selection (per spec). Iterates the frame instead of remembering which sticks
 * held a selection, for the same no-stored-pointer reason as PolySessionStick().
 */
void MyView::DeselectAllVertices()
{
    // vedi FinishPolygonSession() per il null-check su current_frame
    if(my_animation->current_frame == nullptr || my_animation->current_frame->stick_figures.isEmpty())
        return;
    for(StickData *s : my_animation->CurrentFrame()->total_sticks){
        if(IsVertexStick(s) && s->view && !s->view->selected_vertices.isEmpty()){
            s->view->selected_vertices.clear();
            s->view->Refresh(0);
        }
    }
}

/**
 * @details Scans every polygon in the frame and keeps the vertex nearest to the click,
 * accepting it only within PolyPickRadius() — the "on top or very nearby" of the spec.
 */
StickData *MyView::FindPolygonVertex(const QPointF &scene_pos, int *vertex_idx, qint32 type_filter)
{
    *vertex_idx = -1;
    if(my_animation->current_frame->stick_figures.isEmpty())
        return nullptr;
    float radius = PolyPickRadius();
    StickData *best = nullptr;
    float best_dist = 0;
    for(StickData *s : my_animation->CurrentFrame()->total_sticks){
        if(!IsVertexStick(s) || !s->view || (type_filter >= 0 && s->stick_type != type_filter))
            continue;
        float dist = 0;
        int idx = s->view->SelectPolygonPoint(scene_pos, &dist);
        if(idx >= 0 && dist <= radius && (best == nullptr || dist < best_dist)){
            best = s;
            best_dist = dist;
            *vertex_idx = idx;
        }
    }
    return best;
}

/**
 * @details Edge-proximity counterpart of FindPolygonVertex(): identifies the polygon
 * a DRAWPOLY click wants to extend, and where the new vertex slots in on that edge.
 */
StickData *MyView::FindPolygonEdge(const QPointF &scene_pos, int *insert_idx, qint32 type_filter)
{
    *insert_idx = -1;
    if(my_animation->current_frame->stick_figures.isEmpty())
        return nullptr;
    float radius = PolyPickRadius();
    StickData *best = nullptr;
    float best_dist = 0;
    for(StickData *s : my_animation->CurrentFrame()->total_sticks){
        if(!IsVertexStick(s) || !s->view || (type_filter >= 0 && s->stick_type != type_filter)
                || s->view->SceneVertices().length() < 2)
            continue;
        float dist = 0;
        int idx = s->view->NearestEdgeInsertIndex(scene_pos, &dist);
        if(idx >= 0 && dist <= radius && (best == nullptr || dist < best_dist)){
            best = s;
            best_dist = dist;
            *insert_idx = idx;
        }
    }
    return best;
}

// --------------------------------------------------------------------- DRAW_IK ----

/**
 * @details Mirrors PolySessionStick()'s no-stored-pointer approach: "pending" means
 * ik_drawing is true AND CurrentStick() is already IK_START (set at the first click,
 * see the DRAW_IK case of mousePressEvent) but has no target yet, i.e. the second
 * click hasn't committed a chain. An undo/frame-switch mid-gesture simply makes this
 * return nullptr instead of a dangling pointer.
 */
StickData *MyView::IkPendingStart()
{
    if(!ik_drawing || my_animation->current_frame->stick_figures.isEmpty())
        return nullptr;
    StickData *cs = my_animation->CurrentFrame()->CurrentStick();
    if(cs != nullptr && cs->ik_role == StickData::IK_START && cs->ik_target == nullptr)
        return cs;
    return nullptr;
}

void MyView::UpdateIkOverlay()
{
    // i marcatori sono disegnati in drawForeground() (ridisegnato a ogni repaint del
    // viewport); qui basta forzare un repaint per i casi in cui è cambiata solo la
    // geometria dei dati (es. un drag FABRIK) senza altrimenti sporcare il viewport
    if(viewport() != nullptr)
        viewport()->update();
}

void MyView::drawForeground(QPainter *painter, const QRectF &rect)
{
    QGraphicsView::drawForeground(painter, rect);
    if(my_animation == nullptr || my_animation->current_frame == nullptr)
        return;
    // marcatori delle catene IX: come le maniglie, sempre visibili nell'editor e MAI
    // negli export/render (che disegnano una render scene separata, non questa view).
    // Disegnati qui (foreground della view) invece che con un item di scena, così sono
    // affidabili come le maniglie: ridisegnati a ogni repaint, per OGNI catena, senza
    // dipendere da un item pigro da creare/aggiornare.
    painter->save();
    for(StickFigure *S : my_animation->current_frame->stick_figures){
        if(S == nullptr) continue;
        for(StickData *s : S->stick_list){
            if(s->ik_role != StickData::IK_START || s->ik_target == nullptr)
                continue;
            QPointF target_pos = s->ik_target->my_line.p2();
            QPen line_pen(Qt::black, 1, Qt::DotLine);
            line_pen.setCosmetic(true); // spessore costante a ogni zoom, come le maniglie
            painter->setPen(line_pen);
            painter->drawLine(s->my_line.p1(), target_pos);
            bool on = s->ik_target->ik_role == StickData::IK_TARGET_ON;
            float sz = handle_size * 1.1f; // estensione in unità di scena (scala come le maniglie)
            QPen cross_pen(on ? QColor(220, 30, 30) : QColor(130, 130, 130), 2.5);
            cross_pen.setCosmetic(true);
            painter->setPen(cross_pen);
            painter->drawLine(target_pos + QPointF(-sz, -sz), target_pos + QPointF(sz, sz));
            painter->drawLine(target_pos + QPointF(sz, -sz), target_pos + QPointF(-sz, sz));
        }
    }
    painter->restore();

    // --- selezione pixel del draw mode: pixel flottanti + marquee + gizmo -------------
    if(draw_mode && draw_canvas &&
       (!draw_selection.isEmpty() || draw_sel_gesture == 1 || draw_sel_gesture == 2)){
        painter->save();
        QPointF half(draw_canvas->width()/2.0, draw_canvas->height()/2.0);
        // draw_canvas pixels are addressed from its top-left corner, but draw_disp (the same
        // scene<->canvas map paint() uses) is built around the canvas's centre; fromTranslate()
        // re-centres a pixel coordinate first, then draw_disp places it in the scene, so this
        // overlay lands exactly on top of whatever paint() already drew for the canvas.
        QTransform pix_to_scene = QTransform::fromTranslate(-half.x(), -half.y()) * draw_disp;

        // 1) transformed floating pixels (shows what the gizmo is currently dragging)
        if(draw_float_active && !draw_float.isNull()){
            painter->save();
            // Chains all three mappings the float needs in one QTransform: float-local pixel
            // (0,0) -> its canvas position (fromTranslate to bounds.topLeft()) -> the live
            // rotate/scale/move (PixelGizmo()) -> scene coordinates (pix_to_scene), each stage
            // applied left-to-right per QTransform::operator*'s point-then-matrix convention.
            QTransform m = QTransform::fromTranslate(draw_sel_bounds.left(), draw_sel_bounds.top())
                         * PixelGizmo() * pix_to_scene;
            painter->setTransform(m, true);
            painter->setRenderHint(QPainter::SmoothPixmapTransform);
            painter->drawImage(0, 0, draw_float);
            painter->restore();
        }

        // 2) anteprima gesture in corso (rettangolo o lazo)
        QPen live(QColor(0, 120, 255), 1, Qt::DashLine); live.setCosmetic(true);
        painter->setPen(live); painter->setBrush(Qt::NoBrush);
        if(draw_sel_gesture == 1 && draw_lasso.size() == 2){
            painter->drawRect(QRectF(CanvasToScene(draw_lasso[0]), CanvasToScene(draw_lasso[1])));
        } else if(draw_sel_gesture == 2 && draw_lasso.size() >= 2){
            QPolygonF poly;
            for(const QPointF &p : draw_lasso) poly << CanvasToScene(p);
            painter->drawPolyline(poly);
        }

        // 3) selection marquee (white+black dashes stay visible over any background)
        if(!draw_selection.isEmpty()){
            // QRegion stores an irregular shape as a set of horizontal scanline rectangles;
            // QPainterPath::addRegion() (Qt) adds every one of those rectangles as its own
            // subpath, so for a lasso (many thin strips) their shared internal edges would
            // otherwise draw as a "scribble" of extra lines. QPainterPath::simplified() (Qt)
            // merges overlapping subpaths into just the outer silhouette (a no-op for a plain
            // rectangle selection, which is already a single subpath).
            QPainterPath ants; ants.addRegion(draw_selection);
            ants = ants.simplified();
            ants = pix_to_scene.map(ants);           // canvas-pixel path -> scene coordinates
            QPen white(Qt::white, 1); white.setCosmetic(true);
            painter->setPen(white); painter->drawPath(ants);
            QPen black(Qt::black, 1, Qt::DashLine); black.setCosmetic(true);
            painter->setPen(black); painter->drawPath(ants);

            // 4) box del gizmo + maniglie: il box pixel ruota col gizmo, quindi traccia anche
            // il proprio contorno (draw_outline=true). Maniglie in coordinate di scena.
            QPointF h[9]; GizmoHandles(h);
            QPointF hs[9]; for(int i = 0; i < 9; i++) hs[i] = CanvasToScene(h[i]);
            DrawTransformHandles(painter, hs, true);
        }
        painter->restore();
    }

    // --- lazo di selezione figure in corso (tracciato tratteggiato) --------------------
    if(band_dragging && band_is_lasso && band_poly.size() >= 2){
        painter->save();
        QPen bp(QColor(200, 200, 200), 0); bp.setStyle(Qt::DotLine); // stesso stile del rettangolo
        painter->setPen(bp); painter->setBrush(Qt::NoBrush);
        painter->drawPolyline(band_poly);
        painter->restore();
    }

    // --- transformation box della selezione figure: 8 maniglie di scala + pomello di
    // rotazione (stile del gizmo della selezione pixel). Il box (rettangolo azzurro) e il
    // perno (cerchietto) restano disegnati dagli scene item di UpdateSelectionDecorations;
    // qui si aggiungono solo le maniglie afferrabili.
    if(SelectionHandlesActive()){
        painter->save();
        // draw_outline=false: il contorno del box figure è già lo scene item selection_bbox
        QPointF h[9]; SelectionHandles(h);
        DrawTransformHandles(painter, h, false);
        // TUTTE le maniglie degli stick della selezione (master verde incluso) restano SOPRA
        // la transformation box e la sua maniglia di scala coincidente (spec): ridisegnate qui
        // in foreground DOPO le maniglie del box, replicando l'aspetto reale dei loro scene
        // item (rect/penna/pennello, quindi colore e stato di selezione corretti). Le maniglie
        // sono in coordinate di scena (setRect assoluto, nessun setPos), come questo painter.
        for(StickFigure *S : SelectedFigures())
            for(StickData *s : S->stick_list){
                if(!s->view) continue;
                for(int k = 0; k < 2; k++){
                    QGraphicsEllipseItem *hnd = s->view->stick_handle[k];
                    if(hnd && hnd->isVisible()){
                        painter->setPen(hnd->pen());
                        painter->setBrush(hnd->brush());
                        painter->drawEllipse(hnd->rect());
                    }
                }
            }
        painter->restore();
    }

    // --- frecce della texture: fino a due (riempimento e contorno), disegnate per ULTIME così
    // restano sopra la transformation box, coerentemente col fatto che si afferrano per ultime.
    // Colori distinti perché le due frecce si sovrappongono esattamente finché le rispettive
    // texture hanno la stessa trasformazione.
    for(bool for_fill : { true, false }){
        QPointF tail, tip;
        if(!TextureArrowPoints(for_fill, &tail, &tip))
            continue;
        painter->save();
        const QColor color = for_fill ? QColor(230, 130, 0) : QColor(150, 60, 200);
        QPen shaft(color, 2); shaft.setCosmetic(true); // spessore costante a ogni zoom
        painter->setPen(shaft);
        painter->setBrush(Qt::NoBrush);
        painter->drawLine(tail, tip);
        const double hz = PolyPickRadius() * 0.7;
        // coda: cerchietto vuoto (= l'offset, si trascina per spostare la texture)
        painter->drawEllipse(tail, hz, hz);
        // punta: triangolo pieno orientato come la freccia (= angolo e scala insieme)
        QLineF axis(tail, tip);
        if(axis.length() > 1e-6){
            const QPointF dir = (tip - tail) / axis.length();
            const QPointF normal(-dir.y(), dir.x());
            QPolygonF head;
            head << tip
                 << tip - dir * (hz * 3.0) + normal * hz
                 << tip - dir * (hz * 3.0) - normal * hz;
            painter->setBrush(color);
            painter->drawPolygon(head);
        }
        painter->restore();
    }
}

void MyView::ClearIkPreviewItems()
{
    if(ik_preview_line != nullptr){
        scene()->removeItem(ik_preview_line);
        delete ik_preview_line;
        ik_preview_line = nullptr;
    }
    if(ik_preview_cross != nullptr){
        scene()->removeItem(ik_preview_cross);
        delete ik_preview_cross;
        ik_preview_cross = nullptr;
    }
}

void MyView::CancelIkDrawing()
{
    StickData *pending = IkPendingStart();
    if(pending != nullptr){
        // sicuro incondizionatamente: FindIkStartCandidate() ammette solo sticks con
        // ik_role già NONE al primo click, quindi questo non può mai "rubare" un ruolo
        // che lo stick aveva legittimamente per un'altra catena (si veda la spec: "a
        // meno che non avesse già un'altra catena ik" — con questa regola di
        // idoneità quella clausola non è mai raggiungibile)
        pending->ik_role = StickData::IK_NONE;
        if(pending->view)
            pending->view->Refresh(0);
    }
    ik_drawing = false;
    ClearIkPreviewItems();
}

StickData *MyView::FindIkStartCandidate(StickFigure *figure, const QPointF &scene_pos)
{
    if(figure == nullptr) return nullptr;
    float radius = PolyPickRadius();
    StickData *best = nullptr;
    float best_dist = 0;
    for(StickData *s : figure->stick_list){
        if(s->ik_role != StickData::IK_NONE || s->children.isEmpty())
            continue;
        float dist = QLineF(scene_pos, s->my_line.p1()).length();
        if(dist > radius)
            continue;
        if(best == nullptr){ best = s; best_dist = dist; continue; }
        // più vicino vince; a PARITÀ (P1 coincidenti — uno stepchild condivide l'origine
        // del suo genitore, tipicamente il master) si preferisce lo stepchild, altrimenti
        // il master vinceva sempre il tie sull'handle condiviso e una catena derivata da
        // uno stepchild non era mai avviabile: il target risultava non selezionabile
        const float EPS = 0.5f;
        if(dist < best_dist - EPS)
            { best = s; best_dist = dist; }
        else if(dist <= best_dist + EPS && s->stepchild && !best->stepchild)
            { best = s; best_dist = dist; }
    }
    return best;
}

StickData *MyView::FindIkTargetHandle(StickFigure *figure, const QPointF &scene_pos)
{
    if(figure == nullptr) return nullptr;
    float radius = PolyPickRadius();
    StickData *best = nullptr;
    float best_dist = 0;
    for(StickData *s : figure->stick_list){
        if(s->ik_role != StickData::IK_TARGET_ON && s->ik_role != StickData::IK_TARGET_OFF)
            continue;
        float dist = QLineF(scene_pos, s->my_line.p2()).length();
        if(dist <= radius && (best == nullptr || dist < best_dist)){
            best = s;
            best_dist = dist;
        }
    }
    return best;
}

StickData *MyView::ResolveIkStartForTarget(StickData *target, const QPointF &anchor_p1, StickData *provisional_start)
{
    if(target == nullptr) return nullptr;
    float radius = PolyPickRadius();
    // cammina verso la radice; `child` è l'anello sotto `cur` sul percorso verso il target
    StickData *child = target;
    for(StickData *cur = target->parent; cur != nullptr; cur = cur->parent){
        // il giunto dev'essere continuo: la P1 del figlio sta sulla P2 di cur (p0 è posto
        // esattamente su p2 del genitore in StartDrawing, quindi confronto esatto a meno
        // del drift float). Uno strappo qui = nessuno start valido più in alto.
        if(QLineF(child->my_line.p1(), cur->my_line.p2()).length() > 0.5f)
            break;
        bool eligible = (cur->ik_role == StickData::IK_NONE) || (cur == provisional_start);
        if(eligible && !cur->children.isEmpty()
                && QLineF(cur->my_line.p1(), anchor_p1).length() <= radius)
            return cur; // primo antenato ancorato al punto di start: la radice dell'arto cliccato
        child = cur;
    }
    return nullptr;
}

StickData *MyView::FindIkTargetCandidate(StickFigure *figure, StickData *provisional_start, const QPointF &scene_pos)
{
    if(figure == nullptr || provisional_start == nullptr) return nullptr;
    const QPointF anchor = provisional_start->my_line.p1();
    float radius = PolyPickRadius();
    StickData *best = nullptr;
    float best_dist = 0;
    for(StickData *s : figure->stick_list){
        if(s == provisional_start || s->ik_role != StickData::IK_NONE)
            continue;
        // dev'esserci uno start valido ancorato al primo click e raggiungibile da s con
        // una spina continua: se piu' arti condividono l'ancora, e' il RAMO del target a
        // scegliere lo start giusto, non il tie-break arbitrario del primo click (Group D
        // follow-up: "non si riusciva a selezionare il target con un ramo tra start e fine")
        if(ResolveIkStartForTarget(s, anchor, provisional_start) == nullptr)
            continue;
        // eligibilità: vicino al SEGMENTO (così il click a metà stick funziona), come FigureAtPoint()
        QPointF a = s->my_line.p1(), b = s->my_line.p2();
        QPointF ab = b - a;
        float len2 = ab.x()*ab.x() + ab.y()*ab.y();
        float u = (len2 < 1e-9f) ? 0.0f
                : qBound(0.0f, float(((scene_pos.x()-a.x())*ab.x() + (scene_pos.y()-a.y())*ab.y()) / len2), 1.0f);
        float seg_dist = QLineF(scene_pos, a + u*ab).length();
        if(seg_dist > qMax(radius, float(s->pen.widthF()/2.0)))
            continue;
        // ...ma la SCELTA fra più candidati usa la MANIGLIA DI ESTREMITÀ (P2 = l'effettore):
        // in una catena stepchild gli anelli si toccano, e col solo test di segmento la scelta
        // scattava sempre sull'ultimo (secondo-ultimo quasi impossibile). La P2 del secondo-
        // ultimo è sul giunto, quella dell'ultimo sulla punta, quindi la distanza dalla P2
        // discrimina quando due segmenti condividono il punto cliccato (adjustment 3).
        float p2_dist = QLineF(scene_pos, s->my_line.p2()).length();
        if(best == nullptr || p2_dist < best_dist){
            best = s;
            best_dist = p2_dist;
        }
    }
    return best;
}

// ------------------------------------------------ multi-selezione di stickfigure ----

/**
 * @details Segment-distance hit test (the same metric the pick tools use), against
 * every stick of the frame: the click is "on" a figure if it falls within the stick's
 * half pen width or the zoom-aware pick radius, whichever is larger. Returns nullptr
 * for a genuinely empty spot — the "click on the empty canvas" of the spec.
 */
StickFigure *MyView::FigureAtPoint(const QPointF &scene_pos)
{
    if(my_animation->current_frame == nullptr || my_animation->current_frame->stick_figures.isEmpty())
        return nullptr;
    float radius = PolyPickRadius();
    for(StickFigure *S : my_animation->current_frame->stick_figures){
        for(StickData *s : S->stick_list){
            QPointF a = s->my_line.p1(), b = s->my_line.p2();
            QPointF ab = b - a;
            float len2 = ab.x()*ab.x() + ab.y()*ab.y();
            float u = (len2 < 1e-9f) ? 0.0f
                    : qBound(0.0f, float(((scene_pos.x()-a.x())*ab.x() + (scene_pos.y()-a.y())*ab.y()) / len2), 1.0f);
            float dist = QLineF(scene_pos, a + u*ab).length();
            if(dist <= qMax(radius, float(s->pen.widthF()/2.0)))
                return S;
        }
    }
    return nullptr;
}

/**
 * @details Single place every selection change funnels through: highlights the
 * selected figures, keeps the layer-list rows in sync (guarded against signal
 * feedback), and creates/moves/destroys the transform handle — the thin light-blue
 * circle at the selection center every transformation pivots on.
 */
void MyView::UpdateMultiSelectionUI(bool recenter_pivot)
{
    if(!MultiSelectionActive()){
        ClearMultiSelection();
        return;
    }
    if(recenter_pivot)
        multi_pivot = SelectionPivotDefault(); // la selezione è cambiata: perno alle origini master (spec)
    for(StickFigure *S : my_animation->current_frame->stick_figures)
        S->Highlight(multi_selection.contains(S));
    // righe della lista layer allineate alla selezione (senza rientrare da segnali)
    multi_widget_sync = true;
    my_stick_figure_widget_list->clearSelection();
    for(StickFigure *S : multi_selection){
        if(S->linked_item)
            S->linked_item->setSelected(true);
    }
    multi_widget_sync = false;
    UpdateSelectionDecorations(); // maniglia + bounding box (perno già impostato sopra)
}

/**
 * @details The figures the handle/pivot/bbox act on. A multi-selection (2+) uses its own
 * list; below that the single current figure stands in, so the same handle/space/ctrl
 * machinery covers one selected figure too (spec). Empty frame or a stickless figure →
 * no selection.
 */
QList<StickFigure*> MyView::SelectedFigures() const
{
    if(MultiSelectionActive())
        return multi_selection;
    QList<StickFigure*> out;
    if(my_animation->current_frame != nullptr){
        StickFigure *current = my_animation->CurrentFrame()->CurrentStickFigure();
        if(current != nullptr && !current->stick_list.isEmpty())
            out.append(current);
    }
    return out;
}

/**
 * @details Only the pick/transform tools carry a visible selection handle and bounding
 * box; the drawing and scene-wide tools hide them (there is nothing to pivot on).
 */
bool MyView::ToolUsesSelection() const
{
    switch(tool){
        case NOTOOL: case CLAMP: case ROTATE: case SCALE:
        case MANIPULATE: case ELONGATE:
            return true;
        default:
            return false;
    }
}

/**
 * @details The pivot the handle defaults to: the master stick's origin for a single
 * figure, the mean of the masters' origins for several (spec) — not the endpoint
 * centroid MultiSelectionCenter() computes.
 */
QPointF MyView::SelectionPivotDefault() const
{
    QPointF sum(0, 0);
    int count = 0;
    for(StickFigure *S : SelectedFigures()){
        if(S->master_stick){
            sum += S->master_stick->my_line.p1();
            count++;
        }
    }
    return count > 0 ? sum / count : QPointF(0, 0);
}

QPointF MyView::SelectionPivotCurrent() const
{
    if(!MultiSelectionActive()){
        QList<StickFigure*> sel = SelectedFigures();
        StickFigure *S = sel.isEmpty() ? nullptr : sel.first();
        if(S != nullptr && S->master_stick != nullptr)
            return S->master_stick->my_line.p1() + S->pivot_offset;
    }
    return multi_pivot;
}

/**
 * @details Endpoint bounding box of the selection — the union of every selected stick's
 * P1/P2. ponytail: endpoints only (ignores pen width / shape overflow), matching the
 * endpoint model the rubber band and centroid already use; tighten if shapes visibly spill.
 */
QRectF MyView::SelectionBoundingBox() const
{
    QRectF box;
    for(StickFigure *S : SelectedFigures())
        for(StickData *s : S->stick_list)
            box = box.united(QRectF(s->my_line.p1(), s->my_line.p2()).normalized());
    return box;
}

/**
 * @details Single place the handle (thin light-blue circle on the pivot) and the
 * selection bounding box (thin light-blue no-fill rectangle) are (re)drawn. Removes both
 * when nothing is selected or the active tool doesn't use a selection.
 */
void MyView::UpdateSelectionDecorations(bool recenter_pivot)
{
    if(!ToolUsesSelection() || !HasActiveSelection()){
        RemoveSelectionDecorations();
        return;
    }
    if(!MultiSelectionActive()){
        // singola figura: il perno è persistente, ancorato al master col suo offset — segue
        // il master quando lo si trascina e NON si resetta al cambio selezione/tool (bug 2);
        // solo un Ctrl-trascina lo riposiziona (aggiornando pivot_offset)
        QList<StickFigure*> sel = SelectedFigures();
        StickFigure *S = sel.isEmpty() ? nullptr : sel.first();
        if(S != nullptr && S->master_stick != nullptr)
            multi_pivot = S->master_stick->my_line.p1() + S->pivot_offset;
    }
    else if(recenter_pivot)
        multi_pivot = SelectionPivotDefault();
    float r = handle_size * 1.5f;
    if(multi_handle == nullptr){
        multi_handle = new QGraphicsEllipseItem();
        multi_handle->setPen(QPen(QColor(0x7f, 0xc4, 0xff), 1));
        multi_handle->setBrush(Qt::NoBrush);
        multi_handle->setZValue(200);
        scene()->addItem(multi_handle);
    }
    multi_handle->setRect(multi_pivot.x() - r, multi_pivot.y() - r, 2*r, 2*r);
    if(selection_bbox == nullptr){
        selection_bbox = new QGraphicsRectItem();
        selection_bbox->setPen(QPen(QColor(0x7f, 0xc4, 0xff), 0)); // cosmetic (zoom-invariant) thin edge
        selection_bbox->setBrush(Qt::NoBrush);
        selection_bbox->setZValue(198);
        scene()->addItem(selection_bbox);
    }
    selection_bbox->setRect(SelectionBoundingBox());
    // le maniglie/box della transformation box sono un overlay di drawForeground: al cambio
    // selezione vanno ridisegnate esplicitamente, altrimenti quelle della selezione precedente
    // restano "fantasma" nelle zone non invalidate dallo spostamento dei soli scene item (spec).
    if(viewport()) viewport()->update();
}

void MyView::RemoveSelectionDecorations()
{
    if(multi_handle != nullptr){
        scene()->removeItem(multi_handle);
        delete multi_handle;
        multi_handle = nullptr;
    }
    if(selection_bbox != nullptr){
        scene()->removeItem(selection_bbox);
        delete selection_bbox;
        selection_bbox = nullptr;
    }
    // ridisegna il foreground: cancella l'overlay di maniglie/master della selezione appena
    // rimossa (vedi UpdateSelectionDecorations per il motivo del repaint esplicito).
    if(viewport()) viewport()->update();
}

// ===== Stickfigure transformation box ================================================
// The move/scale/rotate gizmo drawn around the current stickfigure selection under the arrow
// (NOTOOL) tool. It is the figure-space twin of the draw-mode pixel selection above: same
// look (they share only the renderer, DrawTransformHandles()), but this one transforms whole
// FIGURES and reuses the ROTATE/SCALE tools' math instead of a QTransform on pixels.
//
// Geometry:    SelectionHandles() places 8 scale squares on SelectionBoundingBox() plus a
//              rotation knob above it (scene coords); HitSelectionHandle() maps a click to a
//              handle index, deliberately yielding priority to the stick handles (a click on a
//              stick endpoint manipulates that stick — moves from the master, rotates — and
//              never scales). SelectionHandlesActive() gates it to NOTOOL + a live selection.
// Interaction: grabbing a handle in mousePressEvent() starts a gesture — BeginSelectionScale()
//              /BeginSelectionRotate() snapshot the pre-gesture state (and are ALSO the press
//              bodies of the ROTATE/SCALE tools), ApplySelectionScale()/ApplySelectionRotate()
//              apply the drag each mouseMoveEvent(). Both pivot on SelectionPivotCurrent(), the
//              user-movable pivot (drag its handle, or Ctrl+drag, or hold Space).
// Rendering:   drawForeground() draws the box via DrawTransformHandles(draw_outline=false — the
//              outline is the selection_bbox scene item) and redraws every stick handle on top;
//              UpdateSelectionDecorations()/RemoveSelectionDecorations() keep the scene items in
//              sync and force the overlay repaint on any selection change.
// Beneficiaries: the arrow/select toolbar tool (widget.cpp on_cursor_clicked) and, sharing the
//              Begin*/Apply* math, the ROTATE and SCALE tool buttons. The rectangle-vs-lasso
//              rubber band for PICKING figures is chosen by the Selection menu checkbox
//              (mymainwindow.cpp, MyView::stickfig_lasso) — separate from draw-mode's lasso.

/**
 * @details Transformation-box handles (stile del gizmo della selezione pixel, applicato
 * alla selezione di figure): 8 quadretti sul bounding box axis-aligned + un pomello di
 * rotazione sopra il lato superiore. Il pomello sta a distanza schermo-costante
 * (PolyPickRadius) così resta grande uguale a ogni zoom.
 */
void MyView::SelectionHandles(QPointF out[9]) const
{
    QRectF r = SelectionBoundingBox();
    double gap = const_cast<MyView*>(this)->PolyPickRadius() * 3.0;
    out[0] = r.topLeft();  out[1] = r.topRight();  out[2] = r.bottomRight();  out[3] = r.bottomLeft();
    out[4] = (r.topLeft()+r.topRight())/2.0;     out[5] = (r.topRight()+r.bottomRight())/2.0;
    out[6] = (r.bottomRight()+r.bottomLeft())/2.0; out[7] = (r.bottomLeft()+r.topLeft())/2.0;
    out[8] = QPointF(r.center().x(), r.top() - gap);
}

/// @details Which transformation-box handle a scene click grabs: 8 the rotation knob (checked
/// first, it sits above the box), 0-7 a scale handle, -1 none. Enforces the priority rules:
/// a click on/near any stick handle of the selection returns -1 so the click falls through to
/// normal stick manipulation (move from the master, rotate) instead of scaling. Used by
/// mousePressEvent (NOTOOL) to start a box gesture.
int MyView::HitSelectionHandle(const QPointF &scene) const
{
    if(!HasActiveSelection())
        return -1;
    QPointF h[9];
    SelectionHandles(h);
    float radius = const_cast<MyView*>(this)->PolyPickRadius();
    // il pomello di rotazione (8) ha la precedenza: sta sopra il box e non coincide mai con
    // una maniglia di stick, quindi lo si controlla per primo
    if(QLineF(scene, h[8]).length() <= radius)
        return 8;
    // priorità alle maniglie degli stick (spec): se il click cade sopra (o vicino a) un
    // estremo — maniglia — di uno stick della selezione, NON afferrare una maniglia di scala.
    // Lascia passare il click alla manipolazione normale dello stick (trasla dal master,
    // ruota, ecc.). Vale per OGNI maniglia, non solo il master. Gli spigoli del box coincidono
    // con gli estremi esterni, quindi lì vince la maniglia; per scalare restano lati e pomello.
    // "vicino" = raggio della maniglia stessa o quello di aggancio, il più grande dei due.
    float handle_r = qMax(radius, float(handle_size) / 2.0f);
    for(StickFigure *S : SelectedFigures())
        for(StickData *s : S->stick_list){
            if(QLineF(scene, s->my_line.p1()).length() <= handle_r) return -1;
            if(QLineF(scene, s->my_line.p2()).length() <= handle_r) return -1;
        }
    for(int i = 0; i < 8; i++)
        if(QLineF(scene, h[i]).length() <= radius)
            return i;
    return -1;
}

/// @details Unico disegnatore del gizmo di trasformazione, condiviso tra la selezione pixel
/// e la transformation box delle figure (vedi drawForeground). Stile azzurro cosmetico +
/// quadretti bianchi + pomello: identico per entrambi; l'unica variazione è draw_outline
/// (il box pixel ruota col gizmo e traccia qui il proprio contorno; il box figure lo prende
/// dallo scene item selection_bbox).
void MyView::DrawTransformHandles(QPainter *painter, const QPointF pts[9], bool draw_outline)
{
    QPen box(QColor(0, 120, 255), 1); box.setCosmetic(true); // azzurro, spessore costante a ogni zoom
    painter->setPen(box); painter->setBrush(Qt::NoBrush);
    if(draw_outline){
        QPolygonF frame; frame << pts[0] << pts[1] << pts[2] << pts[3];
        painter->drawPolygon(frame);
    }
    painter->drawLine(pts[4], pts[8]);          // stelo del pomello di rotazione
    double hz = PolyPickRadius() * 0.6;         // mezza dimensione maniglia (schermo-costante)
    painter->setBrush(Qt::white);
    for(int i = 0; i < 8; i++)
        painter->drawRect(QRectF(pts[i].x()-hz, pts[i].y()-hz, 2*hz, 2*hz));
    painter->drawEllipse(pts[8], hz, hz);       // pomello di rotazione
}

/// @details `CurrentStick()` is safe on a frame with no current figure (it returns a null
/// placeholder), so only the frame itself has to be checked here.
StickData *MyView::TextureArrowStick() const
{
    if(my_animation == nullptr || my_animation->current_frame == nullptr)
        return nullptr;
    return const_cast<MyView*>(this)->my_animation->CurrentFrame()->CurrentStick();
}

QBrush MyView::CurrentTextureBrush(bool for_fill) const
{
    StickData *stick = TextureArrowStick();
    if(stick == nullptr)
        return QBrush(Qt::NoBrush);
    if(for_fill)
        return stick->brush;
    return stick->pen.style() == Qt::NoPen ? QBrush(Qt::NoBrush) : stick->pen.brush();
}

/// @details What one unit of the brush's own placement measures on the canvas — the conversion
/// that lets one arrow drive both brush kinds. For a TEXTURE it is the tile's longer side, so the
/// arrow's length reads directly as "one tile is this big" and a scale of 1 draws an arrow exactly
/// one tile long. For a GRADIENT, whose coordinates are 0..1 of the box it is anchored to, it is
/// the size of that box: the shape (approximated by the stick's own length) when the gradient
/// follows the stick, the CANVAS when it doesn't — because an unanchored gradient really does span
/// the scene rect (see SceneAnchoredGradient()), and measuring it against the stick instead would
/// make the arrow wildly oversensitive, moving the fill a whole canvas per stick-length dragged.
qreal MyView::ArrowUnit(const QBrush &brush) const
{
    if(brush.style() == Qt::TexturePattern && !brush.textureImage().isNull())
        return qMax(1, qMax(brush.textureImage().width(), brush.textureImage().height()));
    StickData *stick = TextureArrowStick();
    if(stick != nullptr && !stick->brush_follows_stick && scene() != nullptr){
        const QRectF box = scene()->sceneRect();
        return qMax(1.0, qMin(box.width(), box.height()));
    }
    return qMax(40.0, stick == nullptr ? 0.0 : stick->my_line.length());
}

/// @details How far the brush's own placement frame is turned from the scene's, which everything
/// stored in that frame has to be rotated by to be talked about in scene terms. Zero unless the
/// paint is anchored to the stick — StickGraphicsItem::BrushFrameAngle() is the single source of
/// truth, shared with the paint path so the arrow cannot point somewhere the fill isn't.
qreal MyView::ArrowFrameAngle() const
{
    StickData *stick = TextureArrowStick();
    return stick == nullptr ? 0.0 : StickGraphicsItem::BrushFrameAngle(stick);
}

/// @details Reads whichever placement this brush kind keeps and converts it to scene units AND
/// scene axes, so everything downstream — the drawing, the hit test, the drag — is written once
/// instead of twice.
///
/// The axes are the half that was missing: a stick-anchored placement is stored in the stick's own
/// frame (a texture's offset is a translation the local frame then rotates, an ObjectMode
/// gradient's is a displacement inside a box ObjectGradientTurn() turns), so reading it as if it
/// were scene coordinates made a drag move the paint off along the stick's angle instead of along
/// the drag — backwards for a stick pointing the other way.
MyView::ArrowXf MyView::ReadArrow(const QBrush &brush) const
{
    const qreal unit = ArrowUnit(brush);
    const bool is_gradient = (brush.gradient() != nullptr);
    const TextureXf xf = is_gradient ? ReadGradientXf(brush) : ReadTextureXf(brush);
    const qreal turn = ArrowFrameAngle();
    ArrowXf out;
    out.angle = xf.angle + turn;
    // un gradiente tiene ENTRAMBI in frazioni del box; una texture tiene l'offset già in unità
    // di lunghezza (del suo frame), e come scala un fattore sui pixel della tile
    out.offset = QTransform().rotate(turn).map(is_gradient ? xf.offset * unit : xf.offset);
    out.length = xf.scale * unit;
    return out;
}

/// @details The exact inverse of ReadArrow(), so a drag that lands the arrow where the user let go
/// reads back as the arrow they see.
void MyView::WriteArrow(QBrush *brush, const ArrowXf &arrow) const
{
    const qreal unit = ArrowUnit(*brush);
    const bool is_gradient = (brush->gradient() != nullptr);
    const qreal turn = ArrowFrameAngle();
    const QPointF offset = QTransform().rotate(-turn).map(arrow.offset);
    TextureXf xf;
    xf.angle = arrow.angle - turn;
    xf.scale = qMax(1e-3, arrow.length / unit);
    xf.offset = is_gradient ? offset / unit : offset;
    if(is_gradient)
        WriteGradientXf(brush, xf);
    else
        WriteTextureXf(brush, xf);
}

/// @details Anchored beside the stick (TextureArrowAnchor()) rather than on the scene origin:
/// the arrow has to stay next to the stick it belongs to at any zoom or position, and the offset
/// it shows is meaningful relative to the stick anyway. Everything else is the polar form of the
/// texture's own transform — see TextureXf.
bool MyView::TextureArrowPoints(bool for_fill, QPointF *tail, QPointF *tip) const
{
    if(for_fill ? !show_fill_texture_arrow : !show_stroke_texture_arrow)
        return false;
    StickData *stick = TextureArrowStick();
    if(stick == nullptr || stick->view == nullptr)
        return false;
    // un tipo che non passa il pennello al painter non ha un riempimento (o un contorno) da
    // puntare, per quanta geometria il pennello contenga: la freccia sarebbe una maniglia su
    // qualcosa che non si vede. Stessi predicati con cui UpdateSliders() spegne i selettori
    if(for_fill ? !stick->PaintsFill() : !stick->PaintsStroke())
        return false;
    const QBrush brush = CurrentTextureBrush(for_fill);
    // una tinta unita e il vuoto non hanno una geometria da mostrare, come per AlignBrush().
    // Texture e gradiente sì, ognuno la propria: ReadArrow() le porta entrambe in unità di scena
    const bool has_tile = (brush.style() == Qt::TexturePattern && !brush.textureImage().isNull());
    if(!has_tile && brush.gradient() == nullptr)
        return false;
    const ArrowXf arrow = ReadArrow(brush);
    *tail = TextureArrowAnchor(stick, arrow.angle) + arrow.offset;
    *tip = *tail + QLineF::fromPolar(arrow.length, -arrow.angle).p2();
    return true;
}

/// @details The stand-off is what makes the arrow reachable at all, and it has to be measured in
/// GRAB radii rather than in scene units: everything it must not collide with — the stick's own
/// endpoint handles and the transformation box's — is picked within PolyPickRadius() or half a
/// handle, so clearing the larger of those twice over is clear by construction at any zoom and
/// for any stick length. Pushing it out along the stick's NORMAL is what clears the box's side
/// mid-handles, which for a single straight stick (whose bounding box is a degenerate line) sit
/// exactly on the stick itself.
///
/// The SIDE is chosen by where the arrow points, so that the arrow leads away from the stick
/// rather than back across it. Without that the whole stand-off is defeatable by one particular
/// arrow length — the one that lands the tip back on the stick — and since the length is the
/// texture's scale, that length is a value the user can perfectly well be sitting on.
QPointF MyView::TextureArrowAnchor(const StickData *stick, qreal angle) const
{
    const QPointF middle = stick->my_line.pointAt(0.5);
    QLineF axis = stick->my_line;
    if(axis.length() < 1e-6)
        axis = QLineF(middle, middle + QPointF(1, 0)); // stick di lunghezza nulla: normale arbitraria
    QPointF normal(-axis.dy() / axis.length(), axis.dx() / axis.length());
    const QPointF direction = QLineF::fromPolar(1.0, -angle).p2();
    if(QPointF::dotProduct(normal, direction) < 0)
        normal = -normal;
    const qreal stand_off = qMax(const_cast<MyView*>(this)->PolyPickRadius() * 2.5f,
                                 handle_size * 1.5f);
    return middle + normal * stand_off;
}

/// @details The priority rules of the spec, in the order they are checked: a transformation-box
/// handle wins, then any stick endpoint handle of the selection, then the arrows. The stick-
/// handle guard is the same test (and the same radius) HitSelectionHandle() uses for the mirror
/// rule one level up, so the three pickable things agree on what "on a handle" means.
bool MyView::HitTextureArrow(const QPointF &scene, bool *for_fill, bool *on_tip) const
{
    if(SelectionHandlesActive() && HitSelectionHandle(scene) >= 0)
        return false;
    const float radius = const_cast<MyView*>(this)->PolyPickRadius();
    const float handle_r = qMax(radius, float(handle_size) / 2.0f);
    for(StickFigure *S : SelectedFigures())
        for(StickData *s : S->stick_list){
            if(QLineF(scene, s->my_line.p1()).length() <= handle_r) return false;
            if(QLineF(scene, s->my_line.p2()).length() <= handle_r) return false;
        }
    // la punta prima della coda, e il riempimento prima del contorno: due frecce con la stessa
    // trasformazione si sovrappongono esattamente, e senza un ordine fisso quale delle due si
    // afferra dipenderebbe dal caso
    for(int pass = 0; pass < 2; pass++)
        for(bool fill : { true, false }){
            QPointF tail, tip;
            if(!TextureArrowPoints(fill, &tail, &tip))
                continue;
            if(QLineF(scene, pass == 0 ? tip : tail).length() <= radius * 1.2f){
                *for_fill = fill;
                *on_tip = (pass == 0);
                return true;
            }
        }
    return false;
}

/// @details Writes straight onto the live StickData, like every other drag in this view: the
/// undo snapshot for the whole gesture was pushed once at press.
void MyView::DragTextureArrow(const QPointF &scene)
{
    StickData *stick = TextureArrowStick();
    if(stick == nullptr)
        return;
    const bool for_fill = texture_arrow_is_fill;
    QBrush brush = CurrentTextureBrush(for_fill);
    if(brush.style() != Qt::TexturePattern && brush.gradient() == nullptr)
        return;
    ArrowXf arrow = ReadArrow(brush);
    if(texture_arrow_gesture == 1){
        arrow.offset = scene - TextureArrowAnchor(stick, arrow.angle);
    }
    else{
        const QLineF shaft(texture_arrow_tail, scene); // coda bloccata al press: vedi il campo
        arrow.angle = -shaft.angle();       // QLineF conta in senso antiorario, QTransform in orario
        arrow.length = qMax(1.0, shaft.length());
    }
    WriteArrow(&brush, arrow);
    if(for_fill){
        stick->brush = brush;
    }
    else{
        stick->pen.setBrush(brush);
    }
    if(stick->view)
        stick->view->Refresh(0);
    if(viewport()) viewport()->update(); // la freccia stessa è disegnata nel foreground
}

/// @details La transformation box è attiva/afferrabile solo col tool di selezione
/// (NOTOOL) e una selezione presente: gli altri tool (ROTATE/SCALE dedicati, disegno)
/// non ne mostrano le maniglie.
bool MyView::SelectionHandlesActive() const
{
    return tool == NOTOOL && HasActiveSelection();
}

/// @details Snapshot pre-gesto + buffer per la rotazione, identici a quelli che il tool
/// ROTATE preparava inline: perno = SelectionPivotCurrent() (maniglia mobile), angolo
/// iniziale di ogni stick in angle_buffer2, origine del master in origin_pos_buffer.
void MyView::BeginSelectionRotate()
{
    StoreUndo();
    StickFigure::start_stick_figure_rotation = true;
    multi_center_buffer = SelectionPivotCurrent();
    multi_pivot = multi_center_buffer;
    for(StickFigure *S : SelectedFigures()){
        if(S->master_stick && S->stick_list.count() > 0){
            S->origin_pos_buffer = S->master_stick->my_line.p1();
            for(StickData *s : S->stick_list){
                if(s->view) s->view->angle_buffer2 = s->my_line.angle();
            }
        }
    }
}

/// @details Come BeginSelectionRotate() ma per la scala: memorizza lunghezza/spessore di
/// ogni stick (scale_buffer/width_buffer) e l'origine del master. La scala delle figure è
/// UNIFORME (StickGraphicsItem::Scale prende un solo fattore), quindi anche le maniglie di
/// spigolo scalano in modo uniforme come quelle d'angolo (ponytail: non-uniforme fuori scope,
/// il modello di scala figure non lo supporta).
void MyView::BeginSelectionScale()
{
    StoreUndo();
    StickFigure::start_stick_figure_scale = true;
    multi_center_buffer = SelectionPivotCurrent();
    multi_pivot = multi_center_buffer;
    for(StickFigure *S : SelectedFigures()){
        if(S->master_stick && S->stick_list.count() > 0){
            S->origin_pos_buffer = S->master_stick->my_line.p1();
            for(StickData *s : S->stick_list){
                if(s->view){
                    s->view->scale_buffer = s->my_line.length();
                    s->view->width_buffer = s->pen.width();
                }
            }
        }
    }
}

/// @details Corpo di rotazione, estratto dalla case ROTATE del mouseMove: singola figura e
/// multi ruotano attorno alla maniglia (multi_center_buffer). Incrementale (starting_coord
/// riazzerato a ogni chiamata); ogni stick ruota, poi il master orbita il perno fisso.
void MyView::ApplySelectionRotate()
{
    if(!HasActiveSelection())
        return;
    QPointF center = multi_center_buffer;
    float angle = QLineF(coord, center).angle() - QLineF(starting_coord, center).angle();
    if(angle > 180) angle -= 360;
    else if(angle < -180) angle += 360;
    starting_coord = coord;
    bool single = !MultiSelectionActive();
    for(StickFigure *S : SelectedFigures()){
        if(S->stick_list.count() > 0 && S->master_stick){
            QLineF connect_center(center, S->master_stick->my_line.p1());
            connect_center.setAngle(connect_center.angle() + angle);
            for(StickData *s : S->stick_list){
                if(s->view){
                    s->view->Rotate(angle);
                    s->view->angle_buffer2 += angle;
                }
            }
            QLineF delta(S->origin_pos_buffer, connect_center.p2());
            S->Traslate(delta.dx(), delta.dy());
            S->origin_pos_buffer = S->master_stick->my_line.p1();
            if(single)
                S->pivot_offset = center - S->master_stick->my_line.p1();
        }
    }
    if(single) UpdateSelectionDecorations(false);
    else       UpdateMultiSelectionUI();
    if(viewport()) viewport()->update(); // ridisegna le maniglie della box che seguono la selezione
}

/// @details Corpo di scala, estratto dalla case SCALE del mouseMove: fattore assoluto
/// dist(coord)/dist(press) attorno alla maniglia (multi_center_buffer). Uniforme.
void MyView::ApplySelectionScale()
{
    if(!HasActiveSelection())
        return;
    QPointF center = multi_center_buffer;
    float start_dist = QLineF(starting_coord, center).length();
    if(start_dist < 1)
        return; // press esattamente sul perno: divisione per (quasi) zero
    float scale_amount = QLineF(coord, center).length() / start_dist;
    bool single = !MultiSelectionActive();
    for(StickFigure *S : SelectedFigures()){
        if(S->stick_list.count() > 0 && S->master_stick){
            QLineF actual_dist_from_center(S->master_stick->my_line.p1(), center);
            QLineF original_dist_from_center(S->origin_pos_buffer, center);
            float dx = -original_dist_from_center.dx()*scale_amount + actual_dist_from_center.dx();
            float dy = -original_dist_from_center.dy()*scale_amount + actual_dist_from_center.dy();
            S->Traslate(dx, dy);
            for(StickData *s : S->stick_list){
                if(s->view) s->view->Scale(scale_amount);
            }
            if(single)
                S->pivot_offset = center - S->master_stick->my_line.p1();
        }
    }
    if(single) UpdateSelectionDecorations(false);
    else       UpdateMultiSelectionUI();
    if(viewport()) viewport()->update();
}

void MyView::SetMultiSelectionList(const QList<StickFigure*> &figures)
{
    multi_selection = figures;
    if(multi_selection.count() > 1 && my_animation->current_frame->stick_figures.contains(multi_selection.first()))
        my_animation->UpdateSelection(multi_selection.first());
    UpdateMultiSelectionUI(true);
}

/**
 * @details Shift+click: the first addition seeds the set with the current figure, so
 * "current + clicked" become the initial pair.
 */
void MyView::AddToMultiSelection(StickFigure *figure)
{
    if(figure == nullptr)
        return;
    if(multi_selection.isEmpty() && my_animation->CurrentFrame()->CurrentStickFigure() != nullptr
            && my_animation->CurrentFrame()->CurrentStickFigure() != figure)
        multi_selection.append(my_animation->CurrentFrame()->CurrentStickFigure());
    if(!multi_selection.contains(figure))
        multi_selection.append(figure);
    UpdateMultiSelectionUI(true);
}

void MyView::RemoveFromMultiSelection(StickFigure *figure)
{
    multi_selection.removeAll(figure);
    UpdateMultiSelectionUI(true); // sotto i 2 selezionati la UI azzera tutto da sé
    // scesi a una sola figura: mostra la maniglia/bounding box della selezione singola
    UpdateSelectionDecorations(true);
}

/**
 * @details Called each mouseMove during a band drag. Two figures or more become a
 * multi-selection (transform box + pivot). Exactly one becomes a plain single selection
 * (that figure is promoted to CurrentStickFigure, highlighted, its layer row selected, its
 * handle/bounding box shown) — this is the case the old inline code missed: it stored a
 * one-element multi_selection, which MultiSelectionActive() (count > 1) rejects, so the
 * else branch re-highlighted the OLD current figure and the release path then wiped the
 * one-element list — the band appeared to do nothing. Zero enclosed clears the selection
 * (CurrentStickFigure = nullptr; CurrentStick() stays null-safe, see frame.h).
 */
void MyView::ApplyBandSelection(const QList<StickFigure*> &touched)
{
    Frame *frame = my_animation->current_frame;
    if(frame == nullptr)
        return;
    if(touched.count() >= 2){
        multi_selection = touched;
        UpdateMultiSelectionUI(true); // box + perno al baricentro della selezione
        return;
    }
    // 0 o 1 figura: nessuna selezione multipla; la selezione singola (CurrentStickFigure)
    // diventa la figura racchiusa, oppure nessuna se il band non ne racchiude alcuna
    multi_selection.clear();
    StickFigure *pick = touched.isEmpty() ? nullptr : touched.first();
    for(StickFigure *S : frame->stick_figures)
        S->Highlight(S == pick);
    frame->CurrentStickFigure() = pick;
    // riallinea la lista layer alla selezione singola senza rientrare dai suoi segnali
    multi_widget_sync = true;
    my_stick_figure_widget_list->clearSelection();
    if(pick != nullptr){
        if(!pick->stick_list.isEmpty())
            frame->SelectStick(pick);
        if(pick->linked_item){
            pick->linked_item->setSelected(true);
            my_stick_figure_widget_list->setCurrentRow(my_stick_figure_widget_list->row(pick->linked_item));
        }
    }
    multi_widget_sync = false;
    RemoveSelectionDecorations();      // via l'eventuale box/perno di una selezione multipla precedente
    if(pick != nullptr)
        UpdateSelectionDecorations(true); // maniglia + bounding box della selezione singola
}

void MyView::ClearMultiSelection()
{
    bool had_selection = !multi_selection.isEmpty();
    for(StickFigure *S : multi_selection){
        // le figure possono essere già morte (undo/cambio frame): tocca solo le vive
        if(my_animation->current_frame != nullptr
                && my_animation->current_frame->stick_figures.contains(S)
                && S != my_animation->CurrentFrame()->CurrentStickFigure())
            S->Highlight(false);
    }
    multi_selection.clear();
    // riallinea la lista layer alla selezione singola: senza questo le righe restavano
    // multi-selezionate e la selezione sembrava non essersi rotta
    if(had_selection && my_animation->current_frame != nullptr){
        multi_widget_sync = true;
        my_stick_figure_widget_list->clearSelection();
        StickFigure *current = my_animation->CurrentFrame()->CurrentStickFigure();
        if(current != nullptr && current->linked_item){
            current->linked_item->setSelected(true);
            my_stick_figure_widget_list->setCurrentRow(my_stick_figure_widget_list->row(current->linked_item));
        }
        multi_widget_sync = false;
    }
    RemoveSelectionDecorations(); // maniglia + bounding box
    if(selection_band != nullptr){
        scene()->removeItem(selection_band);
        delete selection_band;
        selection_band = nullptr;
    }
    band_dragging = false;
    multi_dragging = false;
    pivot_dragging = false;
    space_dragging = false;
    sel_box_gesture = 0;
    band_is_lasso = false;
    band_poly.clear();
}

/**
 * @details Mean of every selected stick's endpoints — cheap, stable while dragging,
 * and matching the intuitive "center of what I selected".
 */
QPointF MyView::MultiSelectionCenter() const
{
    QPointF sum(0, 0);
    int count = 0;
    for(StickFigure *S : multi_selection){
        for(StickData *s : S->stick_list){
            sum += s->my_line.p1() + s->my_line.p2();
            count += 2;
        }
    }
    return count > 0 ? sum / count : QPointF(0, 0);
}

QList<int> MyView::MultiSelectionIndices() const
{
    QList<int> indices;
    if(my_animation->current_frame == nullptr)
        return indices;
    for(StickFigure *S : multi_selection){
        int idx = my_animation->current_frame->stick_figures.indexOf(S);
        if(idx >= 0)
            indices.append(idx);
    }
    return indices;
}

void MyView::SetMultiSelectionByIndices(const QList<int> &indices)
{
    QList<StickFigure*> figures;
    for(int idx : indices){
        if(idx >= 0 && idx < my_animation->current_frame->stick_figures.count())
            figures.append(my_animation->current_frame->stick_figures[idx]);
    }
    if(figures.count() > 1)
        SetMultiSelectionList(figures);
    else
        ClearMultiSelection();
}

/**
 * @details Batch delete of the selection: one snapshot, then the single-figure
 * routine per figure (which already enforces the at-least-one-layer rule and moves
 * the current selection to a survivor).
 */
void MyView::DeleteMultiSelection()
{
    if(!MultiSelectionActive())
        return;
    StoreUndo();
    QList<StickFigure*> victims = multi_selection;
    ClearMultiSelection();
    for(StickFigure *S : victims){
        if(!my_animation->current_frame->stick_figures.contains(S))
            continue;
        if(my_animation->current_frame->stick_figures.count() <= 1)
            break; // regola dell'ultimo layer: DeleteStickFigure() rifiuterebbe con un popup a testa
        my_stick_figure_widget_list->clearSelection();
        if(S->linked_item)
            S->linked_item->setSelected(true);
        DeleteStickFigure();
    }
}

//Esci dallo strumento che stai usando
/**
 * @details Runs when a tool is left (from SetTool()). Its real job is on the DRAW*
 * tools: if a stick is half-drawn (draw_count==1) it is removed from the scene and
 * StickFigure::CancelDrawing() throws it away, so switching tools mid-draw never leaves
 * a dangling stick; leaving DRAWIMG also frees image_draw_buffer. Other tools have
 * nothing to undo and fall through.
 */
void MyView::ChangeTool()
{
    // cambiare strumento deselezionata tutti i vertici (spec) e chiude un'eventuale
    // sessione di aggiunta vertici, qualunque fosse il tool attivo
    DeselectAllVertices();
    if(poly_session_active)
        FinishPolygonSession();
    poly_drag_idx = -1;
    poly_vertex_drag = false;
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
        case(DRAWTEXT):
        {
            my_animation->UpdateTab(tool); // updates ui panels
            if(!my_animation->current_frame->stick_figures.isEmpty()
                    && my_animation->CurrentFrame()->CurrentStickFigure() != nullptr)
            {
                // se stavi disegnando uno stick e cambi prima di averlo finito, cancellalo!
                if(my_animation->CurrentFrame()->CurrentStickFigure()->draw_count==1)
                {
                    // niente removeItem manuale qui: dal Phase 2c CurrentStick() è uno StickData
                    // (NON un QGraphicsItem — il vecchio cast era un type-pun su un puntatore
                    // scorrelato, UB); CancelDrawing() stacca già la view dalla scena via
                    // FramePresenter::DetachView()
                    my_animation->CurrentFrame()->CurrentStickFigure()->CancelDrawing();
                }
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
        case(DRAWFREEHAND):
        {
            // uscendo dal tool freehand: chiudi un'eventuale sessione di disegno (spec:
            // qualunque altro tool esce da draw mode), oppure scarta una base a metà click
            if(draw_mode)
                ExitDrawMode();
            else if(!my_animation->current_frame->stick_figures.isEmpty()
                    && my_animation->CurrentFrame()->CurrentStickFigure() != nullptr
                    && my_animation->CurrentFrame()->CurrentStickFigure()->draw_count==1){
                my_animation->CurrentFrame()->CurrentStickFigure()->CancelDrawing();
            }
            break;
        }
        case (DRAW_IK):
        {
            if(ik_drawing)
                CancelIkDrawing();
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
        case(DRAWPOLY):
        case(DRAWMULTI):
        {
            my_animation->UpdateTab(tool); // updates ui panels
            if(!my_animation->current_frame->stick_figures.isEmpty()
                    && my_animation->CurrentFrame()->CurrentStickFigure() != nullptr)
            {
                // base a metà (un solo click): buttala via come per gli altri DRAW*
                if(my_animation->CurrentFrame()->CurrentStickFigure()->draw_count==1)
                {
                    my_animation->CurrentFrame()->CurrentStickFigure()->CancelDrawing();
                }
            }
            break;
        }
    }
}

/**
 * @details The wheel steps the current-stick selection up/down the frame's z-order,
 * skipping over sticks that belong to other figures (only stops on ones in the current
 * figure's stick_list). Handy for reaching an overlapped stick without clicking it.
 * Selection changes go through Frame::SelectStick(); Highlight(false) clears the old
 * emphasis first.
 */
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
/**
 * @details Ends a gesture. On left release it finalizes an interactive stick rotation
 * (StickGraphicsItem::EndRotation()) and refreshes the figure icon/layer thumbnail; for
 * the scene-wide tools (MOVESCENE/ROTATESCENE/SCALESCENE) it refreshes EVERY figure's
 * icon, since those tools move the whole frame, not just the selected layer. Right/
 * middle release restores the default transform anchor used by pan/zoom.
 */
void MyView::mouseReleaseEvent(QMouseEvent *event)
{
    // draw mode: chiude la pennellata corrente, niente della logica di selezione/tool sotto
    if(draw_mode){
        if(event->button()==Qt::LeftButton && (draw_tool==DT_SEL_RECT || draw_tool==DT_SEL_LASSO)){
            if(draw_sel_gesture)
                DrawSelRelease(coord);
        } else if(event->button()==Qt::LeftButton && draw_stroking){
            is_pressed = false;
            draw_stroking = false;
            draw_pressure_val = 1.0; // reset: la prossima pennellata col mouse (senza tabletEvent) resta a piena pressione
            PushDrawUndo(); // ogni pennellata/gommata COMPLETATA = un passo di undo/redo locale
        }
        QGraphicsView::mouseReleaseEvent(event);
        return;
    }
    if(event->button()==Qt::LeftButton){
        is_pressed = false;
        poly_drag_idx = -1;      // fine di un eventuale trascinamento di vertice (DRAWPOLY)
        poly_vertex_drag = false; // fine di un eventuale Ctrl-drag della selezione vertici
        multi_dragging = false;
        pivot_dragging = false;
        sel_box_gesture = 0; // fine di un eventuale drag di una maniglia della transformation box
        texture_arrow_gesture = 0; // fine di un eventuale drag di una freccia della texture
        if(band_dragging){
            band_dragging = false;
            // il rubber band sparisce sempre al rilascio del mouse (la selezione resta)
            if(selection_band != nullptr){
                scene()->removeItem(selection_band);
                delete selection_band;
                selection_band = nullptr;
            }
            band_is_lasso = false;
            band_poly.clear();
            if(viewport()) viewport()->update(); // cancella il tracciato del lazo dal foreground
            // NON ripulire qui: ApplyBandSelection() ha già lasciato lo stato corretto durante
            // il drag (multipla, singola, o nessuna). Il vecchio ClearMultiSelection() su
            // !MultiSelectionActive() cancellava proprio la selezione SINGOLA appena creata da
            // un band che racchiude una sola figura (maniglia/bounding box inclusi).
        }
        if(my_animation->current_frame->stick_figures.count()>0
                && my_animation->CurrentFrame()->CurrentStickFigure() != nullptr) // può essere nullo dopo un ripristino undo/redo
        {
            if(my_animation->CurrentFrame()->CurrentStickFigure()->stick_list.count()>0
                     && my_stick_figure_widget_list->selectedItems().count() == 1)
            {
                //termina una rotazione di stick al rilascio
                if(my_animation->CurrentFrame()->CurrentStick() && my_animation->CurrentFrame()->CurrentStick()->view)
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
        // il gesto può aver spostato/ruotato/scalato la selezione: riadatta bounding box
        // (e maniglia, se il perno è al default) alla nuova posizione delle figure
        UpdateSelectionDecorations();
        UpdateIkOverlay(); // un edit può aver mosso lo start o il target di una catena
    }else if(event->button()==Qt::RightButton){
        is_right_pressed = false;
        setTransformationAnchor(QGraphicsView::AnchorViewCenter);
    }else if(event->button()==Qt::MiddleButton){
        is_middle_pressed = false;
        //zoom_lvl = zoom_lvl_buf/zoom_lvl;
        setTransformationAnchor(QGraphicsView::AnchorViewCenter);
    }

    QGraphicsView::mouseReleaseEvent(event);
}

/**
 * @details The body of every drag. Switches on the active `tool` and applies the motion
 * to the model: rotate a stick/subtree (NOTOOL/CLAMP), preview the free end while drawing
 * (DRAW*, StickFigure::PreviewDrawing()), manipulate/elongate/rotate/scale the current
 * stick or figure, or transform the whole frame (MOVESCENE/ROTATESCENE/SCALESCENE). The
 * scene-wide branches use `starting_coord` as the per-step reference and rotate/scale
 * every figure about the canvas centre. Independently of the tool, a held right button
 * pans and a held middle button zooms, using `prev_coord` as the previous position.
 */
void MyView::mouseMoveEvent(QMouseEvent *event)
{
    static QPointF prev_coord(0,0);
    MapMyCoords(coord, event->pos());
    // draw mode: trascinamento col sinistro premuto = pennellata continua (interpolata)
    if(draw_mode){
        if(draw_tool == DT_SEL_RECT || draw_tool == DT_SEL_LASSO){
            if(draw_sel_gesture && (event->buttons() & Qt::LeftButton))
                DrawSelMove(coord);
        } else if(draw_stroking && (event->buttons() & Qt::LeftButton)){
            PaintStrokeSegment(draw_last_pos, coord);
            draw_last_pos = coord;
        }
        return;
    }
    switch(tool)
    {
        case CLAMP:
        case NOTOOL:
        {
            if(is_pressed && band_dragging)
            {
                // rubber band di selezione: rettangolo o lazo secondo stickfig_lasso. Una figura
                // entra se un estremo di uno dei suoi stick cade nel rettangolo (intersects) o
                // dentro il poligono del lazo.
                QList<StickFigure*> touched;
                if(band_is_lasso){
                    band_poly << coord;
                    for(StickFigure *S : my_animation->current_frame->stick_figures){
                        for(StickData *s : S->stick_list){
                            if(band_poly.containsPoint(s->my_line.p1(), Qt::OddEvenFill) ||
                               band_poly.containsPoint(s->my_line.p2(), Qt::OddEvenFill)){
                                touched.append(S);
                                break;
                            }
                        }
                    }
                    if(viewport()) viewport()->update(); // il lazo è disegnato in drawForeground
                } else if(selection_band != nullptr){
                    QRectF band = QRectF(starting_coord, coord).normalized();
                    selection_band->setRect(band);
                    for(StickFigure *S : my_animation->current_frame->stick_figures){
                        for(StickData *s : S->stick_list){
                            if(band.intersects(QRectF(s->my_line.p1(), s->my_line.p2()).normalized())){
                                touched.append(S);
                                break;
                            }
                        }
                    }
                } else {
                    break;
                }
                ApplyBandSelection(touched);
                break;
            }
            if(is_pressed && sel_box_gesture == 2){ ApplySelectionRotate(); break; } // pomello box
            if(is_pressed && sel_box_gesture == 1){ ApplySelectionScale();  break; } // maniglia scala box
            if(is_pressed && texture_arrow_gesture){ DragTextureArrow(coord); break; } // freccia texture
            if(space_dragging && HasActiveSelection())
            {
                // Spazio tenuto premuto: la selezione segue il cursore, trascinata dai
                // suoi stick master (spec) — incrementale come MOVESCENE, senza click
                QPointF delta = coord - starting_coord;
                starting_coord = coord;
                for(StickFigure *S : SelectedFigures()){
                    if(S->master_stick && !S->stick_list.isEmpty())
                        S->Traslate(delta.x(), delta.y());
                }
                multi_pivot += delta;
                UpdateSelectionDecorations();
                break;
            }
            if(is_pressed && pivot_dragging)
            {
                // trascinamento della maniglia (o Ctrl+trascina su tela vuota): sposta
                // solo il perno delle trasformazioni, non la selezione
                multi_pivot = coord;
                // singola figura: memorizza lo spostamento come offset persistente dal master
                // (l'unico punto che aggiorna pivot_offset — bug 2). Va fatto PRIMA di
                // UpdateSelectionDecorations, che per la singola figura ricalcola multi_pivot
                // proprio da master.p1 + pivot_offset.
                if(!MultiSelectionActive()){
                    QList<StickFigure*> sel = SelectedFigures();
                    StickFigure *S = sel.isEmpty() ? nullptr : sel.first();
                    if(S != nullptr && S->master_stick != nullptr)
                        S->pivot_offset = coord - S->master_stick->my_line.p1();
                }
                UpdateSelectionDecorations();
                break;
            }
            if(is_pressed && multi_dragging)
            {
                // trascina tutta la selezione (incrementale come MOVESCENE); il perno
                // viaggia con le figure
                QPointF delta = coord - starting_coord;
                starting_coord = coord;
                for(StickFigure *S : multi_selection){
                    if(S->master_stick && S->stick_list.count() > 0)
                        S->Traslate(delta.x(), delta.y());
                }
                multi_pivot += delta;
                UpdateMultiSelectionUI();
                break;
            }
            if(is_pressed && poly_vertex_drag)
            {
                // trascinamento Ctrl: sposta tutti i vertici selezionati (anche su più
                // poligoni/multilinee) del delta del mouse, incrementale come MOVESCENE
                QPointF delta = coord - starting_coord;
                starting_coord = coord;
                for(StickData *s : my_animation->CurrentFrame()->total_sticks){
                    if(!IsVertexStick(s) || !s->view || s->view->selected_vertices.isEmpty())
                        continue;
                    QPolygonF verts = s->view->SceneVertices();
                    for(int idx : s->view->selected_vertices){
                        if(idx >= 0 && idx < verts.length())
                            s->view->EditPolygonPoint(idx, verts[idx] + delta);
                    }
                }
                break;
            }
            if(is_pressed)
            {
                if(scene()->selectedItems().count() == 1)
                {
                    //trova l'indice dello stick selezionato dentro la lista nella scena cercando il puntatore nelle liste
                    StickGraphicsItem* item = (StickGraphicsItem*)scene()->selectedItems()[0];
                    my_animation->UpdateSelection(item->data);

                    //qDebug("selected item %d", idx);
                    // Ora ruota lo stick (figura/stick correnti possono essere nulli se l'item
                    // selezionato non è stato trovato in nessuna figura del frame)
                    if(my_animation->CurrentFrame()->CurrentStickFigure() != nullptr &&
                            my_animation->CurrentFrame()->CurrentStick() != nullptr &&
                            my_animation->CurrentFrame()->CurrentStickFigure()->stick_list.count()>0){

                        StickData *picked = my_animation->CurrentFrame()->CurrentStick();
                        StickData *ik_chain_start = IkChainStart(picked);
                        bool picked_is_master = (picked == my_animation->CurrentFrame()->CurrentStickFigure()->master_stick);
                        // il master muove/ruota SEMPRE l'intera figura, anche se fa parte di
                        // una catena IK (spec): il blocco della rotazione normale vale solo
                        // per gli altri membri della catena, non per il master
                        if(!picked_is_master && ik_chain_start != nullptr && ik_chain_start->ik_target != nullptr
                                && ik_chain_start->ik_target->ik_role == StickData::IK_TARGET_ON){
                            // lo stick pescato appartiene a una catena IK ATTIVA: la
                            // rotazione normale è bloccata per tutti i suoi membri (spec:
                            // "non può essere ruotato normalmente a meno che l'intero
                            // stickfigure ruoti"); solo il TARGET risponde al drag, e lo
                            // fa tramite FABRIK invece della rotazione standard (spec:
                            // "simile al tool MANIPULATE, ma la catena risponde al movimento")
                            if(picked == ik_chain_start->ik_target){
                                SolveIkChain(ik_chain_start, coord);
                                UpdateIkOverlay(); // il marcatore (croce + linea) segue il target trascinato
                            }
                            break;
                        }

                        if(tool == CLAMP){
                            if(my_animation->CurrentFrame()->CurrentStick()->view)
                                my_animation->CurrentFrame()->CurrentStick()->view->Rotate(&coord);
                        }
                        else{
                            QList<StickGraphicsItem*> to_rotate;
                            if(my_animation->CurrentFrame()->CurrentStick() == my_animation->CurrentFrame()->CurrentStickFigure()->master_stick){
                                // ruotando dal master ruota TUTTA la figura, stepchild inclusi:
                                // il vecchio filtro !s->stepchild lasciava indietro gli arti
                                // agganciati all'origine del master (bug: la figura si spezzava)
                                for(StickData*s:my_animation->CurrentFrame()->CurrentStickFigure()->stick_list){
                                    if(s->view){
                                        to_rotate.append(s->view);
                                    }
                                }
                            }else{
                                for(StickData* s:my_animation->CurrentFrame()->CurrentStick()->children)
                                    if(s->view) to_rotate.append(s->view);
                            }
                            if(my_animation->CurrentFrame()->CurrentStick()->view)
                                to_rotate.append(my_animation->CurrentFrame()->CurrentStick()->view);
                            my_animation->CurrentFrame()->CurrentStickFigure()->RotateStickFigure(&coord,to_rotate,false);
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
        case DRAWFREEHAND: // anteprima gialla della linea tra 1° e 2° click, come un DRAW normale
        case DRAWTEXT:     // idem: la spina segue il mouse finché il testo è vuoto
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
                if(is_pressed){
                    if(my_animation->CurrentFrame()->CurrentStick()->view)
                        my_animation->CurrentFrame()->CurrentStick()->view->Manipulate(&coord);
                    elongate_spinbox->setValue(my_animation->CurrentFrame()->CurrentStick()->my_line.length());
                }
            }
            break;
        }
        case DRAWPOLY:
        case DRAWMULTI:
        {
            StickFigure *fig = my_animation->current_frame->stick_figures.isEmpty()
                    ? nullptr : my_animation->CurrentFrame()->CurrentStickFigure();
            // la linea base segue il mouse come per gli altri tool DRAW*
            if(fig != nullptr && fig->draw_count > 0){
                fig->PreviewDrawing(&coord);
                break;
            }
            StickData *session = PolySessionStick(tool == DRAWPOLY ? StickData::POLYGON
                                                                   : StickData::MULTILINE);
            if(session == nullptr)
                break;
            // trascinamento di un vertice esistente
            if(is_pressed && poly_drag_idx >= 0){
                session->view->EditPolygonPoint(poly_drag_idx, coord);
                break;
            }
            // anteprima live: il prossimo vertice segue il mouse prima del click
            if(poly_session_active){
                int insert_idx;
                if(tool == DRAWMULTI)
                    insert_idx = session->view->SceneVertices().length(); // estende la coda
                else if(poly_session_extends)
                    insert_idx = session->view->NearestEdgeInsertIndex(coord);
                else
                    insert_idx = session->polygon_points.length();
                session->view->SetPolygonPreview(coord, insert_idx);
            }
            break;
        }
        case DRAW_IK:
        {
            // durante la creazione, la linea tratteggiata e la croce seguono il mouse
            // finché il secondo click non commette (o Esc/cambio tool non annulla)
            StickData *pending = IkPendingStart();
            if(pending != nullptr && ik_preview_line != nullptr){
                ik_preview_line->setLine(QLineF(pending->my_line.p1(), coord));
                ik_preview_cross->setPos(coord);
            }
            break;
        }
        case ROTATE:
        {
            if(is_pressed)
                ApplySelectionRotate(); // stessa rotazione del pomello della transformation box
            break;
        }
        case(SCALE):
        {
            if(is_pressed)
                ApplySelectionScale(); // stessa scala delle maniglie della transformation box
            break;
        }
        case(MOVESCENE):
        {
            if(is_pressed && !my_animation->current_frame->stick_figures.isEmpty()){
                int dx = -starting_coord.x()+coord.x();
                int dy = -starting_coord.y()+coord.y();
                starting_coord = coord;
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
            if(is_pressed && !my_animation->current_frame->stick_figures.isEmpty()){
                QPointF center(W/2,H/2); // centro del sistema
                QLineF mouse_to_center_now(coord,center); // linea che connette il centro al mouse
                QLineF mouse_to_center_start(starting_coord,center); // linea che connette il centro alla posizione del mouse precedente
                float starting_angle = mouse_to_center_start.angle(); // angolo assoluto del mouse prima rispetto al centro

                float actual_angle = mouse_to_center_now.angle();// angolo assoluto del mouse ora rispetto al centro
                float angle = actual_angle-starting_angle; // incremento in angolo rispetto a prima
                // normalizza l'incremento: angle() torna un valore in [0,360), quindi attraversare lo
                // 0/360 senza questo produce uno scatto di quasi un giro intero per un singolo frame
                if(angle>180) angle -= 360;
                else if(angle<-180) angle += 360;
                starting_coord = coord; // le coordinate attuali vengono memorizzate per la prossima iterazione
                for(StickFigure*S:my_animation->current_frame->stick_figures){
                    //check it is not an empty layer
                    if( S->stick_list.count()>0 && S->master_stick){
                        QLineF connect_center(center,S->master_stick->my_line.p1()); // linea che connette il master dello stick al centro

                        connect_center.setAngle(connect_center.angle()+angle); // ruotiamola dell'angolo di incremento
                        // ruota ogni singolo stick
                        for(StickData *s:S->stick_list){

                            if(s->view){
                                s->view->Rotate(angle);
                                s->view->angle_buffer2+=angle; // aggiorna il buffer interno dell'angolo dello stick
                            }
                        }

                        QLineF delta(S->origin_pos_buffer,connect_center.p2()); // traslazione della punta della lineacentro-master ruotata
                        //trasla lo stickfigure dell'ammontare
                        S->Traslate(delta.dx(),delta.dy());
                        S->origin_pos_buffer = S->master_stick->my_line.p1(); // agiorna il buffer di posizione dello stickfigure
                    }
                    }
            }
            break;
        }
        case(SCALESCENE):
        {


            if(is_pressed && !my_animation->current_frame->stick_figures.isEmpty()){
                float scale_amount = 1;
                float dist = 0;
                QPointF center(W/2,H/2);
                float start_dist = QLineF(starting_coord,center).length();
                if(start_dist<1) // evita una divisione per zero (o quasi) se si preme esattamente sul centro
                    break;
                dist = QLineF(coord,center).length();
                scale_amount = dist/start_dist;
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
                        QLineF actual_dist_from_center(S->master_stick->my_line.p1(),center);
                        QLineF original_dist_from_center(S->origin_pos_buffer,center);
                        // quindi fai la differenza fra il valore di distaza originale (max) e la distanza attuale modificata dal fattore di scala per dx e dy
                        dx = -original_dist_from_center.dx()*scale_amount+actual_dist_from_center.dx();
                        dy = -original_dist_from_center.dy()*scale_amount+actual_dist_from_center.dy();
                        S->Traslate(dx,dy);
                        for(StickData* s:S->stick_list){
                            if(s->view) s->view->Scale(scale_amount);

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
            if(is_pressed){
                QLineF new_end_point(my_animation->CurrentFrame()->CurrentStick()->my_line.p1(),coord);
                length_buffer = new_end_point.length();
                my_animation->CurrentFrame()->CurrentStickFigure()->Elongate(coord,my_animation->CurrentFrame()->CurrentStick());
                elongate_spinbox->setValue(length_buffer);
            }
            break;
        }

    }
    if(is_right_pressed){
        setTransformationAnchor(QGraphicsView::AnchorUnderMouse);
        QLineF travel(prev_coord,coord);
        translate(travel.dx(),travel.dy());
    }else if(is_middle_pressed){

        //calcola lo scostamento dal centro all'inizio della pressione e quella attuale
        float dist_from_center0 = QLineF(prev_coord,QPointF(W/2,H/2)).length();
        float dist_from_center1 = QLineF(coord,QPointF(W/2,H/2)).length();

        zoom_lvl_buf = zoom_lvl+(dist_from_center1-dist_from_center0)/(0.5*sqrtf(W*W+H*H)); // zoombuf è incrementato dal valore precedente di un valore sempre minore di 1

        scale(zoom_lvl_buf/zoom_lvl,zoom_lvl_buf/zoom_lvl); // la divisione serve a riportare allo zoom 0, lo zoom applicato è zombuf
        zoom_lvl = zoom_lvl_buf;
    }
    prev_coord = coord;
    if(is_pressed) // solo durante un trascinamento: un edit può muovere start/target IK
        UpdateIkOverlay();
    QGraphicsView::mouseMoveEvent(event);
}
//funzione usata per riscalare interamente il frame quando si riscala l'animazione intera
/**
 * @details Scales one frame's figures about the centre of R: each figure is translated
 * so its master keeps its relative offset from the centre, then every stick is scaled
 * by scale_amount (its scale_buffer/width_buffer are refreshed so the scale is applied
 * from the current size). Used by SizeChange()'s REPOSCALE path when the canvas size
 * changes. NOTE: the "Rescale animation" feature this feeds has a known width-only
 * scale-factor bug (see CLAUDE.md "Known unresolved items").
 */
void MyView::ScaleFrame(Frame* F, float scale_amount, QRectF R){
    QPointF center(R.width()/2,R.height()/2);
    if(F->stick_figures.isEmpty())
        return;
    for(StickFigure * S:F->stick_figures){
        if(!S->stick_list.isEmpty()){
            QLineF actual_dist_from_center(S->master_stick->my_line.p1(),center);
            float dx =-(scale_amount-1)*actual_dist_from_center.dx();
            float dy =-(scale_amount-1)*actual_dist_from_center.dy();
            S->Traslate(dx,dy);
            for(StickData* s:S->stick_list){
                if(s->view){
                    s->view->scale_buffer = s->my_line.length();
                    s->view->width_buffer = s->pen.widthF();
                    s->view->Scale(scale_amount);
                }
            }
        }

    }
}
/**
 * @details Thin pass-through to Animation::SetupFrame() (which also inserts the list
 * row and shifts the following frame numbers). Kept as a member so the add-frame button
 * in widget.cpp and RestoreFrameSnapshot() (undo of a delete / redo of an add) share
 * one entry point.
 */
Frame* MyView::SetUpFrame(int pos){
    return my_animation->SetupFrame(pos);
}
/**
 * @details The view/list half of deleting a frame: takes its list row, calls
 * Animation::DeleteFrame() for the model half, then renumbers every later frame's
 * number and list label so the sequence stays contiguous. Used by the delete-frame
 * button (widget.cpp) and by RestoreFrameSnapshot() (undo of an add / redo of a delete).
 */
void MyView::DeleteFrame(Frame* frame){
    int frame_num = frame->frame_number;
    my_frame_widget_list->takeItem(my_frame_widget_list->row(frame->linked_item));
    my_animation->DeleteFrame(frame);
    my_frame_widget_list->clearSelection();
    for(Frame *f: my_animation->frame_list){
        if(f->frame_number>frame_num){
            f->frame_number--;
            f->UpdateLabel(); // il numero è cotto nell'anteprima, vedi Frame::UpdateLabel()
        }
    }
    if(my_animation->current_frame != nullptr) // cancellato l'ultimo frame: niente da selezionare
        my_animation->current_frame->linked_item->setSelected(true);
}
/**
 * @details Le righe della lista layer sono le uniche QListWidgetItem che una StickFigure
 * possa possedere (vedi PopulateLiveFrame()), e finora nessuno azzerava il linked_item di
 * chi le perdeva: ogni frame visitato e poi lasciato restava con puntatori PENDENTI, cosa
 * che non si notava solo perché Animation::DeleteFrame() li rilegge unicamente per
 * `frame == current_frame`, cioè — di norma — per il frame mostrato, le cui righe esistono
 * davvero. "Di norma" non è sempre: Animation::DeleteFrame() riassegna essa stessa
 * current_frame, quindi dentro un ciclo di cancellazioni (FrameBatchUndoCommand::Remove())
 * il current diventa un frame NON mostrato, e cancellarlo passava un QListWidgetItem già
 * distrutto a QListWidget::row() — segfault. Si azzera dove le righe muoiono, così il
 * puntatore non può mai sopravvivere all'oggetto.
 */
void MyView::ClearLayerRows()
{
    if(my_animation->current_frame != nullptr){
        for(StickFigure *S:my_animation->current_frame->stick_figures)
            S->linked_item = nullptr;
    }
    my_stick_figure_widget_list->clear();
}
// routine per passare da un frame ad un altro, svuota la scena, svuota le widget list, ripopola entrambi, aggiorna onion skins e selezioni
/**
 * @details The single frame-switch routine (frame list slot in widget.cpp, undo/redo
 * via RestoreFrameSnapshot(), file load in mymainwindow.cpp and playback all reach it).
 * It detaches the old frame's views/handles from the scene, clears the layer list, then
 * for the new frame: gives every stick a live view — reattaching an existing one or
 * building it with FramePresenter::AttachView() for a just-loaded frame that has none —
 * rebuilds the layer list, restores selection and (outside playback) refreshes onion
 * skins and the frame thumbnail.
 */
void MyView::MoveToFrame(Frame* frame){
    // qualunque cambio frame chiude prima la sessione di disegno: finalizza sul frame
    // CORRENTE (draw_target vive lì) prima di passare a un altro, evitando dangling.
    // No-op se non si è in draw mode; i chiamanti UI escono già prima di StoreUndo
    if(draw_mode)
        ExitDrawMode();
    // il cambio frame invalida la sessione poligono e la selezione vertici (stato UI
    // del frame lasciato); i flag da soli non tengono puntatori, basta azzerarli
    poly_session_active = false;
    poly_session_extends = false;
    poly_drag_idx = -1;
    poly_vertex_drag = false;
    ClearMultiSelection(); // la selezione multipla decade al cambio frame (spec)
    // idem per un'eventuale creazione di catena IK a metà: niente CancelIkDrawing() qui
    // (toccherebbe ik_role di uno stick del frame che si sta per lasciare), solo lo
    // stato di sessione e gli item di anteprima (altrimenti orfani sulla scena)
    ik_drawing = false;
    ClearIkPreviewItems();
    // rimuovi tutti gli stick dalla scena
    auto current_scene = scene();
    if(my_animation->frame_list.count()>=1){
        for(StickFigure *S:my_animation->current_frame->stick_figures){
            RemoveHandles(S);
            for(StickData * s: S->stick_list){
               if(s->view && scene()->items().contains(s->view)){
                    current_scene->removeItem(s->view);

               }
            }


        }
    }
    // rimuovigli elementi dalla lista widget (azzerando i linked_item che stanno per morire)
    ClearLayerRows();

    // assicura che ogni stick del nuovo frame abbia una view PRIMA di AddHandles() qui sotto:
    // un frame appena caricato da file (attach_to_scene=false) non ne ha ancora una, un frame
    // già visitato in precedenza sì, basta ri-attaccarla alla scena
    // num_of_items viene serializzato come campo indipendente: su un file incoerente può
    // eccedere la lista reale, e total_sticks[j] leggerebbe fuori dai limiti
    int attachable = qMin(frame->num_of_items, (int)frame->total_sticks.count());
    for(int j = 0;j<attachable;j++)
    {
        StickData *s = frame->total_sticks[j];
        if(s->view)
            current_scene->addItem(s->view);
        else
            FramePresenter::AttachView(s, current_scene);
    }

    // aggiungi tutta gli stickfigure del nuovo frame alla lista del widget
    for(StickFigure *S:frame->stick_figures)
    {
        QListWidgetItem * added_item = new QListWidgetItem(my_stick_figure_widget_list);
        QVariant new_data(QVariant::fromValue(S));
        added_item->setData(Qt::UserRole,new_data);
        added_item->setData(Qt::DisplayRole,S->name);
        added_item->setIcon(*S->stick_figure_icon);
        S->linked_item = added_item;
        my_stick_figure_widget_list->addItem(S->linked_item);
        // aggiungi alla scena
        if(!play_back){
            S->UpdateBoundingRects();
            S->UpdateIcon();
            AddHandles(S);
        }
    }
    current_scene->clearSelection();
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

    if(!play_back)
    {
        UpdateOnionSkins();//aggiorna gli onion skins
    }
    // se non siamo in modalità playback aggiorna le selezioni nelle widgetlists e delle iconee
    if(play_back == false){
        // ClearAndSelect ESPLICITO: QListWidgetItem::setSelected() sgombra la selezione
        // precedente solo in SingleSelection (Qt prende un ramo diverso per modalità —
        // misurato), e da quando la lista è ContiguousSelection il frame lasciato restava
        // selezionato accanto a quello nuovo — visibilissimo duplicando un frame. Un
        // cambio frame definisce SEMPRE una selezione nuova di un solo elemento: la
        // selezione multipla si estende con shift+clic, che non passa da qui
        // (Widget::on_frameListWidget_itemClicked() esce prima)
        my_frame_widget_list->setCurrentItem(my_animation->current_frame->linked_item,
                                            QItemSelectionModel::ClearAndSelect);
        // niente guardia su stick_figures.isEmpty(): UpdateIcon() gestisce già correttamente un
        // frame vuoto (lo riempie di bianco), senza di essa un frame appena creato restava senza
        // anteprima invece di mostrare una tela bianca
        if(!my_animation->frame_list.isEmpty()){
            my_animation->current_frame->UpdateIcon();
        }
        UpdateSelectionDecorations(true); // maniglia + bounding box della figura corrente
    }
    my_animation->CurrentFrame()->Refresh(0);
    UpdateIkOverlay(); // il nuovo frame può avere le sue catene IK da marcare
}
// routine per calcolare gli onion skins, prima cancellali, ridisegnali su un immagine, e aggiungi l'immagine alla scena
/**
 * @details Rebuilds the faded "ghosts" of the frames before the current one. Old onion
 * pixmaps are removed and freed, then the previous N frames (N = onion_skin_sb spinbox,
 * capped at frame_number) are each rendered onto a pixmap whose opacity grows towards
 * the current frame. Each ghost is drawn from Clone()d StickData wrapped in throwaway
 * views on a private scene, and the render scene is cleared BEFORE the clones are freed
 * so ~StickGraphicsItem() doesn't read a deleted data->view — same rule as
 * StickFigure::UpdateIcon() (see CLAUDE.md "Data model"). `onion_render` marks this
 * special path for code that must behave differently while rendering.
 */
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
    StickGraphicsItem::onion_render = true;
    QGraphicsScene render_scene; // scena fittizia su cui renderizzare gli onionskins
    render_scene.setSceneRect(scene()->sceneRect());
    // dallo spinbox trai il valore del numero di onionskins servono
    int skin_num = onion_skin_sb->value();
    float opacity_val = 0;
    if(my_animation->current_frame->frame_number<skin_num)
    {
        skin_num = my_animation->current_frame->frame_number;
    }
    // disegna sulla scena fittizia gli stickfigures dei frame da onionskinnare che partono dal frame corrente- numero di skin calcolabili
    for(int i = my_animation->current_frame->frame_number-skin_num; i<my_animation->current_frame->frame_number; i++){
        // todo anzichè copiare stickfigure per stickfigure, copiare l'intera scena
        // questi stick sono VIVI (hanno già una view sul frame i): clonarne i dati prima di
        // wrapparli evita che il costruttore di stick rubi il back-pointer data->view alla
        // view reale (vedi StickFigure::UpdateIcon per lo stesso pattern)
        QList<StickData*> render_clones;
        for(StickData *s:my_animation->frame_list[i]->total_sticks)
        {
            StickData* clone = s->Clone();
            render_clones.append(clone);
            render_scene.addItem(new StickGraphicsItem(clone));
        }
        // più è vecchio l'onion skin più è trasparente, più è vicino al frame corrente più è opaco
        opacity_val += 0.5/skin_num;
        // solo l'ingombro delle figure, non tutta la scena: un fantasma costava 1.44x la tela
        // (12 MB a 1080p) a ogni cambio frame, ora costa quanto le figure che mostra
        QRect bounds = render_scene.itemsBoundingRect().intersected(render_scene.sceneRect()).toAlignedRect();
        if(!bounds.isEmpty()){
            // premoltiplicato: è il formato in cui il raster engine dipinge, ARGB32 semplice
            // passava ogni span da una conversione (e QPixmap::fromImage ne faceva un'altra)
            QImage render_img(bounds.size(), QImage::Format_ARGB32_Premultiplied);
            render_img.fill(Qt::transparent);
            QPainter painter(&render_img);
            render_scene.render(&painter, QRectF(), bounds);
            painter.end();
            // trasferisci la scena renderizzata
            QGraphicsPixmapItem* item = new QGraphicsPixmapItem(QPixmap::fromImage(std::move(render_img)));
            item->setOffset(bounds.topLeft());
            item->setOpacity(opacity_val);
            item->setZValue(0);
            onion_skins.append(item);
            scene()->addItem(item);
        }
        render_scene.clear(); // distruggi le view di rendering PRIMA di liberare i dati clonati
        qDeleteAll(render_clones);
    }

    StickGraphicsItem::onion_render = false;
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
        // la selezione multipla viaggia con lo snapshot come indici di riga (i puntatori
        // alle figure muoiono nel restore): undo/redo la ripristinano com'era
        selection_before_ = view_->MultiSelectionIndices();
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
        view_->SetMultiSelectionByIndices(selection_after_);
    }
    void undo() override
    {
        if(after_ == nullptr){
            after_ = ExtractFrameData(*view_->my_animation->current_frame);
            selection_after_ = view_->MultiSelectionIndices();
        }
        view_->RestoreFrameSnapshot(command_tag_, MODE_UNDO, before_);
        view_->SetMultiSelectionByIndices(selection_before_);
    }
private:
    MyView *view_;
    int command_tag_;
    FrameData *before_ = nullptr;
    FrameData *after_ = nullptr;
    QList<int> selection_before_;
    QList<int> selection_after_;
    bool is_first_redo_ = true;
};

/**
 * @brief Undo entry for a contiguous BLOCK of frames appearing or disappearing at once,
 *        as ONE step: the inbetweens of MyView::GenerateInbetweens(), a multi-frame
 *        paste (MyView::InsertFrames()) or a multi-frame delete (MyView::DeleteFrames()).
 *
 * @details FrameUndoCommand above covers exactly one frame, so it cannot express any of
 * these; this command owns the N ready-made FrameData snapshots instead and is a
 * textbook QUndoCommand (unlike FrameUndoCommand, the mutation is NOT performed inline
 * by the caller): `QUndoStack::push()` calls redo() and that is what actually inserts —
 * or removes — the frames, so undo/redo are perfectly symmetric. The two directions are
 * the same pair of routines with `inserting_` deciding which way round they run, since
 * "undo an insertion" and "do a deletion" are the same operation; the snapshots are what
 * a deletion is restored FROM, and are cheap to keep (QImage is copy-on-write, so a
 * snapshot shares its pixels with the live sticks — see CLAUDE.md "Images are shared").
 * Frames outside the block are never touched by either direction.
 */
class FrameBatchUndoCommand : public QUndoCommand
{
public:
    /**
     * @param view the canvas whose animation is edited.
     * @param first_index index the first frame of the block occupies.
     * @param snapshots one snapshot per frame of the block, in order; taken over by this
     *        command and freed in its destructor.
     * @param inserting true → redo() inserts the block (paste, inbetweens), false →
     *        redo() removes it (multi-frame delete).
     */
    FrameBatchUndoCommand(MyView *view, int first_index, const QList<FrameData*> &snapshots,
                          bool inserting)
        : view_(view), first_index_(first_index), snapshots_(snapshots), inserting_(inserting)
    {
    }
    ~FrameBatchUndoCommand() override
    {
        qDeleteAll(snapshots_);
    }
    void redo() override
    {
        if(inserting_)
            Insert();
        else
            Remove();
    }
    void undo() override
    {
        if(inserting_)
            Remove();
        else
            Insert();
    }
private:
    /// Rebuilds the block at first_index_, one live frame per snapshot, and shows the first.
    void Insert()
    {
        for(int i = 0; i < snapshots_.count(); i++){
            Frame *added = view_->SetUpFrame(first_index_ + i);
            // PopulateLiveFrame() copia frame_number DAL dato sul frame vivo, sovrascrivendo
            // quello che SetUpFrame() gli ha appena dato: allinea lo snapshot alla posizione
            // in cui sta entrando (per un paste il numero salvato è quello di origine)
            snapshots_[i]->frame_number = first_index_ + i;
            // attach_to_scene = false di proposito: con true gli stick di TUTTI i frame
            // del blocco finirebbero insieme sulla scena, e MoveToFrame() ne rimuove solo
            // quelli di UN frame. Stesso schema del load da file: le view le crea
            // MoveToFrame() (FramePresenter::AttachView) per il solo frame mostrato
            PopulateLiveFrame(added, *snapshots_[i], false, false);
            added->UpdateIcon(); // l'anteprima invece serve subito: il frame non passa da MoveToFrame
        }
        // mostra il primo frame del blocco, così l'effetto dell'operazione è visibile
        view_->MoveToFrame(view_->my_animation->frame_list[first_index_]);
    }
    /// Deletes the block again and shows the frame that preceded it (or the first surviving one).
    void Remove()
    {
        // Animation::DeleteFrame() ripulisce le righe della lista layer per il solo
        // current_frame, ma è essa stessa a riassegnare current_frame a ogni giro: dal
        // secondo in poi il current NON è più il frame mostrato, e la sua figura non ha
        // righe vere. Le righe si tolgono qui, tutte in una volta e finché i puntatori sono
        // ancora coerenti (qui il current è per forza il frame mostrato), così nessun giro
        // del ciclo può passarne una a QListWidget::row(); la MoveToFrame() in fondo le
        // ricostruisce per il frame che resta
        view_->ClearLayerRows();
        // dall'ultimo al primo: cancellare un frame più avanti non sposta gli indici di
        // quelli prima, quindi first_index_+i resta valido a ogni giro
        for(int i = snapshots_.count()-1; i >= 0; i--)
            view_->DeleteFrame(view_->my_animation->frame_list[first_index_ + i]);
        // il frame prima del blocco, che esiste sempre tranne quando il blocco partiva da 0
        int show = qBound(0, first_index_ - 1, (int)view_->my_animation->frame_list.count()-1);
        view_->MoveToFrame(view_->my_animation->frame_list[show]);
    }
    MyView *view_;
    int first_index_;
    QList<FrameData*> snapshots_;
    bool inserting_;
};

/**
 * @details Thin push of a FrameBatchUndoCommand in the inserting direction — the command's
 * own redo(), which `QUndoStack::push()` calls, is what creates the frames. Shared by the
 * multi-frame paste (widget.cpp) and by GenerateInbetweens().
 */
void MyView::InsertFrames(int insert_at, const QList<FrameData*> &snapshots)
{
    if(snapshots.isEmpty())
        return;
    undo_stack.push(new FrameBatchUndoCommand(this, insert_at, snapshots, true));
}

/**
 * @details Snapshots the whole block BEFORE anything is removed (that is what an undo
 * restores it from) and hands it to FrameBatchUndoCommand in the removing direction, whose
 * redo() performs the deletion. Refuses to empty the animation: at least one frame must
 * survive, exactly like the single-frame delete button.
 */
bool MyView::DeleteFrames(int first_index, int count)
{
    if(count < 1 || first_index < 0 || first_index + count > my_animation->frame_list.count())
        return false;
    if(count >= my_animation->frame_list.count())
        return false;
    QList<FrameData*> snapshots;
    for(int i = 0; i < count; i++)
        snapshots.append(ExtractFrameData(*my_animation->frame_list[first_index + i]));
    undo_stack.push(new FrameBatchUndoCommand(this, first_index, snapshots, false));
    return true;
}

/**
 * @details Replaces the pre-Phase-4 `undoRedo()`'s dispatch body verbatim, just
 * driven by a QUndoCommand's before/after snapshot instead of a popped buffer entry.
 * No flag needed here anymore: `Frame::ClearFrame()` and `~StickFigure()` each own
 * exactly the cleanup they perform, so nothing needs to skip work for the other.
 */
void MyView::RestoreFrameSnapshot(int command_tag, int mode, FrameData *snapshot)
{
    // un undo/redo ricostruisce gli stick del frame: qualunque sessione poligono o
    // trascinamento in corso si riferiva a oggetti che stanno per sparire
    poly_session_active = false;
    poly_session_extends = false;
    poly_drag_idx = -1;
    poly_vertex_drag = false;
    ClearMultiSelection(); // le figure selezionate stanno per essere ricostruite
    ik_drawing = false; // idem per una creazione di catena IK a metà: vedi MoveToFrame()
    ClearIkPreviewItems();
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
        Frame* added_frame = SetUpFrame(snapshot->frame_number);
        PopulateLiveFrame(added_frame,*snapshot,true,true);
        // spostati sulla destinazione
        MoveToFrame(added_frame);
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

/**
 * @details Validates first and modifies nothing until every check passes (the caller only
 * gets a message back), then builds the N snapshots and hands them to
 * InsertFrames(), whose FrameBatchUndoCommand::redo() — run by `undo_stack.push()` — is
 * what actually inserts the frames. Each snapshot starts as a Clone() of the base frame, so
 * everything that cannot be interpolated (stick types, images, IK roles, layer names,
 * z-order) comes from a real frame, and only the pose of each stick is rewritten by
 * InterpolateStickData() at t = i/(N+1) — t never reaches 0 or 1, so no generated frame
 * duplicates a keyframe.
 */
QString MyView::GenerateInbetweens(int start_index, int count)
{
    // PRIMA di validare: ExitDrawMode() finalizza (e può cancellare, se non si è dipinto
    // niente) lo stick freehand del frame corrente, che è uno dei due keyframe. Chiuderla
    // dopo i controlli li renderebbe stantii, e le due liste di stick verrebbero appaiate
    // per indice dopo una rimozione a monte
    ExitDrawMode();
    QList<Frame*> &frames = my_animation->frame_list;
    if(count < 1)
        return tr("The number of inbetweens to generate must be at least 1.");
    if(start_index < 0 || start_index + 1 >= frames.count())
        return tr("Both frames must exist: there is no frame on that side of the selected one.");
    Frame *start_frame = frames[start_index];
    Frame *end_frame = frames[start_index + 1];
    if(start_frame->stick_figures.count() != end_frame->stick_figures.count()){
        return tr("Frames %1 and %2 have a different number of stickfigures (%3 and %4).")
                .arg(start_frame->frame_number).arg(end_frame->frame_number)
                .arg(start_frame->stick_figures.count()).arg(end_frame->stick_figures.count());
    }
    for(int f = 0; f < start_frame->stick_figures.count(); f++){
        StickFigure *from_figure = start_frame->stick_figures[f];
        StickFigure *to_figure = end_frame->stick_figures[f];
        if(from_figure->stick_list.count() != to_figure->stick_list.count()){
            return tr("Stickfigure %1 has a different number of sticks in the two frames (%2 and %3).")
                    .arg(f).arg(from_figure->stick_list.count()).arg(to_figure->stick_list.count());
        }
        for(int s = 0; s < from_figure->stick_list.count(); s++){
            if(from_figure->stick_list[s]->stick_type != to_figure->stick_list[s]->stick_type){
                return tr("Stick %1 of stickfigure %2 is not of the same type in the two frames.")
                        .arg(s).arg(f);
            }
        }
    }
    UpdateFrameOrder(my_animation->CurrentFrame()); // l'ordine z del base deve essere una permutazione valida
    // "una copia del frame corrente" (spec): dalla UI il corrente è sempre uno dei due
    // estremi; il ripiego sul frame iniziale copre un chiamante che passasse altri indici,
    // ed è comunque strutturalmente identico all'altro estremo per i controlli qui sopra
    Frame *base_frame = (my_animation->CurrentFrame() == end_frame) ? end_frame : start_frame;
    FrameData *base = ExtractFrameData(*base_frame);
    QList<FrameData*> snapshots;
    for(int i = 1; i <= count; i++){
        FrameData *inbetween = base->Clone();
        inbetween->name.clear(); // i frame generati nascono senza nome, non col nome del base
        float t = float(i)/float(count + 1);
        for(int f = 0; f < inbetween->stick_figures.count(); f++){
            for(int s = 0; s < inbetween->stick_figures[f]->stick_list.count(); s++){
                InterpolateStickData(*inbetween->stick_figures[f]->stick_list[s],
                                     *start_frame->stick_figures[f]->stick_list[s],
                                     *end_frame->stick_figures[f]->stick_list[s], t);
            }
        }
        snapshots.append(inbetween);
    }
    delete base;
    InsertFrames(start_index + 1, snapshots);
    return QString();
}
/**
 * @details Renumbers the z-order `order` field of every stick in the current frame to
 * its list position and refreshes `num_of_items`. The `f` parameter is retained for
 * call-site compatibility but the routine always operates on the current frame.
 */
void MyView::UpdateFrameOrder(Frame* f){
    Q_UNUSED(f); // ritenuto per compatibilità con i call site; opera sempre sul frame corrente
    int j = 0;
    for(StickData*s:my_animation->CurrentFrame()->total_sticks)
    {
        s->order = j;
        j++;
    }
    my_animation->CurrentFrame()->num_of_items = my_animation->CurrentFrame()->total_sticks.length();
}

/**
 * @details Puts each stick's two endpoint handles (StickGraphicsItem::stick_handle[])
 * on the scene, painting the master's origin handle green so the figure's root is easy
 * to spot. Sticks without a live view are skipped. Paired with RemoveHandles(); called
 * whenever a figure becomes editable on screen (MoveToFrame(), split/merge, load).
 */
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

/**
 * @details Takes a figure's endpoint handles off the scene WITHOUT deleting them (they
 * belong to the StickGraphicsItem and are re-shown by AddHandles()). Called before a
 * frame switch or a structural edit (MoveToFrame(), SplitStickFigure(), JOIN) so stale
 * handles don't linger on the canvas.
 */
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
void MyView::SaveLibrary(QString file_name){
    QFile file(file_name);
    if(!file.open(QIODevice::WriteOnly)){
        return;
    }
    QDataStream out(&file);
    out << *stick_library_buffer;
    file.close();
}
/**
 * @details Builds a throwaway one-figure FrameData (the same record type a *.marlib file
 * holds) and writes it out — the currently-open library buffer (stick_library_buffer) is
 * never touched, so this can't disturb whatever the Libraries tab has loaded.
 */
void MyView::SaveStickFigureAsLibrary(const QString &file_name, StickFigure *figure){
    if(!figure || figure->stick_list.isEmpty())
        return;
    QFile file(file_name);
    if(!file.open(QIODevice::WriteOnly)){
        return;
    }
    FrameData single_figure_lib;
    single_figure_lib.stick_figures.append(ExtractStickFigureData(*figure));
    QDataStream out(&file);
    out << single_figure_lib;
    file.close();
}
/**
 * @details One prompt per load, not per stick: StickData::operator>> has already applied
 * the Arial fallback to every TEXT stick whose font is missing and collected the family
 * names, so this only reports and resets. Called by every load path (see myview.h).
 *
 * It also releases the load-time image pool, for the same reason and at the same moment:
 * both are state that operator>> accumulates across a whole file and that only a load's
 * end can know is complete. Necessarily BEFORE the early return below — the pool has to be
 * dropped after every load, not only after one that happened to hit a missing font.
 */
void MyView::ReportMissingTextFonts()
{
    ClearStickImagePool();
    if(missing_text_fonts.isEmpty())
        return;
    QMessageBox::warning(this, tr("Missing fonts"),
                         tr("These fonts of the imported text sticks are not installed on "
                            "this system and were replaced with Arial:\n\n%1")
                         .arg(missing_text_fonts.join(", ")));
    missing_text_fonts.clear();
}
/**
 * @details The library buffer owns plain data: replacing it is delete + new, no
 * per-figure cleanup choreography (freeLibraryFrame is gone).
 */
void MyView::ClearCurrentLib(){
    my_current_library_widget->clear();
    delete stick_library_buffer;
    stick_library_buffer = new FrameData();
}
/**
 * @details Reads the library file into the data buffer, then builds the list-widget
 * entries: each item stores its StickFigureData pointer and an icon rendered from the
 * data. mode!=0 keeps the widget untouched (legacy semantics, currently unused).
 */
void MyView::LoadLibrary(QString file_name, int mode){
    QFile file(file_name);
    if(!file.open(QIODevice::ReadOnly)){
        return;
    }
    QDataStream in(&file);
    my_current_library_widget->clear();
    delete stick_library_buffer;
    stick_library_buffer = new FrameData();
    in >> *stick_library_buffer;
    file.close();
    ReportMissingTextFonts();
    if(mode)
        return;
    // aggiungi tutti gli stickfigure della libreria alla lista del widget
    for(StickFigureData *figure_data:stick_library_buffer->stick_figures){
        QListWidgetItem * added_item = new QListWidgetItem();
        added_item->setData(Qt::UserRole,QVariant::fromValue(figure_data));
        added_item->setData(Qt::DisplayRole,figure_data->name);
        added_item->setIcon(RenderLibraryIcon(*figure_data));
        my_current_library_widget->addItem(added_item);
    }
}
/**
 * @details Extracts the current figure into a data record, appends it to the library
 * buffer and creates its widget entry (insert-after-current + select, as before).
 * @return the live source figure, or nullptr if it has no sticks.
 */
StickFigure* MyView::AddStickFigureToLibrary(const QString &name){
    // con una selezione multipla ogni figura selezionata entra in libreria — l'unico
    // export ammesso per una selezione multipla (spec); la selezione resta attiva
    if(MultiSelectionActive()){
        StickFigure *last_added = nullptr;
        QList<StickFigure*> selection = multi_selection;
        for(StickFigure *S : selection){
            if(S->stick_list.isEmpty())
                continue;
            StickFigureData *figure_data = ExtractStickFigureData(*S);
            stick_library_buffer->stick_figures.append(figure_data);
            QListWidgetItem *added_item = new QListWidgetItem();
            added_item->setData(Qt::UserRole,QVariant::fromValue(figure_data));
            added_item->setData(Qt::DisplayRole,figure_data->name);
            added_item->setIcon(RenderLibraryIcon(*figure_data));
            my_current_library_widget->addItem(added_item);
            last_added = S;
        }
        return last_added;
    }
    if(my_animation->CurrentFrame()->CurrentStickFigure() == nullptr) // possibile dopo un ripristino undo/redo
        return nullptr;
    if(my_animation->CurrentFrame()->CurrentStickFigure()->stick_list.count()>0)
    {
        StickFigureData *figure_data = ExtractStickFigureData(*my_animation->CurrentFrame()->CurrentStickFigure());
        if(!name.isEmpty())
            figure_data->name = name;
        stick_library_buffer->stick_figures.append(figure_data);
        QListWidgetItem * added_item = new QListWidgetItem();
        added_item->setData(Qt::UserRole,QVariant::fromValue(figure_data));
        added_item->setData(Qt::DisplayRole,figure_data->name);
        added_item->setIcon(RenderLibraryIcon(*figure_data));
        int new_idx = my_current_library_widget->currentRow()+1;
        my_current_library_widget->insertItem(new_idx,added_item);
        my_current_library_widget->clearSelection();
        added_item->setSelected(true);
        my_current_library_widget->setCurrentRow(new_idx);
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
    QListWidgetItem *item_to_remove = my_current_library_widget->takeItem(idx);
    StickFigureData* to_remove = qvariant_cast<StickFigureData*>(item_to_remove->data(Qt::UserRole));
    delete item_to_remove;
    stick_library_buffer->stick_figures.removeAll(to_remove);
    delete to_remove;
}
/**
 * @brief Copies the "what it is painted with" half of one pen onto another.
 * @param target pen to modify; its width, cap and join style are left untouched.
 * @param source pen to copy the paint from.
 *
 * @details The paint of a pen is its whole QBrush, not just a colour: since the stroke
 * type combo box can make it a gradient or a tile, ATTRIBUTE_PENCOLOR cannot use
 * QPen::setColor() any more — that call REPLACES the brush with a flat one and would
 * silently drop the gradient it was asked to apply. Qt::NoPen ("no stroke") is a pen style
 * rather than a brush, so it is carried across separately, and leaving it restores a solid
 * line without disturbing any other line style.
 */
static void CopyPenPaint(QPen *target, const QPen &source)
{
    if(source.style() == Qt::NoPen){
        target->setStyle(Qt::NoPen);
        return;
    }
    if(target->style() == Qt::NoPen)
        target->setStyle(Qt::SolidLine);
    target->setBrush(source.brush());
}

/**
 * @details Copies one attribute (pen paint/width or brush, per `attribute`) from
 * my_pen/my_brush — the values the Style column's controls in widget.cpp keep in sync —
 * onto either the current stick (`all==false`) or every stick in the current figure
 * (`all==true`), refreshing the touched views and the figure icon. Snapshotted with
 * StoreUndo() first so the change is undoable.
 *
 * @note ATTRIBUTE_BRUSHCOLOR assigns the WHOLE brush (and ATTRIBUTE_PENCOLOR the whole pen
 * paint, through CopyPenPaint()), where both used to assign only a colour. That is what
 * makes the pattern/gradient/image fills applicable at all — a QBrush::setColor() call is
 * a documented no-op on a gradient or a texture brush. The visible consequence is that
 * applying a fill to a stick that had none now gives it one: "no fill" is the type combo's
 * None entry, not something the Apply buttons preserve.
 */
void MyView::SetGraphics(bool all, int attribute){
    // guardie prima dello StoreUndo, così un click a vuoto (nessuna figura/stick corrente,
    // possibile dopo un ripristino undo/redo) non lascia uno snapshot senza modifica sullo stack
    if(my_animation->CurrentFrame()->CurrentStickFigure() == nullptr)
        return;
    if(!all && my_animation->CurrentFrame()->CurrentStick() == nullptr)
        return;
    StoreUndo(CMD_SIMPLE);
    // con una selezione multipla attiva le modifiche "globali" (all) si applicano a
    // TUTTE le figure selezionate, senza rompere la selezione (spec)
    if(all && MultiSelectionActive()){
        for(StickFigure *S : multi_selection){
            for(StickData *s : S->stick_list){
                if(attribute == ATTRIBUTE_PENCOLOR){
                    CopyPenPaint(&s->pen, my_pen);
                    s->brush_follows_stick = my_brush_follows_stick;
                    s->stroke_brush = my_stroke_brush;
                }
                else if(attribute == ATTRIBUTE_PENWIDTH)
                    s->pen.setWidth(my_pen.width());
                else if(attribute == ATTRIBUTE_BRUSHCOLOR){
                    s->brush = my_brush;
                    s->brush_follows_stick = my_brush_follows_stick;
                    s->texture_tiled = my_texture_tiled;
                }
                if(s->view)
                    s->view->Refresh(0);
            }
            S->UpdateIcon();
        }
        my_animation->current_frame->UpdateIcon();  // vedi le note in coda a questo metodo
        my_animation->UpdateStyleControls();
        return;
    }
    if(!all){
        if(attribute == ATTRIBUTE_PENCOLOR)
        {
            CopyPenPaint(&my_animation->CurrentFrame()->CurrentStick()->pen, my_pen);
            my_animation->CurrentFrame()->CurrentStick()->brush_follows_stick = my_brush_follows_stick;
            my_animation->CurrentFrame()->CurrentStick()->stroke_brush = my_stroke_brush;
        }
        else if(attribute== ATTRIBUTE_PENWIDTH)
        {
            my_animation->CurrentFrame()->CurrentStick()->pen.setWidth(my_pen.width());
        }
        else if(attribute== ATTRIBUTE_BRUSHCOLOR)
        {
            my_animation->CurrentFrame()->CurrentStick()->brush = my_brush;
            my_animation->CurrentFrame()->CurrentStick()->brush_follows_stick = my_brush_follows_stick;
            my_animation->CurrentFrame()->CurrentStick()->texture_tiled = my_texture_tiled;
        }
        if(my_animation->CurrentFrame()->CurrentStick()->view)
            my_animation->CurrentFrame()->CurrentStick()->view->Refresh(0);
    }
    else{
        for(StickData * s:my_animation->CurrentFrame()->CurrentStickFigure()->stick_list){
            if(attribute == ATTRIBUTE_PENCOLOR){
                CopyPenPaint(&s->pen, my_pen);
                s->brush_follows_stick = my_brush_follows_stick;
                s->stroke_brush = my_stroke_brush;
            }
            else if(attribute== ATTRIBUTE_PENWIDTH)
                s->pen.setWidth(my_pen.width());
            else if(attribute== ATTRIBUTE_BRUSHCOLOR){
                s->brush = my_brush;
                s->brush_follows_stick = my_brush_follows_stick;
                s->texture_tiled = my_texture_tiled;
            }
            if(s->view)
                s->view->Refresh(0);
        }
    }

    my_animation->CurrentFrame()->CurrentStickFigure()->UpdateIcon();
    // ...e la miniatura del FRAME, che StickFigure::UpdateIcon() non tocca: senza questa la
    // striscia dei frame continua a mostrare il riempimento PRECEDENTE finché non si trascina
    // qualcosa sulla tela (mouseReleaseEvent la ridisegna). Vale per ogni Apply, non solo per i
    // riempimenti: un cambio di colore o di spessore lasciava la stessa miniatura vecchia.
    my_animation->current_frame->UpdateIcon();
    // il tipo di riempimento/contorno dello stick può essere appena cambiato (una tinta unita
    // diventata gradiente o texture) senza che la SELEZIONE sia cambiata, e UpdateSliders() gira
    // solo sulla selezione: le caselle e gli slider che dipendono da cosa è dipinto lo stick
    // vanno rivalutati qui, o restano come per il riempimento precedente
    my_animation->UpdateStyleControls();
}
/**
 * @details Breaks the current figure at the selected stick: that stick and its subtree
 * are moved into a brand-new "_split" figure layer, the rest stays behind. Refuses to
 * split at the master (a figure can't lose its root). The tree surgery is done by
 * SplitStickFigures() (StickFigure.cpp); this method handles the surrounding view state
 * — remove/re-add handles, move the split-off sticks between the two figures'
 * total_sticks z-order entries, and select the new branch. Snapshotted with StoreUndo().
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
    split_to_stick_figure = my_animation->CurrentFrame()->CurrentStickFigure();
    RemoveHandles(split_to_stick_figure);
    split_to_stick = split_to_stick_figure->current_stick;
    QList<StickData*> old_stick_list = my_animation->CurrentFrame()->CurrentStickFigure()->stick_list;

    StickFigure * branch = my_animation->current_frame->AddStickFigure(my_stick_figure_widget_list, split_to_stick_figure->name+"_split");
    SplitStickFigures(split_to_stick_figure,split_to_stick,branch);
    split_to_stick_figure->Refresh(0);
    // update the total list
    QList<StickData*> survivor_list = split_to_stick_figure->stick_list;
    for(StickData*s:old_stick_list){
        if(!survivor_list.contains(s)){
            my_animation->CurrentFrame()->total_sticks.removeAll(s);
        }
    }
    my_animation->CurrentFrame()->total_sticks.append(branch->stick_list);
    AddHandles(split_to_stick_figure);
    AddHandles(branch);
    //
    branch->Refresh(0);
    my_animation->CurrentFrame()->CurrentStickFigure() = branch;
    my_animation->CurrentFrame()->CurrentStick() = branch->stick_list[0];
    my_animation->CurrentFrame()->CurrentStickFigure()->UpdateIcon();

}
/**
 * @details Applies a new canvas size to the scene: repaints the page rect (my_rect) and
 * limit rect at the new W/H, then — depending on `option` —
 * recentres every figure of every frame (REPO), recentres and rescales them (REPOSCALE,
 * via ScaleFrame()), or leaves them where they are (NOREPO). Called from
 * MyMainWindow::GetAnimationParams() after the options dialog changes the dimensions.
 */
void MyView::SizeChange(int option){

    QRectF old_rect = my_rect->rect();
    QPointF old_center = my_rect->rect().center();
    float amount =  W/my_rect->rect().width();
    my_rect->setPen(Qt::NoPen);
    my_rect->setBrush(QBrush(QColor(Qt::white)));
    scene()->setBackgroundBrush(QBrush(QColor(Qt::white)));
    my_rect->setRect(0,0,W,H);
    QPointF new_center = my_rect->rect().center();
    limit_rect->setRect(-my_rect->rect().width()/10, -my_rect->rect().height()/10, my_rect->rect().width()*1.1, my_rect->rect().height()*1.1);
    scene()->setSceneRect(limit_rect->rect());
    float dx = new_center.x()-old_center.x();
    float dy = new_center.y()-old_center.y();
    switch(option)
    {

        case REPOSCALE:
        {
            for(Frame*F:my_animation->frame_list){
                ScaleFrame(F,amount,old_rect);
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

}
/**
 * @details Shows the pixmap rendered for frame `idx` (ImageExport in PREVIEW mode, turned
 * into pixmaps by MySleeper::StartPlaying()), removing the
 * previous frame's pixmap first so they don't pile up on the scene during playback (the
 * whole preview_list is freed together only when playback ends).
 */
void MyView::DisplayPreview(int idx){
    // rimuovi il pixmap del frame precedente: altrimenti si accumula uno per frame per tutta
    // la riproduzione, invece di essere ripulito solo (tutto insieme) quando la riproduzione finisce
    if(idx>0)
        scene()->removeItem(preview_list[idx-1]);
    QGraphicsPixmapItem* render_img = preview_list[idx];

    scene()->addItem(render_img);
    //item->setZValue(0);
}
