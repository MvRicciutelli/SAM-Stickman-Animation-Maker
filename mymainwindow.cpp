#include "mymainwindow.h"
#include "ui_mymainwindow.h"
#include "animation.h"
#include "QFileDialog"
#include "imageexport.h"
#include "sleeper.h"
#include  <QDir>
#include <QProcess>
#include <QFileInfo>
#include <animationoptions.h>
#include <ffmpegexport.h>
#include <QMessageBox>
#include <QMenuBar>
#include <QMenu>
#include <QAction>
#include "stkformat.h"
#include "uiItems.h"
extern QDir program_folder;
extern qint32 W;
extern qint32 H;
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
extern MySleeper *play;
int op_count = 0;
// flag sweep: render_flag (scritto dal thread di export ma mai letto da nessuno) e
// load_file (impostato attorno a LoadAnimation ma mai letto da nessuno) sono stati eliminati
QDir render_folder;
QString animation_path = "";
AnimationOptions* my_options = nullptr;
ImageExport *exporter = nullptr;
QString avi_export_folder;
QStringList ffmpeg_options;
/**
 * @details Builds the menu/window from the .ui file and launches a CheckUpdate worker
 * that pings the update site in the background; if a newer build exists it fires
 * UpdatePresent(). `view` and `central_w` are wired up by the startup code that creates
 * this window (see DoConnections()), not here.
 */
MyMainWindow::MyMainWindow(QWidget *parent) :
    QMainWindow(parent),
    ui(new Ui::MyMainWindow)
{
    ui->setupUi(this);
    CheckUpdate* check = new CheckUpdate();
    QObject::connect(check, &CheckUpdate::UpdatePresent,this, &MyMainWindow::UpdatePresent);
    check->start();

}


MyMainWindow::~MyMainWindow()
{
    delete ui;
}

/**
 * @details Saves the current figure alone to a file: native .stck
 * (StickFigure::SaveStickFigure()), Pivot .stk through StkFormat::ExportStk(), Stick Nodes
 * .nodes through StkFormat::ExportNodes(), or a self-contained *.marlib holding just this
 * one figure (MyView::SaveStickFigureAsLibrary(), so it can be dropped straight into
 * someone else's Libraries tab) — the selected filter (not the extension, which both Pivot
 * dialects share) picks the format. Export warnings about degraded features are shown once,
 * joined. No-op if the user cancels the dialog or there is no current figure.
 */
void MyMainWindow::on_actionsave_triggered()
{
    // una selezione multipla si esporta SOLO nella libreria nativa (spec): il salvataggio
    // su file singolo (.stck/.stk/.nodes) riguarda una figura sola
    if(view->MultiSelectionActive()){
        QMessageBox::information(this, tr("Multiple selection"),
                                 tr("A multiple selection can only be exported to the native library: use the Add To Library button, or reduce the selection to a single stickfigure."));
        return;
    }
    QString selected_filter;
    QString file_name = QFileDialog::getSaveFileName(this,tr("Save StickFigure"),
                       my_stick_figures_folder.path(),
                       "Stickfigure (*.stck);;Pivot 5 figure (*.stk);;Pivot 2 figure - Stick Nodes compatible (*.stk);;Stick Nodes figure (*.nodes);;Marionetta Library (*.marlib)",
                       &selected_filter);
    if(file_name.length()<=1 || !view->my_animation->CurrentFrame()->CurrentStickFigure())
        return;
    if(selected_filter.contains("Marionetta Library")){
        if(!file_name.endsWith(".marlib", Qt::CaseInsensitive))
            file_name += ".marlib";
        view->SaveStickFigureAsLibrary(file_name, view->my_animation->CurrentFrame()->CurrentStickFigure());
        return;
    }
    if(selected_filter.contains("Pivot") || selected_filter.contains("Stick Nodes figure")){
        bool nodes_format = selected_filter.contains("Stick Nodes figure");
        QString extension = nodes_format ? QStringLiteral(".nodes") : QStringLiteral(".stk");
        if(!file_name.endsWith(extension, Qt::CaseInsensitive))
            file_name += extension;
        QStringList export_warnings;
        QString export_error;
        StickFigureData *figure_data =
                ExtractStickFigureData(*view->my_animation->CurrentFrame()->CurrentStickFigure());
        bool exported;
        if(nodes_format){
            exported = StkFormat::ExportNodes(file_name, *figure_data,
                                              &export_warnings, &export_error);
        } else {
            StkFormat::StkVariant variant = selected_filter.contains("Pivot 2")
                    ? StkFormat::PIVOT2 : StkFormat::PIVOT5;
            exported = StkFormat::ExportStk(file_name, *figure_data, variant,
                                            &export_warnings, &export_error);
        }
        delete figure_data;
        if(!exported)
            QMessageBox::warning(this, tr("Export failed"), export_error);
        else if(!export_warnings.isEmpty())
            QMessageBox::information(this, tr("Export notes"), export_warnings.join("\n"));
        return;
    }
    view->my_animation->CurrentFrame()->CurrentStickFigure()->SaveStickFigure(file_name);
}

/**
 * @details Loads a figure into a fresh layer of the current frame: snapshots undo,
 * adds an empty figure (Widget::AddStick()), fills it from file, then registers its
 * sticks in the frame's total_sticks z-order list and shows their handles
 * (MyView::AddHandles()). Native .stck goes through StickFigure::LoadStickFigure();
 * Pivot .stk (both dialects, auto-detected) and Stick Nodes .nodes are converted by
 * StkFormat and land through the same PopulateLiveStickFigure() bridge the .stck
 * path uses. Foreign files are converted BEFORE any UI mutation, so a corrupt file
 * leaves the animation untouched; conversion warnings are shown once, joined.
 * A *.marlib is a different kind of file (a whole library, not one figure): it replaces
 * the open library buffer instead (MyView::LoadLibrary(), same as the Libraries tab's own
 * "Load library" button) and switches to the Libraries tab so the loaded entries are
 * immediately visible, rather than adding anything to the current frame.
 */
void MyMainWindow::on_actionload_triggered()
{

    QString file_name = QFileDialog::getOpenFileName(this,tr("Load StickFigure"),
                       my_stick_figures_folder.path(),
                       "Stickfigure (*.stck);;Pivot Animator figure (*.stk);;Stick Nodes figure (*.nodes);;Marionetta Library (*.marlib);;All files (*)");
    if(!QFile::exists(file_name))
        return;
    QString suffix = QFileInfo(file_name).suffix().toLower();
    if(suffix == "marlib"){
        view->LoadLibrary(file_name);
        my_tab_widget->setCurrentWidget(library_tab);
        return;
    }
    if(suffix == "stk" || suffix == "nodes"){
        QStringList import_warnings;
        QString import_error;
        // foreign coordinates are origin-relative: recenter onto the canvas
        QPointF canvas_center = my_rect ? my_rect->rect().center() : QPointF(W / 2.0, H / 2.0);
        StickFigureData *figure_data = (suffix == "stk")
                ? StkFormat::ImportStk(file_name, canvas_center, &import_warnings, &import_error)
                : StkFormat::ImportNodes(file_name, canvas_center, &import_warnings, &import_error);
        if(!figure_data){
            QMessageBox::warning(this, tr("Import failed"), import_error);
            return;
        }
        view->StoreUndo();
        central_w->AddStick();
        PopulateLiveStickFigure(view->my_animation->CurrentFrame()->CurrentStickFigure(),
                                *figure_data, true, true);
        delete figure_data;
        for(StickData*s:view->my_animation->CurrentFrame()->CurrentStickFigure()->stick_list){
            view->my_animation->CurrentFrame()->total_sticks.append(s);
        }
        view->AddHandles(view->my_animation->CurrentFrame()->CurrentStickFigure());
        if(!import_warnings.isEmpty())
            QMessageBox::information(this, tr("Import notes"), import_warnings.join("\n"));
        return;
    }
    view->StoreUndo();
    central_w->AddStick();
    view->my_animation->CurrentFrame()->CurrentStickFigure()->LoadStickFigure(file_name);
    view->ReportMissingTextFonts(); // font dei text stick assenti su questa macchina
    for(StickData*s:view->my_animation->CurrentFrame()->CurrentStickFigure()->stick_list){
        view->my_animation->CurrentFrame()->total_sticks.append(s);
    }
    view->AddHandles(view->my_animation->CurrentFrame()->CurrentStickFigure());
}

/**
 * @details "Save As" for the whole animation (*.mAnim, Animation::SaveAnimation()).
 * On success it records the path in the global `animation_path` (so later "Save" and
 * autosave target it) and resets op_count. See the inline note on why animation_path is
 * only written when the user actually confirms a file.
 */
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

/**
 * @details Loads a whole .mAnim animation, replacing the current document: clears undo
 * (stale frames must not be restorable onto the new document), records the path, then
 * shows frame 0. A corrupt file can leave frame_list empty, so that is checked before
 * MoveToFrame().
 */
void MyMainWindow::on_actionload_animation_triggered()
{
    QString file_name = QFileDialog::getOpenFileName(this,tr("Load Animation"),
                       my_animations_folder.path(), "Marionetta Animation (*.mAnim)");
    // ClearUndo/animation_path vanno toccati solo se l'utente conferma un file valido: annullare
    // la dialog non deve cancellare la cronologia undo o il path dell'animazione corrente
    if(QFile::exists(file_name)){
        view->ExitDrawMode(); // caricare ricostruisce tutto: chiudi il disegno per non lasciare draw_target penzolante
        view->ClearUndo();
        animation_path = file_name;
        view->my_animation->LoadAnimation(file_name);
        view->ReportMissingTextFonts(); // font dei text stick assenti su questa macchina
        // Animation::operator>> gia' applica W/H/my_rect dal file caricato; se il file e'
        // corrotto puo' interrompersi lasciando frame_list vuota, quindi va controllata qui
        if(!view->my_animation->frame_list.isEmpty()){
            view->MoveToFrame(view->my_animation->frame_list[0]);
        }
        op_count = 0;
    }
}

/**
 * @details AVI export, step 1: asks for the output file, (re)creates the scratch render
 * folder, empties it, then renders the image sequence into it (ExportImageSequence with
 * option AVI). When the render thread finishes, ImageRenderOver() calls ExportAvi() to
 * encode those images.
 */
void MyMainWindow::on_actionexport_as_AVI_file_triggered()
{
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
    QString full_path  = path+"/render.png";
    ExportImageSequence(full_path, AVI);

}
/**
 * @details Launches the bundled image2Avi.exe helper, feeding it the render folder,
 * output path, fps and geometry over stdin, then lets it run: the process frees itself
 * when it exits, like the ffmpeg one in ImageRenderOver(). It used to waitForFinished(),
 * whose 30 s default timeout was followed by a delete that KILLED the helper mid-file on
 * any animation that took longer to encode, freezing the UI until then.
 */
void MyMainWindow::ExportAvi(){
    // generate folder for renderings
    QString file_name = avi_export_folder;
    // apri il programma avi2Image, passagli i parametri per iniziale la conversione da immagini a file video
    // 1 in folder, 2 outfolder, 3 fps, 4 larghezza, 5 altezza, n immagini
    QProcess *p = new QProcess(this);
    QStringList param_list;
    param_list<<render_folder.path()
                    <<file_name
                    <<"24"
                    <<QString::number(W)
                    <<QString::number(H)
                    <<QString::number(view->my_animation->frame_list.count());
    // la path in cui si trova l'eseguibile di marionetta
    QString exe_path = QCoreApplication::applicationDirPath();
    // estrai il file exporter di avi
    QFile::copy(":/image2Avi.exe",exe_path+"/image2Avi.exe");
    // eseguilo
    QObject::connect(p, &QProcess::finished, p, &QObject::deleteLater);
    p->start(exe_path+"/image2Avi.exe");
    if (!p->waitForStarted()){
        QMessageBox::warning(this,"Video export failed","image2Avi could not be run: "+p->errorString());
        delete p;
        return;
    }
    for(const QString &s: param_list){
        // la lunghezza la dà QByteArray: s.length() conta i caratteri UTF-16, non i byte, e
        // troncava ogni percorso con lettere accentate
        p->write(s.toUtf8());
        p->write("\n");
    }
}
/**
 * @details Spawns a background ImageExport thread that renders every frame to a numbered
 * PNG. Guards against a second concurrent export: the previous thread owns the global
 * `exporter` pointer, and overwriting it would orphan the running thread.
 */
void MyMainWindow::ExportImageSequence(QString path, int option, QString video_file, int video_fps){
    view->ExitDrawMode(); // finalizza il disegno prima di renderizzare i frame (canvas ritagliato, non quello di sessione)
    // evita di avviare un secondo export mentre uno precedente è ancora in corso: altrimenti
    // il puntatore globale exporter verrebbe sovrascritto e il thread precedente resterebbe orfano
    if(exporter && exporter->isRunning()){
        QMessageBox::information(this,"Export in progress","An export is already running, please wait for it to finish.");
        return;
    }
    exporter = new ImageExport(this,path,option,video_file,video_fps);
    QObject::connect(exporter,&ImageExport::ImageExportFinished,this,&MyMainWindow::ImageRenderOver);
    exporter->start();

}

/**
 * @details "Save" for the current animation: overwrites `animation_path` if one is set
 * (and resets op_count), otherwise falls back to the "Save As" dialog. Also the target
 * of AutoSave() and of the save-on-close prompt.
 */
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

/**
 * @details Discards the current document for a fresh one after a confirmation prompt:
 * clears undo, forgets `animation_path` (so a later "Save" can't silently overwrite the
 * old file with the empty animation), empties the model (Animation::ClearAnimation())
 * and adds one starter frame (Widget::AddFrame()).
 */
void MyMainWindow::on_actionNew_Blank_Animation_triggered()
{

    int ret  = QMessageBox::warning(this, tr("Warning"),
                                    tr("This will delete all frames int eh current workspace, this action cannot be undone.\n"
                                       "Do you want to proceed?"),
                                    QMessageBox::Yes | QMessageBox::No
                                    );
    if(ret == QMessageBox::Yes)
    {
        view->ExitDrawMode(); // svuotare l'animazione libera tutti i frame: chiudi prima il disegno
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
/**
 * @details Applies the values the AnimationOptions dialog collected (fps, geometry,
 * handle size, autosave interval) and rescales/repositions the scene via SizeChange().
 * The dialog is torn down with deleteLater() because this slot fires from the dialog's
 * own OK-button handler, so an immediate delete would free it mid-callstack.
 */
void MyMainWindow::GetAnimationParams(int option_val){
    FPS = my_options->fps;
    W   = my_options->width;
    H   = my_options->height;
    handle_size = my_options->handle_size;
    view->ExitDrawMode(); // SizeChange riscala/riposiziona ogni stick: chiudi prima il disegno
    view->SizeChange(option_val);
    my_widget->WriteJson();
    auto_save_interval = my_options->auto_save_interval;
    // deleteLater invece di delete: questo slot viene chiamato dal click handler del bottone
    // OK dello stesso dialogo, quindi cancellarlo subito distruggerebbe l'oggetto mentre il suo
    // stack di chiamata è ancora attivo
    my_options->deleteLater();
}
/**
 * @details Opens the Animation Options dialog (fps, size, handle size, autosave). Single
 * instance: an already-open dialog is just raised. WA_DeleteOnClose frees it if closed
 * without OK, and the destroyed hook nulls `my_options`; on OK it emits DialogOut, wired
 * to GetAnimationParams() which applies the values.
 */
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

/**
 * @details Entry point for video export: it only opens the dialog (ExportUsingFFMPEG()).
 * It used to gate that on a valid FFMPEGPath, which no longer holds - mp4 is encoded by
 * Windows itself, and locating an ffmpeg is a button inside the dialog.
 */
void MyMainWindow::on_actionExport_video_using_FFMPEG_triggered()
{
    // ffmpeg non e' piu' una condizione per aprire la finestra: l'mp4 lo encoda Windows
    // (mfvideo.h), e i formati che invece sono di ffmpeg si abilitano dal bottone
    // "Locate FFmpeg..." della finestra stessa, che e' anche l'unico modo per cambiare
    // quel percorso una volta impostato
    ExportUsingFFMPEG();
}

FfmpegExport* ffmpeg_dialog = nullptr;
/**
 * @details Opens the FFMPEG export options dialog (single instance: re-focuses the
 * existing one instead of leaking a second). On accept it emits ExportVideo, wired to
 * StartFFMPEGRender().
 */
void MyMainWindow::ExportUsingFFMPEG(){
    // se è già aperta, portala in primo piano invece di aprirne un'altra (perderesti il puntatore alla precedente)
    if(ffmpeg_dialog){
        ffmpeg_dialog->raise();
        ffmpeg_dialog->activateWindow();
        return;
    }
    ffmpeg_dialog = new FfmpegExport;
    ffmpeg_dialog->show();
    QObject::connect(ffmpeg_dialog, &FfmpegExport::ExportVideo,this, &MyMainWindow::StartFFMPEGRender);
    QObject::connect(ffmpeg_dialog, &FfmpegExport::ExportVideoMF,this, &MyMainWindow::StartMediaFoundationRender);
    QObject::connect(ffmpeg_dialog, &QObject::destroyed, this, [](){ ffmpeg_dialog = nullptr; });
}

/**
 * @details Accept slot of the ffmpeg dialog: stashes the chosen ffmpeg arguments in the
 * global `ffmpeg_options`, empties the render folder and exports the image sequence with
 * option FFMPEG — ImageRenderOver() then launches ffmpeg on the result once the render
 * thread is done.
 */
void MyMainWindow::StartFFMPEGRender(QStringList options){
    // per ogni frame crea una scena temporanea, popolala con cloni degli sticks del progetto,
    // renderizza la scena su una QImage tramite un painter e salva l'immagine nella cartella
    ffmpeg_options = options;
    EmptyDirectory( render_folder.path());
    ExportImageSequence(render_folder.path()+"/img.png", FFMPEG);
}

/**
 * @details Accept slot of the export dialog for the mp4 format: renders exactly the same
 * image sequence the ffmpeg path uses, but ImageExport::run() then encodes it with the
 * Windows H.264 encoder instead of launching a process (mfvideo.h) - so no ffmpeg, no
 * bundled codec and nothing to locate.
 */
void MyMainWindow::StartMediaFoundationRender(QString file, int fps){
    EmptyDirectory( render_folder.path());
    ExportImageSequence(render_folder.path()+"/img.png", MFVIDEO, file, fps);
}

/**
 * @details The direct mp4 menu entry, shaped after on_actionexport_as_AVI_file_triggered():
 * asks for the output file, (re)creates and empties the scratch render folder, then renders
 * the image sequence into it with option MFVIDEO. The encoding happens in ImageExport::run()
 * as soon as the frames exist (mfvideo.h), so unlike the AVI and ffmpeg entries nothing
 * further is dispatched from ImageRenderOver(). It uses the animation's own FPS - there is
 * no dialog here in which to dial a different one.
 */
void MyMainWindow::on_actionExport_as_mp4_triggered()
{
    QString file_name = QFileDialog::getSaveFileName(this,tr("Export animation as an mp4 h264 video"),
                       my_video_export_folder.path(), "Mp4 video (*.mp4)");
    if(file_name.length() == 0)
        return;
    QString path = program_folder.path()+"/render";
    render_folder = QDir(path);
    if(!render_folder.exists())
        render_folder.mkpath(path);
    render_folder.setPath(path);
    // per ogni frame renderizza la scena su una QImage e salvala nella cartella; poi il
    // thread di export encoda quelle immagini con l'encoder H.264 di Windows
    EmptyDirectory( path);
    ExportImageSequence(path+"/img.png", MFVIDEO, file_name, FPS);
}

/**
 * @details Bound to view->AutoSaveTrigger (see DoConnections()): saves to the current
 * animation path via the "Save current" slot. Only actually writes if a path exists.
 */
void MyMainWindow::AutoSave()
{
    on_actionsave_animation_current_triggered();
}

/**
 * @details Shown when the background CheckUpdate worker reports a newer build is
 * available (fired from the constructor's connection).
 */
void MyMainWindow::UpdatePresent(QString ver)
{
    QMessageBox::about(this,"Old version Alert", "A newer version of this Software is available at Marionetta's website for download!");
}
// se questa funzione è stata chiamata per l'auto render avi, chiama una funzione che apra il processo
// stessa cosa se originariamente l'esportazione delle immagini dovrà invece essere usata da ffmpeg
/**
 * @details Called when the ImageExport thread finishes. Frees the exporter and nulls
 * the global pointer BEFORE dispatching, since ExportImageSequence()'s in-progress
 * guard reads it. Then hands the rendered sequence to the AVI helper or to ffmpeg - the
 * mp4 path has already encoded itself on the render thread, so all that is left there is
 * reporting a failure.
 */
void MyMainWindow::ImageRenderOver(int option){
    // il segnale è queued: può essere consegnato prima che run() sia davvero tornata, e
    // distruggere un QThread ancora in esecuzione è UB — wait() chiude quella finestra
    exporter->wait();
    const QString video_error = exporter->video_error; // l'exporter sta per essere distrutto
    delete exporter;
    // azzeralo subito: la guardia in ExportImageSequence legge questo puntatore, e dopo la delete sarebbe pendente
    exporter = nullptr;
    switch(option){
    case NONE:

        break;
    case AVI:
        ExportAvi();
        break;
    case MFVIDEO:
        // l'encoding e' gia' avvenuto sul thread di render (ImageExport::run())
        if(!video_error.isEmpty())
            QMessageBox::warning(this,"Video export failed",video_error);
        break;
    case FFMPEG:{
        QProcess *p = new QProcess(this);
        // cancella il processo solo quando ha finito, non subito dopo start(): altrimenti il
        // deleteLater() precedente distruggeva p mentre ffmpeg stava ancora incodificando il video.
        // Riporta anche l'errore: senza questo un codec assente o un file non scrivibile
        // facevano fallire l'export senza un messaggio
        QObject::connect(p, &QProcess::finished, this, [this,p](int code, QProcess::ExitStatus){
            if(code != 0)
                QMessageBox::warning(this,"Video export failed",
                                     QString::fromLocal8Bit(p->readAllStandardError()).right(1500));
            p->deleteLater();
        });
        QObject::connect(p, &QProcess::errorOccurred, this, [this,p](QProcess::ProcessError){
            QMessageBox::warning(this,"Video export failed","ffmpeg could not be run: "+p->errorString());
            p->deleteLater();
        });
        p->start(ResolveFfmpeg(),ffmpeg_options);
        break;
    }
    }
}


/**
 * @details Plain image-sequence export (option NONE): renders each frame to a numbered
 * PNG at the chosen path and stops there — no encoder step in ImageRenderOver().
 */
void MyMainWindow::on_actionExport_as_image_sequence_triggered()
{

    QString file_name = QFileDialog::getSaveFileName(this,tr("Save animation as a sequence of images"),
                       "C:/", "PNG Image (*.png)");
    if(file_name.length()>0)
        ExportImageSequence(file_name,NONE);
}
/**
 * @details Wires this window to `view`; called once at startup after `view` is created.
 * Currently just routes MyView::AutoSaveTrigger to AutoSave().
 */
void MyMainWindow::DoConnections(){
        QObject::connect(view, &MyView::AutoSaveTrigger,this, &MyMainWindow::AutoSave);
        // menu "Selection": scelta rettangolo/lazo per il rubber band di selezione figure.
        // Costruito in codice (come i pannelli vertici/IK) invece che nel .ui; scrive
        // direttamente MyView::stickfig_lasso.
        QMenu *selection_menu = menuBar()->addMenu(tr("Selection"));
        QAction *lasso_action = selection_menu->addAction(tr("Lasso select figures"));
        lasso_action->setCheckable(true);
        lasso_action->setChecked(view->stickfig_lasso);
        QObject::connect(lasso_action, &QAction::toggled, this, [this](bool on){ view->stickfig_lasso = on; });
}
/**
 * @details On close: first waits for any running export thread to finish (it is a child
 * of this window and would otherwise be destroyed mid-run), then, if there is unsaved
 * work (op_count>0), offers Save/Discard/Cancel — Cancel aborts the close via
 * event->ignore().
 */
void MyMainWindow::closeEvent(QCloseEvent *event)
{
    // se un export è ancora in corso, attendi che finisca prima di chiudere l'app: altrimenti
    // il thread (ora figlio di questa finestra) verrebbe distrutto mentre è ancora in esecuzione
    if(exporter && exporter->isRunning()){
        exporter->wait();
    }
    // idem per il thread che prepara l'anteprima: renderizza con font e paint engine che
    // QApplication smonta all'uscita
    if(play)
        play->WaitForPreview();
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

/**
 * @details Shows the About box: product identity, licensing, warranty disclaimer and
 * the author's channels. This dialog is also where the app discharges LGPLv3 section
 * 4(a)/4(c) for Qt — a combined work that displays copyright notices while running must
 * name the library, its copyright holder and where its license text can be found, which
 * is why the Qt paragraph is not optional decoration. NOTICE.md carries the same
 * statement for the source tree; "About Qt" next to this entry shows Qt's own box.
 */
void MyMainWindow::on_actionAbout_triggered()
{
    QMessageBox::about(this, "About MarionettaPlus",
        "<b>MarionettaPlus 0.9.0</b><br>"
        "Frame-by-frame stickfigure animation studio.<br><br>"

        "Draw and rig stick figures from segments, circles, tapers, polygons, text and "
        "images, pose them frame by frame with onion-skin reference and generated "
        "inbetweens, then export the result as an image sequence or as video "
        "through FFmpeg.<br><br>"

        "Designed and created by Matteo Vittorio Ricciutelli.<br>"
        "Copyright (C) 2026 Matteo Vittorio Ricciutelli. All rights reserved.<br><br>"

        "Distributed free of charge for personal, hobby, educational and other "
        "noncommercial use under the PolyForm Strict License 1.0.0. Animations you "
        "create with this program are your own and may be used commercially. "
        "Redistributing or modifying the program itself requires the author's written "
        "authorization, which you may request by email at "
        "matteoricciutelli(at)mail.com. The full terms, including that permission, "
        "are in the LICENSE file distributed with the program.<br><br>"

        "This program is provided \"as is\", without warranty or condition of any kind. "
        "The author accepts no liability for its use, for any works created with it, or "
        "for any loss of data.<br><br>"

        "Built with the Qt framework, Copyright (C) The Qt Company Ltd., used unmodified "
        "and dynamically linked under the GNU Lesser General Public License version 3. "
        "Copies of the LGPLv3 and the GPLv3 it refers to are supplied in the LICENSES "
        "folder, and you may replace the Qt libraries with your own build of them. "
        "Choose Help / About Qt to see the version in use.<br><br>"

        "Website: https://sites.google.com/view/marionetta/home<br>"
        "Facebook: https://www.facebook.com/Marionetta-StickFigure-Animation-Studio-2330682093680488"
        );
}


/**
 * @details Qt's own about box: version, and the LGPLv3 notice Qt requires apps built
 * against it to make available. See NOTICE.md for the full statement (version linked,
 * source location, unmodified).
 */
void MyMainWindow::on_actionAboutQt_triggered()
{
    QMessageBox::aboutQt(this, "About Qt");
}

extern QString license;
extern int terms_accepted;
/**
 * @details Shows the terms via Widget::TermsAndConditions() and enforces acceptance: if
 * the user declines (terms_accepted==0) it asks once more, and quits the program if they
 * still refuse, otherwise loops back to re-present them.
 */
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
