#include "ffmpegexport.h"
#include "ui_ffmpegexport.h"

#include <QFileDialog>
#include <QProcess>
extern qint32 W;
extern qint32 H;
extern int FPS;
QString output_file = ""; ///< chosen output video path; kept in sync with the selected format
extern QString FFMPEGPath;
extern QDir program_folder;
extern QDir render_folder;
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

    QStringList param_list;
    param_list<<
                "-r"<<QString::number(ui->fpsSpinbox->value())<< // framerate
                "-f"<<"image2"<< // force format, image2 is the image muxer
                "-s"<<QString::number(W)+"x"+QString::number(H)<< //size
                "-i"<<img_name+"_%d.png"; //input location
    if(ui->vmwRadioBtn->isChecked())
        param_list<<"-vcodec"<<"msmpeg4"<<"-qscale"<<"2"; //codec
    else if(ui->mp4RadioBtn->isChecked())
        param_list<<"-vcodec"<<"libx264"; //codec
    else if(ui->aviRadioButton->isChecked())
       param_list<< "-vcodec"<<"mpeg4"<<"-vtag"<<"xvid"; //codec
    else if(ui->gifRadioBtn->isChecked())
       {}
    param_list<<"-crf"<<"25"<<// quality
                output_file;
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
