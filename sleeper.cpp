#include "sleeper.h"
#include "imageexport.h"
extern void SceneRemover(QGraphicsScene *scene_to_clear);
extern MySleeper *play;
/**
 * @details Kicks off a background ImageExport thread in PREVIEW mode to render every
 * frame once; StartPlaying() (connected to its completion signal) begins the actual
 * playback timer only once that render finishes, so the preview never shows an
 * unrendered frame.
 */
MySleeper::MySleeper(MyView * V, int time)
{
    v = V;
    counter = 0;
    interval_time = time;
    preview_generator = new ImageExport(this,"",PREVIEW); // è un thread
    // connetti il segnale del thread di fine elaborazione allo slot che partire la riproduzione
    QObject::connect(preview_generator,&ImageExport::ImageExportFinished,this,&MySleeper::StartPlaying);
    v->play_back = true;
    SceneRemover(v->scene());
    preview_generator->start(); // fai partire il thread


}

/**
 * @details Starts the display timer once the preview render thread signals completion.
 * The wait() first closes the tiny race between the thread's completion signal (queued,
 * can be delivered before run() has fully returned) and its self-deleteLater().
 * The timer→Connected() connection uses the old string-based SIGNAL/SLOT macros rather
 * than the compile-checked new-style syntax used everywhere else in this codebase —
 * flagged during the Phase 7 naming sweep (REFACTOR_PLAN.md) as the project's only
 * remaining string-based connection, kept as-is rather than converted incidentally here.
 */
void MySleeper::StartPlaying()
{
    preview_generator->wait(); // il segnale è queued: run() potrebbe non essere ancora tornata
    // i pixmap si creano qui, sul thread della UI: QPixmap non è utilizzabile da un thread
    // secondario. Il thread li renderizza allo zoom della view, scalarli di 1/preview_scale
    // li riporta a coprire la tela esattamente come prima
    for(QImage &frame : preview_generator->preview_frames){
        QGraphicsPixmapItem *item = new QGraphicsPixmapItem(QPixmap::fromImage(std::move(frame)));
        item->setScale(1.0 / preview_generator->preview_scale);
        v->preview_list.append(item);
    }
    preview_generator->preview_frames.clear();
    connect(&timer, SIGNAL(timeout()), SLOT(Connected()));
    timer.setSingleShot(false);
    timer.setInterval(interval_time);
    timer.start();
}
/**
 * @details Only flips a flag; Connected() checks it on its next timer tick and ends
 * playback there rather than being interrupted mid-frame.
 */
void MySleeper::Stop(){
    pause_pressed = true;
}
/// @details No-op once the thread has finished: QPointer is null after its deleteLater().
void MySleeper::WaitForPreview()
{
    if(preview_generator)
        preview_generator->wait();
}
/**
 * @details Called once per timer tick (see StartPlaying()). While frames remain and
 * Stop() hasn't been requested, shows the next rendered frame
 * (MyView::DisplayPreview()) and advances `counter`. Otherwise ends playback: stops the
 * timer, removes the preview pixmaps from the scene, returns the view to the last frame
 * shown, clears the global `play` pointer, and deletes itself — MySleeper has no owner,
 * so this is the only place it's ever freed.
 */
Q_SLOT void MySleeper::Connected()
{

   // preview_list, non frame_list: un frame aggiunto durante la riproduzione non ha un'anteprima,
   // e preview_list[counter] leggeva oltre la fine
   if (v->preview_list.count()>counter && pause_pressed == false){
       //v->MoveToFrame(v->my_animation->frame_list[counter]);
       v->DisplayPreview(counter);
       timer.start();
       counter++;

   }
   else{
       v->play_back = false;
       timer.stop();
       // removeItem su un item non presente nella scena è innocuo (solo un warning): sulla
       // scena c'è al più l'ultimo frame mostrato, gli altri vanno comunque liberati
       for(QGraphicsPixmapItem * item:v->preview_list){
           v->scene()->removeItem(item);
           delete item;
       }
       v->preview_list.clear();
       // counter è 0 se Stop() arriva prima del primissimo tick, e i frame cancellati durante la
       // riproduzione possono aver lasciato frame_list più corta di counter: entrambi leggevano
       // fuori dai limiti
       const QList<Frame*> &frames = v->my_animation->frame_list;
       if(!frames.isEmpty())
           v->MoveToFrame(frames[qBound(0, counter-1, frames.count()-1)]);
       // azzera 'play' PRIMA della self-delete: è ciò che segnala a Widget::on_PlayButton_clicked
       // che non c'è più una riproduzione in corso (sostituisce il vecchio flag busy_play)
       play = nullptr;
       delete this; // nessun accesso a membri dopo questa riga
   }


   //do some processing
}
