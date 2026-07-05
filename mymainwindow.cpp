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
#define CS view->myAnimation->currentFrame->currentStickFigure->currentStick
#define CURRENTSTICKFIGURE view->myAnimation->currentFrame->currentStickFigure
#define CURRENTFRAME view->myAnimation->currentFrame
extern QDir programFolder;
extern int W;
extern int H;
extern int FPS;
extern int autoSaveInterval;
extern QGraphicsRectItem *myRect;
extern Widget *myWidget;
extern QString FFMPEGPath;
extern QDir tempRenderFolder;
extern QDir myAnimationsFolder;
extern QDir myStickFiguresFolder;
extern QDir myVideoExportFolder;
extern int HANDLESIZE;
int opCount = 0;
bool renderFlag = false;
QDir renderFolder;
QString animationPath = "";
animationOptions* myOptions = nullptr;
bool loadFile = false;
imageExport *exporter = nullptr;
QString aviExportFolder;
QStringList ffmpegOptions;
myMainWindow::myMainWindow(QWidget *parent) :
    QMainWindow(parent),
    ui(new Ui::myMainWindow)
{
    ui->setupUi(this);
    checkUpdate* Check = new checkUpdate();
    QObject::connect(Check, &checkUpdate::updatePresent,this, &myMainWindow::updatePresent);
    Check->start();

}


myMainWindow::~myMainWindow()
{
    delete ui;
}

void myMainWindow::on_actionsave_triggered()
{
    QString fileName = QFileDialog::getSaveFileName(this,tr("Save StickFigure"),
                       myStickFiguresFolder.path(), "Stickfigure (*.stck)");
    if(fileName.length()>1){
        QDataStream myStream;
        CURRENTSTICKFIGURE->saveStickFigure(fileName);
    }
}

void myMainWindow::on_actionload_triggered()
{

    QString fileName = QFileDialog::getOpenFileName(this,tr("Load StickFigure"),
                       myStickFiguresFolder.path(), "Stickfigure (*.stck)");
    if(QFile::exists(fileName)){
        QDataStream myStream;
        centralW->addStick();
        CURRENTSTICKFIGURE->loadStickFigure(fileName);
        for(stick*s:CURRENTSTICKFIGURE->stickList){
            CURRENTFRAME->totalSticks.append(s);
        }
        view->addHandles(CURRENTSTICKFIGURE);
    }
}

void myMainWindow::on_actionsave_animation_triggered()
{
    QString fileName = QFileDialog::getSaveFileName(this,tr("Save Animation"),
                       myAnimationsFolder.path(), "Marionetta Animation (*.mAnim)");
    animationPath = fileName;
    if(fileName.length()>1){
        view->myAnimation->saveAnimation(fileName);
        opCount = 0;
    }
}

void myMainWindow::on_actionload_animation_triggered()
{
    QString fileName = QFileDialog::getOpenFileName(this,tr("Load Animation"),
                       myAnimationsFolder.path(), "Marionetta Animation (*.mAnim)");
     view->clearUndo();
     animationPath = fileName;
    if(QFile::exists(fileName)){
        loadFile = true;
        QDataStream myStream;
        view->myAnimation->loadAnimation(fileName);
        if(view->myAnimation->width != W || view->myAnimation->height != H){
            W = view->myAnimation->width;
            H = view->myAnimation->height;
            view->sizeChange(NOREPO);
        }
        view->moveToFrame(view->myAnimation->frameList[0]);
        loadFile = false;
        opCount = 0;
    }
}

void myMainWindow::on_actionexport_as_AVI_file_triggered()
{
    //QVideoEncoder encoder;
    /*

*/
    // generate folder for renderings
    aviExportFolder = QFileDialog::getSaveFileName(this,tr("Export animation as an avi file"),
                       myVideoExportFolder.path(), "avi video file (*.avi)");
    QString path = programFolder.path()+"/render";
    renderFolder = QDir(path);
    if(aviExportFolder.length() == 0)
        return;
    if(renderFolder.exists()){
        qDebug("folder located");
    }
    else{
        qDebug("folder created");
        renderFolder.mkpath(path);
    }
    renderFolder.setPath(path);
    // per ogni frame crea una scena temporanea, popolala con cloni degli sticks del progetto,
    // renderizza la scena su una QImage tramite un painter e salva l'immagine nella cartella
    emptyDirectory( path);
    QString fullpath  = path+"/render.png";
    exportImageSequence(fullpath, AVI);

}
void myMainWindow::exportAvi(){
    // generate folder for renderings
    QString fileName = aviExportFolder;
    QString imgPath = programFolder.path()+"/render";
    // apri il programma avi2Image, passagli i parametri per iniziale la conversione da immagini a file video
    // 1 in folder, 2 outfolder, 3 fps, 4 larghezza, 5 altezza, n immagini
    QProcess *p = new QProcess(this);
    QStringList paramList;
    paramList<<renderFolder.path()
                    <<fileName
                    <<"24"
                    <<QString::number(W)
                    <<QString::number(H)
                    <<QString::number(view->myAnimation->frameList.count());
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
void myMainWindow::exportImageSequence(QString path, int option){
    // evita di avviare un secondo export mentre uno precedente è ancora in corso: altrimenti
    // il puntatore globale exporter verrebbe sovrascritto e il thread precedente resterebbe orfano
    if(exporter && exporter->isRunning()){
        QMessageBox::information(this,"Export in progress","An export is already running, please wait for it to finish.");
        return;
    }
    exporter = new imageExport(this,path,option);
    QObject::connect(exporter,&imageExport::imageExportFinished,this,&myMainWindow::imageRenderOver);
    exporter->start();

}

void myMainWindow::on_actionsave_animation_current_triggered()
{
    if(QFile::exists(animationPath)){
        view->myAnimation->saveAnimation(animationPath);
        opCount = 0;
    }
    else{
        on_actionsave_animation_triggered();
    }
}

void myMainWindow::on_actionNew_Blank_Animation_triggered()
{

    int ret  = QMessageBox::warning(this, tr("Warning"),
                                    tr("This will delete all frames int eh current workspace, this action cannot be undone.\n"
                                       "Do you want to proceed?"),
                                    QMessageBox::Yes | QMessageBox::No
                                    );
    if(ret == QMessageBox::Yes)
    {
        view->myAnimation->clearAnimation();
        myWidget->addFrame();
        opCount = 0;
    }
}
void myMainWindow::getAnimationParams(int optionVal){
    FPS = myOptions->fps;
    W   = myOptions->width;
    H   = myOptions->height;
    HANDLESIZE = myOptions->handleSize;
    view->sizeChange(optionVal);
    myWidget->writeJson();
    autoSaveInterval = myOptions->autoSaveInterval;
    // deleteLater invece di delete: questo slot viene chiamato dal click handler del bottone
    // OK dello stesso dialogo, quindi cancellarlo subito distruggerebbe l'oggetto mentre il suo
    // stack di chiamata è ancora attivo
    myOptions->deleteLater();
}
void myMainWindow::on_actionAnimation_Options_triggered()
{
    // se è già aperto, portalo in primo piano invece di aprirne un altro (perderesti il puntatore al precedente)
    if(myOptions){
        myOptions->raise();
        myOptions->activateWindow();
        return;
    }
    myOptions = new animationOptions(nullptr,W,H,FPS,HANDLESIZE,autoSaveInterval);
    // senza questo, chiudere la finestra senza premere OK la nasconde soltanto: non viene mai distrutta
    myOptions->setAttribute(Qt::WA_DeleteOnClose);
    myOptions->show();
    QObject::connect(myOptions, &animationOptions::dialogOut,this, &myMainWindow::getAnimationParams);
    QObject::connect(myOptions, &QObject::destroyed, this, [this](){ myOptions = nullptr; });
}

void myMainWindow::on_actionExport_video_using_FFMPEG_triggered()
{
    if(QFile::exists(FFMPEGPath)){
        exportUsingFFMPEG();
    }
    else{
        QMessageBox::about(this,"FFMPEG Location","Please locate where ffmpeg executable is located on your system");
        QString fileName = QFileDialog::getOpenFileName(this,tr("find FFMPEG executable file"),
                           myVideoExportFolder.path(), "ffmpeg(*.exe)");
        if(QFile::exists(fileName)){
                if(QFileInfo(QFile(fileName)).fileName()=="ffmpeg.exe"){
                    FFMPEGPath = fileName;
                    myWidget->writeJson();
                    exportUsingFFMPEG();
                }
        }
    }
}

ffmpegExport* ffmpegDialog = nullptr;
void myMainWindow::exportUsingFFMPEG(){
    // se è già aperta, portala in primo piano invece di aprirne un'altra (perderesti il puntatore alla precedente)
    if(ffmpegDialog){
        ffmpegDialog->raise();
        ffmpegDialog->activateWindow();
        return;
    }
    ffmpegDialog = new ffmpegExport;
    ffmpegDialog->show();
    QObject::connect(ffmpegDialog, &ffmpegExport::exportVideo,this, &myMainWindow::startffmpegRender);
    QObject::connect(ffmpegDialog, &QObject::destroyed, this, [](){ ffmpegDialog = nullptr; });
}

void myMainWindow::startffmpegRender(QStringList options){
    // per ogni frame crea una scena temporanea, popolala con cloni degli sticks del progetto,
    // renderizza la scena su una QImage tramite un painter e salva l'immagine nella cartella
    ffmpegOptions = options;
    emptyDirectory( renderFolder.path());
    exportImageSequence(renderFolder.path()+"/img.png", FFMPEG);
    //

    //QString imagePath = programFolder.path()+"/render";


}

void myMainWindow::autoSave()
{
    on_actionsave_animation_current_triggered();
}

void myMainWindow::updatePresent(QString ver)
{
    QMessageBox::about(this,"Old version Alert", "A newer version of this Software is available at Marionetta's website for download!");
}
// se questa funzione è stata chiamata per l'auto render avi, chiama una funzione che apra il processo
// stessa cosa se originariamente l'esportazione delle immagini dovrà invece essere usata da ffmpeg
void myMainWindow::imageRenderOver(int option){
    // added delete exporter
    delete exporter;
    // azzeralo subito: la guardia in exportImageSequence legge questo puntatore, e dopo la delete sarebbe pendente
    exporter = nullptr;
    switch(option){
    case NONE:

        break;
    case AVI:
        exportAvi();
        break;
    case FFMPEG:
        QProcess *p = new QProcess(this);
        // cancella il processo solo quando ha finito, non subito dopo start(): altrimenti il
        // deleteLater() precedente distruggeva p mentre ffmpeg stava ancora incodificando il video
        QObject::connect(p, &QProcess::finished, p, &QObject::deleteLater);
        p->start(FFMPEGPath,ffmpegOptions);
        break;
    }
}


void myMainWindow::on_actionExport_as_image_sequence_triggered()
{

    QString fileName = QFileDialog::getSaveFileName(this,tr("Save animation as a sequence of images"),
                       "C:/", "PNG Image (*.png)");
    if(fileName.length()>0)
        exportImageSequence(fileName,NONE);
}
void myMainWindow::doConnections(){
        QObject::connect(view, &myView::autoSaveTrigger,this, &myMainWindow::autoSave);
}
void myMainWindow::closeEvent(QCloseEvent *event)
{
    // se un export è ancora in corso, attendi che finisca prima di chiudere l'app: altrimenti
    // il thread (ora figlio di questa finestra) verrebbe distrutto mentre è ancora in esecuzione
    if(exporter && exporter->isRunning()){
        exporter->wait();
    }
    if(opCount>0){
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

void myMainWindow::on_actionAbout_triggered()
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
extern int termsAccepted;
void myMainWindow::on_actionTerms_and_Conditions_triggered()
{
    //QMessageBox::information(this, " conditions",license);
    license_info_begin:
    myWidget->termsAndConditions();
    if(termsAccepted == 0){
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

