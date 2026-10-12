#include "ffmpegexport.h"
#include "ui_ffmpegexport.h"

#include "widget.h"

#include <QFile>
#include <QFileDialog>
#include <QFileInfo>
#include <QMessageBox>
#include <QProcess>
#include <QPushButton>
#include <QVBoxLayout>
extern qint32 W;
extern qint32 H;
extern int FPS;
QString output_file = ""; ///< chosen output video path; kept in sync with the selected format
extern QString FFMPEGPath;
extern QDir program_folder;
extern QDir render_folder;
extern QDir my_video_export_folder;
extern Widget *my_widget;
/**
 * @details Pre-fills the fps control from the current animation FPS and marks the dialog
 * WA_DeleteOnClose so closing it via the window X actually destroys it (otherwise it is
 * only hidden and the pointer in MyMainWindow leaks).
 */
FfmpegExport::FfmpegExport(QWidget *parent) :
    QWidget(parent),
    ui(new Ui::FfmpegExport)
{
    ui->setupUi(this);
    ui->fpsSpinbox->setValue(FPS);
    // ffmpeg non viene distribuito col programma (NOTICE.md): da qui l'utente indica la
    // propria installazione, ed e' quello che abilita i tre formati diversi dall'mp4
    locate_ffmpeg_btn = new QPushButton(tr("Locate FFmpeg..."), this);
    ui->verticalLayout->addWidget(locate_ffmpeg_btn);
    connect(locate_ffmpeg_btn, &QPushButton::clicked, this, &FfmpegExport::LocateFfmpeg);
    RefreshFfmpegAvailability();
    // senza questo, chiudere la finestra dalla X la nasconde soltanto: non viene mai distrutta
    setAttribute(Qt::WA_DeleteOnClose);
}

FfmpegExport::~FfmpegExport()
{
    delete ui;
}


//https://walterebert.com/blog/creating-wmv-videos-with-ffmpeg/
// https://hamelot.io/visualization/using-ffmpeg-to-convert-a-set-of-images-into-a-video/
// https://trac.ffmpeg.org/wiki/Encode/MPEG-4
/**
 * @details Assembles the ffmpeg command line and emits it. First it (re)creates the
 * scratch render folder MyMainWindow::StartFFMPEGRender() will fill with img_%d.png
 * frames, then builds the common arguments (framerate, image2 muxer, size, input
 * pattern) and appends codec flags for the checked format radio (wmv/mp4/avi; gif needs
 * none). The result travels through ExportVideo() to MyMainWindow::StartFFMPEGRender().
 */
void FfmpegExport::on_exportVideoBtn_clicked()
{
    // Prima salviamo tutit i frame come immagini, poi chiamiamo ffmpeg.exe per convertirle in video
    QString image_path = program_folder.path()+"/render";
    render_folder = QDir(image_path);
    QString img_name = image_path+"/img";
    if(render_folder.exists()){
        qDebug("folder located");
    }
    else{
        qDebug("folder created");
        render_folder.mkpath(image_path);
    }
    render_folder.setPath(image_path);

    // l'mp4 non passa da ffmpeg: lo encoda l'encoder H.264 di Windows (mfvideo.h), quindi
    // funziona anche su un'installazione che non ha ffmpeg
    if(ui->mp4RadioBtn->isChecked()){
        emit ExportVideoMF(output_file, ui->fpsSpinbox->value());
        return;
    }

    QStringList param_list;
    param_list<<
                "-r"<<QString::number(ui->fpsSpinbox->value())<< // framerate
                "-f"<<"image2"<< // force format, image2 is the image muxer
                "-s"<<QString::number(W)+"x"+QString::number(H)<< //size
                "-i"<<img_name+"_%d.png"; //input location
    // -qscale:v e non -crf: il crf e' un'opzione privata di libx264/libvpx, che un build LGPL
    // non contiene, e sugli encoder nativi qui sotto veniva ignorato con un warning
    if(ui->vmwRadioBtn->isChecked())
        param_list<<"-c:v"<<"msmpeg4"<<"-qscale:v"<<"2"; //codec
    else if(ui->aviRadioButton->isChecked())
       param_list<< "-c:v"<<"mpeg4"<<"-vtag"<<"xvid"<<"-qscale:v"<<"3"; //codec
    // il gif non ha codec da specificare
    // -y: il file d'uscita e' stato scelto con getSaveFileName, quindi esiste gia' - senza
    // questo ffmpeg resta in attesa di una conferma su stdin che nessuno gli dara' mai
    param_list<<"-y"<<output_file;
    emit ExportVideo(param_list);
}

/**
 * @details Asks for an output path with a file filter matching the checked format, then
 * (if confirmed) shows it, enables Export and stores it in output_file.
 */
void FfmpegExport::on_selectFolderBtn_clicked()
{
    QString file_name = "";
    if(ui->mp4RadioBtn->isChecked()){
       file_name = QFileDialog::getSaveFileName(this,tr("Export animation as an mp4 h264 video"),
                           "C:/", "Mp4 video (*.mp4)");
    }
    else if(ui->vmwRadioBtn->isChecked()){
       file_name = QFileDialog::getSaveFileName(this,tr("Export animation as windows media video"),
                           "C:/", "Windows media video (*.wmv)");
    }
    else if(ui->aviRadioButton->isChecked()){
       file_name = QFileDialog::getSaveFileName(this,tr("Export animation as a avi mpeg4-4 video"),
                           "C:/", "avi video (*.avi)");
    }
    else if(ui->gifRadioBtn->isChecked()){
       file_name = QFileDialog::getSaveFileName(this,tr("Export animation as a gif"),
                           "C:/", "gif  (*.gif)");
    }
    if(file_name.length()>0){
        ui->folderLabel->setText(file_name);
        ui->exportVideoBtn->setEnabled(true);
        output_file = file_name;
    }

}

/**
 * @details Truncates output_file back to (and including) the last '.', then appends the
 * new extension, so "clip.mp4" -> "clip.avi". Does nothing if no file has been chosen
 * yet, or if the name has no extension to replace.
 */
void FfmpegExport::ReplaceOutputExtension(const QString &extension)
{
    if(output_file.length()<=1)
        return;
    int dot = output_file.lastIndexOf(".");
    if(dot == -1) // nessuna estensione da sostituire
        return;
    output_file.truncate(dot+1); // tieni tutto fino al punto incluso
    output_file.append(extension);
}

void FfmpegExport::on_aviRadioButton_clicked()
{
    ReplaceOutputExtension("avi");
}

void FfmpegExport::on_mp4RadioBtn_clicked()
{
    ReplaceOutputExtension("mp4");
}

void FfmpegExport::on_vmwRadioBtn_clicked()
{
    ReplaceOutputExtension("wmv");
}

void FfmpegExport::on_gifRadioBtn_clicked()
{
    ReplaceOutputExtension("gif");
}

/**
 * @details One reader for the remembered path, so the dialog's format enablement, the
 * export button and MyMainWindow::ImageRenderOver()'s process launch cannot disagree about
 * whether an ffmpeg is available.
 */
QString ResolveFfmpeg()
{
    return QFile::exists(FFMPEGPath) ? FFMPEGPath : QString();
}

/**
 * @details The advanced half of this dialog: mp4 needs nothing installed (Windows encodes
 * it, mfvideo.h), while wmv, avi and gif are rendered by an ffmpeg the user owns. The path
 * goes through Widget::WriteJson() so it outlives the session, and the radios are
 * re-enabled straight away instead of at the next opening.
 */
void FfmpegExport::LocateFfmpeg()
{
    const QString file_name = QFileDialog::getOpenFileName(this,tr("find FFMPEG executable file"),
                       my_video_export_folder.path(), "ffmpeg(*.exe)");
    if(!QFile::exists(file_name)) // annullato
        return;
    if(QFileInfo(file_name).fileName().compare("ffmpeg.exe",Qt::CaseInsensitive)!=0){
        QMessageBox::warning(this,"FFMPEG location","That file is not an ffmpeg.exe.");
        return;
    }
    FFMPEGPath = file_name;
    my_widget->WriteJson(); // altrimenti il percorso si perde alla chiusura del programma
    RefreshFfmpegAvailability();
}

/**
 * @details Disabling the three radios beats letting the user pick a format whose export
 * would then quietly never start. mp4 is never disabled - it does not go through ffmpeg -
 * and is force-selected when it is the only thing left available.
 */
void FfmpegExport::RefreshFfmpegAvailability()
{
    const QString ffmpeg = ResolveFfmpeg();
    const bool have_ffmpeg = !ffmpeg.isEmpty();
    ui->aviRadioButton->setEnabled(have_ffmpeg);
    ui->vmwRadioBtn->setEnabled(have_ffmpeg);
    ui->gifRadioBtn->setEnabled(have_ffmpeg);
    if(!have_ffmpeg)
        ui->mp4RadioBtn->setChecked(true);
    locate_ffmpeg_btn->setToolTip(have_ffmpeg ? ffmpeg
        : tr("wmv, avi and gif are rendered by ffmpeg, which is not included; mp4 does not need it"));
}

/**
 * @details Non-recursive wipe: lists the plain files in `path` and removes them one by
 * one, leaving any subdirectories in place. Called to clear the scratch render folder
 * before writing a fresh image sequence.
 */
void EmptyDirectory(QString path){
    QDir dir(path);
    dir.setNameFilters(QStringList() << "*.*");
    dir.setFilter(QDir::Files);
    foreach(QString dir_file, dir.entryList())
    {
        dir.remove(dir_file);
    }
}
