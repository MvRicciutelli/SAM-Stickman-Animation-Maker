#include "animation.h"

#include <QBoxLayout>
#include <QCoreApplication>
#include <QFile>
#include <QSpinBox>
#include "advancedtab.h"
#include "advancedlinewidget.h"
#include "advancedcirclewidget.h"
#include "advancedtaperwidget.h"
#include <QCheckBox>
#include <QComboBox>
#include <QStandardItemModel>
#include "uiItems.h"
#include "widget.h" // solo per Widget::StyleKind, i valori UserData delle due combo di stile
#include "StickFigure.h" // IkChainStart()/StickIsInActiveIkChain() per il tab IK

extern qint32 W;
extern qint32 H;
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
    // confronta col tipo dei pannelli mostrati, non con lo stick corrente: scegliere un tool di
    // disegno (MyView::ChangeTool) cambia i pannelli senza cambiare lo stick corrente, e
    // ricliccare uno stick dello stesso tipo lasciava su i pannelli del tool
    if(selectedStickFigure->stick_list[selectedIdx]->stick_type != tab_type)
        UpdateTab(selectedStickFigure->stick_list[selectedIdx]->stick_type);

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
    tab_type = t;
    int idx = my_tab_widget->currentIndex();
    // dal supporto POLYGON un tipo può portare PIÙ tab (proprietà + vertici): svuota tutto
    // tranne il tab base (0) e la tab Libraries invece di rimuovere solo la posizione 1.
    // Va all'indietro cosi' gli indici dei tab ancora da rimuovere non si spostano sotto di noi,
    // e salta esplicitamente library_tab ovunque sia finito: un blind "removeTab(1)" ripetuto
    // la cancellava a sua volta appena veniva spinta in posizione 1 da un altro insertTab(1,...).
    for(int i = my_tab_widget->count() - 1; i >= 1; i--){
        if(my_tab_widget->widget(i) == library_tab)
            continue;
        my_tab_widget->removeTab(i);
    }
    if(t == StickData::IMAGE){
        my_tab_widget->insertTab(1,advanced_img_tab,"Image Editing");
    }
    else if(t == StickData::FREEHAND){
        my_tab_widget->insertTab(1,advanced_draw_tab,"Draw Mode");
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
    else if(t == StickData::POLYGON){
        // pannello vertici per la curvatura degli spigoli (la riga larghezza è solo per le
        // MULTILINE); offset e rotazione stanno nella Stick Transform qui sotto
        my_tab_widget->insertTab(1,advanced_vertex_tab,"Polygon Vertices");
        vertex_width_row->hide();
    }
    else if(t == StickData::MULTILINE){
        my_tab_widget->insertTab(1,advanced_vertex_tab,"Multiline Editing");
        vertex_width_row->show();
    }
    else if(t == StickData::TEXT){
        // pannello testo (contenuto/font/giustificazione/interlinea/word wrap); il
        // posizionamento è la Stick Transform qui sotto
        my_tab_widget->insertTab(1,advanced_text_tab,"Text Editing");
    }
    // la Stick Transform vale per ogni tipo: dopo il pannello del tipo, prima delle Libraries
    // (indexOf() di un library_tab nullo, nei test, è -1, e insertTab(-1) accoda)
    my_tab_widget->insertTab(my_tab_widget->indexOf(library_tab), advanced_transform_tab, "Stick Transform");
    // la riga "Convert to" (Widget) è UN solo widget e un widget ha un solo genitore: va
    // spostata in cima al primo pannello del tipo mostrato a ogni cambio di tipo
    QWidget *panel = my_tab_widget->count() > 1 ? my_tab_widget->widget(1) : nullptr;
    if(convert_type_combo != nullptr && panel != nullptr && panel != library_tab){
        QWidget *row = convert_type_combo->parentWidget();
        if(row->parentWidget() && row->parentWidget()->layout())
            row->parentWidget()->layout()->removeWidget(row);
        if(QBoxLayout *box = qobject_cast<QBoxLayout*>(panel->layout()))
            box->insertWidget(0, row);
        convert_type_combo->setCurrentIndex(convert_type_combo->findData(t));
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

/**
 * @details Split out of UpdateSliders() because these three check boxes depend on WHAT THE
 * SELECTED STICK IS PAINTED WITH, and that changes without any selection change: pressing one of
 * the Style column's Apply buttons (MyView::SetGraphics()) swaps a stick's solid fill for a
 * gradient or a texture while the same stick stays selected. UpdateSliders() only runs on a
 * selection change, so the boxes kept the enablement computed while the fill was still solid and
 * stayed greyed out — looking, correctly enough, like they did not respond to a click. Both
 * callers now refresh them: the selection path through UpdateSliders(), and SetGraphics().
 *
 * Deliberately narrow — setEnabled() plus one signal-blocked setChecked(), nothing that can
 * re-enter an edit — so SetGraphics() can call it mid-edit, which the whole of UpdateSliders()
 * (which drives every slider in the panel) could not safely be.
 */
void Animation::UpdateStyleControls()
{
    StickData *cs = current_frame == nullptr ? nullptr : current_frame->CurrentStick();
    if(cs == nullptr)
        return;
    // texture e gradiente hanno entrambi una geometria da ancorare/puntare, ognuno attraverso
    // campi propri (vedi MyView::ReadArrow()); una tinta unita e il vuoto no
    auto has_geometry = [](const QBrush &b){
        return b.style() == Qt::TexturePattern || b.gradient() != nullptr;
    };
    // l'ancoraggio: l'eco va zittita, altrimenti riallineare la casella allo stick selezionato
    // passerebbe per una modifica dell'utente e spingerebbe un undo a vuoto (stessa ragione del
    // blockSignals sulla casella IK). Un solo flag per penna E pennello, quindi basta che uno
    // dei due abbia qualcosa da ancorare.
    if(brush_follows_check != nullptr){
        QSignalBlocker mute(brush_follows_check);
        brush_follows_check->setChecked(cs->brush_follows_stick);
        brush_follows_check->setEnabled(has_geometry(cs->brush)
                                       || (cs->pen.style() != Qt::NoPen
                                           && has_geometry(cs->pen.brush())));
    }
    // la ripetizione della texture: solo il RIEMPIMENTO ha qualcosa da ripetere
    if(texture_tiled_check != nullptr){
        QSignalBlocker mute(texture_tiled_check);
        texture_tiled_check->setChecked(cs->texture_tiled);
        texture_tiled_check->setEnabled(cs->brush.style() == Qt::TexturePattern);
    }
    // le due frecce: nulla da rileggere dallo stick — sono stato dell'interfaccia, non del
    // disegno — quindi qui solo l'abilitazione, senza eco e senza QSignalBlocker
    // ...e su un tipo che non passa mai brush/pen al painter la freccia non ha nulla da puntare,
    // anche quando il pennello contiene una geometria: PaintsFill()/PaintsStroke(), gli stessi
    // predicati con cui UpdateSliders() spegne i due selettori di tipo
    if(fill_texture_arrow_check != nullptr && stroke_texture_arrow_check != nullptr){
        fill_texture_arrow_check->setEnabled(cs->PaintsFill() && has_geometry(cs->brush));
        stroke_texture_arrow_check->setEnabled(cs->PaintsStroke()
                                               && cs->pen.style() != Qt::NoPen
                                               && has_geometry(cs->pen.brush()));
    }
    // l'opacità: gli slider agiscono ORA sullo stick selezionato, quindi vanno riallineati a
    // quel che lo stick ha davvero — altrimenti il primo tocco gli imporrebbe il valore rimasto
    // dallo stick precedente. Dove il numero sta dipende dal tipo di pennello (il colore per una
    // tinta unita e per una texture, gli stop per un gradiente): lo sa ReadPaintOpacity(), qui
    // basta sapere che solo il vuoto non ha nulla da sbiadire.
    auto fadeable = [](const QBrush &b){ return b.style() != Qt::NoBrush; };
    if(brush_opacity_slider != nullptr && brush_opacity_spinbox != nullptr){
        const bool can_fade = fadeable(cs->brush);
        brush_opacity_slider->setEnabled(can_fade);
        brush_opacity_spinbox->setEnabled(can_fade);
        if(can_fade){
            QSignalBlocker mute_slider(brush_opacity_slider), mute_box(brush_opacity_spinbox);
            const int percent = qRound(ReadPaintOpacity(cs->brush) * 100.0);
            brush_opacity_slider->setValue(percent);
            brush_opacity_spinbox->setValue(percent);
        }
    }
    if(pen_opacity_slider != nullptr && pen_opacity_spinbox != nullptr){
        const bool can_fade = cs->pen.style() != Qt::NoPen && fadeable(cs->pen.brush());
        pen_opacity_slider->setEnabled(can_fade);
        pen_opacity_spinbox->setEnabled(can_fade);
        if(can_fade){
            QSignalBlocker mute_slider(pen_opacity_slider), mute_box(pen_opacity_spinbox);
            const int percent = qRound(ReadPaintOpacity(cs->pen.brush()) * 100.0);
            pen_opacity_slider->setValue(percent);
            pen_opacity_spinbox->setValue(percent);
        }
    }
}

void Animation::UpdateSliders(){
    StickData *cs = current_frame->CurrentStick();
    if(current_frame->CurrentStick() == nullptr)
        return;
    // la Stick Transform, per ogni tipo: il pannello (e quale dei due insiemi, locale o globale,
    // sta mostrando) è di Widget, quindi l'eco passa da lì. Nullo nei test
    if(refresh_transform_panel)
        refresh_transform_panel(cs);
    // Stesso criterio per i due selettori di tipo riempimento/contorno della colonna Style:
    // un IMAGE/FREEHAND non ha né l'uno né l'altro (il suo aspetto È il raster, che paint()
    // disegna senza guardare pen e brush), mentre LINE, MULTILINE e TEXT hanno solo il
    // contorno — per loro il brush non viene mai passato al painter. Disabilitati invece
    // di lasciarli sembrare guasti. Nulli nei test, che non linkano widget.cpp.
    if(fill_type_combo != nullptr && stroke_type_combo != nullptr){
        fill_type_combo->setEnabled(cs->PaintsFill());
        stroke_type_combo->setEnabled(cs->PaintsStroke());
        // ...e la voce "Brush" da sola vale meno: un TEXT ha un contorno ma non un percorso da
        // percorrere a timbri (StrokesAPath()), quindi lì la voce è spenta e il resto no
        const int brush_row = stroke_type_combo->findData(Widget::STYLE_BRUSH);
        QStandardItemModel *entries = qobject_cast<QStandardItemModel*>(stroke_type_combo->model());
        if(brush_row >= 0 && entries != nullptr)
            entries->item(brush_row)->setEnabled(cs->StrokesAPath());
    }
    UpdateStyleControls();
    if(cs->stick_type == StickData::RECT){
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
    else if(cs->stick_type == StickData::TEXT){
        // il pannello testo vive in widget.cpp: l'eco dei suoi controlli passa da qui
        // (nullo nei test, dove il pannello non esiste)
        if(refresh_text_panel)
            refresh_text_panel(cs);
    }
    edit_line_thickness_slider->setValue(cs->pen.width());
    elongate_spinbox->setValue(  current_frame->CurrentStick()->my_line.length());

    // tab "IK properties": compare solo quando lo stick selezionato fa parte di una
    // catena IK, e sparisce quando non lo è (spec). Gestito qui (non in UpdateTab, che
    // scatta solo al CAMBIO di stick_type) perché il ruolo IK è ortogonale al tipo, e
    // UpdateSliders() gira a ogni selezione. La casella riflette lo stato acceso/spento
    // della catena; il segnale è bloccato durante l'aggiornamento per non innescare
    // l'handler (che altrimenti spingerebbe un undo spurio) — vedi Widget's ctor.
    if(my_tab_widget != nullptr && advanced_ik_tab != nullptr){
        int ik_tab_idx = my_tab_widget->indexOf(advanced_ik_tab);
        if(cs->ik_role != StickData::IK_NONE){
            if(ik_tab_idx < 0)
                my_tab_widget->addTab(advanced_ik_tab, "IK properties");
            if(ik_active_check != nullptr){
                ik_active_check->blockSignals(true);
                ik_active_check->setChecked(StickIsInActiveIkChain(cs));
                ik_active_check->blockSignals(false);
            }
        }
        else if(ik_tab_idx >= 0){
            my_tab_widget->removeTab(ik_tab_idx);
        }
    }
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
            // il numero è cotto nell'anteprima, non nell'etichetta nativa dell'item: senza
            // ripassare dalla fascia, ogni frame spostato dall'inserimento continuava a
            // mostrare il numero vecchio (evidentissimo generando N inbetween in un colpo)
            f->UpdateLabel();
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
/**
 * @details v3 writes every distinct image ONCE, in a table ahead of the frames, and lets the
 * stick records index into it. Before it, each record inlined its own pixels: the same
 * picture, texture or brush tip was re-encoded once per stick PER FRAME, so a figure that
 * simply exists through the animation multiplied its images by the frame count — a measured
 * 218 MB for sixty frames of one image stick, three brush borders and two pattern fills, of
 * which 1.9 MB was actual content.
 *
 * The table is written in TWO passes because it can only be complete once every record has
 * been asked for its images, yet it has to sit BEFORE them (a QDataStream is read front to
 * back, and the records need it as they are read — unlike the v2 name trailer, which nothing
 * depends on). So the frames go into a scratch buffer first, filling the table as a side
 * effect, and the buffer is then written after it. Serializing into the buffer through the
 * very same operators, rather than walking the model separately to collect images, is what
 * keeps the table from ever disagreeing with what the records actually reference.
 */
QDataStream &operator<<(QDataStream& stream, const Animation& my_animation){
    qint32 frameNum = (qint32)my_animation.frame_list.count();
    stream<<my_animation.version;
    stream<<frameNum;
    stream<<W;
    stream<<H;

    // la tabella è gated sulla versione scritta nell'header, non solo presente: scriverla
    // mentre l'header dice v2 produrrebbe un file che il lettore disallinea al primo frame
    if(my_animation.version >= 3){
        ImageTable table;
        QByteArray frame_payload;
        {
            QDataStream frames(&frame_payload, QIODevice::WriteOnly);
            frames.setVersion(stream.version()); // stessa versione: il payload è parte dello stesso file
            ImageTable *previous = InstallImageTable(&table);
            for(Frame* f: my_animation.frame_list){
                frames<<*f;
            }
            InstallImageTable(previous); // sempre ripristinata, anche se sopra è andato storto qualcosa
        }
        stream<<table;
        // a blocchi: writeRawData() prende un int, e passargli una dimensione che non ci sta
        // troncherebbe il salvataggio in silenzio. Le immagini ora stanno nella tabella, quindi
        // qui restano solo i record: il ciclo gira una volta sola in ogni caso reale.
        qsizetype written = 0;
        while(written < frame_payload.size()){
            const int chunk = (int)qMin<qsizetype>(1 << 30, frame_payload.size() - written);
            if(stream.writeRawData(frame_payload.constData() + written, chunk) != chunk)
                break; // dispositivo pieno o in errore: lo status dello stream lo riporta
            written += chunk;
        }
    }
    else{
        for(Frame* f: my_animation.frame_list){
            stream<<*f;
        }
    }

    // v2: i nomi dei frame in coda al file. Stanno QUI e non in FrameData::operator<<
    // perché in coda sono ignorabili: un lettore v1 non li vedrebbe mai, e soprattutto
    // un file v1 (che non li ha) resta leggibile senza toccare il layout dei frame
    for(Frame* f: my_animation.frame_list){
        stream<<f->name;
    }
    return stream;
}
/**
 * @brief Everything a .sam file holds, read into pure data: no scene, no widget, no live frame.
 * @details Parsing first and applying second is what lets LoadAnimation() refuse a file without
 * having changed anything. Owns its frames.
 */
struct AnimationFileData {
    qint32 version = 0;
    qint32 width = 0;
    qint32 height = 0;
    QList<FrameData*> frames;
    QStringList names;          ///< v2 trailer, parallel to frames (may be shorter on a truncated file)
    ~AnimationFileData() { qDeleteAll(frames); }
};

/**
 * @details Mirror of operator<<. Stops at the first problem and keeps what was read before it, so a
 * truncated file still yields its leading frames exactly as it always did. A newer version is the
 * exception: it is reported through file_load_error (RejectNewerVersion()) and LoadAnimation()
 * discards the lot — the header is checked before anything else, and each stick/figure record
 * checks its own, because a newer build can add a record field without bumping the file's header.
 */
static void ReadAnimationFileData(QDataStream &stream, AnimationFileData &file)
{
    qint32 frame_count = 0;
    stream >> file.version;
    if (RejectNewerVersion(stream, "animation", file.version, Animation::CURRENT_VERSION))
        return;
    stream >> frame_count;
    stream >> file.width;
    stream >> file.height;
    // un file valido non ha comunque un numero enorme di frame: senza questo controllo, un file
    // troncato o corrotto (es. un int con qualche bit invertito) farebbe partire un ciclo che
    // alloca centinaia di migliaia di Frame/QListWidgetItem prima di accorgersi che i dati non ci sono
    if (stream.status() != QDataStream::Ok || frame_count < 0 || frame_count > 100000)
        return;
    // v3: la tabella delle immagini precede i frame, che la indicizzano invece di portarsi
    // dietro una copia dei propri pixel ciascuno (vedi operator<<)
    ImageTable table;
    if (file.version >= 3) {
        stream >> table;
        if (stream.status() != QDataStream::Ok)
            return; // tabella illeggibile: i frame sarebbero solo immagini nulle
    }
    ImageTable *previous = InstallImageTable(file.version >= 3 ? &table : nullptr);
    for (int i = 0; i < frame_count; i++) {
        FrameData *frame = new FrameData();
        stream >> *frame;
        file.frames.append(frame);
        if (stream.status() != QDataStream::Ok)
            break; // file troncato a metà frame: i frame già letti restano validi
    }
    // ripristinata appena finiti i record: una tabella lasciata installata punterebbe a memoria
    // di questo stack frame
    InstallImageTable(previous);
    // trailer dei nomi (solo v2): l'iterazione è su frames, quindi un file troncato a metà
    // frame ne ha semplicemente meno da nominare
    if (file.version >= 2) {
        for (int i = 0; i < file.frames.count(); i++) {
            if (stream.status() != QDataStream::Ok)
                break;
            QString frame_name;
            stream >> frame_name;
            file.names.append(frame_name);
        }
    }
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
bool Animation::LoadAnimation(QString name)
{
    file_load_error.clear();
    QFile file(name);
    if(!file.open(QIODevice::ReadOnly))
    {
        file_load_error = QCoreApplication::translate("FileVersion", "The file could not be opened.");
        return false;
    }
    AnimationFileData parsed;
    {
        QDataStream in(&file);
        ReadAnimationFileData(in, parsed);
    }
    file.close();
    if(file_load_error.isEmpty() && parsed.frames.isEmpty())
        file_load_error = QCoreApplication::translate("FileVersion", "The file contains no readable frame.");
    if(!file_load_error.isEmpty())
        return false; // rifiutato: niente è stato toccato

    ClearAnimation();
    // in memoria si tiene sempre il formato corrente: un salvataggio successivo a un load
    // di un file v1/v2 deve scrivere un v3 completo, non un header vecchio con contenuto nuovo
    version = CURRENT_VERSION;
    width = parsed.width;
    height = parsed.height;
    // applica subito le dimensioni appena lette: prima non veniva fatto, quindi caricare un file con
    // una canvas di dimensioni diverse da quella corrente lasciava my_rect/limit_rect legati
    // alla vecchia canvas (nessuno li ricalcolava finché l'utente non ridimensionava manualmente)
    W = width;
    H = height;
    my_rect->setRect(0,0,W,H);
    SceneRemover(scene);
    limit_rect->setRect(-my_rect->rect().width()/10, -my_rect->rect().height()/10, my_rect->rect().width()*1.1, my_rect->rect().height()*1.1);
    scene->addItem(my_rect);
    scene->setSceneRect(limit_rect->rect());
    // sticks staccati e icone non renderizzate come sempre per il load di un file (vedi
    // PopulateLiveFrame): MoveToFrame() attacca e aggiorna il frame che viene mostrato davvero
    for(int i = 0; i < parsed.frames.count(); i++){
        Frame *added_frame = SetupFrame(i);
        PopulateLiveFrame(added_frame, *parsed.frames[i], false, false);
        added_frame->UpdateIcon();
        if(i < parsed.names.count()){
            added_frame->name = parsed.names[i];
            added_frame->UpdateLabel(); // il nome entra nella fascia dell'anteprima già renderizzata sopra
        }
    }
    qDebug("load finished");
    return true;
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
