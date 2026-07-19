#include "myview.h"
#include <QtAlgorithms>
#include <algorithm>
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
extern QPointF onion_offset;
extern QImage * image_draw_buffer ;
extern QString animation_path;
extern int auto_save_interval;
extern int op_count;
extern int handle_size;

#define MAXUNDO 50 ///< cap on undo_stack depth, enforced via QUndoStack::setUndoLimit()

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
    ChangeTool();

    tool = new_tool;
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
                        // click sulla tela vuota: la selezione multipla decade e parte
                        // il rettangolo di selezione (bordo sottile tratteggiato)
                        ClearMultiSelection();
                        selection_band = new QGraphicsRectItem(QRectF(coord, coord));
                        QPen band_pen(QColor(200, 200, 200), 0);
                        band_pen.setStyle(Qt::DotLine);
                        selection_band->setPen(band_pen);
                        selection_band->setBrush(Qt::NoBrush);
                        selection_band->setZValue(199);
                        scene()->addItem(selection_band);
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
                break;
            }
            case DRAW:
            case DRAWCIRCLE:
            case DRAWRECT:
            case DRAWTRAPEZOID:
            case DRAWTAPER:
            case DRAWIMG:
            {
                DrawCmd(&coord,(int)tool);
                break;
            }
            case(ROTATE):
            {
                StoreUndo();
                StickFigure::start_stick_figure_rotation = true;
                if(MultiSelectionActive()){
                    // rotazione della selezione multipla attorno alla maniglia: stessi
                    // buffer del tool ROTATESCENE, ma centro = PERNO (la posizione
                    // visibile della maniglia, eventualmente spostata dall'utente)
                    multi_center_buffer = multi_pivot;
                    for(StickFigure *S : multi_selection){
                        if(S->master_stick && S->stick_list.count() > 0){
                            S->origin_pos_buffer = S->master_stick->my_line.p1();
                            for(StickData *s : S->stick_list){
                                if(s->view) s->view->angle_buffer2 = s->my_line.angle();
                            }
                        }
                    }
                    break;
                }
                if(my_animation->CurrentFrame()->CurrentStickFigure() != nullptr){ // vedi il caso NOTOOL
                    for(StickData*s:my_animation->CurrentFrame()->CurrentStickFigure()->stick_list){
                        if(s->view) s->view->angle_buffer2 = s->my_line.angle();
                    }
                }
                break;
            }
            case(SCALE):
            {
                StoreUndo();
                StickFigure::start_stick_figure_scale = true;
                if(MultiSelectionActive()){
                    // scala della selezione multipla attorno alla maniglia (vedi
                    // SCALESCENE); centro = PERNO visibile, non il baricentro ricalcolato
                    multi_center_buffer = multi_pivot;
                    for(StickFigure *S : multi_selection){
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
                    break;
                }
                if(my_animation->CurrentFrame()->CurrentStickFigure() != nullptr){ // vedi il caso NOTOOL
                    for(StickData*s:my_animation->CurrentFrame()->CurrentStickFigure()->stick_list){
                        if(s->view){
                            s->view->scale_buffer = s->my_line.length();
                            s->view->width_buffer = s->pen.width();
                        }
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
                // click sul bordo di un poligono esistente: inserisci lì e apri la sessione
                int insert_idx = -1;
                StickData *edge_hit = FindPolygonEdge(coord, &insert_idx, StickData::POLYGON);
                if(edge_hit != nullptr){
                    StoreUndo();
                    my_animation->UpdateSelection(edge_hit);
                    edge_hit->view->AddPolygonPoint(coord, insert_idx);
                    poly_session_active = true;
                    poly_session_extends = true;
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
    }
    }
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
            // Spazio tenuto premuto: la maniglia della multi-selezione entra in
            // anteprima e segue il cursore; il perno si sposta al rilascio del tasto
            if(MultiSelectionActive() && !event->isAutoRepeat())
                pivot_previewing = true;
            break;
        }
    }

    QGraphicsView::keyPressEvent(event);
}

/**
 * @details Only job: releasing Space commits the previewed transform-handle position —
 * the pivot lands on the current mouse coordinate (spec).
 */
void MyView::keyReleaseEvent(QKeyEvent *event)
{
    if(event->key() == Qt::Key_Space && !event->isAutoRepeat() && pivot_previewing){
        pivot_previewing = false;
        if(MultiSelectionActive()){
            multi_pivot = coord; // `coord` è aggiornata da ogni mouseMoveEvent
            UpdateMultiSelectionUI();
        }
    }
    QGraphicsView::keyReleaseEvent(event);
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
        multi_pivot = MultiSelectionCenter(); // la selezione è cambiata: perno al baricentro
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
    // maniglia di trasformazione: cerchio sottile, riempimento trasparente, bordo
    // azzurro, disegnata SUL PERNO (che l'utente può spostare: è l'origine di ogni
    // trasformazione, non un semplice indicatore)
    float r = handle_size * 1.5f;
    if(multi_handle == nullptr){
        multi_handle = new QGraphicsEllipseItem();
        multi_handle->setPen(QPen(QColor(0x7f, 0xc4, 0xff), 1));
        multi_handle->setBrush(Qt::NoBrush);
        multi_handle->setZValue(200);
        scene()->addItem(multi_handle);
    }
    multi_handle->setRect(multi_pivot.x() - r, multi_pivot.y() - r, 2*r, 2*r);
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
    if(multi_handle != nullptr){
        scene()->removeItem(multi_handle);
        delete multi_handle;
        multi_handle = nullptr;
    }
    if(selection_band != nullptr){
        scene()->removeItem(selection_band);
        delete selection_band;
        selection_band = nullptr;
    }
    band_dragging = false;
    multi_dragging = false;
    pivot_dragging = false;
    pivot_previewing = false;
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
    if(event->button()==Qt::LeftButton){
        is_pressed = false;
        poly_drag_idx = -1;      // fine di un eventuale trascinamento di vertice (DRAWPOLY)
        poly_vertex_drag = false; // fine di un eventuale Ctrl-drag della selezione vertici
        multi_dragging = false;
        pivot_dragging = false;
        if(band_dragging){
            band_dragging = false;
            // il rettangolo sparisce sempre al rilascio del mouse (la selezione resta)
            if(selection_band != nullptr){
                scene()->removeItem(selection_band);
                delete selection_band;
                selection_band = nullptr;
            }
            if(!MultiSelectionActive())
                ClearMultiSelection();
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
    switch(tool)
    {
        case CLAMP:
        case NOTOOL:
        {
            if(is_pressed && band_dragging && selection_band != nullptr)
            {
                // estendi il rettangolo e seleziona ogni figura che tocca
                QRectF band = QRectF(starting_coord, coord).normalized();
                selection_band->setRect(band);
                QList<StickFigure*> touched;
                for(StickFigure *S : my_animation->current_frame->stick_figures){
                    for(StickData *s : S->stick_list){
                        if(band.intersects(QRectF(s->my_line.p1(), s->my_line.p2()).normalized())){
                            touched.append(S);
                            break;
                        }
                    }
                }
                multi_selection = touched;
                if(MultiSelectionActive())
                    UpdateMultiSelectionUI(true); // la selezione cambia: perno al baricentro
                else{
                    // sotto le 2 figure la maniglia sparisce (il rettangolo resta attivo
                    // fino al rilascio) e le evidenziazioni del passaggio precedente cadono
                    for(StickFigure *S : my_animation->current_frame->stick_figures)
                        S->Highlight(S == my_animation->CurrentFrame()->CurrentStickFigure());
                    if(multi_handle != nullptr){
                        scene()->removeItem(multi_handle);
                        delete multi_handle;
                        multi_handle = nullptr;
                    }
                }
                break;
            }
            if(is_pressed && pivot_dragging)
            {
                // trascinamento della maniglia: sposta solo il perno delle trasformazioni
                multi_pivot = coord;
                UpdateMultiSelectionUI();
                break;
            }
            if(pivot_previewing && multi_handle != nullptr)
            {
                // Spazio premuto: la maniglia segue il cursore in anteprima (il perno
                // vero si sposta al rilascio del tasto)
                float r = handle_size * 1.5f;
                multi_handle->setRect(coord.x() - r, coord.y() - r, 2*r, 2*r);
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
                        if(tool == CLAMP){
                            if(my_animation->CurrentFrame()->CurrentStick()->view)
                                my_animation->CurrentFrame()->CurrentStick()->view->Rotate(&coord);
                        }
                        else{
                            QList<StickGraphicsItem*> to_rotate;
                            if(my_animation->CurrentFrame()->CurrentStick() == my_animation->CurrentFrame()->CurrentStickFigure()->master_stick){
                                for(StickData*s:my_animation->CurrentFrame()->CurrentStickFigure()->stick_list){
                                    if(!s->stepchild && s->view){
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
        case ROTATE:
        {
            if(is_pressed && MultiSelectionActive()){
                // come ROTATESCENE, ma sul solo insieme selezionato e con la maniglia
                // (baricentro della selezione) come perno
                QPointF center = multi_center_buffer;
                QLineF mouse_to_center_now(coord, center);
                QLineF mouse_to_center_start(starting_coord, center);
                float angle = mouse_to_center_now.angle() - mouse_to_center_start.angle();
                if(angle > 180) angle -= 360;
                else if(angle < -180) angle += 360;
                starting_coord = coord;
                for(StickFigure *S : multi_selection){
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
                    }
                }
                break;
            }
            if(my_animation->CurrentFrame()->CurrentStickFigure() != nullptr)
            {
                if(is_pressed){
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
            if(is_pressed && MultiSelectionActive()){
                // come SCALESCENE, con la maniglia come centro di scala
                QPointF center = multi_center_buffer;
                float start_dist = QLineF(starting_coord, center).length();
                if(start_dist < 1)
                    break; // click esattamente sul centro: divisione per (quasi) zero
                float scale_amount = QLineF(coord, center).length() / start_dist;
                for(StickFigure *S : multi_selection){
                    if(S->stick_list.count() > 0 && S->master_stick){
                        QLineF actual_dist_from_center(S->master_stick->my_line.p1(), center);
                        QLineF original_dist_from_center(S->origin_pos_buffer, center);
                        float dx = -original_dist_from_center.dx()*scale_amount + actual_dist_from_center.dx();
                        float dy = -original_dist_from_center.dy()*scale_amount + actual_dist_from_center.dy();
                        S->Traslate(dx, dy);
                        for(StickData *s : S->stick_list){
                            if(s->view) s->view->Scale(scale_amount);
                        }
                    }
                }
                break;
            }
            if(my_animation->CurrentFrame()->CurrentStickFigure() != nullptr && is_pressed){
               my_animation->CurrentFrame()->CurrentStickFigure()->Scale(&coord);
            }
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
            f->linked_item->setData(Qt::DisplayRole, QString::number(f->frame_number));
        }
    }
    if(my_animation->current_frame != nullptr) // cancellato l'ultimo frame: niente da selezionare
        my_animation->current_frame->linked_item->setSelected(true);
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
    // il cambio frame invalida la sessione poligono e la selezione vertici (stato UI
    // del frame lasciato); i flag da soli non tengono puntatori, basta azzerarli
    poly_session_active = false;
    poly_session_extends = false;
    poly_drag_idx = -1;
    poly_vertex_drag = false;
    ClearMultiSelection(); // la selezione multipla decade al cambio frame (spec)
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
    // rimuovigli elementi dalla lista widget
    my_stick_figure_widget_list->clear();

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
        my_animation->current_frame->linked_item->setSelected(true);
        my_frame_widget_list->setCurrentItem(my_animation->current_frame->linked_item);
        // niente guardia su stick_figures.isEmpty(): UpdateIcon() gestisce già correttamente un
        // frame vuoto (lo riempie di bianco), senza di essa un frame appena creato restava senza
        // anteprima invece di mostrare una tela bianca
        if(!my_animation->frame_list.isEmpty()){
            my_animation->current_frame->UpdateIcon();
        }
    }
    my_animation->CurrentFrame()->Refresh(0);
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
    // immagine fittizia su cui registrare gli onion skins
    QImage render_img =  QImage(scene()->sceneRect().width(),scene()->sceneRect().height(),QImage::Format_ARGB32);

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
        QPainter painter(&render_img);
        render_img.fill(Qt::transparent);
        painter.setBackground(QBrush(QColor(Qt::transparent)));
        render_scene.render(&painter);
        // più è vecchio l'onion skin più è trasparente, più è vicino al frame corrente più è opaco
        opacity_val += 0.5/skin_num;
        // trasferisci la scena renderizzata
        QGraphicsPixmapItem* item = new QGraphicsPixmapItem(QPixmap::fromImage((render_img)));
        item->setOffset(onion_offset);
        item->setOpacity(opacity_val);
        item->setZValue(0);
        onion_skins.append(item);
        scene()->addItem(item);
        render_scene.clear(); // distruggi le view di rendering PRIMA di liberare i dati clonati
        qDeleteAll(render_clones);
        painter.end();

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
StickFigure* MyView::AddStickFigureToLibrary(){
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
 * @details Copies one attribute (pen colour/width or brush colour, per `attribute`)
 * from my_pen/my_brush — the values the colour/width controls in widget.cpp keep in
 * sync — onto either the current stick (`all==false`) or every stick in the current
 * figure (`all==true`), refreshing the touched views and the figure icon. Snapshotted
 * with StoreUndo() first so a colour change is undoable.
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
                if(attribute == ATTRIBUTE_PENCOLOR)
                    s->pen.setColor(my_pen.color());
                else if(attribute == ATTRIBUTE_PENWIDTH)
                    s->pen.setWidth(my_pen.width());
                else if(attribute == ATTRIBUTE_BRUSHCOLOR)
                    s->brush.setColor(my_brush.color());
                if(s->view)
                    s->view->Refresh(0);
            }
            S->UpdateIcon();
        }
        return;
    }
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
 * limit rect at the new W/H, recomputes the onion offset, then — depending on `option` —
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
    onion_offset = QPointF(-my_rect->rect().width()/10,-my_rect->rect().height()/10);
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
 * @details Flattens every frame to a single pixmap up front so playback can just swap
 * images instead of rebuilding the scene each tick. Like the onion-skin/icon renderers
 * it draws from Clone()d StickData wrapped in throwaway views, and clears the render
 * scene BEFORE freeing the clones so no view outlives its data. Sets `play_back` so the
 * rest of the app skips interactive-only work while previewing. DisplayPreview() then
 * shows the frames one at a time.
 */
void MyView::PreparePreview(){

    play_back = true;
    for(Frame* f :my_animation->frame_list){
        QGraphicsScene render_scene; //scena fittizia
        QImage *render_img = new QImage(W,H,QImage::Format_ARGB32); //immagine temporanea


        QPainter painter(render_img); // painter che esegue il drawing sull immagine
        render_scene.setSceneRect(my_rect->rect());
        // questi stick sono VIVI: clona i dati prima di wrapparli, altrimenti il costruttore
        // di stick ruberebbe il back-pointer data->view alla view reale sullo schermo
        QList<StickData*> render_clones;
        for(StickFigure *S:f->stick_figures)
        {
            for(StickData* s: S->stick_list){
                StickData* clone = s->Clone();
                render_clones.append(clone);
                render_scene.addItem(new StickGraphicsItem(clone));
            }
        }
        //disegna sull' immagine e salvala
        render_img->fill(Qt::white);
        painter.setBackground(QBrush(QColor(Qt::white)));
        render_scene.render(&painter);
        painter.end();
        QGraphicsPixmapItem* item = new QGraphicsPixmapItem(QPixmap::fromImage(*(render_img)));
        preview_list.append(item);
        delete render_img;
        render_scene.clear(); // distruggi le view di rendering PRIMA di liberare i dati clonati
        qDeleteAll(render_clones);
        //...add all other video frames here
    }
    play_back = false;
}
/**
 * @details Shows the pixmap PreparePreview() rendered for frame `idx`, removing the
 * previous frame's pixmap first so they don't pile up on the scene during playback (the
 * whole preview_list is freed together only when playback ends).
 */
void MyView::DisplayPreview(int idx){
    // rimuovi il pixmap del frame precedente: altrimenti si accumula uno per frame per tutta
    // la riproduzione, invece di essere ripulito solo (tutto insieme) quando la riproduzione finisce
    if(idx>0)
        scene()->removeItem(preview_list[idx-1]);
    QGraphicsPixmapItem* render_img = preview_list[idx];

    //item->setOffset(onion_offset);
    scene()->addItem(render_img);
    //item->setZValue(0);
}
