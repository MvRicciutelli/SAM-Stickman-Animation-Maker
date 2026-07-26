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
#include <QHBoxLayout>
#include <QVBoxLayout>
#include <QPushButton>
#include <QPixmap>
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
QWidget * advanced_vertex_tab;
QWidget * advanced_draw_tab; ///< freehand draw-mode panel (brush/eraser/size/opacity/color); built in Widget's ctor
QWidget * advanced_ik_tab;
QCheckBox * ik_active_check;
QWidget * vertex_width_row;
QSlider * vertex_width_slider;
QSlider * vertex_curve_slider;
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
// sliderFlags è diventato un membro privato di Widget (widget.h): solo gli slot di
// questa classe lo toccano, non c'era motivo che fosse un globale a livello di file;
// play_back è diventato un membro di MyView (la classe che lo legge in MoveToFrame)
float zoom_lvl               = 1;
bool  first_time_opened       = false;
QImage * image_draw_buffer;
QString FFMPEGPath = "";
/**
 * @details Wires every global widget-pointer alias (uiItems.h) to its Designer-managed
 * `ui->` counterpart, builds the canvas (`scene`/`view`) and the four advanced-property
 * tabs, then loads (or creates, on first run) the default library and config.json. The
 * terms-and-conditions prompt only appears if `terms_accepted` is still 0 in the loaded
 * config; declining it exits the process outright rather than leaving a half-built
 * window on screen.
 */
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

    // ---- pannello "vertici" per POLYGON/MULTILINE (costruito in codice: due sole ----
    // ---- righe slider, un .ui dedicato sarebbe stato più ingombrante del codice) ----
    // Larghezza (solo MULTILINE, la riga si nasconde per i POLYGON) e curvatura degli
    // spigoli (entrambi i tipi); agiscono sui vertici selezionati con Ctrl(+Shift).
    advanced_vertex_tab = new QWidget();
    {
        QVBoxLayout *rows = new QVBoxLayout(advanced_vertex_tab);
        vertex_width_row = new QWidget(advanced_vertex_tab);
        QHBoxLayout *width_row_layout = new QHBoxLayout(vertex_width_row);
        width_row_layout->setContentsMargins(0, 0, 0, 0);
        width_row_layout->addWidget(new QLabel(tr("Vertex width"), vertex_width_row));
        vertex_width_slider = new QSlider(Qt::Horizontal, vertex_width_row);
        vertex_width_slider->setRange(0, W);
        QSpinBox *width_spin = new QSpinBox(vertex_width_row);
        width_spin->setRange(0, W);
        width_row_layout->addWidget(vertex_width_slider, 1);
        width_row_layout->addWidget(width_spin);
        rows->addWidget(vertex_width_row);

        QWidget *curve_row = new QWidget(advanced_vertex_tab);
        QHBoxLayout *curve_row_layout = new QHBoxLayout(curve_row);
        curve_row_layout->setContentsMargins(0, 0, 0, 0);
        curve_row_layout->addWidget(new QLabel(tr("Corner curvature"), curve_row));
        vertex_curve_slider = new QSlider(Qt::Horizontal, curve_row);
        vertex_curve_slider->setRange(0, 100); // percento: 0 = spigolo vivo, 100 = curvatura 1.0
        QSpinBox *curve_spin = new QSpinBox(curve_row);
        curve_spin->setRange(0, 100);
        curve_row_layout->addWidget(vertex_curve_slider, 1);
        curve_row_layout->addWidget(curve_spin);
        rows->addWidget(curve_row);
        rows->addStretch(1);

        // gli spinbox rispecchiano gli slider (stesso schema degli altri pannelli)
        connect(width_spin, QOverload<int>::of(&QSpinBox::valueChanged),
                vertex_width_slider, &QSlider::setValue);
        connect(curve_spin, QOverload<int>::of(&QSpinBox::valueChanged),
                vertex_curve_slider, &QSlider::setValue);
        connect(vertex_width_slider, &QSlider::valueChanged, this, [this, width_spin](int value){
            width_spin->setValue(value);
            StickData *cs = view->my_animation->CurrentFrame()->CurrentStick();
            if(cs == nullptr || cs->stick_type != StickData::MULTILINE
                    || !cs->view || cs->view->selected_vertices.isEmpty())
                return;
            if(!sliderFlags.vertexWidth){ // un drag = un solo snapshot di undo
                view->StoreUndo(CMD_SIMPLE);
                sliderFlags.vertexWidth = true;
            }
            for(int idx : cs->view->selected_vertices){
                if(idx >= 0 && idx < cs->vertex_widths.length())
                    cs->vertex_widths[idx] = value;
            }
            view->my_animation->CurrentFrame()->CurrentStickFigure()->Refresh();
        });
        connect(vertex_width_slider, &QSlider::sliderReleased, this, [this](){
            sliderFlags.vertexWidth = false;
            view->my_animation->current_frame->UpdateIcon();
        });
        connect(vertex_curve_slider, &QSlider::valueChanged, this, [this, curve_spin](int value){
            curve_spin->setValue(value);
            StickData *cs = view->my_animation->CurrentFrame()->CurrentStick();
            if(cs == nullptr || !cs->view || cs->view->selected_vertices.isEmpty())
                return;
            if(cs->stick_type != StickData::POLYGON && cs->stick_type != StickData::MULTILINE)
                return;
            if(!sliderFlags.vertexCurve){
                view->StoreUndo(CMD_SIMPLE);
                sliderFlags.vertexCurve = true;
            }
            float curvature = qBound(0.0f, value / 100.0f, 1.0f);
            for(int idx : cs->view->selected_vertices){
                // la lista può essere più corta dei vertici (file importati): allungala
                while(cs->vertex_curvatures.length() <= idx)
                    cs->vertex_curvatures.append(0.0f);
                if(idx >= 0)
                    cs->vertex_curvatures[idx] = curvature;
            }
            view->my_animation->CurrentFrame()->CurrentStickFigure()->Refresh();
        });
        connect(vertex_curve_slider, &QSlider::sliderReleased, this, [this](){
            sliderFlags.vertexCurve = false;
            view->my_animation->current_frame->UpdateIcon();
        });
    }
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

    // ---- pannello "IK properties": una sola casella acceso/spento della catena. Come
    // per il pannello vertici è costruito in codice; Animation::UpdateSliders() lo
    // aggiunge/rimuove da my_tab_widget a seconda del ruolo IK dello stick selezionato. ----
    advanced_ik_tab = new QWidget();
    {
        QVBoxLayout *ik_rows = new QVBoxLayout(advanced_ik_tab);
        ik_active_check = new QCheckBox(tr("Chain active"), advanced_ik_tab);
        ik_rows->addWidget(ik_active_check);
        ik_rows->addStretch(1);
        // toggle utente: accende/spegne la catena dello stick corrente (IK_TARGET_ON<->OFF).
        // L'aggiornamento programmatico della casella (UpdateSliders) blocca il segnale, ma
        // la guardia currently_on==checked lo rende comunque idempotente.
        connect(ik_active_check, &QCheckBox::toggled, this, [this](bool checked){
            StickData *cs = view->my_animation->CurrentFrame()->CurrentStick();
            if(cs == nullptr) return;
            StickData *start = IkChainStart(cs);
            if(start == nullptr || start->ik_target == nullptr) return;
            StickData *target = start->ik_target;
            bool currently_on = target->ik_role == StickData::IK_TARGET_ON;
            if(currently_on == checked) return;
            view->StoreUndo();
            ToggleIkChain(target);
            view->UpdateIkOverlay();
            if(target->view) target->view->Refresh(0);
        });
    }

    // ---- pannello "draw mode" del freehand: strumenti di disegno (pennello/gomma,
    // dimensione, opacità, colore, pressione) + lista pennelli. Costruito in codice come i
    // pannelli vertici/IK; Animation::UpdateTab() lo inserisce quando è selezionato uno
    // stick FREEHAND. I controlli scrivono direttamente lo stato pennello di MyView. ----
    advanced_draw_tab = new QWidget();
    {
        QVBoxLayout *col = new QVBoxLayout(advanced_draw_tab);

        // pennello / gomma (mutuamente esclusivi)
        QHBoxLayout *tool_row = new QHBoxLayout();
        QPushButton *brush_btn = new QPushButton(tr("Brush"), advanced_draw_tab);
        QPushButton *eraser_btn = new QPushButton(tr("Eraser"), advanced_draw_tab);
        brush_btn->setCheckable(true);
        eraser_btn->setCheckable(true);
        brush_btn->setChecked(true);
        tool_row->addWidget(brush_btn);
        tool_row->addWidget(eraser_btn);
        col->addLayout(tool_row);
        connect(brush_btn, &QPushButton::clicked, this, [this, brush_btn, eraser_btn]{
            brush_btn->setChecked(true);
            eraser_btn->setChecked(false);
            view->draw_eraser = false;
        });
        connect(eraser_btn, &QPushButton::clicked, this, [this, brush_btn, eraser_btn]{
            eraser_btn->setChecked(true);
            brush_btn->setChecked(false);
            view->draw_eraser = true;
        });

        // dimensione
        QHBoxLayout *size_row = new QHBoxLayout();
        size_row->addWidget(new QLabel(tr("Size"), advanced_draw_tab));
        QSpinBox *size_spin = new QSpinBox(advanced_draw_tab);
        size_spin->setRange(1, 500);
        size_spin->setValue(view->draw_brush_size);
        size_row->addWidget(size_spin);
        col->addLayout(size_row);
        connect(size_spin, QOverload<int>::of(&QSpinBox::valueChanged), this, [this](int v){ view->draw_brush_size = v; });

        // opacità
        QHBoxLayout *op_row = new QHBoxLayout();
        op_row->addWidget(new QLabel(tr("Opacity"), advanced_draw_tab));
        QSpinBox *op_spin = new QSpinBox(advanced_draw_tab);
        op_spin->setRange(0, 100);
        op_spin->setValue(view->draw_brush_opacity);
        op_row->addWidget(op_spin);
        col->addLayout(op_row);
        connect(op_spin, QOverload<int>::of(&QSpinBox::valueChanged), this, [this](int v){ view->draw_brush_opacity = v; });

        // colore
        QPushButton *color_btn = new QPushButton(tr("Brush color"), advanced_draw_tab);
        color_btn->setStyleSheet(QString("background-color: %1").arg(view->draw_brush_color.name()));
        col->addWidget(color_btn);
        connect(color_btn, &QPushButton::clicked, this, [this, color_btn]{
            QColor c = QColorDialog::getColor(view->draw_brush_color, this, tr("Brush color"));
            if(c.isValid()){
                view->draw_brush_color = c;
                color_btn->setStyleSheet(QString("background-color: %1").arg(c.name()));
            }
        });

        // modulazione da pressione penna (attiva di default; inerte in Phase 1, mouse-only)
        QCheckBox *pressure_check = new QCheckBox(tr("Pen pressure"), advanced_draw_tab);
        pressure_check->setChecked(view->draw_pressure);
        col->addWidget(pressure_check);
        connect(pressure_check, &QCheckBox::toggled, this, [this](bool on){ view->draw_pressure = on; });

        // lista pennelli: Phase 1 solo il tondo integrato; il caricamento .abr arriva in Phase 2
        QListWidget *brush_list = new QListWidget(advanced_draw_tab);
        brush_list->setViewMode(QListView::IconMode);
        brush_list->setIconSize(QSize(48, 48));
        brush_list->setMaximumHeight(80);
        QListWidgetItem *round = new QListWidgetItem(QIcon(QPixmap::fromImage(view->draw_brush_stamp)), tr("Round"));
        brush_list->addItem(round);
        brush_list->setCurrentItem(round);
        col->addWidget(brush_list);

        // azioni: rientra in draw mode su un freehand esistente + undo/redo di sessione
        QHBoxLayout *action_row = new QHBoxLayout();
        QPushButton *enter_btn = new QPushButton(tr("Edit drawing"), advanced_draw_tab);
        QPushButton *undo_btn = new QPushButton(tr("Undo"), advanced_draw_tab);
        QPushButton *redo_btn = new QPushButton(tr("Redo"), advanced_draw_tab);
        action_row->addWidget(enter_btn);
        action_row->addWidget(undo_btn);
        action_row->addWidget(redo_btn);
        col->addLayout(action_row);
        connect(enter_btn, &QPushButton::clicked, this, [this]{
            // in draw mode il bottone ESCE (come Esc/Enter); fuori, RIENTRA sul freehand selezionato
            if(view->draw_mode){
                view->ExitDrawMode();
                return;
            }
            if(view->my_animation->current_frame == nullptr) return;
            StickData *cs = view->my_animation->CurrentFrame()->CurrentStick();
            if(cs && cs->stick_type == StickData::FREEHAND)
                view->EnterDrawMode(cs, true);
        });
        // etichetta sincronizzata con lo stato di draw mode, comunque esso cambi
        connect(view, &MyView::DrawModeChanged, this, [enter_btn](bool active){
            enter_btn->setText(active ? tr("Exit draw mode") : tr("Edit drawing"));
        });
        connect(undo_btn, &QPushButton::clicked, this, [this]{ view->DrawStrokeUndo(); });
        connect(redo_btn, &QPushButton::clicked, this, [this]{ view->DrawStrokeRedo(); });

        col->addStretch(1);
    }

    AddFrame();
    num = scene->items();
    view->my_animation->CurrentFrame()->CurrentStickFigure() = view->my_animation->CurrentFrame()->stick_figures[0];
    //inserisci il pannello nella finestra
    ui->viewLayout->addWidget(view);
    // multi-selezione dalla lista layer: Ctrl/Shift-click sulle righe seleziona più
    // figure insieme, esattamente come il rettangolo di selezione sulla tela
    ui->stickLayerView->setSelectionMode(QAbstractItemView::ExtendedSelection);
    connect(ui->stickLayerView, &QListWidget::itemSelectionChanged, this, [this](){
        if(view->multi_widget_sync)
            return; // eco della sincronizzazione fatta da MyView, non input dell'utente
        QList<QListWidgetItem*> rows = ui->stickLayerView->selectedItems();
        if(rows.count() > 1){
            QList<StickFigure*> figures;
            for(QListWidgetItem *item : rows)
                figures.append(qvariant_cast<StickFigure*>(item->data(Qt::UserRole)));
            view->SetMultiSelectionList(figures);
        }
        else if(view->MultiSelectionActive()){
            view->ClearMultiSelection(); // tornati a una riga sola: la multi decade
        }
    });
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

/**
 * @details Rebuilds `recordObject` from the file and, if every expected key is present,
 * copies each value into its matching global. If any key is missing — an older or
 * hand-edited config — the file is discarded and CreateJson() writes a fresh one with
 * current defaults instead of leaving stale/missing settings in place.
 */
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
/**
 * @details Frameless, modal Yes/No box quoting the full license text. Accepting sets
 * `terms_accepted = 1`; declining sets it back to 0 — the constructor checks this flag
 * right after calling here and calls `exit(EXIT_FAILURE)` if it's still 0, so declining
 * closes the whole application, not just this dialog. Either way the result is written
 * back to config.json immediately (WriteJson()) so the prompt doesn't reappear on the
 * next launch once accepted.
 */
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
/**
 * @details Always targets the standard config.json path under `program_folder` — unlike
 * CreateJson(), which takes an explicit path so ReadJson() can also use it to replace a
 * malformed config found elsewhere-named during a read.
 */
void Widget::WriteJson( ){
    QString path = program_folder.path()+"/config.json";
    CreateJson(path);
}
extern int op_count;
/**
 * @details Snapshots every persisted setting (canvas size, fps, FFmpeg path, autosave
 * interval, handle size, terms-accepted flag) into a fresh QJsonObject and overwrites
 * the file at `path`. The sole writer of config.json; ReadJson() and TermsAndConditions()
 * both funnel through here rather than building JSON themselves.
 */
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
/**
 * @details Each of the six per-user folders (program root, libraries, temp render,
 * animations, stick figures, video export) is created independently if missing — a
 * fresh install has none of them yet, but a folder deleted by hand later should also be
 * silently recreated rather than causing every subsequent file operation to fail.
 */
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
/**
 * @details Iterates every entry in the libraries folder and adds one list item per
 * *.marlib file found, storing its full path as the item's user-role data (read back by
 * on_libraryList_currentRowChanged() and friends). Called at startup and after any
 * operation that adds/removes/renames a library file, since the list widget has no
 * other way to notice filesystem changes made outside the app.
 */
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
    advanced_ik_tab->setParent(nullptr);
    delete advanced_ik_tab;
    advanced_draw_tab->setParent(nullptr);
    delete advanced_draw_tab;
    delete ui;
}


/// @details Switches to the freehand line tool (MyView::mousePressEvent() dispatches on it).
void Widget::on_drawBtn_clicked()
{
    view->SetTool(DRAW);
}

/// @details Switches back to plain selection/arrow mode (no active drawing/edit tool).
void Widget::on_cursor_clicked()
{
    view->SetTool(NOTOOL);
}

/// @details Snapshots undo, then delegates to AddStick() for the actual layer creation.
void Widget::on_addStickBtn_clicked()
{
    view->ExitDrawMode(); // aggiungere un layer chiude la sessione di disegno (nuovo layer diventa corrente)
    view->StoreUndo();
    AddStick();
}

StickFigure* Widget::AddStick(){
    StickFigure * added = view->my_animation->CurrentFrame()->AddStickFigure(ui->stickLayerView, stick_figure_name_text->toPlainText());
    stick_figure_name_text->clear();
    return added;
}
/// @details Mirrors the new pen width onto the (read-only display) thickness slider.
void Widget::on_thicknessSpinBox_valueChanged(int arg1)
{
    view->my_pen.setWidth(arg1);
    ui->lineThicknessSlider->setValue(arg1);
}

/**
 * @details Designer fires this both when the user clicks a different layer row and when
 * a row is removed out from under the current selection (current == nullptr in that
 * case, correctly ignored here since there is nothing to select).
 */
void Widget::on_stickLayerView_currentItemChanged(QListWidgetItem *current, QListWidgetItem *previous)
{
    // cambiare layer selezionato chiude la sessione di disegno (finalizza il freehand del
    // layer corrente prima di spostarsi). Guardato su draw_mode così i cambi di selezione
    // PROGRAMMATICI (ripopolamento lista) non paghino nulla fuori da draw mode
    if(view->draw_mode)
        view->ExitDrawMode();
    if(current != nullptr)
    {
        qDebug("layer clicked");
        QVariant  retrievedData = (current->data(Qt::UserRole));
        StickFigure* cs = qvariant_cast<StickFigure*>(retrievedData);
        view->my_animation->UpdateSelection(cs);

    }

}

/// @details Deletes the stickfigure layer currently selected in the layer list widget
/// (or the whole multi-selection when one is active).
void Widget::on_deleteStickFigureBtn_clicked()
{
    // finalizza la sessione di disegno prima di rimuovere il layer (che libererebbe lo
    // stick del freehand → dangling / use-after-free)
    view->ExitDrawMode();
    if(view->MultiSelectionActive()){
        view->DeleteMultiSelection();
        return;
    }
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

/// @details Switches to the circle-drawing tool.
void Widget::on_drawCircleBtn_clicked()
{
    view->SetTool(DRAWCIRCLE);
}

/// @details Switches to the elongate tool (drag a stick's free end to lengthen/shorten it).
void Widget::on_elongateBtn_clicked()
{
    view->SetTool(ELONGATE);
}

/// @details In draw mode steps the draw-session stroke chain, otherwise the frame undo
/// stack — MyView::Undo() routes it (same as the Ctrl+Z shortcut).
void Widget::on_undoBtn_clicked()
{
    view->Undo();
}

/// @details Draw-mode-aware redo, mirror of on_undoBtn_clicked(); MyView::Redo() routes it.
void Widget::on_redoBtn_clicked()
{
    view->Redo();
}
/// @details Delegates to AddFrame(); the undo snapshot is taken there, not here.
void Widget::on_addFrameBtn_clicked()
{
    AddFrame();
}
/**
 * @details Inserts the new frame right after whichever frame is current (or at position
 * 0 for the very first frame), moves to it, and gives it one empty stickfigure layer so
 * the user always has something to draw into immediately.
 */
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

/// @details Thin forwarder — see the header doc for MyView::SetUpFrame()'s actual work.
Frame* Widget::SetUpFrame(int pos){
    return view->SetUpFrame(pos);

}
/**
 * @details Extracts the current frame into a fresh FrameData snapshot, creates a new
 * frame right after it, and repopulates that new frame from the snapshot
 * (PopulateLiveFrame()) — i.e. a duplicate, not a reference: editing the copy never
 * touches the original.
 */
void Widget::on_copyFrame_clicked()
{
    view->ExitDrawMode(); // finalizza il disegno prima di duplicare il frame (snapshot pulito)
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

/// @details Re-renders the onion-skin overlay for the new skin count.
void Widget::on_onionSkinSpinBox_valueChanged(const QString &arg1)
{
    view->UpdateOnionSkins();
}

/// @details Refuses to delete the last remaining frame — an animation always has at least one.
void Widget::on_deleteFrameBtn_clicked()
{
    // finalizza un'eventuale sessione di disegno PRIMA di cancellare il frame (che
    // libererebbe lo stick del freehand → dangling); così lo snapshot cattura il disegno
    // già ritagliato e non c'è use-after-free
    view->ExitDrawMode();
    if(view->my_animation->frame_list.count()>1){
        view->StoreUndo(CMD_DELETEFRAME);
        view->DeleteFrame(view->my_animation->CurrentFrame());
        view->MoveToFrame(view->my_animation->CurrentFrame());
    }
}

/// @details Switches the displayed frame to whichever row was clicked in the frame list.
void Widget::on_frameListWidget_itemClicked(QListWidgetItem *item)
{
    view->ExitDrawMode(); // il cambio frame chiude la sessione di disegno (finalizza sul frame corrente)
    view->StoreUndo(CMD_MOVETOFRAME);
    QVariant  retrievedData = (item->data(Qt::UserRole));
    Frame* cs = qvariant_cast<Frame*>(retrievedData);

    view->MoveToFrame(cs);
}

/// @details Prompts for a new name and renames the double-clicked layer row.
void Widget::on_stickLayerView_itemDoubleClicked(QListWidgetItem *item)
{
    QString text = QInputDialog::getText(this,"rename layer","text");
    QListWidgetItem *myItem = my_stick_figure_widget_list->item((my_stick_figure_widget_list->currentRow()));
    myItem->setData(Qt::DisplayRole, text);
    QVariant  retrievedData = (item->data(Qt::UserRole));
    StickFigure* S = qvariant_cast<StickFigure*>(retrievedData);
    S->name = text;
}
MySleeper *play = nullptr;
/**
 * @details First click: jumps to frame 0, hides onion skins for the duration of
 * playback, computes a per-frame delay from the fps spinbox, and starts a MySleeper
 * (kept in the global `play`) which renders every frame once and then flips through
 * them on its own timer. Second click (while `play` is non-null, i.e. playback is
 * running — this replaces the old separate `busy_play` flag, which always mirrored
 * exactly that condition): stops that MySleeper instead of starting a new one — this
 * button is a toggle, not a one-shot action.
 */
void Widget::on_PlayButton_clicked()
{
    if(play == nullptr){
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
        play = new MySleeper(view,delay);
        onion_skin_sb->setValue(onions);
    }else{
        play->Stop();
    }
    //view->MoveToFrame(view->my_animation->frame_list[0]);

}

/**
 * @details `zoom_lvl` tracks the view's current absolute scale factor (QGraphicsView::
 * scale() is relative to the current transform, not absolute), so each handler must
 * first undo the old scale (`1/zoom_lvl`) before applying the new one. MoveToFrame()
 * afterward re-centers the view on the current frame at the new zoom.
 */
void Widget::on_zoomInBtn_clicked()
{
    view->scale(1/zoom_lvl,1/zoom_lvl);
    zoom_lvl*=2;
    view->scale(zoom_lvl,zoom_lvl);
    view->MoveToFrame(view->my_animation->CurrentFrame());
}

/// @details See on_zoomInBtn_clicked() for why the scale is un-applied before re-applying.
void Widget::on_zoomOutBtn_clicked()
{

    view->scale(1/zoom_lvl,1/zoom_lvl);
    zoom_lvl/=2;
    view->scale(zoom_lvl,zoom_lvl);
    view->MoveToFrame(view->my_animation->CurrentFrame());
}

/// @details Returns the canvas to 1:1 zoom.
void Widget::on_resetZoomBtn_clicked()
{
    view->scale(1/zoom_lvl,1/zoom_lvl);
    zoom_lvl = 1.0;
    view->MoveToFrame(view->my_animation->CurrentFrame());
}

/**
 * @details Loads the chosen file into `image_draw_buffer` and switches to the
 * image-drawing tool; MyView::DrawCmd() reads this buffer when the user actually places
 * the image stick, so nothing is added to the scene until then.
 */
void Widget::on_drawImageBtn_clicked()
{
    QString file_name = QFileDialog::getOpenFileName(this,tr("Load Image"),
                       "C:/", tr("Images (*.png *.bmp *.jpg)"));
    if(QFile::exists(file_name)){
        // SetTool PRIMA di riempire il buffer: se il tool attivo è già DRAWIMG (l'utente
        // sceglie una seconda immagine), ChangeTool() libera il buffer precedente — con
        // l'ordine invertito liberava quello appena creato (leak del vecchio + crash al
        // primo click di disegno su un puntatore nullo)
        view->SetTool(DRAWIMG);
        image_draw_buffer = new QImage(file_name );
        image_name_buffer = QFileInfo(QFile(file_name)).fileName();
    }

}
/// @details Appends another image variant to the current IMAGE stick's image list.
void Widget::on_addImgBtn_clicked()
{
    StickData* cs = view->my_animation->CurrentFrame()->CurrentStick();
    if(cs == nullptr) // nessuno stick corrente: cs->view sotto dereferenziava un nullo
        return;
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

/// @details Removes the image list's currently-selected variant; refuses to remove the last one, since an IMAGE stick must always have at least one variant.
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

/// @details Sets the stick's displayed image to whichever variant is selected in the image list.
void Widget::on_setImgBtn_clicked()
{
    int idx = img_list_widget->currentRow(); // indice immagine correntmente selezionata
    if(view->my_animation->CurrentFrame()->CurrentStick() != nullptr){
        // idx viene dal widget: senza selezione è -1, e la lista immagini dello stick può
        // essere più corta delle righe mostrate — indicizzarla senza controllo legge fuori dai limiti
        if(view->my_animation->CurrentFrame()->CurrentStick()->stick_type == StickData::IMAGE
                && idx >= 0 && idx < view->my_animation->CurrentFrame()->CurrentStick()->stick_img_list.count()){
            view->StoreUndo();
            view->my_animation->CurrentFrame()->CurrentStick()->stick_img = view->my_animation->CurrentFrame()->CurrentStick()->stick_img_list[idx];
            view->my_animation->CurrentFrame()->CurrentStickFigure()->Refresh(0);
        }
    }
}

/**
 * @details Clears the "drag in progress" flag for this slider (see `sliderFlags`) so
 * the next drag on it takes a fresh undo snapshot instead of continuing to coalesce
 * into the one already on the stack.
 */
void Widget::on_imgHOffsetSlider_sliderReleased()
{
    sliderFlags.sliderHOffset = false;
}

/// @details See on_imgHOffsetSlider_sliderReleased() — same coalescing-flag pattern, for the vertical offset slider.
void Widget::on_imgVOfsetSlider_sliderReleased()
{
    sliderFlags.sliderVOffset = false;
}

/// @details See on_imgHOffsetSlider_sliderReleased() — same coalescing-flag pattern, for the width-scale slider.
void Widget::on_imgWSlider_sliderReleased()
{
    sliderFlags.sliderWScale = false;
}

/// @details See on_imgHOffsetSlider_sliderReleased() — same coalescing-flag pattern, for the height-scale slider.
void Widget::on_imgHSlider_sliderReleased()
{
    sliderFlags.sliderHScale = false;
}

/// @details See on_imgHOffsetSlider_sliderReleased() — same coalescing-flag pattern, for the rotation slider.
void Widget::on_imgRotationSlider_sliderReleased()
{
    sliderFlags.sliderRot = false;
}



/// @details Saves the current (in-memory, not yet on disk) library buffer as a new *.marlib file, then refreshes the library file list to include it.
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



/// @details Adds the current stickfigure to the currently-open library buffer as a reusable item.
void Widget::on_addItemToLibBtn_clicked()
{
    view->AddStickFigureToLibrary();
}

/// @details Replaces the current library buffer with the contents of the chosen *.marlib file.
void Widget::on_loadLibraryBtn_clicked()
{
    QString file_name = QFileDialog::getOpenFileName(this,tr("load marionetta library"),
                       lib_folder.path(),"Marionetta Library(*.marlib)");
    if(QFile::exists(file_name)){
        view->LoadLibrary(file_name);
    }
}

/// @details Rescans the libraries folder in case files were added/removed outside the app.
void Widget::on_refreshLibrariesBtn_clicked()
{
    my_library_list_widget->clear();
    DetectLibraries();
}

/// @details Loads the newly-selected library file's contents into the current-library item view.
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

/// @details Removes the currently-selected item from the open library buffer.
void Widget::on_removeFromLibraryBtn_clicked()
{
    view->RemoveFromLibrary();
}

/**
 * @details Adds a new stickfigure layer populated from the selected library item
 * (PopulateLiveStickFigure() from the item's stored StickFigureData). Guards against a
 * click with nothing actually selected first, so an empty click can't add a stray blank
 * layer or dereference a nonexistent item.
 */
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

/// @details Re-saves the open library buffer back to the *.marlib file it was loaded from.
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

/**
 * @details `sliderFlags.sliderHOffset` coalesces an entire drag into one undo snapshot:
 * the first valueChanged() of a drag takes it (and raises the flag), every subsequent
 * one during the same drag does not — sliderReleased() lowers the flag again so the
 * next drag starts its own snapshot. Same pattern repeats for every image-property
 * slider below.
 */
void Widget::on_imgHOffsetSlider_valueChanged(int value)
{
    StickData*cs =  view->my_animation->CurrentFrame()->CurrentStick();
    if(cs == nullptr)
        return;
    else{
        // il POLYGON riusa l'offset dell'immagine per traslare in blocco i suoi vertici
        if(cs->stick_type == StickData::IMAGE || cs->stick_type == StickData::POLYGON){
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


/// @details Vertical counterpart of on_imgHOffsetSlider_valueChanged() — see its doc for the undo-coalescing pattern.
void Widget::on_imgVOfsetSlider_valueChanged(int value)
{
    StickData*cs =  view->my_animation->CurrentFrame()->CurrentStick();
    if(cs == nullptr)
        return;
    else{
        // vedi on_imgHOffsetSlider_valueChanged(): vale anche per i POLYGON
        if(cs->stick_type == StickData::IMAGE || cs->stick_type == StickData::POLYGON){
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

/// @details Width-scale counterpart of on_imgHOffsetSlider_valueChanged(); the ternary maps the slider's signed range onto a >0 scale factor (shrinking below 1 on the negative side instead of going negative).
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

/// @details Height-scale counterpart of on_imgWSlider_valueChanged() — same scale-factor mapping.
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

/// @details Rotation-offset counterpart of on_imgHOffsetSlider_valueChanged() — see its doc for the undo-coalescing pattern.
void Widget::on_imgRotationSlider_valueChanged(int value)
{
    StickData*cs =  view->my_animation->CurrentFrame()->CurrentStick();
    if(cs == nullptr)
        return;
    else{
        // vedi on_imgHOffsetSlider_valueChanged(): il POLYGON ruota i vertici attorno a P1
        if(cs->stick_type == StickData::IMAGE || cs->stick_type == StickData::POLYGON){
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

/**
 * @details These five spinboxes are the numeric-entry twin of the sliders above —
 * mirroring the typed value onto the slider is what actually applies it, since the
 * sliders' own valueChanged() handlers hold the undo/property-update logic.
 */
void Widget::on_imgHOffsetSpinbox_valueChanged(int arg1)
{
    img_h_offset_slider->setValue(img_h_offset_spinbox->value());
}

/// @details See on_imgHOffsetSpinbox_valueChanged() — mirrors onto the vertical-offset slider.
void Widget::on_imgVOffsetSpinbox_valueChanged(int arg1)
{
    img_v_offset_slider->setValue(img_v_offset_spinbox->value());
}

/// @details See on_imgHOffsetSpinbox_valueChanged() — mirrors onto the width-scale slider.
void Widget::on_imgWidthSpinbox_valueChanged(int arg1)
{
    img_width_slider->setValue(img_width_spinbox->value());
}

/// @details See on_imgHOffsetSpinbox_valueChanged() — mirrors onto the height-scale slider.
void Widget::on_imgHeightSpinbox_valueChanged(int arg1)
{
    img_height_slider->setValue(img_height_spinbox->value());
}

/// @details See on_imgHOffsetSpinbox_valueChanged() — mirrors onto the rotation slider.
void Widget::on_imgRotationSpinbox_valueChanged(int arg1)
{
    img_rotation_slider->setValue(img_rotation_spinbox->value());
}
/// @details Opens a color picker and sets it as the pen color for future drawing (and updates the swatch button's background to match).
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
/**
 * @details `SetGraphics(all, attribute)`'s first argument is the actual difference
 * between each of these button pairs below: false applies to just the current stick,
 * true applies to every stick in the current stickfigure.
 */
void Widget::on_setStickColorBtn_clicked()
{
    view->SetGraphics(false, ATTRIBUTE_PENCOLOR);
}

/// @details All-sticks counterpart of on_setStickColorBtn_clicked() — see its doc.
void Widget::on_setAllSticksColorBtn_clicked()
{
    view->SetGraphics(true,ATTRIBUTE_PENCOLOR);
}

/// @details Single-stick pen-width counterpart — see on_setStickColorBtn_clicked()'s doc.
void Widget::on_setPenThicknessBtn_clicked()
{
    view->SetGraphics(false, ATTRIBUTE_PENWIDTH);
}

/// @details All-sticks pen-width counterpart — see on_setStickColorBtn_clicked()'s doc.
void Widget::on_setPenThicknessAllBtn_clicked()
{
    view->SetGraphics(true, ATTRIBUTE_PENWIDTH);
}

/// @details Opens a color picker for the fill/brush color, applying the current fill-opacity slider value as alpha.
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

/// @details All-sticks brush-color counterpart — see on_setStickColorBtn_clicked()'s doc.
void Widget::on_setAllBrushColorBtn_clicked()
{
    view->SetGraphics(true, ATTRIBUTE_BRUSHCOLOR);
}

/// @details Single-stick brush-color counterpart — see on_setStickColorBtn_clicked()'s doc.
void Widget::on_setBrushColorBtn_clicked()
{
    view->SetGraphics(false, ATTRIBUTE_BRUSHCOLOR);
}

/// @details Switches to the manipulate tool (drag a stick's free end, dragging its children along).
void Widget::on_manipulateBtn_clicked()
{
    view->SetTool(MANIPULATE);
}



/// @details Applies the slider's value as alpha on the current pen color, mirroring it onto the numeric spinbox.
void Widget::on_penOpacitySlider_valueChanged(int value)
{
    QColor c = view->my_pen.color();
    c.setAlphaF((float)pen_opacity_slider->value()/100);
    view->my_pen.setColor(c);
    pen_opacity_spinbox->setValue(pen_opacity_slider->value());
}

/// @details Brush-opacity counterpart of on_penOpacitySlider_valueChanged().
void Widget::on_fillOpacitySlider_valueChanged(int value)
{
    QColor c = view->my_brush.color();
    c.setAlphaF((float)brush_opacity_slider->value()/100);
    view->my_brush.setColor(c);
    brush_opacity_spinbox->setValue(brush_opacity_slider->value());
}

/// @details Numeric-entry twin of the pen-opacity slider — mirrors onto it.
void Widget::on_penOpacitySpinbox_valueChanged(int arg1)
{
    pen_opacity_slider->setValue(pen_opacity_spinbox->value());
}

/// @details Numeric-entry twin of the brush-opacity slider — mirrors onto it.
void Widget::on_fillOpacitySpinbox_valueChanged(int arg1)
{
    brush_opacity_slider->setValue(brush_opacity_spinbox->value());
}


/// @details Switches to the whole-stickfigure rotate tool (drag to rotate every stick together around the figure's origin).
void Widget::on_rotateStickFigureBtn_clicked()
{
    view->SetTool(ROTATE);
}

/// @details Switches to the whole-stickfigure scale tool (drag to resize every stick together, proportionally).
void Widget::on_scaleStickFigureBtn_clicked()
{
    view->SetTool(SCALE);
}

/**
 * @details Numeric alternative to the interactive scale tool: snapshots each stick's
 * pre-scale length/pen-width as the reference (mirroring what the interactive drag does
 * on press), applies the percentage from the spinbox to every stick in the current
 * figure, then resets the spinbox back to 100 (the control is a one-shot multiplier,
 * not a persistent scale-factor display).
 */
void Widget::on_stickFigureScaleSpinbox_editingFinished()
{
    // la guardia sotto arrivava DOPO il primo ciclo, che dereferenziava già la figura corrente
    if(view->my_animation->CurrentFrame()->CurrentStickFigure() == nullptr)
        return;
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

/**
 * @details Numeric alternative to the interactive rotate tool: applies the spinbox's
 * degree value as an absolute rotation of every stick in the current figure around its
 * origin, then resets the spinbox to 0 (a one-shot rotation amount, not a persistent
 * angle display, mirroring on_stickFigureScaleSpinbox_editingFinished()'s pattern).
 */
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

/// @details Switches to the whole-scene pan tool (drag to move every stickfigure in the frame together).
void Widget::on_moveSceneBtn_clicked()
{
    view->SetTool(MOVESCENE);
}

/// @details Switches to the whole-scene scale tool (drag to resize every stickfigure in the frame together).
void Widget::on_scaleSceneBtn_clicked()
{
    view->SetTool(SCALESCENE);
}

/// @details Switches to the whole-scene rotate tool (drag to rotate every stickfigure in the frame together).
void Widget::on_rotateSceneBtn_clicked()
{
    view->SetTool(ROTATESCENE);
}

/// @details Flips one figure with the historical per-stick rotation trick (the fixed
/// point is the figure's master origin); `horizontal` picks the mirror axis.
static void FlipOneFigure(StickFigure *figure, bool horizontal)
{
    for(StickData* s:figure->stick_list){
        if(s->view){
            s->view->angle_buffer2 = s->my_line.angle();
            s->view->Rotate(horizontal ? 2*(90-s->my_line.angle())
                                       : (-2*s->my_line.angle()));
        }
    }
}

/// @details Mirrors every stick in the current figure across its own horizontal axis by
/// rotating each 180° minus twice its current angle. With a multi-selection the flip
/// pivots on the transform handle: each figure flips in place, then its anchor is
/// mirrored across the handle's horizontal axis.
void Widget::on_verticalFlipBtn_clicked()
{
    if(view->MultiSelectionActive()){
        view->StoreUndo();
        QPointF center = view->multi_pivot; // il perno visibile della maniglia
        for(StickFigure *S : view->multi_selection){
            if(S->master_stick == nullptr || S->stick_list.isEmpty())
                continue;
            FlipOneFigure(S, false);
            S->Traslate(0, 2*(center.y() - S->master_stick->my_line.p1().y()));
        }
        view->UpdateMultiSelectionUI();
        return;
    }
    if(view->my_animation->CurrentFrame()->CurrentStickFigure() != nullptr){
        view->StoreUndo(); // mancava: il flip era l'unica mutazione senza snapshot
        FlipOneFigure(view->my_animation->CurrentFrame()->CurrentStickFigure(), false);
    }
}

/// @details Vertical-axis counterpart of on_verticalFlipBtn_clicked() — same rotation trick, mirrored the other way.
void Widget::on_HorizontalFlipBtn_clicked()
{
    if(view->MultiSelectionActive()){
        view->StoreUndo();
        QPointF center = view->multi_pivot; // il perno visibile della maniglia
        for(StickFigure *S : view->multi_selection){
            if(S->master_stick == nullptr || S->stick_list.isEmpty())
                continue;
            FlipOneFigure(S, true);
            S->Traslate(2*(center.x() - S->master_stick->my_line.p1().x()), 0);
        }
        view->UpdateMultiSelectionUI();
        return;
    }
    if(view->my_animation->CurrentFrame()->CurrentStickFigure() != nullptr){
        view->StoreUndo(); // mancava, come nel flip verticale
        FlipOneFigure(view->my_animation->CurrentFrame()->CurrentStickFigure(), true);
    }
}

/// @details Snapshots the current stickfigure into `view->copy_stick_figure_buffer` (replacing whatever was copied before) for on_pasteStickFigureBtn_clicked() to consume. With a multi-selection active, every selected figure is snapshotted into `view->multi_copy_buffer` instead.
void Widget::on_copyStickFigureBtn_clicked()
{
    view->ExitDrawMode(); // finalizza il disegno prima di estrarre la figura (snapshot pulito)
    // la copia più recente vince: copiare svuota SEMPRE l'altro buffer
    qDeleteAll(view->multi_copy_buffer);
    view->multi_copy_buffer.clear();
    if(view->MultiSelectionActive()){
        for(StickFigure *S : view->multi_selection){
            if(!S->stick_list.isEmpty())
                view->multi_copy_buffer.append(ExtractStickFigureData(*S));
        }
        return;
    }
    if(view->my_animation->CurrentFrame()->CurrentStickFigure() != nullptr){
        // il buffer è puro dato (Phase 2b): liberarlo e riestrarlo è tutto qui,
        // niente copyFlag e niente pulizia manuale degli stick
        delete view->copy_stick_figure_buffer;
        view->copy_stick_figure_buffer = ExtractStickFigureData(*view->my_animation->CurrentFrame()->CurrentStickFigure());
    }
}

/// @details Adds a new layer populated from the copy buffer (PopulateLiveStickFigure()); refuses with an error dialog if nothing has been copied yet this session. A multi-selection copy pastes one new layer per copied figure and selects the pasted set.
void Widget::on_pasteStickFigureBtn_clicked()
{
   view->ExitDrawMode(); // il paste aggiunge/riempie layer: chiudi prima la sessione di disegno
   if(!view->multi_copy_buffer.isEmpty()){
       view->StoreUndo(CMD_SIMPLE);
       QList<StickFigure*> pasted;
       for(StickFigureData *data : view->multi_copy_buffer){
           StickFigure* added = AddStick();
           PopulateLiveStickFigure(added,*data,true,true);
           added->Refresh(0);
           for(StickData*s:added->stick_list){
               view->my_animation->CurrentFrame()->total_sticks.append(s);
           }
           view->AddHandles(added);
           pasted.append(added);
       }
       view->SetMultiSelectionList(pasted); // le figure incollate diventano la selezione
       return;
   }
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

/**
 * @details Validates a stickfigure is selected and non-empty, then arms the JOIN tool —
 * the actual merge happens in MyView::mousePressEvent()'s JOIN case once the user picks
 * the second stickfigure to join into this one.
 */
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

/**
 * @details Welds the current stick back together with the stick(s) it was previously
 * Chop()-ped from (StickFigure::Weld()), and removes the welded-away sticks from the
 * frame's z-order list since they no longer exist as separate entries.
 */
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

/**
 * @details Splits the current stick into two at its midpoint (StickFigure::Chop()),
 * refreshing handles around the operation since the figure's stick count changes.
 */
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



/// @details Re-roots the current stickfigure's tree at the current stick (StickFigure::SetMaster()).
void Widget::on_setMasterNodeBtn_clicked()
{
    if(view->my_animation->CurrentFrame()->CurrentStickFigure() == nullptr ||
            view->my_animation->CurrentFrame()->CurrentStickFigure()->stick_list.isEmpty()||
            view->my_animation->CurrentFrame()->CurrentStick() == nullptr)
        return;
    view->StoreUndo();
    view->my_animation->CurrentFrame()->CurrentStickFigure()->SetMaster(view->my_animation->CurrentFrame()->CurrentStick());
}

/**
 * @details Saves the currently-open library first (so its in-progress edits aren't
 * lost), then creates a brand new, empty *.marlib file at the chosen path and switches
 * to it as the open library.
 */
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
        // il file può essere stato salvato FUORI dalla cartella librerie: DetectLibraries()
        // non lo elenca, e [0] su una lista vuota leggeva fuori dai limiti
        QList<QListWidgetItem*> found = my_library_list_widget->findItems(QFileInfo(file_name).fileName(),Qt::MatchExactly);
        if(!found.isEmpty()){
            my_library_list_widget->setCurrentRow(my_library_list_widget->row(found[0]));
        }
    }

}

/**
 * @details Sets every stick in the current figure to the same Z depth, then re-sorts
 * `total_sticks` by Z so push-to-top/bottom and move-stick-up/down (which walk that
 * list to find the next-nearest neighbor) see the new order immediately rather than
 * the stale pre-change one.
 */
void Widget::on_setAllDepthBtn_clicked()
{
    if(view->my_animation->CurrentFrame()->CurrentStickFigure() != nullptr
            && !view->my_animation->CurrentFrame()->CurrentStickFigure()->stick_list.isEmpty()){
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

/// @details Single-stick counterpart of on_setAllDepthBtn_clicked() — see its doc for the total_sticks re-sort.
void Widget::on_setDepthBtn_clicked()
{
    if(view->my_animation->CurrentFrame()->CurrentStickFigure() != nullptr
            && !view->my_animation->CurrentFrame()->CurrentStickFigure()->stick_list.isEmpty()
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

/// @details Mirrors the typed depth value onto the slider (scaled ×100, since the slider is integer-stepped and depth is a float).
void Widget::on_depthSpinbox_valueChanged(double arg1)
{
    depth_slider->setValue(arg1*100);
}

/// @details Mirrors the slider back onto the spinbox (÷100 — see on_depthSpinbox_valueChanged()).
void Widget::on_depthSlider_valueChanged(int value)
{
    float val = (float)value;
    depth_spinbox->setValue(val/100);
}

/// @details Switches to the rectangle-drawing tool.
void Widget::on_drawRectBtn_clicked()
{
        view->SetTool(DRAWRECT);
}

/// @details Debug/dev toggle: swaps the second tools-tab between the image-editing panel and the advanced-stick panel.
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

/// @details Switches to the taper-drawing tool.
void Widget::on_drawTaperBtn_clicked()
{
    view->SetTool(DRAWTAPER);
}

/// @details Switches to the freehand tool: two clicks place an invisible FREEHAND stick,
/// then MyView drops into draw mode (see DrawCmd()/EnterDrawMode()).
void Widget::on_drawFreehandBtn_clicked()
{
    view->SetTool(DRAWFREEHAND);
}

/// @details Switches to the trapezoid-drawing tool.
void Widget::on_drawTrapezoidBtn_clicked()
{
    view->SetTool(DRAWTRAPEZOID);
}

/// @details Translates every stick in the current figure so its master stick's origin lands at the canvas center.
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

/**
 * @details Moves the current stick to the end of `total_sticks` (frontmost draw order)
 * and raises its Z to just above the current maximum across every stick in the frame
 * (clamped to MAXDEPTH), so it's guaranteed to render in front of everything else, not
 * just reordered without a matching Z change.
 */
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

/// @details Bottom-of-stack counterpart of on_pushToTopBtn_clicked() — moves to index 0 and lowers Z to just below the current minimum (clamped to MINDEPTH).
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



/// @details Numeric alternative to the interactive elongate tool: resizes the current stick's line to the typed length, keeping its start point and angle fixed (StickFigure::Elongate()).
void Widget::on_elongateSpinbox_editingFinished()
{
    if(view->my_animation->CurrentFrame()->CurrentStickFigure()!= nullptr && view->my_animation->CurrentFrame()->CurrentStick() !=nullptr){
        QLineF lineBuf = view->my_animation->CurrentFrame()->CurrentStick()->my_line;
        lineBuf.setLength(elongate_spinbox->value());
        QPointF coord= lineBuf.p2();

        view->my_animation->CurrentFrame()->CurrentStickFigure()->Elongate(coord,view->my_animation->CurrentFrame()->CurrentStick());
    }

}

/// @details Moves the entire current stickfigure layer to the very front of the frame's draw order.
void Widget::on_pushStickFigureToTop_clicked()
{
    view->StoreUndo();
    view->MoveStickFigureZ(0,MODE_TOP);
}

/// @details Moves the entire current stickfigure layer to the very back of the frame's draw order.
void Widget::on_pushStickFigureToBottom_clicked()
{
    view->StoreUndo();
    view->MoveStickFigureZ(0,MODE_BOTTOM);
}

/**
 * @details Moves the current stick one step forward in draw order: first looks for
 * another stick at the exact same Z already ahead of it in `total_sticks` (a plain
 * reorder, no Z change); failing that, finds the next-higher Z across the whole frame
 * and adopts it (a Z change, since there was no tie to slot in next to).
 */
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

/// @details Mirror image of on_moveStickUp_clicked() — looks backward instead of forward for a same-Z tie, then the next-lower Z across the frame.
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

/**
 * @details Confirms before deleting the *.marlib file itself; the list row is removed
 * either way (via the `removeLibraryGoto` label) whether the on-disk file existed and
 * was actually deleted or had already gone missing out from under the app.
 */
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
/// @details Snapshots the current frame into `frame_copy_buffer` for on_pasteFrame_clicked() to consume.
void Widget::on_cpyFrame_clicked()
{
    // il buffer è puro dato (Phase 2b): estrazione diretta, niente stream né flag
    delete frame_copy_buffer;
    frame_copy_buffer = ExtractFrameData(*view->my_animation->CurrentFrame());
}

/**
 * @details Adds one new layer per stickfigure in the copy buffer to the *current*
 * frame (not a new one) — pasting a frame merges its content into whatever frame is
 * displayed, rather than creating a separate frame for it. The last figure pasted
 * becomes the new current selection.
 */
void Widget::on_pasteFrame_clicked()
{
    view->ExitDrawMode(); // il paste aggiunge layer al frame corrente: chiudi prima il disegno
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


/// @details Mirrors the slider back onto the numeric spinbox (see on_thicknessSpinBox_valueChanged() for the reverse direction).
void Widget::on_lineThicknessSlider_sliderMoved(int position)
{
    ui->thicknessSpinBox->setValue(position);
}

/// @details Switches to the clamp tool.
void Widget::on_clampToolBtn_clicked()
{
    view->SetTool(CLAMP);
}
///@brief switches to the polygon drawing tool
void Widget::on_addPolyBtn_clicked()
{
    view->SetTool(DRAWPOLY);
}

///@brief switches to the multi-line drawing tool
void Widget::on_multiLineBtn_clicked()
{
    view->SetTool(DRAWMULTI);
}

///@brief adds or deletes ik chains
void Widget::on_addIkBtn_clicked()
{
    view->SetTool(DRAW_IK);
}

