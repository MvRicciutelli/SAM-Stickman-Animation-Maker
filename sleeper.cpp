#include "sleeper.h"
#include "sleeper.h"
#include "imageexport.h"
extern bool play_back;
extern void SceneRemover(QGraphicsScene *scene_to_clear);
extern  bool busy_play;
extern MySleeper *play;
bool pausePressed;
int intervalTime = 0;
ImageExport *previewGenerator;
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
    intervalTime = time;
    previewGenerator = new ImageExport(this,"",PREVIEW); // è un thread
    // connetti il segnale del thread di fine elaborazione allo slot che partire la riproduzione
    QObject::connect(previewGenerator,&ImageExport::ImageExportFinished,this,&MySleeper::StartPlaying);
    play_back = true;
    SceneRemover(v->scene());
    previewGenerator->start(); // fai partire il thread


}

/**
 * @details Starts the display timer once the preview render thread signals completion.
 * The timer→Connected() connection uses the old string-based SIGNAL/SLOT macros rather
 * than the compile-checked new-style syntax used everywhere else in this codebase —
 * flagged during the Phase 7 naming sweep (REFACTOR_PLAN.md) as the project's only
 * remaining string-based connection, kept as-is rather than converted incidentally here.
 */
void MySleeper::StartPlaying()
{
    connect(&timer, SIGNAL(timeout()), SLOT(Connected()));
    timer.setSingleShot(false);
    timer.setInterval(intervalTime);
    timer.start();
}
/**
 * @details Only flips a flag; Connected() checks it on its next timer tick and ends
 * playback there rather than being interrupted mid-frame.
 */
void MySleeper::Stop(){
    pausePressed = true;
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

   if (v->my_animation->frame_list.count()>counter && pausePressed == false){
       //v->MoveToFrame(v->my_animation->frame_list[counter]);
       v->DisplayPreview(counter);
       timer.start();
       counter++;

   }
   else{
       play_back = false;
       timer.stop();
       //v->scene()->clear();
       int i = 0;
       for(QGraphicsPixmapItem * item:v->preview_list){
           if(i<=counter)
                v->scene()->removeItem(item);
           delete item;
       }
       v->preview_list.clear();
       v->MoveToFrame(v->my_animation->frame_list[counter-1]);
       busy_play = false;
       // evita che 'play' resti pendente dopo l'auto-distruzione (oggi busy_play già impedisce
       // che venga riletto prima del prossimo click su Play, ma così è sicuro anche in futuro)
       play = nullptr;
       delete this;
       pausePressed = false;
   }


   //do some processing
}
