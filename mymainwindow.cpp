#include "mymainwindow.h"
#include "ui_mymainwindow.h"
#include "animation.h"
#include "QFileDialog"
#include "imageexport.h"
#include  <QDir>
#include <QProcess>
#include <QFileInfo>
#include <animationoptions.h>
#include <ffmpegexport.h>
#include <QMessageBox>
extern QDir program_folder;
extern int W;
extern int H;
extern int FPS;
extern int auto_save_interval;
extern QGraphicsRectItem *my_rect;
extern Widget *my_widget;
extern QString FFMPEGPath;
extern QDir temp_render_folder;
extern QDir my_animations_folder;
extern QDir my_stick_figures_folder;
extern QDir my_video_export_folder;
extern int handle_size;
int op_count = 0;
bool render_flag = false;
QDir render_folder;
QString animation_path = "";
AnimationOptions* my_options = nullptr;
bool load_file = false;
ImageExport *exporter = nullptr;
QString avi_export_folder;
QStringList ffmpeg_options;
MyMainWindow::MyMainWindow(QWidget *parent) :
    QMainWindow(parent),
    ui(new Ui::MyMainWindow)
{
    ui->setupUi(this);
    CheckUpdate* Check = new CheckUpdate();
    QObject::connect(Check, &CheckUpdate::UpdatePresent,this, &MyMainWindow::UpdatePresent);
    Check->start();

}


MyMainWindow::~MyMainWindow()
{
    delete ui;
}

void MyMainWindow::on_actionsave_triggered()
{
    QString file_name = QFileDialog::getSaveFileName(this,tr("Save StickFigure"),
                       my_stick_figures_folder.path(), "Stickfigure (*.stck)");
    if(file_name.length()>1 && view->my_animation->CurrentFrame()->CurrentStickFigure()){
        view->my_animation->CurrentFrame()->CurrentStickFigure()->SaveStickFigure(file_name);
    }
}

void MyMainWindow::on_actionload_triggered()
{

    QString file_name = QFileDialog::getOpenFileName(this,tr("Load StickFigure"),
                       my_stick_figures_folder.path(), "Stickfigure (*.stck)");
    if(QFile::exists(file_name)){
        view->StoreUndo();
        central_w->AddStick();
        view->my_animation->CurrentFrame()->CurrentStickFigure()->LoadStickFigure(file_name);
        for(StickData*s:view->my_animation->CurrentFrame()->CurrentStickFigure()->stick_list){
            view->my_animation->CurrentFrame()->total_sticks.append(s);
        }
        view->AddHandles(view->my_animation->CurrentFrame()->CurrentStickFigure());
    }
}

void MyMainWindow::on_actionsave_animation_triggered()
{
    QString file_name = QFileDialog::getSaveFileName(this,tr("Save Animation"),
                       my_animations_folder.path(), "Marionetta Animation (*.mAnim)");
    // animation_path va scritto solo qui dentro: se l'utente annulla la dialog, file_name e'
    // vuoto e non deve invalidare il path del file gia' aperto (rompe autosave e "Save")
    if(file_name.length()>1){
        animation_path = file_name;
        view->my_animation->SaveAnimation(file_name);
        op_count = 0;
    }
}

void MyMainWindow::on_actionload_animation_triggered()
{
    QString file_name = QFileDialog::getOpenFileName(this,tr("Load Animation"),
                       my_animations_folder.path(), "Marionetta Animation (*.mAnim)");
    // ClearUndo/animation_path vanno toccati solo se l'utente conferma un file valido: annullare
    // la dialog non deve cancellare la cronologia undo o il path dell'animazione corrente
    if(QFile::exists(file_name)){
        view->ClearUndo();
        animation_path = file_name;
        load_file = true;
        view->my_animation->LoadAnimation(file_name);
        // Animation::operator>> gia' applica W/H/my_rect dal file caricato; se il file e'
        // corrotto puo' interrompersi lasciando frame_list vuota, quindi va controllata qui
        if(!view->my_animation->frame_list.isEmpty()){
            view->MoveToFrame(view->my_animation->frame_list[0]);
        }
        load_file = false;
        op_count = 0;
    }
}

void MyMainWindow::on_actionexport_as_AVI_file_triggered()
{
    //QVideoEncoder encoder;
    /*

*/
    // generate folder for renderings
    avi_export_folder = QFileDialog::getSaveFileName(this,tr("Export animation as an avi file"),
                       my_video_export_folder.path(), "avi video file (*.avi)");
    QString path = program_folder.path()+"/render";
    render_folder = QDir(path);
    if(avi_export_folder.length() == 0)
        return;
    if(render_folder.exists()){
        qDebug("folder located");
    }
    else{
        qDebug("folder created");
        render_folder.mkpath(path);
    }
    render_folder.setPath(path);
    // per ogni frame crea una scena temporanea, popolala con cloni degli sticks del progetto,
    // renderizza la scena su una QImage tramite un painter e salva l'immagine nella cartella
    EmptyDirectory( path);
    QString fullpath  = path+"/render.png";
    ExportImageSequence(fullpath, AVI);

}
void MyMainWindow::ExportAvi(){
    // generate folder for renderings
    QString file_name = avi_export_folder;
    QString imgPath = program_folder.path()+"/render";
    // apri il programma avi2Image, passagli i parametri per iniziale la conversione da immagini a file video
    // 1 in folder, 2 outfolder, 3 fps, 4 larghezza, 5 altezza, n immagini
    QProcess *p = new QProcess(this);
    QStringList paramList;
    paramList<<render_folder.path()
                    <<file_name
                    <<"24"
                    <<QString::number(W)
                    <<QString::number(H)
                    <<QString::number(view->my_animation->frame_list.count());
    // la path in cui si trova l'é'eseguibile di marionetta
    QString exePath = QCoreApplication::applicationDirPath();
    // estrai il file exporter di avi
    QFile::copy(":/image2Avi.exe",exePath+"/image2Avi.exe");
    // eseguilo
    p->start(exePath+"/image2Avi.exe");
    if (!p->waitForStarted())
        qDebug("starting render problem");
    for(QString s: paramList){
        p->write(s.toUtf8(),s.length());
        p->write("\n");
    }
    p->waitForFinished();
    QString output = (p->readAllStandardOutput());
    delete p;

}
void MyMainWindow::ExportImageSequence(QString path, int option){
    // evita di avviare un secondo export mentre uno precedente è ancora in corso: altrimenti
    // il puntatore globale exporter verrebbe sovrascritto e il thread precedente resterebbe orfano
    if(exporter && exporter->isRunning()){
        QMessageBox::information(this,"Export in progress","An export is already running, please wait for it to finish.");
        return;
    }
    exporter = new ImageExport(this,path,option);
    QObject::connect(exporter,&ImageExport::ImageExportFinished,this,&MyMainWindow::ImageRenderOver);
    exporter->start();

}

void MyMainWindow::on_actionsave_animation_current_triggered()
{
    if(QFile::exists(animation_path)){
        view->my_animation->SaveAnimation(animation_path);
        op_count = 0;
    }
    else{
        on_actionsave_animation_triggered();
    }
}

void MyMainWindow::on_actionNew_Blank_Animation_triggered()
{

    int ret  = QMessageBox::warning(this, tr("Warning"),
                                    tr("This will delete all frames int eh current workspace, this action cannot be undone.\n"
                                       "Do you want to proceed?"),
                                    QMessageBox::Yes | QMessageBox::No
                                    );
    if(ret == QMessageBox::Yes)
    {
        // senza ClearUndo() l'undo buffer terrebbe frame della vecchia animazione applicabili
        // a questa nuova; senza azzerare animation_path, "Save" sovrascriverebbe silenziosamente
        // il vecchio file salvato con la nuova animazione vuota
        view->ClearUndo();
        animation_path.clear();
        view->my_animation->ClearAnimation();
        my_widget->AddFrame();
        op_count = 0;
    }
}
void MyMainWindow::GetAnimationParams(int option_val){
    FPS = my_options->fps;
    W   = my_options->width;
    H   = my_options->height;
    handle_size = my_options->handle_size;
    view->SizeChange(option_val);
    my_widget->WriteJson();
    auto_save_interval = my_options->auto_save_interval;
    // deleteLater invece di delete: questo slot viene chiamato dal click handler del bottone
    // OK dello stesso dialogo, quindi cancellarlo subito distruggerebbe l'oggetto mentre il suo
    // stack di chiamata è ancora attivo
    my_options->deleteLater();
}
void MyMainWindow::on_actionAnimation_Options_triggered()
{
    // se è già aperto, portalo in primo piano invece di aprirne un altro (perderesti il puntatore al precedente)
    if(my_options){
        my_options->raise();
        my_options->activateWindow();
        return;
    }
    my_options = new AnimationOptions(nullptr,W,H,FPS,handle_size,auto_save_interval);
    // senza questo, chiudere la finestra senza premere OK la nasconde soltanto: non viene mai distrutta
    my_options->setAttribute(Qt::WA_DeleteOnClose);
    my_options->show();
    QObject::connect(my_options, &AnimationOptions::DialogOut,this, &MyMainWindow::GetAnimationParams);
    QObject::connect(my_options, &QObject::destroyed, this, [this](){ my_options = nullptr; });
}

void MyMainWindow::on_actionExport_video_using_FFMPEG_triggered()
{
    if(QFile::exists(FFMPEGPath)){
        ExportUsingFFMPEG();
    }
    else{
        QMessageBox::about(this,"FFMPEG Location","Please locate where ffmpeg executable is located on your system");
        QString file_name = QFileDialog::getOpenFileName(this,tr("find FFMPEG executable file"),
                           my_video_export_folder.path(), "ffmpeg(*.exe)");
        if(QFile::exists(file_name)){
                if(QFileInfo(QFile(file_name)).fileName()=="ffmpeg.exe"){
                    FFMPEGPath = file_name;
                    my_widget->WriteJson();
                    ExportUsingFFMPEG();
                }
        }
    }
}

FfmpegExport* ffmpegDialog = nullptr;
void MyMainWindow::ExportUsingFFMPEG(){
    // se è già aperta, portala in primo piano invece di aprirne un'altra (perderesti il puntatore alla precedente)
    if(ffmpegDialog){
        ffmpegDialog->raise();
        ffmpegDialog->activateWindow();
        return;
    }
    ffmpegDialog = new FfmpegExport;
    ffmpegDialog->show();
    QObject::connect(ffmpegDialog, &FfmpegExport::ExportVideo,this, &MyMainWindow::StartFFMPEGRender);
    QObject::connect(ffmpegDialog, &QObject::destroyed, this, [](){ ffmpegDialog = nullptr; });
}

void MyMainWindow::StartFFMPEGRender(QStringList options){
    // per ogni frame crea una scena temporanea, popolala con cloni degli sticks del progetto,
    // renderizza la scena su una QImage tramite un painter e salva l'immagine nella cartella
    ffmpeg_options = options;
    EmptyDirectory( render_folder.path());
    ExportImageSequence(render_folder.path()+"/img.png", FFMPEG);
    //

    //QString imagePath = program_folder.path()+"/render";


}

void MyMainWindow::AutoSave()
{
    on_actionsave_animation_current_triggered();
}

void MyMainWindow::UpdatePresent(QString ver)
{
    QMessageBox::about(this,"Old version Alert", "A newer version of this Software is available at Marionetta's website for download!");
}
// se questa funzione è stata chiamata per l'auto render avi, chiama una funzione che apra il processo
// stessa cosa se originariamente l'esportazione delle immagini dovrà invece essere usata da ffmpeg
void MyMainWindow::ImageRenderOver(int option){
    // added delete exporter
    delete exporter;
    // azzeralo subito: la guardia in ExportImageSequence legge questo puntatore, e dopo la delete sarebbe pendente
    exporter = nullptr;
    switch(option){
    case NONE:

        break;
    case AVI:
        ExportAvi();
        break;
    case FFMPEG:
        QProcess *p = new QProcess(this);
        // cancella il processo solo quando ha finito, non subito dopo start(): altrimenti il
        // deleteLater() precedente distruggeva p mentre ffmpeg stava ancora incodificando il video
        QObject::connect(p, &QProcess::finished, p, &QObject::deleteLater);
        p->start(FFMPEGPath,ffmpeg_options);
        break;
    }
}


void MyMainWindow::on_actionExport_as_image_sequence_triggered()
{

    QString file_name = QFileDialog::getSaveFileName(this,tr("Save animation as a sequence of images"),
                       "C:/", "PNG Image (*.png)");
    if(file_name.length()>0)
        ExportImageSequence(file_name,NONE);
}
void MyMainWindow::DoConnections(){
        QObject::connect(view, &MyView::AutoSaveTrigger,this, &MyMainWindow::AutoSave);
}
void MyMainWindow::closeEvent(QCloseEvent *event)
{
    // se un export è ancora in corso, attendi che finisca prima di chiudere l'app: altrimenti
    // il thread (ora figlio di questa finestra) verrebbe distrutto mentre è ancora in esecuzione
    if(exporter && exporter->isRunning()){
        exporter->wait();
    }
    if(op_count>0){
        QMessageBox::StandardButton reply;
        reply = QMessageBox::question(this, "Save", "There is unsaved progress in your work, do you want to save your changes?",
            QMessageBox::Save | QMessageBox::Discard | QMessageBox::Cancel);
        if (reply == QMessageBox::Save) {
            on_actionsave_animation_current_triggered();
            QMainWindow::closeEvent(event);

        }
        else if (reply == QMessageBox::Discard)
        {

        }
        else if(reply == QMessageBox::Cancel)
        {
            event->ignore();
            return;
        }

    }
    else
        QMainWindow::closeEvent(event);
}

void MyMainWindow::on_actionAbout_triggered()
{
    QMessageBox::about(this, "About Marionetta 1.00",
                                  "Marionetta is a FREE stickfigure animation software designed and created"
                                  " by Matteo Vittorio Ricciutelli.\r\n\r\n"
                                  "Get the latest updates and infos at Marionetta official website: \r\n"
                                  "https://sites.google.com/view/marionetta/home \r\n\n"
                                  "Like Marionetta official facebook page! \r\n"
                                  "https://www.facebook.com/Marionetta-StickFigure-Animation-Studio-2330682093680488"
                       "\r\n\r\nAny distribution of this software must undergo authorization from its orginal creator"
                                                         "you may contact him via email at the address  \r\nmatteoricciutelli(at)mail.com"
                                                         " \r\n"
                       );
}


extern QString license;
extern int terms_accepted;
void MyMainWindow::on_actionTerms_and_Conditions_triggered()
{
    //QMessageBox::information(this, " conditions",license);
    license_info_begin:
    my_widget->TermsAndConditions();
    if(terms_accepted == 0){
        QMessageBox* confirm =  new QMessageBox(QMessageBox::Question, "Warning",
        " are you sure? (the program will be closed). \r\n" ,
                                      QMessageBox::Yes|QMessageBox::No);
        // exec() è modale/bloccante, quindi possiamo cancellare confirm subito dopo che è tornato,
        // prima ancora di decidere cosa fare col risultato (altrimenti si perdeva un'istanza ad ogni "No")
        int ret = confirm->exec();
        delete confirm;
        if(ret == QMessageBox::Yes){
            exit(EXIT_FAILURE);
        }else{
            goto license_info_begin;
        }
    }
}

