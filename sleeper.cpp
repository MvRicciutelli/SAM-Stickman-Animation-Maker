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

void MySleeper::StartPlaying()
{
    connect(&timer, SIGNAL(timeout()), SLOT(Connected()));
    timer.setSingleShot(false);
    timer.setInterval(intervalTime);
    timer.start();
}
void MySleeper::Stop(){
    pausePressed = true;
}
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
