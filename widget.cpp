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
int terms_accepted = 0;
QString image_name_buffer;
QGraphicsScene *scene;
Ui::Widget *my_ui;
MyView* V;
QList <StickFigure*> layer_list;
QTextEdit * stick_figure_name_text;
QGraphicsRectItem *my_rect;
QGraphicsRectItem *limit_rect;
QPointF onion_offset;
QDir program_folder;
QDir lib_folder;
QDir temp_render_folder;
QDir my_animations_folder;
QDir my_stick_figures_folder;
QDir my_video_export_folder;
// -----------------
QSpinBox* fps_spin_box;
QSlider* img_rotation_slider;
QSpinBox * onion_skin_sb;
QSpinBox * img_h_offset_spinbox;
QSpinBox * img_v_offset_spinbox;
QSpinBox * img_width_spinbox;
QSpinBox * img_height_spinbox;
QSpinBox * img_rotation_spinbox;
QSpinBox * pen_opacity_spinbox;
QSpinBox * brush_opacity_spinbox;
QSpinBox* line_edit_thickness_width_spinbox;
QDoubleSpinBox * elongate_spinbox;
QSlider * pen_opacity_slider;
QSlider * brush_opacity_slider;
QSlider * depth_slider;
QSpinBox *stick_figure_scale_spinbox;
QSpinBox *stick_figure_rotation_spinbox;
QDoubleSpinBox * depth_spinbox;
QListWidget * my_stick_figure_widget_list;
QListWidget * my_frame_widget_list;
QListWidget * my_library_list_widget ;
QListWidget * my_current_library_widget;
QListWidget * img_list_widget;
QLabel * dbg_lbl;
QSlider* img_h_offset_slider;
QSlider* img_v_offset_slider;
QSlider* img_width_slider;
QSlider* img_height_slider;
QSlider* taper_w_slider;
QSlider* advanced_rect_slider;
QSlider* edit_line_thickness_slider;
QSlider* taper_h_slider;
QCheckBox* hard_top_check;
QCheckBox* hard_bottom_check;
AdvancedTab* advanced_rect_tab;
QTabWidget* my_tab_widget;
QWidget * advanced_img_tab;
AdvancedLineWidget *advanced_line_tab;
AdvancedCircleWidget *advanced_circle_tab;
AdvancedTaperWidget * advanced_taper_tab;
QSlider*  circle_squash_slider;
QSpinBox * circle_squash_spinbox;
FrameData * frame_copy_buffer = nullptr; ///< buffer copia-frame: solo dati da Phase 2b

//----------------------
qint32 W                   = DEFAULTWIDTH;
qint32 H                   = DEFAULTHEIGHT;
int FPS                 = DEFAULTFPS;
int handle_size          = DEFAULTHANDLES;
int auto_save_interval    = DEFAULTAUTOSAVE;
struct{
    bool sliderHOffset  = false;
    bool sliderVOffset  = false;
    bool sliderWScale   = false;
    bool sliderHScale   = false;
    bool sliderRot      = false;
}sliderFlags;
bool play_back               = false;
float zoom_lvl               = 1;
bool  first_time_opened       = false;
QImage * image_draw_buffer;
QString FFMPEGPath = "";
Widget::Widget(QWidget *parent)
    : QWidget(parent)
    , ui(new Ui::Widget)
{
    ui->setupUi(this);
    my_ui = ui;
    //crea la scena virtuale
    advanced_img_tab = ui->advancedTab;
    advanced_line_tab = new AdvancedLineWidget();
    advanced_circle_tab = new AdvancedCircleWidget();
    advanced_taper_tab = new AdvancedTaperWidget();
    my_tab_widget = ui->toolsTabWidget;
    ui->toolsTabWidget->setCurrentWidget(ui->drawingTab);
    my_stick_figure_widget_list = ui->stickLayerView;
    my_library_list_widget = ui->libraryList;
    my_current_library_widget = ui->currentLibrary;
    my_frame_widget_list = ui->frameListWidget;
    onion_skin_sb = ui->onionSkinSpinBox;
    stick_figure_name_text = ui->stickFigureNameText;
    fps_spin_box = ui->fpsSpinBox;
    img_h_offset_slider = ui->imgHOffsetSlider;
    img_h_offset_spinbox = ui->imgHOffsetSpinbox;
    img_v_offset_slider = ui->imgVOfsetSlider;
    img_v_offset_spinbox = ui->imgVOffsetSpinbox;
    img_width_slider = ui->imgWSlider;
    img_width_spinbox = ui->imgWidthSpinbox;
    img_height_slider = ui->imgHSlider;
    img_height_spinbox = ui->imgHeightSpinbox;
    img_rotation_slider = ui->imgRotationSlider;
    img_rotation_spinbox = ui->imgRotationSpinbox;
    elongate_spinbox = ui->elongateSpinbox;
    pen_opacity_slider = ui->penOpacitySlider;
    pen_opacity_spinbox = ui->penOpacitySpinbox;
    brush_opacity_slider = ui->fillOpacitySlider;
    brush_opacity_spinbox = ui->fillOpacitySpinbox;
    stick_figure_scale_spinbox = ui->stickFigureScaleSpinbox;
    stick_figure_rotation_spinbox = ui->stickFigureRotationSpinbox;
    depth_spinbox = ui->depthSpinbox;
    depth_slider = ui->depthSlider;
    img_list_widget = ui->imgListWidget;
    advanced_rect_tab = new AdvancedTab();
   // dbg_lbl = ui->debugLabel;
    CreatePaths();
    DetectLibraries();
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
    view = new MyView(this);
    V = view;
    view->setBackgroundBrush(QBrush(QColor(Qt::darkGray)));
    scene->setBackgroundBrush(QBrush(QColor(Qt::white)));
    auto num = scene->items();
    view->setScene(scene);
    view->my_animation->scene = scene;
    AddFrame();
    num = scene->items();
    view->my_animation->CurrentFrame()->CurrentStickFigure() = view->my_animation->CurrentFrame()->stick_figures[0];
    //inserisci il pannello nella finestra
    ui->viewLayout->addWidget(view);
    //json read
    if(!QFile::exists(lib_folder.path()+"/defaultLib.marlib")){
        view->SaveLibrary(lib_folder.path()+"/defaultLib.marlib");
    }
    else{
        view->LoadLibrary(lib_folder.path()+"/defaultLib.marlib");
    }
    QString jsonConfigPath = program_folder.path()+"/config.json";
    if(!QFile::exists(jsonConfigPath)){
        CreateJson(jsonConfigPath);
    }
    else{
        ReadJson(jsonConfigPath);
    }
    if(terms_accepted == 0){
        TermsAndConditions();
    }
    if(terms_accepted == 0){
        exit(EXIT_FAILURE);
    }



    //crea la cornice
    my_rect = new QGraphicsRectItem(0, 0, W, H);
    my_rect->setBrush(QBrush(QColor(Qt::white)));
    limit_rect = new QGraphicsRectItem(-my_rect->rect().width()/10, -my_rect->rect().height()/10, my_rect->rect().width()*1.1, my_rect->rect().height()*1.1);
    limit_rect->setBrush(QBrush(QColor(Qt::white)));
    my_rect->setZValue(-100);
    my_rect->setPen(Qt::NoPen);
    scene->addItem(my_rect);
    scene->setSceneRect(limit_rect->rect());
    num = scene->items();
    // adjust image editing offset sliders according to max size of scene
    img_v_offset_slider->setRange(-my_rect->rect().height(), my_rect->rect().height());
    img_v_offset_spinbox->setRange(-my_rect->rect().height(), my_rect->rect().height());
    img_h_offset_slider->setRange(-my_rect->rect().width(), my_rect->rect().width());
    img_h_offset_spinbox->setRange(-my_rect->rect().width(), my_rect->rect().width());
    elongate_spinbox->setRange(0,W);
    // set onion rendered image skin offset to be displayed properly on the scene
    onion_offset = QPointF(-my_rect->rect().width()/10,-my_rect->rect().height()/10);
    //check if default library exists

    my_library_list_widget->clear();
    DetectLibraries();
    my_library_list_widget->setCurrentRow(0);
    frame_copy_buffer = new FrameData();
}

//leggi il file json, se tutti i campi sono validi,leggilo, senno creane uno nuovo
void Widget::ReadJson(QString path ){

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
        CreateJson(path);
    }

    W           = recordObject.value(QString("Width")).toInt();
    H           = recordObject.value(QString("Height")).toInt();
    FPS         = recordObject.value(QString("Fps")).toInt();
    first_time_opened     = recordObject.value(QString("First time opened")).toInt();
    FFMPEGPath          = recordObject.value("FFMPEG path").toString();
    auto_save_interval    = recordObject.value(QString("Autosave operation interval")).toInt();
    handle_size          = recordObject.value(QString("Handles size")).toInt();
    terms_accepted       = recordObject.value(QString("Terms Accepted")).toInt();
    file.close();

}
void Widget::TermsAndConditions(){
    QMessageBox* myBox =  new QMessageBox(QMessageBox::Question, "Terms And Conditions",
license+" Click on the \"Yes\" button if you accept the above terms, click on the \"No\" button if you don't agree (the program will be closed). \r\n\
                                  ",
                                  QMessageBox::Yes|QMessageBox::No);

    myBox->setWindowFlag(Qt::FramelessWindowHint);
    if (myBox->exec() == QMessageBox::Yes) {
       terms_accepted = 1;       //cicciottaEasterEgg!!!!
       QMessageBox::information(this,"info"," you can review the terms accepted in the help menu",QMessageBox::Ok);
    } else {
        terms_accepted = 0;
    }
    delete myBox;
    WriteJson();
}
void Widget::WriteJson( ){
    QString path = program_folder.path()+"/config.json";
    CreateJson(path);
}
extern int op_count;
void Widget::CreateJson(QString path ){
    QJsonObject recordObject;
    recordObject.insert("First time opened", QJsonValue::fromVariant(true));
    recordObject.insert("Width", QJsonValue::fromVariant(W));
    recordObject.insert("Height", QJsonValue::fromVariant(H));
    recordObject.insert("Fps", QJsonValue::fromVariant(FPS));
    recordObject.insert("FFMPEG Located", QJsonValue::fromVariant(false));
    recordObject.insert("FFMPEG path", QJsonValue::fromVariant(FFMPEGPath));
    recordObject.insert("Autosave operation interval", QJsonValue::fromVariant(auto_save_interval));
    recordObject.insert("Handles size", QJsonValue::fromVariant(handle_size));
    recordObject.insert("Terms Accepted", QJsonValue::fromVariant((int)terms_accepted));
    QJsonDocument doc(recordObject);
    QFile file(path);
    if(!file.open(QIODevice::WriteOnly))
        return;
    file.write(doc.toJson());
    file.close();
}
void Widget::CreatePaths(){
    QString  path = QStandardPaths::writableLocation(QStandardPaths::DocumentsLocation);
    program_folder = QDir(path+"/MarionettaPlus");
    if(program_folder.exists()){
        qDebug("folder located");
    }
    else{
        qDebug("folder created");
        program_folder.mkpath(path+"/MarionettaPlus");
    }
    lib_folder = QDir(path+"/MarionettaPlus/Libraries");
    if(!lib_folder.exists()){
        qDebug("library folder created");
        lib_folder.mkpath(path+"/MarionettaPlus/Libraries");
    }
    temp_render_folder = QDir(path+"/MarionettaPlus/temporaryRender");
    if(!temp_render_folder.exists()){
        qDebug("temporary render folder created");
        temp_render_folder.mkpath(path+"/MarionettaPlus/temporaryRender");
    }
    my_animations_folder = QDir(path+"/MarionettaPlus/myAnimations");
    if(!my_animations_folder.exists()){
        qDebug("my_animation folder created");
        my_animations_folder.mkpath(path+"/MarionettaPlus/myAnimations");
    }
    my_stick_figures_folder = QDir(path+"/MarionettaPlus/myStickFigures");
    if(!my_stick_figures_folder.exists()){
        qDebug("Stickfigure folder created");
        my_stick_figures_folder.mkpath(path+"/MarionettaPlus/myStickFigures");
    }

    my_video_export_folder = QDir(path+"/MarionettaPlus/myExportedVideos");
    if(!my_video_export_folder.exists()){
        qDebug("Stickfigure folder created");
        my_video_export_folder.mkpath(path+"/MarionettaPlus/myExportedVideos");
    }
}
void Widget::DetectLibraries(){
    QDirIterator dirIt(lib_folder.path(),QDir::AllEntries);
    while (dirIt.hasNext()) {
        dirIt.next();
        if (QFileInfo(dirIt.filePath()).isFile()){
            if (QFileInfo(dirIt.filePath()).suffix() == "marlib"){
                QListWidgetItem * item = new QListWidgetItem(my_library_list_widget);
                QVariant newData(QVariant::fromValue(dirIt.filePath()));
                item->setData(Qt::UserRole,newData);
                item->setData(Qt::DisplayRole,dirIt.fileName());
                my_library_list_widget->addItem(item);
            }
        }
    }
}
/**
 * @details undoFlag (removed in Phase 4) used to be raised here before `delete view`.
 * It's no longer needed: `~StickFigure()`'s cleanup (scene removal, img_list_widget
 * entries, the sticks themselves) is safe to run unconditionally now, and every
 * object it touches — `scene`, `img_list_widget` (owned by `ui`) — is still alive at
 * this point, since `delete ui` runs after `delete view` below.
 */
Widget::~Widget()
{
    delete view;
    // setParent(nullptr) prima della delete: se il tab è attualmente inserito in my_tab_widget,
    // lo scollega evitando che venga cancellato due volte (una qui, una dalla cascata di Qt);
    // se non è mai stato selezionato in questa sessione non ha comunque un parent, quindi è un no-op sicuro
    advanced_line_tab->setParent(nullptr);
    delete advanced_line_tab;
    advanced_circle_tab->setParent(nullptr);
    delete advanced_circle_tab;
    advanced_taper_tab->setParent(nullptr);
    delete advanced_taper_tab;
    advanced_rect_tab->setParent(nullptr);
    delete advanced_rect_tab;
    delete ui;
}


void Widget::on_drawBtn_clicked()
{
    view->SetTool(DRAW);
}

void Widget::on_cursor_clicked()
{
    view->SetTool(NOTOOL);
}

void Widget::on_addStickBtn_clicked()
{
    view->StoreUndo();
    AddStick();
}

StickFigure* Widget::AddStick(){
    StickFigure * added = view->my_animation->CurrentFrame()->AddStickFigure(ui->stickLayerView, stick_figure_name_text->toPlainText());
    stick_figure_name_text->clear();
    return added;
}
void Widget::on_thicknessSpinBox_valueChanged(int arg1)
{
    view->my_pen.setWidth(arg1);
    ui->lineThicknessSlider->setValue(arg1);
}

void Widget::on_stickLayerView_currentItemChanged(QListWidgetItem *current, QListWidgetItem *previous)
{
    if(current != nullptr)
    {
        qDebug("layer clicked");
        QVariant  retrievedData = (current->data(Qt::UserRole));
        StickFigure* cs = qvariant_cast<StickFigure*>(retrievedData);
        view->my_animation->UpdateSelection(cs);

    }

}

void Widget::on_deleteStickFigureBtn_clicked()
{
    view->DeleteStickFigure();
}

void Widget::on_moveLayerUpBtn_clicked()
{
    // increment negativo = verso la riga 0 = verso l'alto nella lista: prima era invertito
    // rispetto alla freccia dell'icona (spostava il layer verso il basso)
    view->MoveStickFigureZ(-1, MODE_UP);
}

void Widget::on_moveLayerDownBtn_clicked()
{
    // increment positivo = verso l'ultima riga = verso il basso nella lista: prima era invertito
    // rispetto alla freccia dell'icona (spostava il layer verso l'alto)
    view->MoveStickFigureZ(1, MODE_DOWN);
}

void Widget::on_drawCircleBtn_clicked()
{
    view->SetTool(DRAWCIRCLE);
}

void Widget::on_elongateBtn_clicked()
{
    view->SetTool(ELONGATE);
}

void Widget::on_undoBtn_clicked()
{
    view->undo_stack.undo();
}

void Widget::on_redoBtn_clicked()
{
    view->undo_stack.redo();
}
void Widget::on_addFrameBtn_clicked()
{
    AddFrame();
}
void Widget::AddFrame(void)
{
    view->StoreUndo(CMD_ADDFRAME);
    int pos = 0;
    if(view->my_animation->frame_list.count()>=1)
    {
        pos = view->my_animation->CurrentFrame()->frame_number+1;
    }
    Frame* addedFrame = SetUpFrame(pos);
    view->MoveToFrame(addedFrame);

    auto num = scene->items();
    AddStick();
    num = scene->items();
    //view->my_animation->StoreUndo();
}

// questa funzione crea il frame e gli oggetti lista appropriati
Frame* Widget::SetUpFrame(int pos){
    return view->SetUpFrame(pos);

}
void Widget::on_copyFrame_clicked()
{
    view->StoreUndo(CMD_ADDFRAME);
    // snapshot dati del frame corrente (Phase 2b): niente stream né switchFrameFlag
    FrameData *snapshot = ExtractFrameData(*view->my_animation->CurrentFrame());
    // prepare new frame
    int pos = 0;
    if(view->my_animation->frame_list.count()>=1)
    {
        pos = view->my_animation->CurrentFrame()->frame_number+1;
    }
    Frame* addedFrame = SetUpFrame(pos);
    PopulateLiveFrame(addedFrame,*snapshot,true,true);
    delete snapshot;
    // update frame name
    addedFrame->frame_number++;
    // move to new frame
    view->MoveToFrame(addedFrame);
    // update list icons
    view->UpdateFrameOrder(view->my_animation->CurrentFrame());

}

void Widget::on_onionSkinSpinBox_valueChanged(const QString &arg1)
{
    view->UpdateOnionSkins();
}

void Widget::on_deleteFrameBtn_clicked()
{
    if(view->my_animation->frame_list.count()>1){
        view->StoreUndo(CMD_DELETEFRAME);
        view->DeleteFrame(view->my_animation->CurrentFrame());
        view->MoveToFrame(view->my_animation->CurrentFrame());
    }
}

void Widget::on_frameListWidget_itemClicked(QListWidgetItem *item)
{
    view->StoreUndo(CMD_MOVETOFRAME);
    QVariant  retrievedData = (item->data(Qt::UserRole));
    Frame* cs = qvariant_cast<Frame*>(retrievedData);

    view->MoveToFrame(cs);
}

void Widget::on_stickLayerView_itemDoubleClicked(QListWidgetItem *item)
{
    QString text = QInputDialog::getText(this,"rename layer","text");
    QListWidgetItem *myItem = my_stick_figure_widget_list->item((my_stick_figure_widget_list->currentRow()));
    myItem->setData(Qt::DisplayRole, text);
    QVariant  retrievedData = (item->data(Qt::UserRole));
    StickFigure* S = qvariant_cast<StickFigure*>(retrievedData);
    S->name = text;
}
bool busy_play = false;
MySleeper *play;
void Widget::on_PlayButton_clicked()
{
    if(busy_play == false){
        //parti dal frame 0, scegli il framerate
        view->MoveToFrame(view->my_animation->frame_list[0]);
        int fps = fps_spin_box->value();
        int onions = onion_skin_sb->value();
        // non voglio onionskins in questa modalità
        onion_skin_sb->setValue(0);
        view->MoveToFrame(view->my_animation->frame_list[0]);

        // calcola il delay da usare nel timer di passaggio da un frame al segunte
        int delay = 1000/fps;
        if(delay>5)
            delay = delay/2;
        busy_play = true;
        play = new MySleeper(view,delay);
        onion_skin_sb->setValue(onions);
    }else{
        play->Stop();
    }
    //view->MoveToFrame(view->my_animation->frame_list[0]);

}

void Widget::on_zoomInBtn_clicked()
{
    view->scale(1/zoom_lvl,1/zoom_lvl);
    zoom_lvl*=2;
    view->scale(zoom_lvl,zoom_lvl);
    view->MoveToFrame(view->my_animation->CurrentFrame());
}

void Widget::on_zoomOutBtn_clicked()
{

    view->scale(1/zoom_lvl,1/zoom_lvl);
    zoom_lvl/=2;
    view->scale(zoom_lvl,zoom_lvl);
    view->MoveToFrame(view->my_animation->CurrentFrame());
}

void Widget::on_resetZoomBtn_clicked()
{
    view->scale(1/zoom_lvl,1/zoom_lvl);
    zoom_lvl = 1.0;
    view->MoveToFrame(view->my_animation->CurrentFrame());
}

void Widget::on_drawImageBtn_clicked()
{
    QString file_name = QFileDialog::getOpenFileName(this,tr("Load Image"),
                       "C:/", tr("Images (*.png *.bmp *.jpg)"));
    if(QFile::exists(file_name)){
        //image_draw_buffer = new QImage("C:/Users/riccim3/Pictures/immagine.jpg" );
        image_draw_buffer = new QImage(file_name );
        image_name_buffer = QFileInfo(QFile(file_name)).fileName();
        view->SetTool(DRAWIMG);
    }

}
void Widget::on_addImgBtn_clicked()
{
    StickData* cs = view->my_animation->CurrentFrame()->CurrentStick();
    QString file_name = QFileDialog::getOpenFileName(this,tr("Load Image"),
                       "C:/", tr("Images (*.png *.bmp *.jpg)"));
    if(file_name.length()>0 && cs->view){
        //image_draw_buffer = new QImage("C:/Users/riccim3/Pictures/immagine.jpg" );
        QImage* myImg = new QImage(file_name);
        view->StoreUndo();
        cs->view->AddImage(myImg,
                                                                                    QFileInfo(QFile(file_name)).fileName());
    }
}

void Widget::on_removeImgBtn_clicked()
{
    if(view->my_animation->CurrentFrame()->CurrentStickFigure()== nullptr)
        return;
    if(view->my_animation->CurrentFrame()->CurrentStick() != nullptr){
        if(view->my_animation->CurrentFrame()->CurrentStick()->stick_type == StickData::IMAGE && view->my_animation->CurrentFrame()->CurrentStick()->stick_img_list.length()>1
                && view->my_animation->CurrentFrame()->CurrentStick()->view){
            int idx = img_list_widget->currentRow(); // indice immagine correntmente selezionata
            view->StoreUndo();
            view->my_animation->CurrentFrame()->CurrentStick()->view->RemoveImgFromList(idx);
            view->my_animation->CurrentFrame()->CurrentStickFigure()->Refresh(0);
        }
    }
}

// imposta l'immagine contenuta nello stick dalla lista delle immagini
void Widget::on_setImgBtn_clicked()
{
    int idx = img_list_widget->currentRow(); // indice immagine correntmente selezionata
    if(view->my_animation->CurrentFrame()->CurrentStick() != nullptr){
        if(view->my_animation->CurrentFrame()->CurrentStick()->stick_type == StickData::IMAGE){
            view->StoreUndo();
            view->my_animation->CurrentFrame()->CurrentStick()->stick_img = view->my_animation->CurrentFrame()->CurrentStick()->stick_img_list[idx];
            view->my_animation->CurrentFrame()->CurrentStickFigure()->Refresh(0);
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
    QString file_name = QFileDialog::getSaveFileName(this,tr("save marionetta library"),
                       lib_folder.path(),"Marionetta Library(*.marlib)");
    if(file_name.length()>0){
        view->SaveLibrary(file_name);
        my_library_list_widget->clear();
        DetectLibraries();
    }
}



void Widget::on_addItemToLibBtn_clicked()
{
    view->AddStickFigureToLibrary();
}

void Widget::on_loadLibraryBtn_clicked()
{
    QString file_name = QFileDialog::getOpenFileName(this,tr("load marionetta library"),
                       lib_folder.path(),"Marionetta Library(*.marlib)");
    if(QFile::exists(file_name)){
        view->LoadLibrary(file_name);
    }
}

void Widget::on_refreshLibrariesBtn_clicked()
{
    my_library_list_widget->clear();
    DetectLibraries();
}

void Widget::on_libraryList_currentRowChanged(int current_row)
{
    QString file_name = "";
    int idx = my_library_list_widget->currentRow();
    if(idx>=0 && idx<my_library_list_widget->count()){
        QVariant buffer;
        buffer = my_library_list_widget->item(idx)->data(Qt::UserRole);
        file_name = qvariant_cast<QString>(buffer);
        if(QFile::exists(file_name)){
            view->LoadLibrary(file_name,0);
        }
    }

}

void Widget::on_removeFromLibraryBtn_clicked()
{
    view->RemoveFromLibrary();
}

void Widget::on_addItemFromLibraryToSceneBtn_clicked()
{
    // get selected item
    int idx = my_current_library_widget->currentRow();
    // niente selezionato: controllato PRIMA di AddStick(), altrimenti un click a vuoto aggiungeva
    // comunque un layer vuoto indesiderato oltre a rischiare item(-1)->isSelected() in crash
    if(idx<0 || idx>=my_current_library_widget->count() || !my_current_library_widget->item(idx)->isSelected()){
        return;
    }
    StickFigure * added = AddStick();
    QListWidgetItem *itemToAdd = my_current_library_widget->item(idx);
    // estrai il dato utente salvato in esso: da Phase 2b la libreria contiene record dati
    QVariant  retrievedData = (itemToAdd->data(Qt::UserRole));
    StickFigureData* toAdd = qvariant_cast<StickFigureData*>(retrievedData);
    PopulateLiveStickFigure(added,*toAdd,true,true);
    for(StickData*s:added->stick_list){
        view->my_animation->CurrentFrame()->total_sticks.append(s);
    }
    view->AddHandles(added);
    added->linked_item->setData(Qt::DisplayRole,added->name);
    view->my_animation->UpdateSelection(added);
    my_stick_figure_widget_list->setCurrentRow(my_stick_figure_widget_list->row(added->linked_item));
    added->Refresh();
    //added->Refresh();
    //scene->update();
    //view->my_animation->CurrentFrame()->CurrentStickFigure()->LoadStickFigure(file_name)
}

void Widget::on_saveCurrentLibraryBtn_clicked()
{
    QString file_name = "";
    int idx = my_library_list_widget->currentRow();
    if(idx>=0 && idx<my_library_list_widget->count()){
        QVariant buffer;
        QListWidgetItem *item;
        item = my_library_list_widget->item(idx);
        buffer = item->data(Qt::UserRole);
        file_name = qvariant_cast<QString>(buffer);
        if(QFile::exists(file_name)){
            view->SaveLibrary(file_name);
            my_library_list_widget->clear();
            DetectLibraries();
        }
    }
}

void Widget::on_imgHOffsetSlider_valueChanged(int value)
{
    StickData*cs =  view->my_animation->CurrentFrame()->CurrentStick();
    if(cs == nullptr)
        return;
    else{
        if(cs->stick_type == StickData::IMAGE){
            if(!sliderFlags.sliderHOffset){
                view->StoreUndo(CMD_SIMPLE);
                sliderFlags.sliderHOffset = true;
            }
            float val = 0;
            val = (float)img_h_offset_slider->value();
            img_h_offset_spinbox->setValue(img_h_offset_slider->value());
            cs->img_offset.setX(val);
             view->my_animation->CurrentFrame()->CurrentStickFigure()->Refresh();
        }
    }
}


void Widget::on_imgVOfsetSlider_valueChanged(int value)
{
    StickData*cs =  view->my_animation->CurrentFrame()->CurrentStick();
    if(cs == nullptr)
        return;
    else{
        if(cs->stick_type == StickData::IMAGE){
            float val = 0;
            if(!sliderFlags.sliderVOffset){
                view->StoreUndo(CMD_SIMPLE);
                sliderFlags.sliderVOffset = true;
            }
            img_v_offset_spinbox->setValue(img_v_offset_slider->value());
            val = (float)img_v_offset_slider->value();
            cs->img_offset.setY(val);
             view->my_animation->CurrentFrame()->CurrentStickFigure()->Refresh();
        }
    }
}

void Widget::on_imgWSlider_valueChanged(int value)
{
    StickData*cs =  view->my_animation->CurrentFrame()->CurrentStick();
    if(cs == nullptr)
        return;
    else{
        if(cs->stick_type == StickData::IMAGE){
            float scale = 0;
            if(!sliderFlags.sliderWScale){
                view->StoreUndo(CMD_SIMPLE);
                sliderFlags.sliderWScale = true;
            }
            img_width_slider->value()>0?scale = (float)img_width_slider->value()/10+1:scale = 1/((float)(-img_width_slider->value())/10+1);
            img_width_spinbox->setValue(img_width_slider->value());
            cs->img_w_scale = scale;
            view->my_animation->CurrentFrame()->CurrentStickFigure()->Refresh();
        }
    }
}

void Widget::on_imgHSlider_valueChanged(int value)
{
    StickData*cs =  view->my_animation->CurrentFrame()->CurrentStick();
    if(cs == nullptr)
        return;
    else{
        if(cs->stick_type == StickData::IMAGE){
            float scale = 0;
            if(!sliderFlags.sliderHScale){
                view->StoreUndo(CMD_SIMPLE);
                sliderFlags.sliderHScale = true;
            }
            img_height_slider->value()>0?scale = (float)img_height_slider->value()/10+1 : scale = 1/((float)(-img_height_slider->value())/10+1);
            img_height_spinbox->setValue(img_height_slider->value());
            cs->img_h_scale = scale;
             view->my_animation->CurrentFrame()->CurrentStickFigure()->Refresh();
        }
    }
}

void Widget::on_imgRotationSlider_valueChanged(int value)
{
    StickData*cs =  view->my_animation->CurrentFrame()->CurrentStick();
    if(cs == nullptr)
        return;
    else{
        if(cs->stick_type == StickData::IMAGE){
            if(!sliderFlags.sliderRot){
                view->StoreUndo(CMD_SIMPLE);
                sliderFlags.sliderRot = true;
            }
            float val = 0;
            val =img_rotation_slider->value();
            img_rotation_spinbox->setValue(val);
            cs->img_angle_offset = val;
             view->my_animation->CurrentFrame()->CurrentStickFigure()->Refresh();
        }
    }
}

void Widget::on_imgHOffsetSpinbox_valueChanged(int arg1)
{
    img_h_offset_slider->setValue(img_h_offset_spinbox->value());
}

void Widget::on_imgVOffsetSpinbox_valueChanged(int arg1)
{
    img_v_offset_slider->setValue(img_v_offset_spinbox->value());
}

void Widget::on_imgWidthSpinbox_valueChanged(int arg1)
{
    img_width_slider->setValue(img_width_spinbox->value());
}

void Widget::on_imgHeightSpinbox_valueChanged(int arg1)
{
    img_height_slider->setValue(img_height_spinbox->value());
}

void Widget::on_imgRotationSpinbox_valueChanged(int arg1)
{
    img_rotation_slider->setValue(img_rotation_spinbox->value());
}
void Widget::on_chooseColorBtn_clicked()
{
    QColor color = QColorDialog::getColor(Qt::yellow, this );
    view->my_pen.setColor(color);
    // aggiorna il colore del segnacolore
    if(color.isValid()) {
       QString qss = QString("background-color: %1").arg(color.name());
       ui->currentColorBtn->setStyleSheet(qss);
    }
}
void Widget::on_setStickColorBtn_clicked()
{
    view->SetGraphics(false, ATTRIBUTE_PENCOLOR);
}

void Widget::on_setAllSticksColorBtn_clicked()
{
    view->SetGraphics(true,ATTRIBUTE_PENCOLOR);
}

void Widget::on_setPenThicknessBtn_clicked()
{
    view->SetGraphics(false, ATTRIBUTE_PENWIDTH);
}

void Widget::on_setPenThicknessAllBtn_clicked()
{
    view->SetGraphics(true, ATTRIBUTE_PENWIDTH);
}

void Widget::on_selectBrushColorBtn_clicked()
{
    QColor color = QColorDialog::getColor(Qt::yellow, this );
    color.setAlphaF((float)brush_opacity_slider->value()/100);
    view->my_brush.setColor(color);
    // aggiorna il colore del segnacolore
    if(color.isValid()) {
       QString qss = QString("background-color: %1").arg(color.name());
       ui->currentBrushColorBtn->setStyleSheet(qss);
    }
    //view->SetGraphics(true,ATTRIBUTE_PENCOLOR);
}

void Widget::on_setAllBrushColorBtn_clicked()
{
    view->SetGraphics(true, ATTRIBUTE_BRUSHCOLOR);
}

void Widget::on_setBrushColorBtn_clicked()
{
    view->SetGraphics(false, ATTRIBUTE_BRUSHCOLOR);
}

void Widget::on_manipulateBtn_clicked()
{
    view->SetTool(MANIPULATE);
}



void Widget::on_penOpacitySlider_valueChanged(int value)
{
    QColor c = view->my_pen.color();
    c.setAlphaF((float)pen_opacity_slider->value()/100);
    view->my_pen.setColor(c);
    pen_opacity_spinbox->setValue(pen_opacity_slider->value());
}

void Widget::on_fillOpacitySlider_valueChanged(int value)
{
    QColor c = view->my_brush.color();
    c.setAlphaF((float)brush_opacity_slider->value()/100);
    view->my_brush.setColor(c);
    brush_opacity_spinbox->setValue(brush_opacity_slider->value());
}

void Widget::on_penOpacitySpinbox_valueChanged(int arg1)
{
    pen_opacity_slider->setValue(pen_opacity_spinbox->value());
}

void Widget::on_fillOpacitySpinbox_valueChanged(int arg1)
{
    brush_opacity_slider->setValue(brush_opacity_spinbox->value());
}


void Widget::on_rotateStickFigureBtn_clicked()
{
    view->SetTool(ROTATE);
}

void Widget::on_scaleStickFigureBtn_clicked()
{
    view->SetTool(SCALE);
}

extern bool start_stick_figure_scale;


void Widget::on_stickFigureScaleSpinbox_editingFinished()
{
    view->StoreUndo();
    for(StickData*s:view->my_animation->CurrentFrame()->CurrentStickFigure()->stick_list){
        if(s->view){
            s->view->scale_buffer = s->my_line.length();
            s->view->width_buffer = s->pen.width();
        }
    }
    if(view->my_animation->CurrentFrame()->CurrentStickFigure() != nullptr){
        for(StickData* s:view->my_animation->CurrentFrame()->CurrentStickFigure()->stick_list){
            if(s->view)
                s->view->Scale((float)stick_figure_scale_spinbox->value()/100);

        }

    }
    stick_figure_scale_spinbox->setValue(100);
}

void Widget::on_stickFigureRotationSpinbox_editingFinished()
{
    view->StoreUndo();
    if(view->my_animation->CurrentFrame()->CurrentStickFigure() != nullptr){
        for(StickData* s:view->my_animation->CurrentFrame()->CurrentStickFigure()->stick_list){
            if(s->view){
                s->view->angle_buffer2 = s->my_line.angle();
                s->view->Rotate(stick_figure_rotation_spinbox->value());
                s->view->angle_buffer2 = s->my_line.angle();
            }
        }

    }
    stick_figure_rotation_spinbox->setValue(0);
}

void Widget::on_moveSceneBtn_clicked()
{
    view->SetTool(MOVESCENE);
}

void Widget::on_scaleSceneBtn_clicked()
{
    view->SetTool(SCALESCENE);
}

void Widget::on_rotateSceneBtn_clicked()
{
    view->SetTool(ROTATESCENE);
}

void Widget::on_verticalFlipBtn_clicked()
{
    if(view->my_animation->CurrentFrame()->CurrentStickFigure() != nullptr){
        for(StickData* s:view->my_animation->CurrentFrame()->CurrentStickFigure()->stick_list){
            //s->my_line.setAngle(s->my_line.angle()+180);
            if(s->view){
                s->view->angle_buffer2 = s->my_line.angle();
                s->view->Rotate((-2*s->my_line.angle()));
            }
            //s->Refresh(0);
        }
    }
}

void Widget::on_HorizontalFlipBtn_clicked()
{
    if(view->my_animation->CurrentFrame()->CurrentStickFigure() != nullptr){
        for(StickData* s:view->my_animation->CurrentFrame()->CurrentStickFigure()->stick_list){
            //s->my_line.setAngle(s->my_line.angle()+180);
            if(s->view){
                s->view->angle_buffer2 = s->my_line.angle();
                s->view->Rotate(2*(90-s->my_line.angle()));
            }
            //s->Refresh(0);
        }
    }
}

void Widget::on_copyStickFigureBtn_clicked()
{
    if(view->my_animation->CurrentFrame()->CurrentStickFigure() != nullptr){
        // il buffer è puro dato (Phase 2b): liberarlo e riestrarlo è tutto qui,
        // niente copyFlag e niente pulizia manuale degli stick
        delete view->copy_stick_figure_buffer;
        view->copy_stick_figure_buffer = ExtractStickFigureData(*view->my_animation->CurrentFrame()->CurrentStickFigure());
    }
}

void Widget::on_pasteStickFigureBtn_clicked()
{
   if(view->copy_stick_figure_buffer != nullptr){
       view->StoreUndo(CMD_SIMPLE);
       StickFigure* added =AddStick();
       PopulateLiveStickFigure(added,*view->copy_stick_figure_buffer,true,true);
       added->Refresh(0);
       for(StickData*s:added->stick_list){
           view->my_animation->CurrentFrame()->total_sticks.append(s);

       }
       view->AddHandles(added);
   }
   else{
       QMessageBox::about(this,"error","no stickfigure was copied in memory");
   }

}

void Widget::on_joinBtn_clicked()
{
    if(view->my_animation->CurrentFrame()->CurrentStickFigure()== nullptr){
        QMessageBox::about(this,"Error","First you must select a stickfigure");
        return;

    }else{
        if(view->my_animation->CurrentFrame()->CurrentStickFigure()->stick_list.count()==0){
            QMessageBox::about(this,"Error","current stickfigure is empty cannot join an empty stickfigure");
            return;
        }else{
            // conditions are met, set the tool
            view->SetTool(JOIN);
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

    view->SplitStickFigure();
}

void Widget::on_joinStick_clicked()
{
    if(view->my_animation->CurrentFrame()->CurrentStickFigure() == nullptr || view->my_animation->CurrentFrame()->CurrentStick() == nullptr)
        return;
    view->StoreUndo();
    QList<StickData*> toWeld = view->my_animation->CurrentFrame()->CurrentStickFigure()->Weld(view->my_animation->CurrentFrame()->CurrentStick());
    for(StickData*s:toWeld){
        view->my_animation->CurrentFrame()->total_sticks.removeAll(s);
    }
    //view->my_animation->CurrentFrame()->CleanTotalSticks();
}

void Widget::on_splitStick_clicked()
{
    if(view->my_animation->CurrentFrame()->CurrentStickFigure() == nullptr || view->my_animation->CurrentFrame()->CurrentStick() == nullptr)
    {
        return;
    }
    view->StoreUndo();
    view->RemoveHandles(view->my_animation->CurrentFrame()->CurrentStickFigure());
    StickData* added = view->my_animation->CurrentFrame()->CurrentStickFigure()->Chop(view->my_animation->CurrentFrame()->CurrentStick());
    view->AddHandles(view->my_animation->CurrentFrame()->CurrentStickFigure());
    view->my_animation->CurrentFrame()->total_sticks.append(added);
}



void Widget::on_setMasterNodeBtn_clicked()
{
    if(view->my_animation->CurrentFrame()->CurrentStickFigure() == nullptr ||
            view->my_animation->CurrentFrame()->CurrentStickFigure()->stick_list.isEmpty()||
            view->my_animation->CurrentFrame()->CurrentStick() == nullptr)
        return;
    view->StoreUndo();
    view->my_animation->CurrentFrame()->CurrentStickFigure()->SetMaster(view->my_animation->CurrentFrame()->CurrentStick());
}

void Widget::on_createNewLibraryBtn_clicked()
{
    on_saveCurrentLibraryBtn_clicked();
    QString file_name = QFileDialog::getSaveFileName(this,tr("create a new marionetta library"),
                       lib_folder.path(),"Marionetta Library(*.marlib)");
    if(file_name.length()>0){
        view->ClearCurrentLib();
        view->SaveLibrary(file_name);
        my_library_list_widget->clear();
        DetectLibraries();
    }
    if(QFile::exists(file_name)){
        view->LoadLibrary(file_name);

        QListWidgetItem *added = my_library_list_widget->findItems(QFileInfo(file_name).fileName(),Qt::MatchExactly)[0];
        int row = my_library_list_widget->row(added);
        my_library_list_widget->setCurrentRow(row);
    }

}

void Widget::on_setAllDepthBtn_clicked()
{
    if(!view->my_animation->CurrentFrame()->CurrentStickFigure()->stick_list.isEmpty()){
        view->StoreUndo(CMD_SIMPLE);
        float val = depth_spinbox->value();
        for(StickData* s:view->my_animation->CurrentFrame()->CurrentStickFigure()->stick_list){
            s->z = val;
            if(s->view)
                s->view->setZValue(val); // mancava: senza setZValue() il riordino non si vedeva a schermo
        }
        view->my_animation->CurrentFrame()->CurrentStickFigure()->base_z = val;
        // riordina total_sticks in base alla nuova Z: usato da push-to-top/bottom e move-stick-up/down
        // per trovare il vicino corretto, altrimenti restava nell'ordine (ormai sbagliato) precedente
        std::sort(view->my_animation->CurrentFrame()->total_sticks.begin(),view->my_animation->CurrentFrame()->total_sticks.end(),
                  [](StickData* a, StickData* b){ return a->z < b->z; });
        view->my_animation->CurrentFrame()->Refresh();
    }
}

void Widget::on_setDepthBtn_clicked()
{
    if(!view->my_animation->CurrentFrame()->CurrentStickFigure()->stick_list.isEmpty()
            && view->my_animation->CurrentFrame()->CurrentStick()){
        view->StoreUndo(CMD_SIMPLE);
        float val = depth_spinbox->value();
        view->my_animation->CurrentFrame()->CurrentStick()->z = val;
        if(view->my_animation->CurrentFrame()->CurrentStick()->view)
            view->my_animation->CurrentFrame()->CurrentStick()->view->setZValue(val); // mancava: senza setZValue() il riordino non si vedeva a schermo
        // riordina total_sticks in base alla nuova Z, per lo stesso motivo di on_setAllDepthBtn_clicked
        std::sort(view->my_animation->CurrentFrame()->total_sticks.begin(),view->my_animation->CurrentFrame()->total_sticks.end(),
                  [](StickData* a, StickData* b){ return a->z < b->z; });
        view->my_animation->CurrentFrame()->Refresh();
    }
}

void Widget::on_depthSpinbox_valueChanged(double arg1)
{
    depth_slider->setValue(arg1*100);
}

void Widget::on_depthSlider_valueChanged(int value)
{
    float val = (float)value;
    depth_spinbox->setValue(val/100);
}

void Widget::on_drawRectBtn_clicked()
{
        view->SetTool(DRAWRECT);
}

void Widget::on_pushButton_clicked()
{
    static bool yo = false;
    yo = !yo;
    if(yo){
    ui->toolsTabWidget->removeTab(1);
    ui->toolsTabWidget->insertTab(1,advanced_img_tab,"rectangle");
    }
    else{
        ui->toolsTabWidget->removeTab(1);
        ui->toolsTabWidget->insertTab(1,ui->advancedTab,"advanced");
    }
}

void Widget::on_drawTaperBtn_clicked()
{
    view->SetTool(DRAWTAPER);
}

void Widget::on_drawTrapezoidBtn_clicked()
{
    view->SetTool(DRAWTRAPEZOID);
}

void Widget::on_moveStickFigureToCenterBtn_clicked()
{
    if(view->my_animation->CurrentFrame()->CurrentStickFigure() == nullptr)
        return;
    if(view->my_animation->CurrentFrame()->CurrentStickFigure()->stick_list.isEmpty())
        return;
    int dx = -view->my_animation->CurrentFrame()->CurrentStickFigure()->master_stick->my_line.p1().x()+W/2;
    int dy = -view->my_animation->CurrentFrame()->CurrentStickFigure()->master_stick->my_line.p1().y()+H/2;
    view->my_animation->CurrentFrame()->CurrentStickFigure()->Traslate(dx,dy);
}

void Widget::on_pushToTopBtn_clicked()
{
    if(view->my_animation->CurrentFrame()->CurrentStickFigure() == nullptr)
        return;
    if(view->my_animation->CurrentFrame()->CurrentStickFigure()->stick_list.isEmpty() || view->my_animation->CurrentFrame()->CurrentStick()== nullptr)
        return;
    view->StoreUndo();
    view->my_animation->CurrentFrame()->total_sticks.move(view->my_animation->CurrentFrame()->total_sticks.indexOf(view->my_animation->CurrentFrame()->CurrentStick()),view->my_animation->CurrentFrame()->total_sticks.length()-1);

    float MaxZ = view->my_animation->CurrentFrame()->CurrentStick()->z;
    for(StickFigure* S:view->my_animation->CurrentFrame()->stick_figures){
        for(StickData*s:S->stick_list){
            if(s->z>MaxZ){
                MaxZ = s->z;
            }
        }
    }
    if(MaxZ>MAXDEPTH)
        MaxZ = MAXDEPTH;

    view->my_animation->CurrentFrame()->CurrentStick()->z = MaxZ;
    if(view->my_animation->CurrentFrame()->CurrentStick()->view)
        view->my_animation->CurrentFrame()->CurrentStick()->view->setZValue(view->my_animation->CurrentFrame()->CurrentStick()->z);

    view->my_animation->CurrentFrame()->Refresh();

}

void Widget::on_pushToButtonBtn_clicked()
{
    if(view->my_animation->CurrentFrame()->CurrentStickFigure() == nullptr)
        return;
    if(view->my_animation->CurrentFrame()->CurrentStickFigure()->stick_list.isEmpty() || view->my_animation->CurrentFrame()->CurrentStick()== nullptr)
        return;
    view->StoreUndo();
    view->my_animation->CurrentFrame()->total_sticks.move(view->my_animation->CurrentFrame()->total_sticks.indexOf(view->my_animation->CurrentFrame()->CurrentStick()),0);

    float MinZ = view->my_animation->CurrentFrame()->CurrentStick()->z;
    for(StickFigure* S:view->my_animation->CurrentFrame()->stick_figures){
        for(StickData*s:S->stick_list){
            if(s->z<MinZ){
                MinZ = s->z;
            }
        }
    }
    if(MinZ<MINDEPTH)
        MinZ = MINDEPTH;

    view->my_animation->CurrentFrame()->CurrentStick()->z = MinZ;
    if(view->my_animation->CurrentFrame()->CurrentStick()->view)
        view->my_animation->CurrentFrame()->CurrentStick()->view->setZValue(view->my_animation->CurrentFrame()->CurrentStick()->z);

    view->my_animation->CurrentFrame()->Refresh();
}



void Widget::on_elongateSpinbox_editingFinished()
{
    if(view->my_animation->CurrentFrame()->CurrentStickFigure()!= nullptr && view->my_animation->CurrentFrame()->CurrentStick() !=nullptr){
        QLineF lineBuf = view->my_animation->CurrentFrame()->CurrentStick()->my_line;
        lineBuf.setLength(elongate_spinbox->value());
        QPointF coord= lineBuf.p2();

        view->my_animation->CurrentFrame()->CurrentStickFigure()->Elongate(coord,view->my_animation->CurrentFrame()->CurrentStick());
    }

}

void Widget::on_pushStickFigureToTop_clicked()
{
    view->StoreUndo();
    view->MoveStickFigureZ(0,MODE_TOP);
}

void Widget::on_pushStickFigureToBottom_clicked()
{
    view->StoreUndo();
    view->MoveStickFigureZ(0,MODE_BOTTOM);
}

void Widget::on_moveStickUp_clicked()
{
    if(view->my_animation->CurrentFrame()->CurrentStickFigure() == nullptr)
        return;
    if(view->my_animation->CurrentFrame()->CurrentStickFigure()->stick_list.isEmpty() || view->my_animation->CurrentFrame()->CurrentStick()== nullptr)
        return;
    view->StoreUndo();
    int oldIdx = view->my_animation->CurrentFrame()->total_sticks.indexOf(view->my_animation->CurrentFrame()->CurrentStick());
    int newIdx = view->my_animation->CurrentFrame()->total_sticks.indexOf(view->my_animation->CurrentFrame()->CurrentStick())+1;
    if(newIdx >= view->my_animation->CurrentFrame()->total_sticks.length())
        return;
    StickData* minorBuffer = nullptr;
    //1 scansiona la lista cerca se ci sta uno sopra con Z uguale
    for(StickData*s:view->my_animation->CurrentFrame()->total_sticks){
        if(s!=view->my_animation->CurrentFrame()->CurrentStick() && (s->z==view->my_animation->CurrentFrame()->CurrentStick()->z && view->my_animation->CurrentFrame()->total_sticks.indexOf(s)>oldIdx))
        {
            if(minorBuffer == nullptr){
                minorBuffer = s;
            }
            else if(view->my_animation->CurrentFrame()->total_sticks.indexOf(s)<view->my_animation->CurrentFrame()->total_sticks.indexOf(minorBuffer)){
                minorBuffer = s;
            }
        }
    }
    if(minorBuffer != nullptr){
        view->my_animation->CurrentFrame()->total_sticks.move(oldIdx,view->my_animation->CurrentFrame()->total_sticks.indexOf(minorBuffer));
        view->my_animation->CurrentFrame()->Refresh(0);
        return;
    }
    for(StickData*s:view->my_animation->CurrentFrame()->total_sticks){
        if(s!=view->my_animation->CurrentFrame()->CurrentStick() && (s->z>view->my_animation->CurrentFrame()->CurrentStick()->z ))
        {
            if(minorBuffer == nullptr){
                minorBuffer = s;
            }
            else if(s->z <= minorBuffer->z){
                if(s->z == minorBuffer->z && view->my_animation->CurrentFrame()->total_sticks.indexOf(s)<view->my_animation->CurrentFrame()->total_sticks.indexOf(minorBuffer)){
                    minorBuffer = s;
                }
                else if(s->z<minorBuffer->z){
                    minorBuffer = s;
                }
            }
        }
    }
    if(minorBuffer !=nullptr){

        view->my_animation->CurrentFrame()->CurrentStick()->z = minorBuffer->z;
        if(oldIdx<view->my_animation->CurrentFrame()->total_sticks.indexOf(minorBuffer))
            view->my_animation->CurrentFrame()->total_sticks.move(oldIdx,view->my_animation->CurrentFrame()->total_sticks.indexOf(minorBuffer));
        view->my_animation->CurrentFrame()->Refresh(0);
        return;

    }
}

void Widget::on_moveStickDown_clicked()
{
    if(view->my_animation->CurrentFrame()->CurrentStickFigure() == nullptr)
        return;
    if(view->my_animation->CurrentFrame()->CurrentStickFigure()->stick_list.isEmpty() || view->my_animation->CurrentFrame()->CurrentStick()== nullptr)
        return;
    view->StoreUndo();
    int oldIdx = view->my_animation->CurrentFrame()->total_sticks.indexOf(view->my_animation->CurrentFrame()->CurrentStick());
    int newIdx = view->my_animation->CurrentFrame()->total_sticks.indexOf(view->my_animation->CurrentFrame()->CurrentStick())-1;
    if(newIdx < 0)
        return;
    StickData* minorBuffer = nullptr;
    //1 scansiona la lista cerca se ci sta uno sotto con Z uguale
    for(StickData*s:view->my_animation->CurrentFrame()->total_sticks){
        if(s!=view->my_animation->CurrentFrame()->CurrentStick() && (s->z==view->my_animation->CurrentFrame()->CurrentStick()->z && view->my_animation->CurrentFrame()->total_sticks.indexOf(s)<oldIdx))
        {
            if(minorBuffer == nullptr){
                minorBuffer = s;
            }
            else if(view->my_animation->CurrentFrame()->total_sticks.indexOf(s)>view->my_animation->CurrentFrame()->total_sticks.indexOf(minorBuffer)){
                minorBuffer = s;
            }
        }
    }
    if(minorBuffer != nullptr){
        view->my_animation->CurrentFrame()->total_sticks.move(oldIdx,view->my_animation->CurrentFrame()->total_sticks.indexOf(minorBuffer));
        view->my_animation->CurrentFrame()->Refresh(0);
        return;
    }
    for(StickData*s:view->my_animation->CurrentFrame()->total_sticks){
        if(s!=view->my_animation->CurrentFrame()->CurrentStick() && (s->z<view->my_animation->CurrentFrame()->CurrentStick()->z ))
        {
            if(minorBuffer == nullptr){
                minorBuffer = s;
            }
            else if(s->z >= minorBuffer->z){
                if(s->z == minorBuffer->z && view->my_animation->CurrentFrame()->total_sticks.indexOf(s)>view->my_animation->CurrentFrame()->total_sticks.indexOf(minorBuffer)){
                    minorBuffer = s;
                }
                else if(s->z>minorBuffer->z){
                    minorBuffer = s;
                }
            }
        }
    }
    if(minorBuffer !=nullptr){

        view->my_animation->CurrentFrame()->CurrentStick()->z = minorBuffer->z;
        if(oldIdx>view->my_animation->CurrentFrame()->total_sticks.indexOf(minorBuffer))
            view->my_animation->CurrentFrame()->total_sticks.move(oldIdx,view->my_animation->CurrentFrame()->total_sticks.indexOf(minorBuffer));
        view->my_animation->CurrentFrame()->Refresh(0);
        return;

    }
}

void Widget::on_deleteLibrary_clicked()
{
    QString file_name = "";
    int idx = my_library_list_widget->currentRow();
    if(idx>=0 && idx<my_library_list_widget->count()){
        QVariant buffer;
        buffer = my_library_list_widget->item(idx)->data(Qt::UserRole);
        file_name = qvariant_cast<QString>(buffer);
        if(QFile::exists(file_name)){
            // check if user wants to delete a file
            int ret  = QMessageBox::warning(this, tr("delete File"),
                                            tr("This will delete the file associated to the library .\n"
                                               "Do you want to proceed?"),
                                            QMessageBox::Yes | QMessageBox::No
                                            );
            if(ret == QMessageBox::Yes){
                QFile file (file_name);
                file.remove();
                file.close();
                goto removeLibraryGoto;
            }

        }else{
            removeLibraryGoto:

            my_library_list_widget->clearSelection();
            QListWidgetItem *item = my_library_list_widget->takeItem(idx);
            delete  item;
            if(my_library_list_widget->count()>0){
                my_library_list_widget->setCurrentRow(0);
            }else{
                my_current_library_widget->clear();
            }

        }
    }
}
void Widget::on_cpyFrame_clicked()
{
    // il buffer è puro dato (Phase 2b): estrazione diretta, niente stream né flag
    delete frame_copy_buffer;
    frame_copy_buffer = ExtractFrameData(*view->my_animation->CurrentFrame());
}

void Widget::on_pasteFrame_clicked()
{

    view->StoreUndo();
    // add a new stickfigure per every stickfigure in the copy buffer
    for(StickFigureData*S:frame_copy_buffer->stick_figures){
        StickFigure *added = AddStick();
        PopulateLiveStickFigure(added,*S,true,true);
        for(StickData*s:added->stick_list){
            view->my_animation->CurrentFrame()->total_sticks.append(s);
        }
        view->AddHandles(added); // mancava: senza questo i manici degli stick incollati non comparivano
        added->linked_item->setData(Qt::DisplayRole,added->name);
        added->UpdateIcon(); // mancava: l'icona del layer incollato restava quella vuota di AddStick()
        if(frame_copy_buffer->stick_figures.last() == S){
            view->my_animation->UpdateSelection(added);
            my_stick_figure_widget_list->setCurrentRow(my_stick_figure_widget_list->row(added->linked_item));
        }
    }
    // move to new frame
    view->my_animation->CurrentFrame()->Refresh();
    // UpdateFrameOrder (sotto) NON aggiorna nessuna icona nonostante il commento originale:
    // aggiorna solo StickGraphicsItem::order/num_of_items. L'anteprima del frame va aggiornata esplicitamente qui.
    view->my_animation->CurrentFrame()->UpdateIcon();

    // update list icons
    view->UpdateFrameOrder(view->my_animation->CurrentFrame());
}


void Widget::on_lineThicknessSlider_sliderMoved(int position)
{
    ui->thicknessSpinBox->setValue(position);
}

void Widget::on_clampToolBtn_clicked()
{
    view->SetTool(CLAMP);
}
