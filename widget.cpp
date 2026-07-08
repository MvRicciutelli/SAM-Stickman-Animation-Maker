#include "widget.h"
#include "ui_widget.h"
#include "sleeper.h"
#include <QGraphicsRectItem>
#include <QGraphicsView>
#include <QGraphicsScene>
#include <frame.h>
#include <animation.h>
#include <QPen>
#include <QIcon>
#include <QVariant>
#include <QDebug>
#include <QColorDialog>
#include <QInputDialog>
#include <QThread>
#include <QTimer>
#include <QProcess>
#include <QDir>
#include <QStandardPaths>
#include <QFileDialog>
#include <QDirIterator>
#include <QFileInfo>
#include <QJsonObject>
#include <QJsonDocument>
#include <QFile>
#include <QMessageBox>
#include <QtAlgorithms>
#include <algorithm>
#include "advancedtab.h"
#include "advancedlinewidget.h"
#include "advancedcirclewidget.h"
#include "advancedtaperwidget.h"
#include "uiItems.h"
#include "imageexport.h"
#define CS view->myAnimation->currentFrame->currentStickFigure->currentStick
#define CURRENTSTICKFIGURE view->myAnimation->currentFrame->currentStickFigure
#define CURRENTFRAME view->myAnimation->currentFrame
#define DEFAULTWIDTH 720
#define DEFAULTHEIGHT 480
#define DEFAULTFPS 24
#define DEFAULTAUTOSAVE 50
#define DEFAULTHANDLES 10
//#include "QVideoEncoder.h"
//#include "QVideoDecoder.h"
QString license = "The software package Marionetta is distributed as a free of charge Software,\r\n \
   user must hereby acknowledges accept the following conditions to use Marionetta: \r\n\
-The only official distribution of Marionetta is under the authorship of Matteo Vittorio Ricciutelli \r\n\
-The user confirms he has acquired this software package from either: \r\n\
    https://matteovittorioricc.wixsite.com/marionetta \r\n\
    or \r\n\
    https://sites.google.com/view/marionetta/home \r\n\
-The user will not upload this software package, the user will not share this software package via p2p,ftp or email \r\n\
    and  he will not under any mean redistirbute this software.\r\n\
-To get other people to know Marionetta the user is allowed to share links to official sources such as \r\n\
    https://matteovittorioricc.wixsite.com/marionetta \r\n\
    https://sites.google.com/view/marionetta/home \r\n\
-The user agrees not to modify the software package and its contents in any way.\r\n\
-The user acknowledges Marionetta is released as-is, \r\n\
   and that the author does not assume any resposability on the usage of the software or on the content created using the software.\r\n\
";
int termsAccepted = 0;
QString imageNameBuffer;
QGraphicsScene *scene;
Ui::Widget *myUi;
myView* V;
QList <StickFigure*> layerList;
QTextEdit * stickFigureNameText;
QGraphicsRectItem *myRect;
QGraphicsRectItem *limitRect;
QPointF onionOffset;
QDir programFolder;
QDir libFolder;
QDir tempRenderFolder;
QDir myAnimationsFolder;
QDir myStickFiguresFolder;
QDir myVideoExportFolder;
// -----------------
QSpinBox* fpsSpinBox;
QSlider* imgRotationSlider;
QSpinBox * onionSkinSB;
QSpinBox * imgHOffsetSpinbox;
QSpinBox * imgVOffsetSpinbox;
QSpinBox * imgWidthSpinbox;
QSpinBox * imgHeightSpinbox;
QSpinBox * imgRotationSpinbox;
QSpinBox * penOpacitySpinbox;
QSpinBox * brushOpacitySpinbox;
QSpinBox* lineEditThicknessWidthSpinbox;
QDoubleSpinBox * elongateSpinbox;
QSlider * penOpacitySlider;
QSlider * brushOpacitySlider;
QSlider * depthSlider;
QSpinBox *stickFigureScaleSpinbox;
QSpinBox *stickFigureRotationSpinbox;
QDoubleSpinBox * depthSpinbox;
QListWidget * myStickFigureWidgetList;
QListWidget * myFrameWidgetList;
QListWidget * myLibraryListWidget ;
QListWidget * myCurrentLibraryWidget;
QListWidget * imgListWidget;
QLabel * dbgLbl;
QSlider* imgHOffsetSlider;
QSlider* imgVOffsetSlider;
QSlider* imgWidthSlider;
QSlider* imgHeightSlider;
QSlider* taperWSlider;
QSlider* advancedRectSlider;
QSlider* editLineThicknessSlider;
QSlider* taperHSlider;
QCheckBox* hardTopCheck;
QCheckBox* hardBottomCheck;
advancedTab* advancedRectTab;
QTabWidget* myTabWidget;
QWidget * advancedImgTab;
advancedLineWidget *advancedLineTab;
advancedCircleWidget *advancedCircleTab;
advancedTaperWidget * advancedTaperTab;
QSlider*  circleSquashSlider;
QSpinBox * circleSquashSpinbox;
FrameData * frameCopyBuffer = nullptr; ///< buffer copia-frame: solo dati da Phase 2b

//----------------------
int W                   = DEFAULTWIDTH;
int H                   = DEFAULTHEIGHT;
int FPS                 = DEFAULTFPS;
int HANDLESIZE          = DEFAULTHANDLES;
int autoSaveInterval    = DEFAULTAUTOSAVE;
struct{
    bool sliderHOffset  = false;
    bool sliderVOffset  = false;
    bool sliderWScale   = false;
    bool sliderHScale   = false;
    bool sliderRot      = false;
}sliderFlags;
bool playBack               = false;
float zoomLvl               = 1;
bool  firstTimeOpened       = false;
QImage * imageDrawBuffer;
QString FFMPEGPath = "";
Widget::Widget(QWidget *parent)
    : QWidget(parent)
    , ui(new Ui::Widget)
{
    ui->setupUi(this);
    myUi = ui;
    //crea la scena virtuale
    advancedImgTab = ui->advancedTab;
    advancedLineTab = new advancedLineWidget();
    advancedCircleTab = new advancedCircleWidget();
    advancedTaperTab = new advancedTaperWidget();
    myTabWidget = ui->toolsTabWidget;
    ui->toolsTabWidget->setCurrentWidget(ui->drawingTab);
    myStickFigureWidgetList = ui->stickLayerView;
    myLibraryListWidget = ui->libraryList;
    myCurrentLibraryWidget = ui->currentLibrary;
    myFrameWidgetList = ui->frameListWidget;
    onionSkinSB = ui->onionSkinSpinBox;
    stickFigureNameText = ui->stickFigureNameText;
    fpsSpinBox = ui->fpsSpinBox;
    imgHOffsetSlider = ui->imgHOffsetSlider;
    imgHOffsetSpinbox = ui->imgHOffsetSpinbox;
    imgVOffsetSlider = ui->imgVOfsetSlider;
    imgVOffsetSpinbox = ui->imgVOffsetSpinbox;
    imgWidthSlider = ui->imgWSlider;
    imgWidthSpinbox = ui->imgWidthSpinbox;
    imgHeightSlider = ui->imgHSlider;
    imgHeightSpinbox = ui->imgHeightSpinbox;
    imgRotationSlider = ui->imgRotationSlider;
    imgRotationSpinbox = ui->imgRotationSpinbox;
    elongateSpinbox = ui->elongateSpinbox;
    penOpacitySlider = ui->penOpacitySlider;
    penOpacitySpinbox = ui->penOpacitySpinbox;
    brushOpacitySlider = ui->fillOpacitySlider;
    brushOpacitySpinbox = ui->fillOpacitySpinbox;
    stickFigureScaleSpinbox = ui->stickFigureScaleSpinbox;
    stickFigureRotationSpinbox = ui->stickFigureRotationSpinbox;
    depthSpinbox = ui->depthSpinbox;
    depthSlider = ui->depthSlider;
    imgListWidget = ui->imgListWidget;
    advancedRectTab = new advancedTab();
   // dbgLbl = ui->debugLabel;
    createPaths();
    detectLibraries();
    // aggiorna il colore del segnacolore
    QColor color(Qt::black);
    if(color.isValid()) {
       QString qss = QString("background-color: %1").arg(color.name());
       ui->currentColorBtn->setStyleSheet(qss);
    }
    QColor colorBrush(Qt::gray);
    if(color.isValid()) {
       QString qss = QString("background-color: %1").arg(colorBrush.name());
       ui->currentBrushColorBtn->setStyleSheet(qss);
    }
    scene = new QGraphicsScene(this);
    //crea il pannello e collegalo alla scena
    view = new myView(this);
    V = view;
    view->setBackgroundBrush(QBrush(QColor(Qt::darkGray)));
    scene->setBackgroundBrush(QBrush(QColor(Qt::white)));
    auto num = scene->items();
    view->setScene(scene);
    view->myAnimation->scene = scene;
    addFrame();
    num = scene->items();
    CURRENTSTICKFIGURE = CURRENTFRAME->stickFigures[0];
    //inserisci il pannello nella finestra
    ui->viewLayout->addWidget(view);
    //json read
    if(!QFile::exists(libFolder.path()+"/defaultLib.marlib")){
        view->saveLibrary(libFolder.path()+"/defaultLib.marlib");
    }
    else{
        view->loadLibrary(libFolder.path()+"/defaultLib.marlib");
    }
    QString jsonConfigPath = programFolder.path()+"/config.json";
    if(!QFile::exists(jsonConfigPath)){
        createJson(jsonConfigPath);
    }
    else{
        readJson(jsonConfigPath);
    }
    if(termsAccepted == 0){
        termsAndConditions();
    }
    if(termsAccepted == 0){
        exit(EXIT_FAILURE);
    }



    //crea la cornice
    myRect = new QGraphicsRectItem(0, 0, W, H);
    myRect->setBrush(QBrush(QColor(Qt::white)));
    limitRect = new QGraphicsRectItem(-myRect->rect().width()/10, -myRect->rect().height()/10, myRect->rect().width()*1.1, myRect->rect().height()*1.1);
    limitRect->setBrush(QBrush(QColor(Qt::white)));
    myRect->setZValue(-100);
    myRect->setPen(Qt::NoPen);
    scene->addItem(myRect);
    scene->setSceneRect(limitRect->rect());
    num = scene->items();
    // adjust image editing offset sliders according to max size of scene
    imgVOffsetSlider->setRange(-myRect->rect().height(), myRect->rect().height());
    imgVOffsetSpinbox->setRange(-myRect->rect().height(), myRect->rect().height());
    imgHOffsetSlider->setRange(-myRect->rect().width(), myRect->rect().width());
    imgHOffsetSpinbox->setRange(-myRect->rect().width(), myRect->rect().width());
    elongateSpinbox->setRange(0,W);
    // set onion rendered image skin offset to be displayed properly on the scene
    onionOffset = QPointF(-myRect->rect().width()/10,-myRect->rect().height()/10);
    //check if default library exists

    myLibraryListWidget->clear();
    detectLibraries();
    myLibraryListWidget->setCurrentRow(0);
    frameCopyBuffer = new FrameData();
}

//leggi il file json, se tutti i campi sono validi,leggilo, senno creane uno nuovo
void Widget::readJson(QString path ){

    QFile file(path);
    if(!file.open(QIODevice::ReadOnly | QIODevice::Text))
        return;
    QJsonDocument doc = QJsonDocument::fromJson(file.readAll());
    QJsonObject recordObject = doc.object();
    if( recordObject.value(QString("Width"))                == QJsonValue::Undefined||
        recordObject.value(QString("Height"))               == QJsonValue::Undefined||
        recordObject.value(QString("Fps"))                  == QJsonValue::Undefined||
        recordObject.value(QString("First time opened"))    == QJsonValue::Undefined||
        recordObject.value(QString("FFMPEG path"))          == QJsonValue::Undefined||
        recordObject.value(QString("Autosave operation interval"))  == QJsonValue::Undefined||
        recordObject.value(QString("Handles size"))                 == QJsonValue::Undefined||
            recordObject.value(QString("Terms Accepted"))                 == QJsonValue::Undefined
       )
    {
        file.remove();
        createJson(path);
    }

    W           = recordObject.value(QString("Width")).toInt();
    H           = recordObject.value(QString("Height")).toInt();
    FPS         = recordObject.value(QString("Fps")).toInt();
    firstTimeOpened     = recordObject.value(QString("First time opened")).toInt();
    FFMPEGPath          = recordObject.value("FFMPEG path").toString();
    autoSaveInterval    = recordObject.value(QString("Autosave operation interval")).toInt();
    HANDLESIZE          = recordObject.value(QString("Handles size")).toInt();
    termsAccepted       = recordObject.value(QString("Terms Accepted")).toInt();
    file.close();

}
void Widget::termsAndConditions(){
    QMessageBox* myBox =  new QMessageBox(QMessageBox::Question, "Terms And Conditions",
license+" Click on the \"Yes\" button if you accept the above terms, click on the \"No\" button if you don't agree (the program will be closed). \r\n\
                                  ",
                                  QMessageBox::Yes|QMessageBox::No);

    myBox->setWindowFlag(Qt::FramelessWindowHint);
    if (myBox->exec() == QMessageBox::Yes) {
       termsAccepted = 1;       //cicciottaEasterEgg!!!!
       QMessageBox::information(this,"info"," you can review the terms accepted in the help menu",QMessageBox::Ok);
    } else {
        termsAccepted = 0;
    }
    delete myBox;
    writeJson();
}
void Widget::writeJson( ){
    QString path = programFolder.path()+"/config.json";
    createJson(path);
}
extern int opCount;
void Widget::createJson(QString path ){
    QJsonObject recordObject;
    recordObject.insert("First time opened", QJsonValue::fromVariant(true));
    recordObject.insert("Width", QJsonValue::fromVariant(W));
    recordObject.insert("Height", QJsonValue::fromVariant(H));
    recordObject.insert("Fps", QJsonValue::fromVariant(FPS));
    recordObject.insert("FFMPEG Located", QJsonValue::fromVariant(false));
    recordObject.insert("FFMPEG path", QJsonValue::fromVariant(FFMPEGPath));
    recordObject.insert("Autosave operation interval", QJsonValue::fromVariant(autoSaveInterval));
    recordObject.insert("Handles size", QJsonValue::fromVariant(HANDLESIZE));
    recordObject.insert("Terms Accepted", QJsonValue::fromVariant((int)termsAccepted));
    QJsonDocument doc(recordObject);
    QFile file(path);
    if(!file.open(QIODevice::WriteOnly))
        return;
    file.write(doc.toJson());
    file.close();
}
void Widget::createPaths(){
    QString  path = QStandardPaths::writableLocation(QStandardPaths::DocumentsLocation);
    programFolder = QDir(path+"/MarionettaPlus");
    if(programFolder.exists()){
        qDebug("folder located");
    }
    else{
        qDebug("folder created");
        programFolder.mkpath(path+"/MarionettaPlus");
    }
    libFolder = QDir(path+"/MarionettaPlus/Libraries");
    if(!libFolder.exists()){
        qDebug("library folder created");
        libFolder.mkpath(path+"/MarionettaPlus/Libraries");
    }
    tempRenderFolder = QDir(path+"/MarionettaPlus/temporaryRender");
    if(!tempRenderFolder.exists()){
        qDebug("temporary render folder created");
        tempRenderFolder.mkpath(path+"/MarionettaPlus/temporaryRender");
    }
    myAnimationsFolder = QDir(path+"/MarionettaPlus/myAnimations");
    if(!myAnimationsFolder.exists()){
        qDebug("myAnimation folder created");
        myAnimationsFolder.mkpath(path+"/MarionettaPlus/myAnimations");
    }
    myStickFiguresFolder = QDir(path+"/MarionettaPlus/myStickFigures");
    if(!myStickFiguresFolder.exists()){
        qDebug("Stickfigure folder created");
        myStickFiguresFolder.mkpath(path+"/MarionettaPlus/myStickFigures");
    }

    myVideoExportFolder = QDir(path+"/MarionettaPlus/myExportedVideos");
    if(!myVideoExportFolder.exists()){
        qDebug("Stickfigure folder created");
        myVideoExportFolder.mkpath(path+"/MarionettaPlus/myExportedVideos");
    }
}
void Widget::detectLibraries(){
    QDirIterator dirIt(libFolder.path(),QDir::AllEntries);
    while (dirIt.hasNext()) {
        dirIt.next();
        if (QFileInfo(dirIt.filePath()).isFile()){
            if (QFileInfo(dirIt.filePath()).suffix() == "marlib"){
                QListWidgetItem * item = new QListWidgetItem(myLibraryListWidget);
                QVariant newData(QVariant::fromValue(dirIt.filePath()));
                item->setData(Qt::UserRole,newData);
                item->setData(Qt::DisplayRole,dirIt.fileName());
                myLibraryListWidget->addItem(item);
            }
        }
    }
}
extern bool undoFlag;
Widget::~Widget()
{
    undoFlag = true;
    delete view;
    // setParent(nullptr) prima della delete: se il tab è attualmente inserito in myTabWidget,
    // lo scollega evitando che venga cancellato due volte (una qui, una dalla cascata di Qt);
    // se non è mai stato selezionato in questa sessione non ha comunque un parent, quindi è un no-op sicuro
    advancedLineTab->setParent(nullptr);
    delete advancedLineTab;
    advancedCircleTab->setParent(nullptr);
    delete advancedCircleTab;
    advancedTaperTab->setParent(nullptr);
    delete advancedTaperTab;
    advancedRectTab->setParent(nullptr);
    delete advancedRectTab;
    delete ui;
}


void Widget::on_drawBtn_clicked()
{
    view->setTool(DRAW);
}

void Widget::on_cursor_clicked()
{
    view->setTool(NOTOOL);
}

void Widget::on_addStickBtn_clicked()
{
    view->storeUndo();
    addStick();
}

StickFigure* Widget::addStick(){
    StickFigure * added = CURRENTFRAME->addStickFigure(ui->stickLayerView, stickFigureNameText->toPlainText());
    stickFigureNameText->clear();
    return added;
}
void Widget::on_thicknessSpinBox_valueChanged(int arg1)
{
    view->myPen.setWidth(arg1);
    ui->lineThicknessSlider->setValue(arg1);
}

void Widget::on_stickLayerView_currentItemChanged(QListWidgetItem *current, QListWidgetItem *previous)
{
    if(current != nullptr)
    {
        qDebug("layer clicked");
        QVariant  retrievedData = (current->data(Qt::UserRole));
        StickFigure* cs = qvariant_cast<StickFigure*>(retrievedData);
        view->myAnimation->updateSelection(cs);

    }

}

void Widget::on_deleteStickFigureBtn_clicked()
{
    view->deleteStickFigure();
}

void Widget::on_moveLayerUpBtn_clicked()
{
    // increment negativo = verso la riga 0 = verso l'alto nella lista: prima era invertito
    // rispetto alla freccia dell'icona (spostava il layer verso il basso)
    view->moveStickFigureZ(-1, MODE_UP);
}

void Widget::on_moveLayerDownBtn_clicked()
{
    // increment positivo = verso l'ultima riga = verso il basso nella lista: prima era invertito
    // rispetto alla freccia dell'icona (spostava il layer verso l'alto)
    view->moveStickFigureZ(1, MODE_DOWN);
}

void Widget::on_drawCircleBtn_clicked()
{
    view->setTool(DRAWCIRCLE);
}

void Widget::on_elongateBtn_clicked()
{
    view->setTool(ELONGATE);
}

void Widget::on_undoBtn_clicked()
{
    view->undoRedo(MODE_UNDO);
}

void Widget::on_redoBtn_clicked()
{
    view->undoRedo(MODE_REDO);
}
void Widget::on_addFrameBtn_clicked()
{
    addFrame();
}
void Widget::addFrame(void)
{
    view->storeUndo(CMD_ADDFRAME);
    int pos = 0;
    if(view->myAnimation->frameList.count()>=1)
    {
        pos = CURRENTFRAME->frameNumber+1;
    }
    Frame* addedFrame = setUpFrame(pos);
    view->moveToFrame(addedFrame);

    auto num = scene->items();
    addStick();
    num = scene->items();
    //view->myAnimation->storeUndo();
}

// questa funzione crea il frame e gli oggetti lista appropriati
Frame* Widget::setUpFrame(int pos){
    return view->setUpFrame(pos);

}
void Widget::on_copyFrame_clicked()
{
    view->storeUndo(CMD_ADDFRAME);
    // snapshot dati del frame corrente (Phase 2b): niente stream né switchFrameFlag
    FrameData *snapshot = ExtractFrameData(*CURRENTFRAME);
    // prepare new frame
    int pos = 0;
    if(view->myAnimation->frameList.count()>=1)
    {
        pos = CURRENTFRAME->frameNumber+1;
    }
    Frame* addedFrame = setUpFrame(pos);
    PopulateLiveFrame(addedFrame,*snapshot,true,true);
    delete snapshot;
    // update frame name
    addedFrame->frameNumber++;
    // move to new frame
    view->moveToFrame(addedFrame);
    // update list icons
    view->updateFrameOrder(CURRENTFRAME);

}

void Widget::on_onionSkinSpinBox_valueChanged(const QString &arg1)
{
    view->updateOnionSkins();
}

void Widget::on_deleteFrameBtn_clicked()
{
    if(view->myAnimation->frameList.count()>1){
        view->storeUndo(CMD_DELETEFRAME);
        view->deleteFrame(CURRENTFRAME);
        view->moveToFrame(CURRENTFRAME);
    }
}

void Widget::on_frameListWidget_itemClicked(QListWidgetItem *item)
{
    view->storeUndo(CMD_MOVETOFRAME);
    QVariant  retrievedData = (item->data(Qt::UserRole));
    Frame* cs = qvariant_cast<Frame*>(retrievedData);

    view->moveToFrame(cs);
}

void Widget::on_stickLayerView_itemDoubleClicked(QListWidgetItem *item)
{
    QString text = QInputDialog::getText(this,"rename layer","text");
    QListWidgetItem *myItem = myStickFigureWidgetList->item((myStickFigureWidgetList->currentRow()));
    myItem->setData(Qt::DisplayRole, text);
    QVariant  retrievedData = (item->data(Qt::UserRole));
    StickFigure* S = qvariant_cast<StickFigure*>(retrievedData);
    S->name = text;
}
bool busyPlay = false;
mySleeper *play;
void Widget::on_PlayButton_clicked()
{
    if(busyPlay == false){
        //parti dal frame 0, scegli il framerate
        view->moveToFrame(view->myAnimation->frameList[0]);
        int fps = fpsSpinBox->value();
        int onions = onionSkinSB->value();
        // non voglio onionskins in questa modalità
        onionSkinSB->setValue(0);
        view->moveToFrame(view->myAnimation->frameList[0]);

        // calcola il delay da usare nel timer di passaggio da un frame al segunte
        int delay = 1000/fps;
        if(delay>5)
            delay = delay/2;
        busyPlay = true;
        play = new mySleeper(view,delay);
        onionSkinSB->setValue(onions);
    }else{
        play->stop();
    }
    //view->moveToFrame(view->myAnimation->frameList[0]);

}

void Widget::on_zoomInBtn_clicked()
{
    view->scale(1/zoomLvl,1/zoomLvl);
    zoomLvl*=2;
    view->scale(zoomLvl,zoomLvl);
    view->moveToFrame(CURRENTFRAME);
}

void Widget::on_zoomOutBtn_clicked()
{

    view->scale(1/zoomLvl,1/zoomLvl);
    zoomLvl/=2;
    view->scale(zoomLvl,zoomLvl);
    view->moveToFrame(CURRENTFRAME);
}

void Widget::on_resetZoomBtn_clicked()
{
    view->scale(1/zoomLvl,1/zoomLvl);
    zoomLvl = 1.0;
    view->moveToFrame(CURRENTFRAME);
}

void Widget::on_drawImageBtn_clicked()
{
    QString fileName = QFileDialog::getOpenFileName(this,tr("Load Image"),
                       "C:/", tr("Images (*.png *.bmp *.jpg)"));
    if(QFile::exists(fileName)){
        //imageDrawBuffer = new QImage("C:/Users/riccim3/Pictures/immagine.jpg" );
        imageDrawBuffer = new QImage(fileName );
        imageNameBuffer = QFileInfo(QFile(fileName)).fileName();
        view->setTool(DRAWIMG);
    }

}
void Widget::on_addImgBtn_clicked()
{
    stick* cs = CS;
    QString fileName = QFileDialog::getOpenFileName(this,tr("Load Image"),
                       "C:/", tr("Images (*.png *.bmp *.jpg)"));
    if(fileName.length()>0){
        //imageDrawBuffer = new QImage("C:/Users/riccim3/Pictures/immagine.jpg" );
        QImage* myImg = new QImage(fileName);
        view->storeUndo();
        cs->addImage(myImg,
                                                                                    QFileInfo(QFile(fileName)).fileName());
    }
}

void Widget::on_removeImgBtn_clicked()
{
    if(CURRENTSTICKFIGURE== nullptr)
        return;
    if(CS != nullptr){
        if(CS->stickType == stick::IMAGE && CS->stickImgList.length()>1){
            int idx = imgListWidget->currentRow(); // indice immagine correntmente selezionata
            view->storeUndo();
            CS->removeImgFromList(idx);
            CURRENTSTICKFIGURE->refresh(0);
        }
    }
}

// imposta l'immagine contenuta nello stick dalla lista delle immagini
void Widget::on_setImgBtn_clicked()
{
    int idx = imgListWidget->currentRow(); // indice immagine correntmente selezionata
    if(CS != nullptr){
        if(CS->stickType == stick::IMAGE){
            view->storeUndo();
            CS->stickImg = CS->stickImgList[idx];
            CURRENTSTICKFIGURE->refresh(0);
        }
    }
}

void Widget::on_imgHOffsetSlider_sliderReleased()
{
    sliderFlags.sliderHOffset = false;
}

void Widget::on_imgVOfsetSlider_sliderReleased()
{
    sliderFlags.sliderVOffset = false;
}

void Widget::on_imgWSlider_sliderReleased()
{
    sliderFlags.sliderWScale = false;
}

void Widget::on_imgHSlider_sliderReleased()
{
    sliderFlags.sliderHScale = false;
}

void Widget::on_imgRotationSlider_sliderReleased()
{
    sliderFlags.sliderRot = false;
}



void Widget::on_saveLibraryBtn_clicked()
{
    QString fileName = QFileDialog::getSaveFileName(this,tr("save marionetta library"),
                       libFolder.path(),"Marionetta Library(*.marlib)");
    if(fileName.length()>0){
        view->saveLibrary(fileName);
        myLibraryListWidget->clear();
        detectLibraries();
    }
}



void Widget::on_addItemToLibBtn_clicked()
{
    view->addStickFigureToLibrary();
}

void Widget::on_loadLibraryBtn_clicked()
{
    QString fileName = QFileDialog::getOpenFileName(this,tr("load marionetta library"),
                       libFolder.path(),"Marionetta Library(*.marlib)");
    if(QFile::exists(fileName)){
        view->loadLibrary(fileName);
    }
}

void Widget::on_refreshLibrariesBtn_clicked()
{
    myLibraryListWidget->clear();
    detectLibraries();
}

void Widget::on_libraryList_currentRowChanged(int currentRow)
{
    QString fileName = "";
    int idx = myLibraryListWidget->currentRow();
    if(idx>=0 && idx<myLibraryListWidget->count()){
        QVariant buffer;
        buffer = myLibraryListWidget->item(idx)->data(Qt::UserRole);
        fileName = qvariant_cast<QString>(buffer);
        if(QFile::exists(fileName)){
            view->loadLibrary(fileName,0);
        }
    }

}

void Widget::on_removeFromLibraryBtn_clicked()
{
    view->removeFromLibrary();
}

void Widget::on_addItemFromLibraryToSceneBtn_clicked()
{
    // get selected item
    int idx = myCurrentLibraryWidget->currentRow();
    // niente selezionato: controllato PRIMA di addStick(), altrimenti un click a vuoto aggiungeva
    // comunque un layer vuoto indesiderato oltre a rischiare item(-1)->isSelected() in crash
    if(idx<0 || idx>=myCurrentLibraryWidget->count() || !myCurrentLibraryWidget->item(idx)->isSelected()){
        return;
    }
    StickFigure * added = addStick();
    QListWidgetItem *itemToAdd = myCurrentLibraryWidget->item(idx);
    // estrai il dato utente salvato in esso: da Phase 2b la libreria contiene record dati
    QVariant  retrievedData = (itemToAdd->data(Qt::UserRole));
    StickFigureData* toAdd = qvariant_cast<StickFigureData*>(retrievedData);
    PopulateLiveStickFigure(added,*toAdd,true,true);
    for(stick*s:added->stickList){
        CURRENTFRAME->totalSticks.append(s);
    }
    view->addHandles(added);
    added->linkedItem->setData(Qt::DisplayRole,added->name);
    view->myAnimation->updateSelection(added);
    myStickFigureWidgetList->setCurrentRow(myStickFigureWidgetList->row(added->linkedItem));
    added->refresh();
    //added->refresh();
    //scene->update();
    //CURRENTSTICKFIGURE->loadStickFigure(fileName)
}

void Widget::on_saveCurrentLibraryBtn_clicked()
{
    QString fileName = "";
    int idx = myLibraryListWidget->currentRow();
    if(idx>=0 && idx<myLibraryListWidget->count()){
        QVariant buffer;
        QListWidgetItem *item;
        item = myLibraryListWidget->item(idx);
        buffer = item->data(Qt::UserRole);
        fileName = qvariant_cast<QString>(buffer);
        if(QFile::exists(fileName)){
            view->saveLibrary(fileName);
            myLibraryListWidget->clear();
            detectLibraries();
        }
    }
}

void Widget::on_imgHOffsetSlider_valueChanged(int value)
{
    stick*cs =  CS;
    if(cs == nullptr)
        return;
    else{
        if(cs->stickType == stick::IMAGE){
            if(!sliderFlags.sliderHOffset){
                view->storeUndo(CMD_SIMPLE);
                sliderFlags.sliderHOffset = true;
            }
            float val = 0;
            val = (float)imgHOffsetSlider->value();
            imgHOffsetSpinbox->setValue(imgHOffsetSlider->value());
            cs->imgOffset.setX(val);
             CURRENTSTICKFIGURE->refresh();
        }
    }
}


void Widget::on_imgVOfsetSlider_valueChanged(int value)
{
    stick*cs =  CS;
    if(cs == nullptr)
        return;
    else{
        if(cs->stickType == stick::IMAGE){
            float val = 0;
            if(!sliderFlags.sliderVOffset){
                view->storeUndo(CMD_SIMPLE);
                sliderFlags.sliderVOffset = true;
            }
            imgVOffsetSpinbox->setValue(imgVOffsetSlider->value());
            val = (float)imgVOffsetSlider->value();
            cs->imgOffset.setY(val);
             CURRENTSTICKFIGURE->refresh();
        }
    }
}

void Widget::on_imgWSlider_valueChanged(int value)
{
    stick*cs =  CS;
    if(cs == nullptr)
        return;
    else{
        if(cs->stickType == stick::IMAGE){
            float scale = 0;
            if(!sliderFlags.sliderWScale){
                view->storeUndo(CMD_SIMPLE);
                sliderFlags.sliderWScale = true;
            }
            imgWidthSlider->value()>0?scale = (float)imgWidthSlider->value()/10+1:scale = 1/((float)(-imgWidthSlider->value())/10+1);
            imgWidthSpinbox->setValue(imgWidthSlider->value());
            cs->imgWScale = scale;
            CURRENTSTICKFIGURE->refresh();
        }
    }
}

void Widget::on_imgHSlider_valueChanged(int value)
{
    stick*cs =  CS;
    if(cs == nullptr)
        return;
    else{
        if(cs->stickType == stick::IMAGE){
            float scale = 0;
            if(!sliderFlags.sliderHScale){
                view->storeUndo(CMD_SIMPLE);
                sliderFlags.sliderHScale = true;
            }
            imgHeightSlider->value()>0?scale = (float)imgHeightSlider->value()/10+1 : scale = 1/((float)(-imgHeightSlider->value())/10+1);
            imgHeightSpinbox->setValue(imgHeightSlider->value());
            cs->imgHScale = scale;
             CURRENTSTICKFIGURE->refresh();
        }
    }
}

void Widget::on_imgRotationSlider_valueChanged(int value)
{
    stick*cs =  CS;
    if(cs == nullptr)
        return;
    else{
        if(cs->stickType == stick::IMAGE){
            if(!sliderFlags.sliderRot){
                view->storeUndo(CMD_SIMPLE);
                sliderFlags.sliderRot = true;
            }
            float val = 0;
            val =imgRotationSlider->value();
            imgRotationSpinbox->setValue(val);
            cs->imgAngleOffset = val;
             CURRENTSTICKFIGURE->refresh();
        }
    }
}

void Widget::on_imgHOffsetSpinbox_valueChanged(int arg1)
{
    imgHOffsetSlider->setValue(imgHOffsetSpinbox->value());
}

void Widget::on_imgVOffsetSpinbox_valueChanged(int arg1)
{
    imgVOffsetSlider->setValue(imgVOffsetSpinbox->value());
}

void Widget::on_imgWidthSpinbox_valueChanged(int arg1)
{
    imgWidthSlider->setValue(imgWidthSpinbox->value());
}

void Widget::on_imgHeightSpinbox_valueChanged(int arg1)
{
    imgHeightSlider->setValue(imgHeightSpinbox->value());
}

void Widget::on_imgRotationSpinbox_valueChanged(int arg1)
{
    imgRotationSlider->setValue(imgRotationSpinbox->value());
}
void Widget::on_chooseColorBtn_clicked()
{
    QColor color = QColorDialog::getColor(Qt::yellow, this );
    view->myPen.setColor(color);
    // aggiorna il colore del segnacolore
    if(color.isValid()) {
       QString qss = QString("background-color: %1").arg(color.name());
       ui->currentColorBtn->setStyleSheet(qss);
    }
}
void Widget::on_setStickColorBtn_clicked()
{
    view->setGraphics(false, ATTRIBUTE_PENCOLOR);
}

void Widget::on_setAllSticksColorBtn_clicked()
{
    view->setGraphics(true,ATTRIBUTE_PENCOLOR);
}

void Widget::on_setPenThicknessBtn_clicked()
{
    view->setGraphics(false, ATTRIBUTE_PENWIDTH);
}

void Widget::on_setPenThicknessAllBtn_clicked()
{
    view->setGraphics(true, ATTRIBUTE_PENWIDTH);
}

void Widget::on_selectBrushColorBtn_clicked()
{
    QColor color = QColorDialog::getColor(Qt::yellow, this );
    color.setAlphaF((float)brushOpacitySlider->value()/100);
    view->myBrush.setColor(color);
    // aggiorna il colore del segnacolore
    if(color.isValid()) {
       QString qss = QString("background-color: %1").arg(color.name());
       ui->currentBrushColorBtn->setStyleSheet(qss);
    }
    //view->setGraphics(true,ATTRIBUTE_PENCOLOR);
}

void Widget::on_setAllBrushColorBtn_clicked()
{
    view->setGraphics(true, ATTRIBUTE_BRUSHCOLOR);
}

void Widget::on_setBrushColorBtn_clicked()
{
    view->setGraphics(false, ATTRIBUTE_BRUSHCOLOR);
}

void Widget::on_manipulateBtn_clicked()
{
    view->setTool(MANIPULATE);
}



void Widget::on_penOpacitySlider_valueChanged(int value)
{
    QColor c = view->myPen.color();
    c.setAlphaF((float)penOpacitySlider->value()/100);
    view->myPen.setColor(c);
    penOpacitySpinbox->setValue(penOpacitySlider->value());
}

void Widget::on_fillOpacitySlider_valueChanged(int value)
{
    QColor c = view->myBrush.color();
    c.setAlphaF((float)brushOpacitySlider->value()/100);
    view->myBrush.setColor(c);
    brushOpacitySpinbox->setValue(brushOpacitySlider->value());
}

void Widget::on_penOpacitySpinbox_valueChanged(int arg1)
{
    penOpacitySlider->setValue(penOpacitySpinbox->value());
}

void Widget::on_fillOpacitySpinbox_valueChanged(int arg1)
{
    brushOpacitySlider->setValue(brushOpacitySpinbox->value());
}


void Widget::on_rotateStickFigureBtn_clicked()
{
    view->setTool(ROTATE);
}

void Widget::on_scaleStickFigureBtn_clicked()
{
    view->setTool(SCALE);
}

extern bool startStickFigureScale;


void Widget::on_stickFigureScaleSpinbox_editingFinished()
{
    view->storeUndo();
    for(stick*s:CURRENTSTICKFIGURE->stickList){
        s->scaleBuffer = s->myLine.length();
        s->widthBuffer = s->Pen.width();
    }
    if(CURRENTSTICKFIGURE != nullptr){
        for(stick* s:CURRENTSTICKFIGURE->stickList){
            s->scale((float)stickFigureScaleSpinbox->value()/100);

        }

    }
    stickFigureScaleSpinbox->setValue(100);
}

void Widget::on_stickFigureRotationSpinbox_editingFinished()
{
    view->storeUndo();
    for(stick*s:CURRENTSTICKFIGURE->stickList){

    }
    if(CURRENTSTICKFIGURE != nullptr){
        for(stick* s:CURRENTSTICKFIGURE->stickList){
            s->angleBuffer2 = s->myLine.angle();
            s->rotate(stickFigureRotationSpinbox->value());
            s->angleBuffer2 = s->myLine.angle();
        }

    }
    stickFigureRotationSpinbox->setValue(0);
}

void Widget::on_moveSceneBtn_clicked()
{
    view->setTool(MOVESCENE);
}

void Widget::on_scaleSceneBtn_clicked()
{
    view->setTool(SCALESCENE);
}

void Widget::on_rotateSceneBtn_clicked()
{
    view->setTool(ROTATESCENE);
}

void Widget::on_verticalFlipBtn_clicked()
{
    if(CURRENTSTICKFIGURE != nullptr){
        for(stick* s:CURRENTSTICKFIGURE->stickList){
            //s->myLine.setAngle(s->myLine.angle()+180);
            s->angleBuffer2 = s->myLine.angle();
            s->rotate((-2*s->myLine.angle()));
            //s->refresh(0);
        }
    }
}

void Widget::on_HorizontalFlipBtn_clicked()
{
    if(CURRENTSTICKFIGURE != nullptr){
        for(stick* s:CURRENTSTICKFIGURE->stickList){
            //s->myLine.setAngle(s->myLine.angle()+180);
            s->angleBuffer2 = s->myLine.angle();
            s->rotate(2*(90-s->myLine.angle()));
            //s->refresh(0);
        }
    }
}

void Widget::on_copyStickFigureBtn_clicked()
{
    if(CURRENTSTICKFIGURE != nullptr){
        // il buffer è puro dato (Phase 2b): liberarlo e riestrarlo è tutto qui,
        // niente copyFlag e niente pulizia manuale degli stick
        delete view->copyStickFigureBuffer;
        view->copyStickFigureBuffer = ExtractStickFigureData(*CURRENTSTICKFIGURE);
    }
}

void Widget::on_pasteStickFigureBtn_clicked()
{
   if(view->copyStickFigureBuffer != nullptr){
       view->storeUndo(CMD_SIMPLE);
       StickFigure* added =addStick();
       PopulateLiveStickFigure(added,*view->copyStickFigureBuffer,true,true);
       added->refresh(0);
       for(stick*s:added->stickList){
           CURRENTFRAME->totalSticks.append(s);

       }
       view->addHandles(added);
   }
   else{
       QMessageBox::about(this,"error","no stickfigure was copied in memory");
   }

}

void Widget::on_joinBtn_clicked()
{
    if(CURRENTSTICKFIGURE== nullptr){
        QMessageBox::about(this,"Error","First you must select a stickfigure");
        return;

    }else{
        if(CURRENTSTICKFIGURE->stickList.count()==0){
            QMessageBox::about(this,"Error","current stickfigure is empty cannot join an empty stickfigure");
            return;
        }else{
            // conditions are met, set the tool
            view->setTool(JOIN);
            QMessageBox::about(this,"Info","select which stickfigure must be joined with the current one");
        }
    }

}

/**
 * @brief Widget::on_splitBtn_clicked callback to action that
 * divides a single stickfigure item into two by severing the joint selected and its children
 */
void Widget::on_splitBtn_clicked()
{

    view->splitStickFigure();
}

void Widget::on_joinStick_clicked()
{
    if(CURRENTSTICKFIGURE == nullptr || CS == nullptr)
        return;
    view->storeUndo();
    QList<stick*> toWeld = CURRENTSTICKFIGURE->weld(CS);
    for(stick*s:toWeld){
        CURRENTFRAME->totalSticks.removeAll(s);
    }
    //CURRENTFRAME->cleanTotalSticks();
}

void Widget::on_splitStick_clicked()
{
    if(CURRENTSTICKFIGURE == nullptr || CS == nullptr)
    {
        return;
    }
    view->storeUndo();
    view->removeHandles(CURRENTSTICKFIGURE);
    stick* added = CURRENTSTICKFIGURE->chop(CS);
    view->addHandles(CURRENTSTICKFIGURE);
    CURRENTFRAME->totalSticks.append(added);
}



void Widget::on_setMasterNodeBtn_clicked()
{
    if(CURRENTSTICKFIGURE == nullptr ||
            CURRENTSTICKFIGURE->stickList.isEmpty()||
            CS == nullptr)
        return;
    view->storeUndo();
    CURRENTSTICKFIGURE->setMaster(CS);
}

void Widget::on_createNewLibraryBtn_clicked()
{
    on_saveCurrentLibraryBtn_clicked();
    QString fileName = QFileDialog::getSaveFileName(this,tr("create a new marionetta library"),
                       libFolder.path(),"Marionetta Library(*.marlib)");
    if(fileName.length()>0){
        view->clearCurrentLib();
        view->saveLibrary(fileName);
        myLibraryListWidget->clear();
        detectLibraries();
    }
    if(QFile::exists(fileName)){
        view->loadLibrary(fileName);

        QListWidgetItem *added = myLibraryListWidget->findItems(QFileInfo(fileName).fileName(),Qt::MatchExactly)[0];
        int row = myLibraryListWidget->row(added);
        myLibraryListWidget->setCurrentRow(row);
    }

}

void Widget::on_setAllDepthBtn_clicked()
{
    if(!CURRENTSTICKFIGURE->stickList.isEmpty()){
        view->storeUndo(CMD_SIMPLE);
        float val = depthSpinbox->value();
        for(stick* s:CURRENTSTICKFIGURE->stickList){
            s->Z = val;
            s->setZValue(val); // mancava: senza setZValue() il riordino non si vedeva a schermo
        }
        CURRENTSTICKFIGURE->baseZ = val;
        // riordina totalSticks in base alla nuova Z: usato da push-to-top/bottom e move-stick-up/down
        // per trovare il vicino corretto, altrimenti restava nell'ordine (ormai sbagliato) precedente
        std::sort(CURRENTFRAME->totalSticks.begin(),CURRENTFRAME->totalSticks.end(),
                  [](stick* a, stick* b){ return a->Z < b->Z; });
        CURRENTFRAME->refresh();
    }
}

void Widget::on_setDepthBtn_clicked()
{
    if(!CURRENTSTICKFIGURE->stickList.isEmpty()
            && CS){
        view->storeUndo(CMD_SIMPLE);
        float val = depthSpinbox->value();
        CS->Z = val;
        CS->setZValue(val); // mancava: senza setZValue() il riordino non si vedeva a schermo
        // riordina totalSticks in base alla nuova Z, per lo stesso motivo di on_setAllDepthBtn_clicked
        std::sort(CURRENTFRAME->totalSticks.begin(),CURRENTFRAME->totalSticks.end(),
                  [](stick* a, stick* b){ return a->Z < b->Z; });
        CURRENTFRAME->refresh();
    }
}

void Widget::on_depthSpinbox_valueChanged(double arg1)
{
    depthSlider->setValue(arg1*100);
}

void Widget::on_depthSlider_valueChanged(int value)
{
    float val = (float)value;
    depthSpinbox->setValue(val/100);
}

void Widget::on_drawRectBtn_clicked()
{
        view->setTool(DRAWRECT);
}

void Widget::on_pushButton_clicked()
{
    static bool yo = false;
    yo = !yo;
    if(yo){
    ui->toolsTabWidget->removeTab(1);
    ui->toolsTabWidget->insertTab(1,advancedImgTab,"rectangle");
    }
    else{
        ui->toolsTabWidget->removeTab(1);
        ui->toolsTabWidget->insertTab(1,ui->advancedTab,"advanced");
    }
}

void Widget::on_drawTaperBtn_clicked()
{
    view->setTool(DRAWTAPER);
}

void Widget::on_drawTrapezoidBtn_clicked()
{
    view->setTool(DRAWTRAPEZOID);
}

void Widget::on_moveStickFigureToCenterBtn_clicked()
{
    if(CURRENTSTICKFIGURE == nullptr)
        return;
    if(CURRENTSTICKFIGURE->stickList.isEmpty())
        return;
    int dx = -CURRENTSTICKFIGURE->masterStick->myLine.p1().x()+W/2;
    int dy = -CURRENTSTICKFIGURE->masterStick->myLine.p1().y()+H/2;
    CURRENTSTICKFIGURE->traslate(dx,dy);
}

void Widget::on_pushToTopBtn_clicked()
{
    if(CURRENTSTICKFIGURE == nullptr)
        return;
    if(CURRENTSTICKFIGURE->stickList.isEmpty() || CS== nullptr)
        return;
    view->storeUndo();
    CURRENTFRAME->totalSticks.move(CURRENTFRAME->totalSticks.indexOf(CS),CURRENTFRAME->totalSticks.length()-1);

    float maxZ = CS->Z;
    for(StickFigure* S:CURRENTFRAME->stickFigures){
        for(stick*s:S->stickList){
            if(s->Z>maxZ){
                maxZ = s->Z;
            }
        }
    }
    if(maxZ>MAXDEPTH)
        maxZ = MAXDEPTH;

    CS->Z = maxZ;
    CS->setZValue(CS->Z);

    CURRENTFRAME->refresh();

}

void Widget::on_pushToButtonBtn_clicked()
{
    if(CURRENTSTICKFIGURE == nullptr)
        return;
    if(CURRENTSTICKFIGURE->stickList.isEmpty() || CS== nullptr)
        return;
    view->storeUndo();
    CURRENTFRAME->totalSticks.move(CURRENTFRAME->totalSticks.indexOf(CS),0);

    float minZ = CS->Z;
    for(StickFigure* S:CURRENTFRAME->stickFigures){
        for(stick*s:S->stickList){
            if(s->Z<minZ){
                minZ = s->Z;
            }
        }
    }
    if(minZ<MINDEPTH)
        minZ = MINDEPTH;

    CS->Z = minZ;
    CS->setZValue(CS->Z);

    CURRENTFRAME->refresh();
}



void Widget::on_elongateSpinbox_editingFinished()
{
    if(CURRENTSTICKFIGURE!= nullptr && CS !=nullptr){
        QLineF lineBuf = CS->myLine;
        lineBuf.setLength(elongateSpinbox->value());
        QPointF coord= lineBuf.p2();

        CURRENTSTICKFIGURE->elongate(coord,CS);
    }

}

void Widget::on_pushStickFigureToTop_clicked()
{
    view->storeUndo();
    view->moveStickFigureZ(0,MODE_TOP);
}

void Widget::on_pushStickFigureToBottom_clicked()
{
    view->storeUndo();
    view->moveStickFigureZ(0,MODE_BOTTOM);
}

void Widget::on_moveStickUp_clicked()
{
    if(CURRENTSTICKFIGURE == nullptr)
        return;
    if(CURRENTSTICKFIGURE->stickList.isEmpty() || CS== nullptr)
        return;
    view->storeUndo();
    int oldIdx = CURRENTFRAME->totalSticks.indexOf(CS);
    int newIdx = CURRENTFRAME->totalSticks.indexOf(CS)+1;
    if(newIdx >= CURRENTFRAME->totalSticks.length())
        return;
    stick* minorBuffer = nullptr;
    //1 scansiona la lista cerca se ci sta uno sopra con Z uguale
    for(stick*s:CURRENTFRAME->totalSticks){
        if(s!=CS && (s->Z==CS->Z && CURRENTFRAME->totalSticks.indexOf(s)>oldIdx))
        {
            if(minorBuffer == nullptr){
                minorBuffer = s;
            }
            else if(CURRENTFRAME->totalSticks.indexOf(s)<CURRENTFRAME->totalSticks.indexOf(minorBuffer)){
                minorBuffer = s;
            }
        }
    }
    if(minorBuffer != nullptr){
        CURRENTFRAME->totalSticks.move(oldIdx,CURRENTFRAME->totalSticks.indexOf(minorBuffer));
        CURRENTFRAME->refresh(0);
        return;
    }
    for(stick*s:CURRENTFRAME->totalSticks){
        if(s!=CS && (s->Z>CS->Z ))
        {
            if(minorBuffer == nullptr){
                minorBuffer = s;
            }
            else if(s->Z <= minorBuffer->Z){
                if(s->Z == minorBuffer->Z && CURRENTFRAME->totalSticks.indexOf(s)<CURRENTFRAME->totalSticks.indexOf(minorBuffer)){
                    minorBuffer = s;
                }
                else if(s->Z<minorBuffer->Z){
                    minorBuffer = s;
                }
            }
        }
    }
    if(minorBuffer !=nullptr){

        CS->Z = minorBuffer->Z;
        if(oldIdx<CURRENTFRAME->totalSticks.indexOf(minorBuffer))
            CURRENTFRAME->totalSticks.move(oldIdx,CURRENTFRAME->totalSticks.indexOf(minorBuffer));
        CURRENTFRAME->refresh(0);
        return;

    }
}

void Widget::on_moveStickDown_clicked()
{
    if(CURRENTSTICKFIGURE == nullptr)
        return;
    if(CURRENTSTICKFIGURE->stickList.isEmpty() || CS== nullptr)
        return;
    view->storeUndo();
    int oldIdx = CURRENTFRAME->totalSticks.indexOf(CS);
    int newIdx = CURRENTFRAME->totalSticks.indexOf(CS)-1;
    if(newIdx < 0)
        return;
    stick* minorBuffer = nullptr;
    //1 scansiona la lista cerca se ci sta uno sotto con Z uguale
    for(stick*s:CURRENTFRAME->totalSticks){
        if(s!=CS && (s->Z==CS->Z && CURRENTFRAME->totalSticks.indexOf(s)<oldIdx))
        {
            if(minorBuffer == nullptr){
                minorBuffer = s;
            }
            else if(CURRENTFRAME->totalSticks.indexOf(s)>CURRENTFRAME->totalSticks.indexOf(minorBuffer)){
                minorBuffer = s;
            }
        }
    }
    if(minorBuffer != nullptr){
        CURRENTFRAME->totalSticks.move(oldIdx,CURRENTFRAME->totalSticks.indexOf(minorBuffer));
        CURRENTFRAME->refresh(0);
        return;
    }
    for(stick*s:CURRENTFRAME->totalSticks){
        if(s!=CS && (s->Z<CS->Z ))
        {
            if(minorBuffer == nullptr){
                minorBuffer = s;
            }
            else if(s->Z >= minorBuffer->Z){
                if(s->Z == minorBuffer->Z && CURRENTFRAME->totalSticks.indexOf(s)>CURRENTFRAME->totalSticks.indexOf(minorBuffer)){
                    minorBuffer = s;
                }
                else if(s->Z>minorBuffer->Z){
                    minorBuffer = s;
                }
            }
        }
    }
    if(minorBuffer !=nullptr){

        CS->Z = minorBuffer->Z;
        if(oldIdx>CURRENTFRAME->totalSticks.indexOf(minorBuffer))
            CURRENTFRAME->totalSticks.move(oldIdx,CURRENTFRAME->totalSticks.indexOf(minorBuffer));
        CURRENTFRAME->refresh(0);
        return;

    }
}

void Widget::on_deleteLibrary_clicked()
{
    QString fileName = "";
    int idx = myLibraryListWidget->currentRow();
    if(idx>=0 && idx<myLibraryListWidget->count()){
        QVariant buffer;
        buffer = myLibraryListWidget->item(idx)->data(Qt::UserRole);
        fileName = qvariant_cast<QString>(buffer);
        if(QFile::exists(fileName)){
            // check if user wants to delete a file
            int ret  = QMessageBox::warning(this, tr("delete File"),
                                            tr("This will delete the file associated to the library .\n"
                                               "Do you want to proceed?"),
                                            QMessageBox::Yes | QMessageBox::No
                                            );
            if(ret == QMessageBox::Yes){
                QFile file (fileName);
                file.remove();
                file.close();
                goto removeLibraryGoto;
            }

        }else{
            removeLibraryGoto:

            myLibraryListWidget->clearSelection();
            QListWidgetItem *item = myLibraryListWidget->takeItem(idx);
            delete  item;
            if(myLibraryListWidget->count()>0){
                myLibraryListWidget->setCurrentRow(0);
            }else{
                myCurrentLibraryWidget->clear();
            }

        }
    }
}
void Widget::on_cpyFrame_clicked()
{
    // il buffer è puro dato (Phase 2b): estrazione diretta, niente stream né flag
    delete frameCopyBuffer;
    frameCopyBuffer = ExtractFrameData(*CURRENTFRAME);
}

void Widget::on_pasteFrame_clicked()
{

    view->storeUndo();
    // add a new stickfigure per every stickfigure in the copy buffer
    for(StickFigureData*S:frameCopyBuffer->stickFigures){
        StickFigure *added = addStick();
        PopulateLiveStickFigure(added,*S,true,true);
        for(stick*s:added->stickList){
            CURRENTFRAME->totalSticks.append(s);
        }
        view->addHandles(added); // mancava: senza questo i manici degli stick incollati non comparivano
        added->linkedItem->setData(Qt::DisplayRole,added->name);
        added->updateIcon(); // mancava: l'icona del layer incollato restava quella vuota di addStick()
        if(frameCopyBuffer->stickFigures.last() == S){
            view->myAnimation->updateSelection(added);
            myStickFigureWidgetList->setCurrentRow(myStickFigureWidgetList->row(added->linkedItem));
        }
    }
    // move to new frame
    CURRENTFRAME->refresh();
    // updateFrameOrder (sotto) NON aggiorna nessuna icona nonostante il commento originale:
    // aggiorna solo stick::order/numOfItems. L'anteprima del frame va aggiornata esplicitamente qui.
    CURRENTFRAME->updateIcon();

    // update list icons
    view->updateFrameOrder(CURRENTFRAME);
}


void Widget::on_lineThicknessSlider_sliderMoved(int position)
{
    ui->thicknessSpinBox->setValue(position);
}

void Widget::on_clampToolBtn_clicked()
{
    view->setTool(CLAMP);
}
