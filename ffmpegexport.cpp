#include "ffmpegexport.h"
#include "ui_ffmpegexport.h"

#include <QFileDialog>
#include <QProcess>
extern int W;
extern int H;
extern int FPS;
QString outputFile = "";
extern QString FFMPEGPath;
extern QDir program_folder;
extern QDir render_folder;
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
void FfmpegExport::on_exportVideoBtn_clicked()
{
    // Prima salviamo tutit i frame come immagini, poi chiamiamo ffmpeg.exe per convertirle in video
    QString imagePath = program_folder.path()+"/render";
    render_folder = QDir(imagePath);
    QString imgName = imagePath+"/img";
    if(render_folder.exists()){
        qDebug("folder located");
    }
    else{
        qDebug("folder created");
        render_folder.mkpath(imagePath);
    }
    render_folder.setPath(imagePath);

    QStringList paramList;
    paramList<<
                "-r"<<QString::number(ui->fpsSpinbox->value())<< // framerate
                "-f"<<"image2"<< // force format, image2 is the image muxer
                "-s"<<QString::number(W)+"x"+QString::number(H)<< //size
                "-i"<<imgName+"_%d.png"; //input location
    if(ui->vmwRadioBtn->isChecked())
        paramList<<"-vcodec"<<"msmpeg4"<<"-qscale"<<"2"; //codec
    else if(ui->mp4RadioBtn->isChecked())
        paramList<<"-vcodec"<<"libx264"; //codec
    else if(ui->aviRadioButton->isChecked())
       paramList<< "-vcodec"<<"mpeg4"<<"-vtag"<<"xvid"; //codec
    else if(ui->gifRadioBtn->isChecked())
       {}
    paramList<<"-crf"<<"25"<<// quality
                outputFile;
    emit ExportVideo(paramList);
    /*paramList<<"-i"<<"C:/Users/Argasio/Documents/GitHub/MarionettaPlus/MarionettaPlus/input.mp4"
            <<"C:/Users/Argasio/Documents/GitHub/MarionettaPlus/MarionettaPlus/output.webm";*/
}

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
        outputFile = file_name;
    }

}

void FfmpegExport::on_aviRadioButton_clicked()
{
    if(outputFile.length()>1){
        int extBegin = outputFile.lastIndexOf(".")+1;
        if(extBegin == -1)
            return;
        int lastChar = outputFile.length()-1;
        outputFile.remove(extBegin,extBegin-lastChar);
        outputFile.append("avi");
    }
}

void FfmpegExport::on_mp4RadioBtn_clicked()
{
    if(outputFile.length()>1){
        int extBegin = outputFile.lastIndexOf(".")+1;
        if(extBegin == -1)
            return;
        int lastChar = outputFile.length()-1;
        outputFile.remove(extBegin,extBegin-lastChar);
        outputFile.append("mp4");
    }
}

void FfmpegExport::on_vmwRadioBtn_clicked()
{
    if(outputFile.length()>1){
        int extBegin = outputFile.lastIndexOf(".")+1;
        if(extBegin == -1)
            return;
        int lastChar = outputFile.length()-1;
        outputFile.remove(extBegin,extBegin-lastChar);
        outputFile.append("wmv");
    }
}

void EmptyDirectory(QString path){
    QDir dir(path);
    dir.setNameFilters(QStringList() << "*.*");
    dir.setFilter(QDir::Files);
    foreach(QString dirFile, dir.entryList())
    {
        dir.remove(dirFile);
    }
}

void FfmpegExport::on_gifRadioBtn_clicked()
{
    if(outputFile.length()>1){
        int extBegin = outputFile.lastIndexOf(".")+1;
        if(extBegin == -1)
            return;
        int lastChar = outputFile.length()-1;
        outputFile.remove(extBegin,extBegin-lastChar);
        outputFile.append("gif");
    }
}
