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
#include <QLineEdit>
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
#include <QButtonGroup>
#include <QToolButton>
#include <QPixmap>
#include <QPainter>
#include <QSignalBlocker>
#include <QComboBox>
#include <QFontComboBox>
#include <QTextEdit>
#include <QSplitter>
#include <QScrollArea>
#include <QTabBar>
#include <QStyle>
#include <QDialog>
#include <QDialogButtonBox>
#include <QRadioButton>
#include <QApplication>
#include "advancedtab.h"
#include "advancedlinewidget.h"
#include "advancedcirclewidget.h"
#include "advancedtaperwidget.h"
#include "uiItems.h"
#include "imageexport.h"
#include "abrbrush.h"
#include "brushpicker.h"
#include "gradientdialog.h"
#include "patternlibrary.h"
#include "textureoptionsdialog.h"
#define DEFAULTWIDTH 720
#define DEFAULTHEIGHT 480
#define DEFAULTFPS 24
#define DEFAULTAUTOSAVE 50
#define DEFAULTHANDLES 10
//#include "QVideoEncoder.h"
//#include "QVideoDecoder.h"
QString image_name_buffer;
QGraphicsScene *scene;
Ui::Widget *my_ui;
MyView* V;
QList <StickFigure*> layer_list;
QTextEdit * stick_figure_name_text;
QGraphicsRectItem *my_rect;
QGraphicsRectItem *limit_rect;
QDir program_folder;
QDir lib_folder;
QDir brush_folder; ///< cartella pennelli .abr, accanto a Libraries/myStickFigures (creata in CreatePaths)
QDir pattern_folder; ///< cartella pattern .pat, gemella di Brushes: il suo path è l'unico argomento di PatternLibrary (creata in CreatePaths)
QDir temp_render_folder;
QDir my_animations_folder;
QDir my_stick_figures_folder;
QDir my_video_export_folder;
// -----------------
QComboBox * fill_type_combo = nullptr;   ///< vedi uiItems.h: la costruisce BuildStyleCombos(), la disabilita UpdateSliders()
QComboBox * convert_type_combo = nullptr; ///< vedi uiItems.h: riga "Convert to", spostata da UpdateTab()
QComboBox * stroke_type_combo = nullptr; ///< idem, per il contorno
QCheckBox * brush_follows_check = nullptr; ///< idem: l'ancoraggio della texture allo stick
QCheckBox * texture_tiled_check = nullptr; ///< idem: la ripetizione della tile nel riempimento
QCheckBox * fill_texture_arrow_check = nullptr;   ///< idem: accende la freccia della texture di riempimento
QCheckBox * stroke_texture_arrow_check = nullptr; ///< idem, per quella del contorno
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
QWidget * advanced_transform_tab;
QWidget * advanced_vertex_tab;
QWidget * advanced_text_tab; ///< pannello dei TEXT stick; costruito nel ctor di Widget
std::function<void(StickData *)> refresh_text_panel; ///< vedi uiItems.h
std::function<void(StickData *)> refresh_transform_panel; ///< vedi uiItems.h
QWidget * advanced_draw_tab; ///< freehand draw-mode panel (brush/eraser/size/opacity/color); built in Widget's ctor
QWidget * advanced_ik_tab;
QCheckBox * ik_active_check;
QWidget * library_tab;
QWidget * vertex_width_row;
QSlider * vertex_width_slider;
QSlider * vertex_curve_slider;
AdvancedLineWidget *advanced_line_tab;
AdvancedCircleWidget *advanced_circle_tab;
AdvancedTaperWidget * advanced_taper_tab;
QSlider*  circle_squash_slider;
QSpinBox * circle_squash_spinbox;
// frame_copy_buffer è diventato un membro privato di Widget (widget.h), ed è una LISTA:
// si copiano N frame contigui in un colpo

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
 * tabs, then loads (or creates, on first run) the default library and config.json.
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
    library_tab = ui->libraryTab;
    ui->toolsTabWidget->setCurrentWidget(ui->drawingTab);
    my_stick_figure_widget_list = ui->stickLayerView;
    my_library_list_widget = ui->libraryList;
    my_current_library_widget = ui->currentLibrary;
    my_frame_widget_list = ui->frameListWidget;
    // shift+clic seleziona un BLOCCO contiguo di frame (copia/incolla e cancellazione
    // multipla): la modalità nativa di Qt è tutto quello che serve, e a differenza di
    // ExtendedSelection non permette selezioni sparse, che nessuna di quelle operazioni
    // saprebbe trattare come un blocco
    my_frame_widget_list->setSelectionMode(QAbstractItemView::ContiguousSelection);
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

    // ---- pannello "Stick Transform" (ogni tipo di stick): offset, scala e rotazione erano il ----
    // ---- gruppo in cima al pannello immagine, e sono proprio quei widget — riparentati, non ----
    // ---- ricostruiti, così gli slot on_img*_valueChanged() che il .ui auto-connette restano --
    // Ha il suo QScrollArea per la stessa ragione del pannello immagine: un QTabWidget è alto
    // quanto la sua pagina più alta, e il gruppo è alto 400 px fissi.
    advanced_transform_tab = new QWidget();
    {
        QVBoxLayout *col = new QVBoxLayout(advanced_transform_tab);
        col->setContentsMargins(0, 0, 0, 0);
        // Local e Global: gli stessi controlli su uno dei due insiemi di parametri dello stick
        // (StickData::local_xf / global_xf). Una barra senza pagine invece di due pagine di
        // controlli duplicati, così gli slot auto-connessi dal .ui restano uno per controllo
        transform_mode_bar = new QTabBar(advanced_transform_tab);
        transform_mode_bar->addTab(tr("Local"));
        transform_mode_bar->addTab(tr("Global"));
        transform_mode_bar->setTabToolTip(0, tr("Moves this stick's shape only"));
        transform_mode_bar->setTabToolTip(1, tr("Moves this stick's shape and all of its "
                                                "children's, about this stick's joint"));
        col->addWidget(transform_mode_bar);
        QScrollArea *scroll = new QScrollArea(advanced_transform_tab);
        scroll->setWidgetResizable(true);
        scroll->setWidget(ui->groupBox_5); // riparenta, e questo lo toglie dal pannello immagine
        col->addWidget(scroll, 1);
        QHBoxLayout *flips = new QHBoxLayout();
        flip_h_check = new QCheckBox(tr("Flip horizontal"), advanced_transform_tab);
        flip_v_check = new QCheckBox(tr("Flip vertical"), advanced_transform_tab);
        flip_h_check->setToolTip(tr("Mirror across the stick"));
        flip_v_check->setToolTip(tr("Mirror along the stick"));
        flips->addWidget(flip_h_check);
        flips->addWidget(flip_v_check);
        col->addLayout(flips);

        refresh_transform_panel = [this](StickData *cs){
            const StickXf &xf = EditedXf(cs);
            // eco a segnali zittiti: riallineare i controlli allo stick selezionato passerebbe
            // altrimenti per una modifica dell'utente e spingerebbe un undo a vuoto
            auto echo = [](QSlider *slider, QSpinBox *box, int value){
                QSignalBlocker mute_slider(slider), mute_box(box);
                slider->setValue(value);
                box->setValue(value);
            };
            // l'inversa di SliderScale()
            auto scale_to_slider = [](qreal s){ return qRound(s >= 1 ? 10*(s-1) : -10*(1/s-1)); };
            echo(img_h_offset_slider, img_h_offset_spinbox, qRound(xf.offset.x()));
            echo(img_v_offset_slider, img_v_offset_spinbox, qRound(xf.offset.y()));
            echo(img_width_slider, img_width_spinbox, scale_to_slider(xf.scale.x()));
            echo(img_height_slider, img_height_spinbox, scale_to_slider(xf.scale.y()));
            echo(img_rotation_slider, img_rotation_spinbox, qRound(xf.angle));
            QSignalBlocker mute_h(flip_h_check), mute_v(flip_v_check);
            flip_h_check->setChecked(xf.flip_h);
            flip_v_check->setChecked(xf.flip_v);
        };
        connect(transform_mode_bar, &QTabBar::currentChanged, this, [this]{
            StickData *cs = view->my_animation->CurrentFrame()->CurrentStick();
            if(cs != nullptr)
                refresh_transform_panel(cs);
        });
        connect(flip_h_check, &QCheckBox::toggled, this, [this](bool on){ SetTransformFlip(true, on); });
        connect(flip_v_check, &QCheckBox::toggled, this, [this](bool on){ SetTransformFlip(false, on); });
    }

    // ---- pannello "vertici" per POLYGON/MULTILINE (costruito in codice: due sole ----
    // ---- righe slider, un .ui dedicato sarebbe stato più ingombrante del codice) ----
    // Larghezza (solo MULTILINE, la riga si nasconde per i POLYGON) e curvatura degli
    // spigoli (entrambi i tipi); agiscono sui vertici selezionati con Ctrl(+Shift).
    advanced_vertex_tab = new QWidget();
    {
        QVBoxLayout *rows = new QVBoxLayout(advanced_vertex_tab);
        // vertex_width_row raccoglie le righe solo-MULTILINE (larghezza e giunto): una sola
        // cosa da nascondere per i POLYGON (Animation::UpdateTab())
        vertex_width_row = new QWidget(advanced_vertex_tab);
        QVBoxLayout *multiline_rows = new QVBoxLayout(vertex_width_row);
        multiline_rows->setContentsMargins(0, 0, 0, 0);
        QWidget *width_line = new QWidget(vertex_width_row);
        QHBoxLayout *width_row_layout = new QHBoxLayout(width_line);
        width_row_layout->setContentsMargins(0, 0, 0, 0);
        width_row_layout->addWidget(new QLabel(tr("Vertex width"), width_line));
        vertex_width_slider = new QSlider(Qt::Horizontal, width_line);
        vertex_width_slider->setRange(0, W);
        QSpinBox *width_spin = new QSpinBox(width_line);
        width_spin->setRange(0, W);
        width_row_layout->addWidget(vertex_width_slider, 1);
        width_row_layout->addWidget(width_spin);
        multiline_rows->addWidget(width_line);
        // giunto/estremità dei vertici: quello dei prossimi vertici disegnati E dei vertici
        // selezionati con Ctrl (MyView::SetVertexCap()); i vertici di un ramo nascono tondi
        QWidget *cap_line = new QWidget(vertex_width_row);
        QHBoxLayout *cap_row_layout = new QHBoxLayout(cap_line);
        cap_row_layout->setContentsMargins(0, 0, 0, 0);
        cap_row_layout->addWidget(new QLabel(tr("Vertex cap"), cap_line));
        QComboBox *cap_combo = new QComboBox(cap_line);
        cap_combo->addItem(tr("Sharp"), int(StickData::CAP_SHARP));
        cap_combo->addItem(tr("Round"), int(StickData::CAP_ROUND));
        cap_combo->addItem(tr("Bevel"), int(StickData::CAP_BEVEL));
        // lo stesso default di MyView::my_vertex_cap (la view qui non esiste ancora), scelto
        // prima del connect così non lo rimanda indietro
        cap_combo->setCurrentIndex(cap_combo->findData(int(StickData::CAP_ROUND)));
        cap_combo->setToolTip(tr("How the vertices you draw next end or turn a corner; also "
                                 "applied to the vertices selected with Ctrl. Branch vertices "
                                 "are always drawn round, so forks never spike."));
        cap_row_layout->addWidget(cap_combo, 1);
        multiline_rows->addWidget(cap_line);
        connect(cap_combo, QOverload<int>::of(&QComboBox::currentIndexChanged), this,
                [this, cap_combo](int){ view->SetVertexCap(cap_combo->currentData().toInt()); });
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

    // ---- riga "Convert to": un solo widget per tutti i pannelli avanzati, che ----------
    // ---- Animation::UpdateTab() sposta in cima a quello del tipo selezionato ------------
    {
        QWidget *row = new QWidget();
        QHBoxLayout *row_layout = new QHBoxLayout(row);
        row_layout->setContentsMargins(0, 0, 0, 0);
        row_layout->addWidget(new QLabel(tr("Convert to"), row));
        convert_type_combo = new QComboBox(row);
        convert_type_combo->addItem(tr("Line"), StickData::LINE);
        convert_type_combo->addItem(tr("Circle"), StickData::CIRCLE);
        convert_type_combo->addItem(tr("Rectangle"), StickData::RECT);
        convert_type_combo->addItem(tr("Trapezoid"), StickData::TRAPEZOID);
        convert_type_combo->addItem(tr("Taper"), StickData::TAPER);
        convert_type_combo->addItem(tr("Polygon"), StickData::POLYGON);
        convert_type_combo->addItem(tr("Multiline"), StickData::MULTILINE);
        convert_type_combo->addItem(tr("Image"), StickData::IMAGE);
        convert_type_combo->addItem(tr("Freehand"), StickData::FREEHAND);
        convert_type_combo->addItem(tr("Text"), StickData::TEXT);
        row_layout->addWidget(convert_type_combo, 1);
        QPushButton *convert_btn = new QPushButton(tr("Convert"), row);
        row_layout->addWidget(convert_btn);
        connect(convert_btn, &QPushButton::clicked, this, [this]{
            Frame *frame = view->my_animation->CurrentFrame();
            if(frame->CurrentStickFigure() == nullptr || frame->CurrentStick() == nullptr)
                return;
            StickData *stick = frame->CurrentStick();
            int type = convert_type_combo->currentData().toInt();
            if(stick->stick_type == type)
                return;
            // ciò che il nuovo tipo deve ricevere dall'utente si chiede PRIMA: annullare
            // non deve lasciare né uno stick convertito a metà né un passo di undo vuoto
            QImage image;
            QString image_name, text;
            if(type == StickData::IMAGE){
                QString file_name = QFileDialog::getOpenFileName(this, tr("Load Image"),
                                    "C:/", tr("Images (*.png *.bmp *.jpg)"));
                image = QImage(file_name);
                if(image.isNull())
                    return;
                image_name = QFileInfo(file_name).fileName();
            }
            else if(type == StickData::TEXT){
                bool confirmed = false;
                text = QInputDialog::getMultiLineText(this, tr("Text stick"), tr("Text:"),
                                                      QString(), &confirmed);
                if(!confirmed || text.isEmpty())
                    return;
            }
            view->ConvertStick(stick, type, image, image_name, text);
        });
    }

    // ---- pannello "testo" per i TEXT stick (costruito in codice come i pannelli --------
    // ---- vertici/IK/draw). Offset e rotazione NON sono qui: sono la Stick Transform, ---
    // ---- il pannello che Animation::UpdateTab() aggiunge per ogni tipo -----------------
    advanced_text_tab = new QWidget();
    {
        QVBoxLayout *col = new QVBoxLayout(advanced_text_tab);
        QTextEdit *content = new QTextEdit(advanced_text_tab);
        content->setAcceptRichText(false); // il testo è puro: la formattazione è nel QFont
        content->setMinimumHeight(80);
        col->addWidget(content, 1);

        QHBoxLayout *font_row = new QHBoxLayout();
        QFontComboBox *font_combo = new QFontComboBox(advanced_text_tab); // font installati nel sistema
        QSpinBox *size_spin = new QSpinBox(advanced_text_tab);
        size_spin->setRange(1, 400);
        size_spin->setValue(QFont().pointSize() > 0 ? QFont().pointSize() : 12);
        font_row->addWidget(font_combo, 1);
        font_row->addWidget(size_spin);
        col->addLayout(font_row);

        QHBoxLayout *style_row = new QHBoxLayout();
        QCheckBox *bold_check = new QCheckBox(tr("Bold"), advanced_text_tab);
        QCheckBox *italic_check = new QCheckBox(tr("Italic"), advanced_text_tab);
        QCheckBox *underline_check = new QCheckBox(tr("Underline"), advanced_text_tab);
        QCheckBox *strikeout_check = new QCheckBox(tr("Strikeout"), advanced_text_tab);
        style_row->addWidget(bold_check);
        style_row->addWidget(italic_check);
        style_row->addWidget(underline_check);
        style_row->addWidget(strikeout_check);
        style_row->addStretch(1);
        col->addLayout(style_row);

        QHBoxLayout *align_row = new QHBoxLayout();
        align_row->addWidget(new QLabel(tr("Justification"), advanced_text_tab));
        QComboBox *align_combo = new QComboBox(advanced_text_tab);
        align_combo->addItem(tr("Left"), int(Qt::AlignLeft));
        align_combo->addItem(tr("Center"), int(Qt::AlignHCenter));
        align_combo->addItem(tr("Right"), int(Qt::AlignRight));
        align_combo->addItem(tr("Justified"), int(Qt::AlignJustify));
        align_row->addWidget(align_combo, 1);
        col->addLayout(align_row);

        // spaziatura: verticale = interlinea (moltiplicatore), orizzontale = spaziatura fra
        // le lettere in percento (QFont::setLetterSpacing, 100 = normale)
        QHBoxLayout *spacing_row = new QHBoxLayout();
        spacing_row->addWidget(new QLabel(tr("Spacing V/H"), advanced_text_tab));
        QDoubleSpinBox *spacing_spin = new QDoubleSpinBox(advanced_text_tab);
        spacing_spin->setRange(0.5, 5.0);
        spacing_spin->setSingleStep(0.1);
        spacing_spin->setValue(1.0);
        spacing_spin->setToolTip(tr("Vertical spacing: line height multiplier"));
        QSpinBox *letter_spin = new QSpinBox(advanced_text_tab);
        letter_spin->setRange(10, 500);
        letter_spin->setSuffix(tr("%"));
        letter_spin->setValue(100);
        letter_spin->setToolTip(tr("Horizontal spacing: space between letters (100% = normal)"));
        spacing_row->addWidget(spacing_spin, 1);
        spacing_row->addWidget(letter_spin, 1);
        col->addLayout(spacing_row);

        // stretch dei glifi: orizzontale via QFont::setStretch (rientra nella riga di testo,
        // quindi cambia anche il ritorno a capo), verticale via la scala y di TextFrame()
        QHBoxLayout *stretch_row = new QHBoxLayout();
        stretch_row->addWidget(new QLabel(tr("Stretch H/V"), advanced_text_tab));
        QSpinBox *stretch_h_spin = new QSpinBox(advanced_text_tab);
        stretch_h_spin->setRange(10, 400);
        stretch_h_spin->setSuffix(tr("%"));
        stretch_h_spin->setValue(100);
        stretch_h_spin->setToolTip(tr("Horizontal glyph stretch (100% = unstretched)"));
        QSpinBox *stretch_v_spin = new QSpinBox(advanced_text_tab);
        stretch_v_spin->setRange(10, 400);
        stretch_v_spin->setSuffix(tr("%"));
        stretch_v_spin->setValue(100);
        stretch_v_spin->setToolTip(tr("Vertical glyph stretch (100% = unstretched)"));
        stretch_row->addWidget(stretch_h_spin, 1);
        stretch_row->addWidget(stretch_v_spin, 1);
        col->addLayout(stretch_row);

        QCheckBox *wrap_check = new QCheckBox(tr("Word wrap at the stick's length"), advanced_text_tab);
        wrap_check->setChecked(true);
        col->addWidget(wrap_check);

        // ogni controllo scrive sullo stick corrente; text_panel_sync distingue l'eco di
        // refresh_text_panel (selezione cambiata) dall'input dell'utente, come il
        // blockSignals della casella IK
        auto edit = [this](std::function<void(StickData *)> mutate, bool coalesce){
            if(text_panel_sync)
                return;
            StickData *cs = view->my_animation->CurrentFrame()->CurrentStick();
            if(cs == nullptr || cs->stick_type != StickData::TEXT)
                return;
            // coalesce: un'intera digitazione sullo stesso stick = un solo snapshot
            // (altrimenti ogni tasto ne spingerebbe uno); reso a zero da ogni eco
            if(!coalesce || !text_undo_taken){
                view->StoreUndo(CMD_SIMPLE);
                text_undo_taken = true;
            }
            mutate(cs);
            view->my_animation->CurrentFrame()->CurrentStickFigure()->Refresh();
            view->my_animation->current_frame->UpdateIcon();
        };
        connect(content, &QTextEdit::textChanged, this, [this, content, edit](){
            edit([content](StickData *cs){ cs->text = content->toPlainText(); }, true);
        });
        connect(font_combo, &QFontComboBox::currentFontChanged, this, [edit](const QFont &f){
            edit([f](StickData *cs){ cs->text_font.setFamily(f.family()); }, false);
        });
        connect(size_spin, QOverload<int>::of(&QSpinBox::valueChanged), this, [edit](int v){
            edit([v](StickData *cs){ cs->text_font.setPointSize(v); }, false);
        });
        connect(bold_check, &QCheckBox::toggled, this, [edit](bool on){
            edit([on](StickData *cs){ cs->text_font.setBold(on); }, false);
        });
        connect(italic_check, &QCheckBox::toggled, this, [edit](bool on){
            edit([on](StickData *cs){ cs->text_font.setItalic(on); }, false);
        });
        connect(underline_check, &QCheckBox::toggled, this, [edit](bool on){
            edit([on](StickData *cs){ cs->text_font.setUnderline(on); }, false);
        });
        connect(strikeout_check, &QCheckBox::toggled, this, [edit](bool on){
            edit([on](StickData *cs){ cs->text_font.setStrikeOut(on); }, false);
        });
        connect(letter_spin, QOverload<int>::of(&QSpinBox::valueChanged), this, [edit](int v){
            edit([v](StickData *cs){
                cs->text_font.setLetterSpacing(QFont::PercentageSpacing, v);
            }, false);
        });
        connect(stretch_h_spin, QOverload<int>::of(&QSpinBox::valueChanged), this, [edit](int v){
            edit([v](StickData *cs){ cs->text_font.setStretch(v); }, false);
        });
        connect(stretch_v_spin, QOverload<int>::of(&QSpinBox::valueChanged), this, [edit](int v){
            edit([v](StickData *cs){ cs->text_v_stretch = v; }, false);
        });
        connect(align_combo, QOverload<int>::of(&QComboBox::currentIndexChanged), this,
                [edit, align_combo](int){
            int a = align_combo->currentData().toInt();
            edit([a](StickData *cs){ cs->text_align = a; }, false);
        });
        connect(spacing_spin, QOverload<double>::of(&QDoubleSpinBox::valueChanged), this, [edit](double v){
            edit([v](StickData *cs){ cs->text_line_spacing = float(v); }, false);
        });
        connect(wrap_check, &QCheckBox::toggled, this, [edit](bool on){
            edit([on](StickData *cs){ cs->text_word_wrap = on; }, false);
        });

        // eco: Animation::UpdateSliders() chiama questa a ogni cambio di selezione
        refresh_text_panel = [this, content, font_combo, size_spin, bold_check, italic_check,
                              underline_check, strikeout_check, align_combo, spacing_spin,
                              letter_spin, stretch_h_spin, stretch_v_spin, wrap_check](StickData *cs){
            if(cs == nullptr || cs->stick_type != StickData::TEXT)
                return;
            text_panel_sync = true;
            if(content->toPlainText() != cs->text) // preserva il cursore mentre si digita
                content->setPlainText(cs->text);
            font_combo->setCurrentFont(cs->text_font);
            size_spin->setValue(cs->text_font.pointSize() > 0 ? cs->text_font.pointSize() : 12);
            bold_check->setChecked(cs->text_font.bold());
            italic_check->setChecked(cs->text_font.italic());
            underline_check->setChecked(cs->text_font.underline());
            strikeout_check->setChecked(cs->text_font.strikeOut());
            int align_idx = align_combo->findData(cs->text_align);
            align_combo->setCurrentIndex(align_idx >= 0 ? align_idx : 0);
            spacing_spin->setValue(cs->text_line_spacing);
            // un QFont che non ha mai avuto spaziatura/stretch impostati riporta 0
            // (AnyStretch / nessuna spaziatura): mostralo come il 100% di default
            int letter = qRound(cs->text_font.letterSpacing());
            letter_spin->setValue(cs->text_font.letterSpacingType() == QFont::PercentageSpacing
                                  && letter > 0 ? letter : 100);
            stretch_h_spin->setValue(cs->text_font.stretch() > 0 ? cs->text_font.stretch() : 100);
            stretch_v_spin->setValue(cs->text_v_stretch);
            wrap_check->setChecked(cs->text_word_wrap);
            text_panel_sync = false;
            text_undo_taken = false; // altro stick (o ri-selezione): la prossima modifica apre un nuovo undo
        };
    }

    // ---- pannello "draw mode" del freehand: strumenti di disegno (pennello/gomma,
    // dimensione, opacità, colore, pressione) + lista pennelli. Costruito in codice come i
    // pannelli vertici/IK; Animation::UpdateTab() lo inserisce quando è selezionato uno
    // stick FREEHAND. I controlli scrivono direttamente lo stato pennello di MyView. ----
    advanced_draw_tab = new QWidget();
    {
        QVBoxLayout *col = new QVBoxLayout(advanced_draw_tab);

        // strumenti draw mode (mutuamente esclusivi): pennello, gomma, e i due strumenti di
        // selezione pixel (rettangolo, lazo). Il QButtonGroup garantisce l'esclusività; ogni
        // click instrada su MyView::SetDrawTool(), che scarta la selezione passando a pennello/gomma.
        // icone alla Paint di Windows XP, disegnate a 10x10 pixel per pixel (una riga di
        // caratteri per riga di pixel, '.' trasparente, le altre lettere sono la palette) e
        // raddoppiate a 20x20 senza filtro, così restano pixel art nitida
        auto pixel_icon = [](const char *const rows[10]){
            QImage img(10, 10, QImage::Format_ARGB32);
            img.fill(Qt::transparent);
            for(int y = 0; y < 10; y++)
                for(int x = 0; x < 10; x++){
                    switch(rows[y][x]){
                    case 'k': img.setPixel(x, y, qRgb(0, 0, 0)); break;
                    case 'w': img.setPixel(x, y, qRgb(255, 255, 255)); break;
                    case 'g': img.setPixel(x, y, qRgb(128, 128, 128)); break;
                    case 'l': img.setPixel(x, y, qRgb(192, 192, 192)); break;
                    case 'b': img.setPixel(x, y, qRgb(176, 112, 48)); break;
                    case 'p': img.setPixel(x, y, qRgb(255, 160, 176)); break;
                    case 'd': img.setPixel(x, y, qRgb(192, 96, 112)); break;
                    }
                }
            return QIcon(QPixmap::fromImage(img.scaled(20, 20)));
        };
        static const char *const brush_rows[10] = {
            ".......kk.", "......kbbk", ".....kbbk.", "....kbbk..", "...kllk...",
            "..klgk....", ".kkkk.....", "kkkk......", "kkk.......", "k........." };
        static const char *const eraser_rows[10] = {
            "....kkkkkk", "...kwwwwwk", "..kwwwwwkk", ".kwwwwwkdk", "kkkkkkkddk",
            "kpppppkddk", "kpppppkdk.", "kpppppkk..", "kkkkkkk...", ".........." };
        // i due strumenti di selezione: contorno tratteggiato 1-on/1-off come in Paint
        // (rettangolo e stella "a mano libera"), grigio chiaro perché il nero sparisce sullo
        // sfondo scuro dell'applicazione
        auto dashed_icon = [](const QPolygon &shape){
            QPixmap pm(10, 10);
            pm.fill(Qt::transparent);
            QPainter p(&pm);
            QPen pen(QColor(192, 192, 192), 0);
            pen.setDashPattern({ 1, 1 });
            p.setPen(pen);
            p.drawPolygon(shape);
            return QIcon(pm.scaled(20, 20));
        };
        const QPolygon rect_shape({ QPoint(0, 0), QPoint(9, 0), QPoint(9, 9), QPoint(0, 9) });
        const QPolygon lasso_shape({ QPoint(4, 0), QPoint(6, 3), QPoint(9, 3), QPoint(7, 6), QPoint(8, 9),
                                     QPoint(4, 7), QPoint(1, 9), QPoint(2, 5), QPoint(0, 2), QPoint(3, 3) });

        // piatti finché il mouse non ci passa sopra (rilievo), incassati mentre sono premuti
        // o attivi: lo stesso comportamento dei bottoni strumento di Paint
        const QString tool_style =
            "QToolButton { border: 2px solid transparent; background: transparent; padding: 0px; }"
            "QToolButton:hover { border: 2px outset #c0c0c0; background: #e8e8e8; }"
            "QToolButton:checked { border: 2px inset #c0c0c0; background: #ffffff; }"
            "QToolButton:checked:hover { background: #e4ecf8; }"
            "QToolButton:pressed { border: 2px inset #a0a0a0; background: #c8c8c8; }";

        QHBoxLayout *tool_row = new QHBoxLayout();
        tool_row->setSpacing(1);
        QButtonGroup *tool_group = new QButtonGroup(advanced_draw_tab);
        struct { const char *label; int id; QIcon icon; } draw_tools[] = {
            { "Brush",  MyView::DT_BRUSH,      pixel_icon(brush_rows) },
            { "Eraser", MyView::DT_ERASER,     pixel_icon(eraser_rows) },
            { "Select", MyView::DT_SEL_RECT,   dashed_icon(rect_shape) },
            { "Lasso",  MyView::DT_SEL_LASSO,  dashed_icon(lasso_shape) } };
        for(const auto &dt : draw_tools){
            QToolButton *b = new QToolButton(advanced_draw_tab);
            b->setIcon(dt.icon);
            b->setIconSize(QSize(20, 20));
            b->setFixedSize(26, 26);
            b->setToolTip(tr(dt.label));
            b->setStyleSheet(tool_style);
            b->setCheckable(true);
            if(dt.id == MyView::DT_BRUSH) b->setChecked(true);
            tool_group->addButton(b, dt.id);
            tool_row->addWidget(b);
        }
        connect(tool_group, &QButtonGroup::idClicked, this, [this](int id){ view->SetDrawTool(id); });

        // colore: un quadratino 10x10 in rilievo sulla stessa riga degli strumenti (14x14
        // meno il bordo da 2px); bordo blu al passaggio del mouse, incassato alla pressione
        auto swatch_style = [](const QColor &c){
            return QString("QToolButton { background-color: %1; border: 2px outset #c0c0c0; padding: 0px; }"
                           "QToolButton:hover { border-color: #5a8ad8; }"
                           "QToolButton:pressed { border-style: inset; }").arg(c.name());
        };
        QToolButton *color_btn = new QToolButton(advanced_draw_tab);
        color_btn->setFixedSize(14, 14);
        color_btn->setToolTip(tr("Brush color"));
        color_btn->setStyleSheet(swatch_style(view->draw_brush_color));
        tool_row->addSpacing(4);
        tool_row->addWidget(color_btn);
        tool_row->addStretch();
        col->addLayout(tool_row);
        connect(color_btn, &QToolButton::clicked, this, [this, color_btn, swatch_style]{
            QColor c = QColorDialog::getColor(view->draw_brush_color, this, tr("Brush color"));
            if(c.isValid()){
                view->draw_brush_color = c;
                color_btn->setStyleSheet(swatch_style(c));
            }
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

        // spaziatura (% del diametro dab tra un timbro e l'altro lungo il tratto). Oltre il
        // 100% i dab si staccano: la scala arriva a 1000% per i tratti punteggiati
        QHBoxLayout *spacing_row = new QHBoxLayout();
        spacing_row->addWidget(new QLabel(tr("Spacing"), advanced_draw_tab));
        QSpinBox *spacing_spin = new QSpinBox(advanced_draw_tab);
        spacing_spin->setRange(1, 1000);
        spacing_spin->setSuffix(tr("%"));
        spacing_spin->setValue(view->draw_brush_spacing);
        spacing_row->addWidget(spacing_spin);
        col->addLayout(spacing_row);
        connect(spacing_spin, QOverload<int>::of(&QSpinBox::valueChanged), this, [this](int v){ view->draw_brush_spacing = v; });

        // punta tonda integrata, parametri "alla Photoshop": durezza e rotondità sono cotte
        // nello stamp (le connect stanno più sotto, dove esiste rebuild_round() che lo
        // rigenera), l'angolo è applicato al timbro e vale anche sui pennelli .abr importati
        QHBoxLayout *tip_row = new QHBoxLayout();
        tip_row->addWidget(new QLabel(tr("Hardness"), advanced_draw_tab));
        QSpinBox *hard_spin = new QSpinBox(advanced_draw_tab);
        hard_spin->setRange(0, 100);
        hard_spin->setSuffix(tr("%"));
        hard_spin->setValue(view->draw_brush_hardness);
        hard_spin->setToolTip(tr("Built-in round brush: 100% = hard edge, 0% = fully feathered"));
        tip_row->addWidget(hard_spin);
        tip_row->addWidget(new QLabel(tr("Roundness"), advanced_draw_tab));
        QSpinBox *roundness_spin = new QSpinBox(advanced_draw_tab);
        roundness_spin->setRange(1, 100);
        roundness_spin->setSuffix(tr("%"));
        roundness_spin->setValue(view->draw_brush_roundness);
        roundness_spin->setToolTip(tr("Built-in round brush: 100% = circle, lower = flatter ellipse"));
        tip_row->addWidget(roundness_spin);
        col->addLayout(tip_row);

        // angolo della punta e i due jitter (dimensione e angolo, indipendenti) per ogni dab:
        // una riga a testa, le etichette accorpate su una sola non si leggevano
        QHBoxLayout *angle_row = new QHBoxLayout();
        angle_row->addWidget(new QLabel(tr("Angle"), advanced_draw_tab));
        QSpinBox *angle_spin = new QSpinBox(advanced_draw_tab);
        angle_spin->setRange(-180, 180);
        angle_spin->setSuffix(QString(QChar(0xB0))); // il grado, senza dipendere dall'encoding del sorgente
        angle_spin->setValue(view->draw_brush_angle);
        angle_spin->setToolTip(tr("Tip rotation"));
        angle_row->addWidget(angle_spin);
        col->addLayout(angle_row);

        QHBoxLayout *size_jitter_row = new QHBoxLayout();
        size_jitter_row->addWidget(new QLabel(tr("Size jitter"), advanced_draw_tab));
        QSpinBox *size_jitter_spin = new QSpinBox(advanced_draw_tab);
        size_jitter_spin->setRange(0, 100);
        size_jitter_spin->setSuffix(tr("%"));
        size_jitter_spin->setValue(view->draw_brush_size_jitter);
        size_jitter_spin->setToolTip(tr("Random per-dab size reduction (0% = every dab at the set size)"));
        size_jitter_row->addWidget(size_jitter_spin);
        col->addLayout(size_jitter_row);

        QHBoxLayout *angle_jitter_row = new QHBoxLayout();
        angle_jitter_row->addWidget(new QLabel(tr("Angle jitter"), advanced_draw_tab));
        QSpinBox *angle_jitter_spin = new QSpinBox(advanced_draw_tab);
        angle_jitter_spin->setRange(0, 100);
        angle_jitter_spin->setSuffix(tr("%"));
        angle_jitter_spin->setValue(view->draw_brush_angle_jitter);
        angle_jitter_spin->setToolTip(tr("Random per-dab rotation (100% = anywhere in the full circle)"));
        angle_jitter_row->addWidget(angle_jitter_spin);
        col->addLayout(angle_jitter_row);
        connect(angle_spin, QOverload<int>::of(&QSpinBox::valueChanged), this, [this](int v){ view->draw_brush_angle = v; });
        connect(size_jitter_spin, QOverload<int>::of(&QSpinBox::valueChanged), this, [this](int v){ view->draw_brush_size_jitter = v; });
        connect(angle_jitter_spin, QOverload<int>::of(&QSpinBox::valueChanged), this, [this](int v){ view->draw_brush_angle_jitter = v; });

        // modulazione da pressione penna: due checkbox indipendenti (dimensione/opacità),
        // entrambe attive di default; inerti in Phase 1, mouse-only
        QCheckBox *pressure_size_check = new QCheckBox(tr("Pen pressure drives size"), advanced_draw_tab);
        pressure_size_check->setChecked(view->draw_pressure_size);
        col->addWidget(pressure_size_check);
        connect(pressure_size_check, &QCheckBox::toggled, this, [this](bool on){ view->draw_pressure_size = on; });

        QCheckBox *pressure_opacity_check = new QCheckBox(tr("Pen pressure drives opacity"), advanced_draw_tab);
        pressure_opacity_check->setChecked(view->draw_pressure_opacity);
        col->addWidget(pressure_opacity_check);
        connect(pressure_opacity_check, &QCheckBox::toggled, this, [this](bool on){ view->draw_pressure_opacity = on; });

        // cartella pennelli: la stessa registrata da CreatePaths(), accanto a Libraries/myStickFigures.
        // La griglia (collezioni .abr, anteprime, import) è BrushPicker, condivisa con il bordo a
        // pennello della colonna Style: qui restano solo i controlli che scrivono lo stato di
        // disegno di MyView, che un bordo non ha.
        BrushPicker *brush_picker = new BrushPicker(brush_folder.path(), advanced_draw_tab);
        col->addWidget(brush_picker, 1);

        // riporta lo stato pennello di MyView nei controlli (dopo un preset), senza far
        // ripartire le loro connect — una rigenerazione della punta a metà della
        // currentItemChanged cancellerebbe l'item che l'ha appena scatenata
        auto sync_controls = [this, size_spin, op_spin, spacing_spin, hard_spin, roundness_spin,
                              angle_spin, size_jitter_spin, angle_jitter_spin]{
            const QList<QPair<QSpinBox*, int>> controls = {
                { size_spin,         view->draw_brush_size },
                { op_spin,           view->draw_brush_opacity },
                { spacing_spin,      view->draw_brush_spacing },
                { hard_spin,         view->draw_brush_hardness },
                { roundness_spin,    view->draw_brush_roundness },
                { angle_spin,        view->draw_brush_angle },
                { size_jitter_spin,  view->draw_brush_size_jitter },
                { angle_jitter_spin, view->draw_brush_angle_jitter } };
            for(const auto &c : controls){
                QSignalBlocker mute(c.first);
                c.first->setValue(c.second);
            }
        };
        connect(brush_picker, &BrushPicker::brushChosen, this,
                [this](const QImage &stamp){ view->draw_brush_stamp = stamp; });
        // la griglia si popola nel proprio costruttore, quindi il brushChosen della selezione
        // iniziale è già partito prima di questa connect: prendi la punta corrente a mano,
        // altrimenti resterebbe quella messa dal costruttore di MyView finché non si clicca
        view->draw_brush_stamp = brush_picker->CurrentStamp();
        connect(brush_picker, &BrushPicker::presetChosen, this,
                [this, sync_controls](const BrushPreset &p){
            view->draw_brush_size = p.size;
            view->draw_brush_opacity = p.opacity;
            view->draw_brush_spacing = p.spacing;
            view->draw_brush_hardness = p.hardness;
            view->draw_brush_roundness = p.roundness;
            view->draw_brush_size_jitter = p.size_jitter;
            view->draw_brush_angle_jitter = p.angle_jitter;
            view->draw_brush_angle = p.angle;
            sync_controls();
        });
        // quello che "Save brush" mette da parte insieme alla punta: lo stato di disegno com'è
        // adesso, così una punta ritoccata si ritrova alla sessione dopo con i suoi numeri
        brush_picker->SetParamsProvider([this]{
            BrushPreset p;
            p.size = view->draw_brush_size;
            p.opacity = view->draw_brush_opacity;
            p.spacing = view->draw_brush_spacing;
            p.hardness = view->draw_brush_hardness;
            p.roundness = view->draw_brush_roundness;
            p.size_jitter = view->draw_brush_size_jitter;
            p.angle_jitter = view->draw_brush_angle_jitter;
            p.angle = view->draw_brush_angle;
            return p;
        });

        // durezza/rotondità: rigenerano la punta selezionata sul posto (stamp + anteprima),
        // senza ripopolare la lista — altrimenti ritoccare la durezza di un preset avrebbe
        // riportato la selezione su "Round". Sulle collezioni .abr non c'è nulla da
        // rigenerare: lo stamp è quello del file.
        auto rebuild_round = [this, brush_picker]{
            brush_picker->RebuildBuiltIn(view->draw_brush_hardness, view->draw_brush_roundness,
                                         view->draw_brush_size, view->draw_brush_opacity);
        };
        connect(hard_spin, QOverload<int>::of(&QSpinBox::valueChanged), this,
                [this, rebuild_round](int v){ view->draw_brush_hardness = v; rebuild_round(); });
        connect(roundness_spin, QOverload<int>::of(&QSpinBox::valueChanged), this,
                [this, rebuild_round](int v){ view->draw_brush_roundness = v; rebuild_round(); });

        // azioni: rientra in draw mode su un freehand esistente (in testa alla riga strumenti,
        // prima del pennello) + undo/redo di sessione
        QHBoxLayout *action_row = new QHBoxLayout();
        QPushButton *enter_btn = new QPushButton(tr("Edit drawing"), advanced_draw_tab);
        QPushButton *undo_btn = new QPushButton(tr("Undo"), advanced_draw_tab);
        QPushButton *redo_btn = new QPushButton(tr("Redo"), advanced_draw_tab);
        tool_row->insertWidget(0, enter_btn);
        tool_row->insertSpacing(1, 4);
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
        // il canvas si dipinge attraverso la trasformazione con cui è entrato (EnterDrawMode()
        // la cuoce nei pixel e la azzera): cambiarla a metà sessione sposterebbe i tratti
        connect(view, &MyView::DrawModeChanged, advanced_transform_tab, &QWidget::setDisabled);
        connect(undo_btn, &QPushButton::clicked, this, [this]{ view->DrawStrokeUndo(); });
        connect(redo_btn, &QPushButton::clicked, this, [this]{ view->DrawStrokeRedo(); });
        // niente addStretch finale: lo splitter con la griglia riempie lo spazio verticale
    }

    BuildStyleCombos(); // dopo che view esiste: legge my_brush/my_pen per i due segnacolore
    BuildStyleTabs();   // e subito dopo: raggruppa le caselle che BuildStyleCombos() ha creato
    BuildResizableSidebars();

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
    if(!QFile::exists(lib_folder.path()+"/defaultLib.samlib")){
        view->SaveLibrary(lib_folder.path()+"/defaultLib.samlib");
    }
    else{
        view->LoadLibrary(lib_folder.path()+"/defaultLib.samlib");
    }
    QString jsonConfigPath = program_folder.path()+"/config.json";
    if(!QFile::exists(jsonConfigPath)){
        CreateJson(jsonConfigPath);
    }
    else{
        ReadJson(jsonConfigPath);
    }
    // la licenza si accetta nell'installer (SAM.iss, LicenseFile): niente secondo prompt qui



    //crea la cornice
    my_rect = new QGraphicsRectItem(0, 0, W, H);
    my_rect->setBrush(QBrush(QColor(Qt::white)));
    limit_rect = new QGraphicsRectItem(-my_rect->rect().width()/10, -my_rect->rect().height()/10, my_rect->rect().width()*1.1, my_rect->rect().height()*1.1);
    limit_rect->setBrush(QBrush(QColor(Qt::white)));
    my_rect->setZValue(-100);
    my_rect->setPen(Qt::NoPen);
    scene->addItem(my_rect);
    scene->setSceneRect(limit_rect->rect());
    // la finestra all'avvio può essere più piccola della tela: senza questo la view parte con
    // le barre a zero, cioè sull'angolo della scena (che sta un 10% FUORI dal foglio) invece
    // che sul foglio. centerOn() qui basta perché fissa lastCenterPoint: AnchorViewCenter lo
    // riapplica al primo show e a ogni ridimensionamento successivo, e una panoramica
    // dell'utente lo aggiorna, quindi non riporta la vista indietro
    view->setResizeAnchor(QGraphicsView::AnchorViewCenter);
    view->centerOn(my_rect->sceneBoundingRect().center());
    num = scene->items();
    // adjust image editing offset sliders according to max size of scene
    img_v_offset_slider->setRange(-my_rect->rect().height(), my_rect->rect().height());
    img_v_offset_spinbox->setRange(-my_rect->rect().height(), my_rect->rect().height());
    img_h_offset_slider->setRange(-my_rect->rect().width(), my_rect->rect().width());
    img_h_offset_spinbox->setRange(-my_rect->rect().width(), my_rect->rect().width());
    elongate_spinbox->setRange(0,W);
    //check if default library exists

    my_library_list_widget->clear();
    DetectLibraries();
    my_library_list_widget->setCurrentRow(0);
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
        recordObject.value(QString("Handles size"))                 == QJsonValue::Undefined
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
    file.close();

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
 * interval, handle size) into a fresh QJsonObject and overwrites the file at `path`. The
 * sole writer of config.json; ReadJson() and WriteJson() both funnel through here rather
 * than building JSON themselves.
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
    QJsonDocument doc(recordObject);
    QFile file(path);
    if(!file.open(QIODevice::WriteOnly))
        return;
    file.write(doc.toJson());
    file.close();
}
/**
 * @details Each per-user folder (program root, libraries, brushes, patterns, temp render,
 * animations, stick figures, video export) is created independently if missing — a
 * fresh install has none of them yet, but a folder deleted by hand later should also be
 * silently recreated rather than causing every subsequent file operation to fail.
 */
void Widget::CreatePaths(){
    QString  path = QStandardPaths::writableLocation(QStandardPaths::DocumentsLocation);
    program_folder = QDir(path+"/SAM");
    if(program_folder.exists()){
        qDebug("folder located");
    }
    else{
        qDebug("folder created");
        program_folder.mkpath(path+"/SAM");
    }
    lib_folder = QDir(path+"/SAM/Libraries");
    if(!lib_folder.exists()){
        qDebug("library folder created");
        lib_folder.mkpath(path+"/SAM/Libraries");
    }
    brush_folder = QDir(path+"/SAM/Brushes");
    if(!brush_folder.exists()){
        qDebug("brush folder created");
        brush_folder.mkpath(path+"/SAM/Brushes");
    }
    pattern_folder = QDir(path+"/SAM/myPatterns");
    if(!pattern_folder.exists()){
        qDebug("pattern folder created");
        pattern_folder.mkpath(path+"/SAM/myPatterns");
    }
    temp_render_folder = QDir(path+"/SAM/temporaryRender");
    if(!temp_render_folder.exists()){
        qDebug("temporary render folder created");
        temp_render_folder.mkpath(path+"/SAM/temporaryRender");
    }
    my_animations_folder = QDir(path+"/SAM/myAnimations");
    if(!my_animations_folder.exists()){
        qDebug("my_animation folder created");
        my_animations_folder.mkpath(path+"/SAM/myAnimations");
    }
    my_stick_figures_folder = QDir(path+"/SAM/myStickFigures");
    if(!my_stick_figures_folder.exists()){
        qDebug("Stickfigure folder created");
        my_stick_figures_folder.mkpath(path+"/SAM/myStickFigures");
    }

    my_video_export_folder = QDir(path+"/SAM/myExportedVideos");
    if(!my_video_export_folder.exists()){
        qDebug("Stickfigure folder created");
        my_video_export_folder.mkpath(path+"/SAM/myExportedVideos");
    }
}
/**
 * @details Iterates every entry in the libraries folder and adds one list item per
 * *.samlib file found, storing its full path as the item's user-role data (read back by
 * on_libraryList_currentRowChanged() and friends). Called at startup and after any
 * operation that adds/removes/renames a library file, since the list widget has no
 * other way to notice filesystem changes made outside the app.
 */
void Widget::DetectLibraries(){
    QDirIterator dirIt(lib_folder.path(),QDir::AllEntries);
    while (dirIt.hasNext()) {
        dirIt.next();
        if (QFileInfo(dirIt.filePath()).isFile()){
            QString suffix = QFileInfo(dirIt.filePath()).suffix();
            if (suffix == "samlib" || suffix == "marlib"){ // .marlib: estensione pre-rebrand, stesso formato
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
    advanced_transform_tab->setParent(nullptr);
    delete advanced_transform_tab;
    qDeleteAll(frame_copy_buffer); // il buffer copia-frame è nostro (era un globale mai liberato)
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
        // sposta box/perno/maniglie sulla nuova figura: UpdateSelection cambia solo il modello,
        // le decorazioni di selezione resterebbero sulla figura precedente (spec)
        view->RefreshSelectionForCurrentLayer();
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

/**
 * @details Refuses to delete the last remaining frame — an animation always has at least
 * one. With several rows selected in the frame list the whole block goes in ONE undo step
 * (MyView::DeleteFrames()); the single-frame path is the historical one, which needs no
 * snapshot of its own beyond StoreUndo()'s.
 */
void Widget::on_deleteFrameBtn_clicked()
{
    // finalizza un'eventuale sessione di disegno PRIMA di cancellare il frame (che
    // libererebbe lo stick del freehand → dangling); così lo snapshot cattura il disegno
    // già ritagliato e non c'è use-after-free
    view->ExitDrawMode();
    QList<Frame *> selected = SelectedFrames();
    if(selected.count() > 1){
        // il blocco è contiguo per costruzione (ContiguousSelection), quindi primo
        // frame_number + quanti sono lo descrivono per intero
        view->DeleteFrames(selected.first()->frame_number, selected.count());
        return;
    }
    if(view->my_animation->frame_list.count()>1){
        view->StoreUndo(CMD_DELETEFRAME);
        view->DeleteFrame(view->my_animation->CurrentFrame());
        view->MoveToFrame(view->my_animation->CurrentFrame());
    }
}

/**
 * @details Switches the displayed frame to whichever row was clicked in the frame list —
 * unless the click is extending the selection (shift), in which case the frame shown must
 * stay the one the selection started from: Qt has already extended the block by the time
 * this fires, and MoveToFrame() would both switch frame and collapse the selection back to
 * a single row through its setCurrentItem() call.
 */
void Widget::on_frameListWidget_itemClicked(QListWidgetItem *item)
{
    if(QApplication::keyboardModifiers() & Qt::ShiftModifier)
        return;
    view->ExitDrawMode(); // il cambio frame chiude la sessione di disegno (finalizza sul frame corrente)
    view->StoreUndo(CMD_MOVETOFRAME);
    QVariant  retrievedData = (item->data(Qt::UserRole));
    Frame* cs = qvariant_cast<Frame*>(retrievedData);

    view->MoveToFrame(cs);
}

/// @details See the header for why the rows are walked in order instead of via selectedItems().
QList<Frame *> Widget::SelectedFrames() const
{
    QList<Frame *> frames;
    for(int row = 0; row < my_frame_widget_list->count(); row++){
        QListWidgetItem *item = my_frame_widget_list->item(row);
        if(!item->isSelected())
            continue;
        Frame *frame = qvariant_cast<Frame*>(item->data(Qt::UserRole));
        if(frame != nullptr)
            frames.append(frame);
    }
    if(frames.isEmpty() && view->my_animation->CurrentFrame() != nullptr)
        frames.append(view->my_animation->CurrentFrame()); // nessuna riga selezionata: il frame mostrato
    return frames;
}

/**
 * @details Frame names are optional labels shown next to the frame number in the
 * thumbnail (Frame::UpdateLabel() bakes both into the icon, since the list's native item
 * label is not used). itemClicked fires first on a double click, so the frame being
 * renamed is already the current one. Not undoable, exactly like the layer rename in
 * on_stickLayerView_itemDoubleClicked() — but Cancel keeps the previous name instead of
 * clearing it.
 */
void Widget::on_frameListWidget_itemDoubleClicked(QListWidgetItem *item)
{
    Frame *frame = qvariant_cast<Frame*>(item->data(Qt::UserRole));
    if(frame == nullptr)
        return;
    bool confirmed = false;
    QString text = QInputDialog::getText(this, tr("Rename frame"), tr("Frame name"),
                                         QLineEdit::Normal, frame->name, &confirmed);
    if(!confirmed)
        return;
    frame->name = text;
    frame->UpdateLabel();
}

/**
 * @details Builds the prompt in code (a spinbox for N and the two direction radios are
 * smaller here than a dedicated .ui, same reasoning as the vertex panel in the
 * constructor). Each direction is offered only if the neighbour frame exists — previous
 * needs a frame before the current one, next a frame after it — and the pair is always
 * passed to MyView::GenerateInbetweens() as (first frame index, N), the first of the two
 * being the start of the interpolation. Every structural check lives there, so a refusal
 * comes back as text and is shown as-is.
 */
void Widget::on_generateInbetweenBtn_clicked()
{
    view->ExitDrawMode();
    if(view->my_animation->CurrentFrame() == nullptr)
        return;
    int current_index = view->my_animation->CurrentFrame()->frame_number;
    bool has_previous = current_index >= 1;
    bool has_next = current_index + 1 < view->my_animation->frame_list.count();
    if(!has_previous && !has_next){
        QMessageBox::warning(this, tr("Generate inbetweens"),
                             tr("Interpolation needs a second frame: add a frame before or after this one."));
        return;
    }

    QDialog prompt(this);
    prompt.setWindowTitle(tr("Generate inbetweens"));
    QVBoxLayout *rows = new QVBoxLayout(&prompt);
    QHBoxLayout *count_row = new QHBoxLayout();
    count_row->addWidget(new QLabel(tr("Frames to generate"), &prompt));
    QSpinBox *count_spinbox = new QSpinBox(&prompt);
    count_spinbox->setRange(1, 100);
    count_spinbox->setValue(1);
    count_row->addWidget(count_spinbox);
    rows->addLayout(count_row);
    QRadioButton *previous_radio = new QRadioButton(tr("Between the previous frame and this one"), &prompt);
    QRadioButton *next_radio = new QRadioButton(tr("Between this frame and the next one"), &prompt);
    previous_radio->setEnabled(has_previous);
    next_radio->setEnabled(has_next);
    previous_radio->setChecked(has_previous); // default da spec, se il frame precedente esiste
    next_radio->setChecked(!has_previous);
    rows->addWidget(previous_radio);
    rows->addWidget(next_radio);
    QDialogButtonBox *buttons = new QDialogButtonBox(QDialogButtonBox::Ok|QDialogButtonBox::Cancel, &prompt);
    connect(buttons, &QDialogButtonBox::accepted, &prompt, &QDialog::accept);
    connect(buttons, &QDialogButtonBox::rejected, &prompt, &QDialog::reject);
    rows->addWidget(buttons);
    if(prompt.exec() != QDialog::Accepted)
        return;

    // l'ordine dei due frame determina inizio e fine: col precedente si parte da lui,
    // col successivo si parte dal frame corrente
    int start_index = previous_radio->isChecked() ? current_index - 1 : current_index;
    QString refusal = view->GenerateInbetweens(start_index, count_spinbox->value());
    if(!refusal.isEmpty())
        QMessageBox::warning(this, tr("Generate inbetweens"), refusal);
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

/// @details Switches to the view-zoom tool: drag right/up to zoom in, left/down to zoom out,
/// Shift for 10% steps. Was a one-shot "zoom x2" button, with a twin for zoom out.
void Widget::on_zoomInBtn_clicked()
{
    view->SetTool(ZOOMVIEW);
}

/// @details Returns the view to 1:1 zoom with no rotation and no pan (MyView::ResetView()).
void Widget::on_resetZoomBtn_clicked()
{
    view->ResetView();
    view->MoveToFrame(view->my_animation->CurrentFrame());
}

/// @details Switches to the view-rotation tool: drag to turn the viewport, Shift for 15° steps.
void Widget::on_rotateViewBtn_clicked()
{
    view->SetTool(ROTATEVIEW);
}

/// @details Switches to the view-pan tool: drag to move the viewport over the canvas.
void Widget::on_panViewBtn_clicked()
{
    view->SetTool(PANVIEW);
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



/// @details Saves the current (in-memory, not yet on disk) library buffer as a new *.samlib file, then refreshes the library file list to include it.
void Widget::on_saveLibraryBtn_clicked()
{
    QString file_name = QFileDialog::getSaveFileName(this,tr("save SAM library"),
                       lib_folder.path(),"SAM Library(*.samlib)");
    if(file_name.length()>0){
        view->SaveLibrary(file_name);
        my_library_list_widget->clear();
        DetectLibraries();
    }
}



/// @details Adds the current stickfigure (or, with a multi-selection, every selected figure)
/// to the currently-open library buffer as a reusable item. For a single figure, prompts for
/// a name first (defaulting to the figure's current name) and stores that as the library
/// entry's name; a multi-selection is added unprompted, each figure keeping its own name
/// (naming several figures at once via one shared prompt would just collide their names).
void Widget::on_addItemToLibBtn_clicked()
{
    if(view->MultiSelectionActive()){
        view->AddStickFigureToLibrary();
        return;
    }
    StickFigure *current = view->my_animation->CurrentFrame()->CurrentStickFigure();
    if(!current)
        return;
    bool ok = false;
    QString name = QInputDialog::getText(this, tr("Add to library"), tr("Stickfigure name:"),
                                          QLineEdit::Normal, current->name, &ok);
    if(!ok)
        return;
    view->AddStickFigureToLibrary(name);
}

/// @details Replaces the current library buffer with the contents of the chosen *.samlib file.
void Widget::on_loadLibraryBtn_clicked()
{
    QString file_name = QFileDialog::getOpenFileName(this,tr("load SAM library"),
                       lib_folder.path(),"SAM Library(*.samlib *.marlib)");
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

/// @details Prompts for a new name and renames the double-clicked library entry: both the
/// widget row (DisplayRole) and the underlying StickFigureData (so the new name round-trips
/// when the library is saved back to disk).
void Widget::on_currentLibrary_itemDoubleClicked(QListWidgetItem *item)
{
    StickFigureData *figure_data = qvariant_cast<StickFigureData*>(item->data(Qt::UserRole));
    if(!figure_data)
        return;
    bool ok = false;
    QString text = QInputDialog::getText(this, tr("Rename library item"), tr("Name:"),
                                          QLineEdit::Normal, figure_data->name, &ok);
    if(!ok || text.isEmpty())
        return;
    figure_data->name = text;
    item->setData(Qt::DisplayRole, text);
}

/// @details Re-saves the open library buffer back to the *.samlib file it was loaded from.
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
 * @details The five sliders of the "Stick Transform" tab, which act on every stick type (they
 * used to be the image panel's, and only IMAGE/POLYGON/TEXT listened). `sliderFlags.sliderHOffset`
 * coalesces an entire drag into one undo snapshot: the first valueChanged() of a drag takes it
 * (and raises the flag), every subsequent one during the same drag does not — sliderReleased()
 * lowers the flag again so the next drag starts its own snapshot. Same pattern repeats for every
 * transform slider below.
 */
void Widget::on_imgHOffsetSlider_valueChanged(int value)
{
    StickData*cs =  view->my_animation->CurrentFrame()->CurrentStick();
    if(cs == nullptr)
        return;
    if(!sliderFlags.sliderHOffset){
        view->StoreUndo(CMD_SIMPLE);
        sliderFlags.sliderHOffset = true;
    }
    img_h_offset_spinbox->setValue(value);
    EditedXf(cs).offset.setX(value);
    RefreshTransformed(cs);
}


/// @details Vertical counterpart of on_imgHOffsetSlider_valueChanged() — see its doc for the undo-coalescing pattern.
void Widget::on_imgVOfsetSlider_valueChanged(int value)
{
    StickData*cs =  view->my_animation->CurrentFrame()->CurrentStick();
    if(cs == nullptr)
        return;
    if(!sliderFlags.sliderVOffset){
        view->StoreUndo(CMD_SIMPLE);
        sliderFlags.sliderVOffset = true;
    }
    img_v_offset_spinbox->setValue(value);
    EditedXf(cs).offset.setY(value);
    RefreshTransformed(cs);
}

/// @details Maps a scale slider's signed range onto a >0 scale factor: shrinking below 1 on the
/// negative side instead of going negative. Animation::UpdateSliders() echoes back its inverse.
static float SliderScale(int value)
{
    return value > 0 ? (float)value/10+1 : 1/((float)(-value)/10+1);
}

/// @details Width-scale counterpart of on_imgHOffsetSlider_valueChanged() — across the stick.
void Widget::on_imgWSlider_valueChanged(int value)
{
    StickData*cs =  view->my_animation->CurrentFrame()->CurrentStick();
    if(cs == nullptr)
        return;
    if(!sliderFlags.sliderWScale){
        view->StoreUndo(CMD_SIMPLE);
        sliderFlags.sliderWScale = true;
    }
    img_width_spinbox->setValue(value);
    EditedXf(cs).scale.setX(SliderScale(value));
    RefreshTransformed(cs);
}

/// @details Height-scale counterpart of on_imgWSlider_valueChanged() — along the stick.
void Widget::on_imgHSlider_valueChanged(int value)
{
    StickData*cs =  view->my_animation->CurrentFrame()->CurrentStick();
    if(cs == nullptr)
        return;
    if(!sliderFlags.sliderHScale){
        view->StoreUndo(CMD_SIMPLE);
        sliderFlags.sliderHScale = true;
    }
    img_height_spinbox->setValue(value);
    EditedXf(cs).scale.setY(SliderScale(value));
    RefreshTransformed(cs);
}

/// @details Rotation counterpart of on_imgHOffsetSlider_valueChanged(): turns the shape about the stick's origin.
void Widget::on_imgRotationSlider_valueChanged(int value)
{
    StickData*cs =  view->my_animation->CurrentFrame()->CurrentStick();
    if(cs == nullptr)
        return;
    if(!sliderFlags.sliderRot){
        view->StoreUndo(CMD_SIMPLE);
        sliderFlags.sliderRot = true;
    }
    img_rotation_spinbox->setValue(value);
    EditedXf(cs).angle = value;
    RefreshTransformed(cs);
}

/// @details The set the "Stick Transform" tab is showing: Local or Global.
StickXf &Widget::EditedXf(StickData *cs) const
{
    return transform_mode_bar->currentIndex() == 1 ? cs->global_xf : cs->local_xf;
}

/**
 * @details Repaints exactly what a Stick Transform edit moved: the stick alone for a local one, the
 * stick and its descendants (`children` is already the flattened list) for a global one. The
 * sliders used to refresh the whole figure — a bounding rect, and for a text stick a whole text
 * layout, per stick per slider tick — to move one shape.
 */
void Widget::RefreshTransformed(StickData *cs)
{
    if(cs->view)
        cs->view->Refresh(1);
    if(transform_mode_bar->currentIndex() == 1)
        for(StickData *s : cs->children)
            if(s->view)
                s->view->Refresh(1);
}

/// @details One undo step per toggle, like every other per-stick control of the panel.
void Widget::SetTransformFlip(bool horizontal, bool on)
{
    StickData *cs = view->my_animation->CurrentFrame()->CurrentStick();
    if(cs == nullptr)
        return;
    StickXf &xf = EditedXf(cs);
    bool &flip = horizontal ? xf.flip_h : xf.flip_v;
    if(flip == on)
        return;
    view->StoreUndo(CMD_SIMPLE);
    flip = on;
    RefreshTransformed(cs);
    view->my_animation->current_frame->UpdateIcon();
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
namespace {

/**
 * @details Which Widget::StyleKind an existing brush reads as, used to keep a type combo
 * showing what the current fill/stroke actually is even when the choice came from the two
 * historical colour buttons or from the asynchronous pattern picker. Qt has one texture
 * style for both a .pat tile and an imported image, so either reads back as STYLE_PATTERN
 * — the distinction only ever mattered for choosing which chooser to open.
 */
int KindOfBrush(const QBrush &brush)
{
    switch(brush.style()){
    case Qt::NoBrush:                return Widget::STYLE_NONE;
    case Qt::TexturePattern:         return Widget::STYLE_PATTERN;
    case Qt::LinearGradientPattern:  return Widget::STYLE_LINEAR;
    case Qt::RadialGradientPattern:  return Widget::STYLE_RADIAL;
    default:                         return Widget::STYLE_SOLID;
    }
}

/**
 * @details A Qt::TexturePattern brush whose tile paints @p tile_size scene pixels along its
 * longer side, via the brush TRANSFORM rather than by resampling the QImage: the transform
 * is part of what QDataStream writes for a QBrush, so the size survives save/load with no
 * new field, and the tile keeps its full resolution for when the figure is scaled up or the
 * animation is exported at a larger size.
 *
 * The scaling exists because the two sources of a texture — a Photoshop .pat pack and an
 * arbitrary image file — are both authored at sizes one or two orders of magnitude larger
 * than a stick. Painted 1:1 they show one flat crop of themselves, which reads as a plain
 * colour rather than as a pattern.
 */
QBrush TextureBrush(const QImage &tile, int tile_size)
{
    QBrush brush(tile);
    const int longest = qMax(tile.width(), tile.height());
    if(longest > 0 && tile_size > 0)
        brush.setTransform(QTransform::fromScale(qreal(tile_size)/longest,
                                                 qreal(tile_size)/longest));
    return brush;
}

} // namespace

/**
 * @details The entries are identical for the two groups because the target is identical: a
 * QBrush, whether it ends up in StickData::brush or inside StickData::pen. Both combos are
 * inserted at position 0 of their group's existing layout — verticalLayout_16 for "Fill",
 * verticalLayout_15 for "Border" — so the type reads above the swatch that shows it.
 */
void Widget::BuildStyleCombos()
{
    const QList<QPair<QString, int>> entries = {
        { tr("Solid"),           STYLE_SOLID   },
        { tr("Pattern"),         STYLE_PATTERN },
        { tr("Linear gradient"), STYLE_LINEAR  },
        { tr("Radial gradient"), STYLE_RADIAL  },
        { tr("Image"),           STYLE_IMAGE   },
        { tr("None"),            STYLE_NONE    } };

    fill_type_combo = new QComboBox(ui->groupBox_4);
    stroke_type_combo = new QComboBox(ui->groupBox);
    for(QComboBox *combo : { fill_type_combo, stroke_type_combo }){
        for(const QPair<QString, int> &entry : entries)
            combo->addItem(entry.first, entry.second);
        combo->setToolTip(tr("What this is painted with. Pattern opens the pattern library, "
                             "the gradients open the gradient editor, Image loads a file."));
    }
    // solo il CONTORNO: un pennello si timbra lungo un percorso, e un riempimento non ne ha uno
    stroke_type_combo->insertItem(stroke_type_combo->count() - 1, tr("Brush"), STYLE_BRUSH);
    stroke_type_combo->setToolTip(stroke_type_combo->toolTip() + tr(" Brush stamps a brush tip "
                                  "along the outline, using the same library as freehand draw."));
    ui->verticalLayout_16->insertWidget(0, fill_type_combo);
    ui->verticalLayout_15->insertWidget(0, stroke_type_combo);

    // una casella per gruppo (spec): la freccia del riempimento e quella del contorno si
    // accendono separatamente, perché con entrambe accese si sovrappongono e a volte se ne
    // vuole muovere una sola
    fill_texture_arrow_check = new QCheckBox(tr("Show texture handle"), ui->groupBox_4);
    stroke_texture_arrow_check = new QCheckBox(tr("Show texture handle"), ui->groupBox);
    for(QCheckBox *box : { fill_texture_arrow_check, stroke_texture_arrow_check })
        box->setToolTip(tr("Draws an arrow on the selected stick for this texture: drag its "
                           "tail to move the texture, its tip to rotate and scale it."));
    ui->verticalLayout_16->insertWidget(1, fill_texture_arrow_check);
    ui->verticalLayout_15->insertWidget(1, stroke_texture_arrow_check);
    connect(fill_texture_arrow_check, &QCheckBox::toggled, this, [this](bool on){
        view->show_fill_texture_arrow = on;
        if(view->viewport()) view->viewport()->update(); // le frecce vivono nel foreground
    });
    connect(stroke_texture_arrow_check, &QCheckBox::toggled, this, [this](bool on){
        view->show_stroke_texture_arrow = on;
        if(view->viewport()) view->viewport()->update();
    });

    // un solo interruttore per riempimento E contorno, perché StickData::brush_follows_stick
    // è un solo campo: sta sopra i due gruppi, non dentro uno dei due
    brush_follows_check = new QCheckBox(tr("Texture follows stick"), ui->groupBox_3);
    brush_follows_check->setChecked(view->my_brush_follows_stick);
    brush_follows_check->setToolTip(tr("On: a pattern, gradient or image rotates, moves and "
                                      "stretches with the stick. Off: it stays put in the "
                                      "scene and the stick moves over it."));
    ui->verticalLayout_20->insertWidget(0, brush_follows_check);
    // si applica subito allo stick selezionato (come gli altri controlli per-stick), e resta
    // come stato corrente per i prossimi stick disegnati e per i bottoni "applica"
    connect(brush_follows_check, &QCheckBox::toggled, this, [this](bool on){
        view->my_brush_follows_stick = on;
        if(view->my_animation->current_frame == nullptr)
            return;
        StickData *current = view->my_animation->CurrentFrame()->CurrentStick();
        if(current == nullptr || current->brush_follows_stick == on)
            return;
        view->StoreUndo(CMD_SIMPLE);
        current->brush_follows_stick = on;
        if(current->view)
            current->view->Refresh(0);
        if(view->my_animation->CurrentFrame()->CurrentStickFigure())
            view->my_animation->CurrentFrame()->CurrentStickFigure()->UpdateIcon();
    });

    // la ripetizione della tile, accanto all'ancoraggio: stessa forma, stesso motivo per cui è
    // qui e non solo dentro TextureOptionsDialog — il flag è per-stick e appiccicoso, e da lì
    // finiva addosso a ogni riempimento successivo (un pattern preso dalla libreria compreso,
    // che non aveva alcun controllo per rimetterlo a posto)
    texture_tiled_check = new QCheckBox(tr("Tile texture"), ui->groupBox_3);
    texture_tiled_check->setChecked(view->my_texture_tiled);
    texture_tiled_check->setToolTip(tr("On: the tile repeats to cover the shape. Off: one single "
                                       "instance is painted, wherever its scale, angle and offset "
                                       "put it. Fill only — a stroke always repeats."));
    ui->verticalLayout_20->insertWidget(1, texture_tiled_check);
    connect(texture_tiled_check, &QCheckBox::toggled, this, [this](bool on){
        view->my_texture_tiled = on;
        if(view->my_animation->current_frame == nullptr)
            return;
        StickData *current = view->my_animation->CurrentFrame()->CurrentStick();
        if(current == nullptr || current->texture_tiled == on)
            return;
        view->StoreUndo(CMD_SIMPLE);
        current->texture_tiled = on;
        if(current->view)
            current->view->Refresh(0);
        if(view->my_animation->CurrentFrame()->CurrentStickFigure())
            view->my_animation->CurrentFrame()->CurrentStickFigure()->UpdateIcon();
    });

    // activated e non currentIndexChanged: il selettore si apre solo su scelta dell'utente,
    // non quando ApplyStyleBrush() riallinea la combo a un brush appena impostato
    // replace=true: scegliere un tipo dalla combo È assegnare un riempimento nuovo, che
    // sostituisce il precedente per intero (vedi ChooseStyle())
    connect(fill_type_combo, &QComboBox::activated, this,
            [this](int){ ChooseStyle(true, fill_type_combo->currentData().toInt(), true); });
    connect(stroke_type_combo, &QComboBox::activated, this,
            [this](int){ ChooseStyle(false, stroke_type_combo->currentData().toInt(), true); });

    RefreshStyleSwatch(true);
    RefreshStyleSwatch(false);
}

/**
 * @details Two rearrangements of widgets that already exist, so every signal, tooltip and
 * per-stick enablement path (Animation::UpdateSliders()/UpdateStyleControls()) is untouched.
 * The four texture check boxes sat in three different layouts although they are one thing —
 * how the paint is displayed — hence one "Display options" group, in the same tab as the two
 * groups they act on. "Depth" moves into a tab of its own because its 250 px sat UNDER Fill
 * and Border: the column was taller than a small screen and the rest of it fell off the
 * window. In a tab it costs the height of the taller page instead of the sum.
 */
void Widget::BuildStyleTabs()
{
    QGroupBox *display_group = new QGroupBox(tr("Display options"), ui->groupBox_3);
    QVBoxLayout *display_col = new QVBoxLayout(display_group);
    display_col->setContentsMargins(4, 10, 4, 4);
    display_col->setSpacing(2);
    // due caselle omonime nello stesso gruppo non si distinguerebbero più: finora il gruppo
    // che le conteneva diceva a quale delle due frecce si riferissero
    fill_texture_arrow_check->setText(tr("Fill texture handle"));
    stroke_texture_arrow_check->setText(tr("Border texture handle"));
    for(QCheckBox *box : { brush_follows_check, texture_tiled_check,
                           fill_texture_arrow_check, stroke_texture_arrow_check })
        display_col->addWidget(box); // riparenta, e questo le toglie dal layout precedente

    QTabWidget *style_tabs = new QTabWidget(ui->groupBox_3);
    QWidget *paint_tab = new QWidget(style_tabs);
    QVBoxLayout *paint_col = new QVBoxLayout(paint_tab);
    paint_col->setContentsMargins(0, 0, 0, 0);
    paint_col->addWidget(ui->groupBox_4); // "Fill"
    paint_col->addWidget(ui->groupBox);   // "Border"
    paint_col->addWidget(display_group);
    paint_col->addStretch(); // come il vecchio spacer di coda: i gruppi restano alti quanto serve
    QWidget *depth_tab = new QWidget(style_tabs);
    QVBoxLayout *depth_col = new QVBoxLayout(depth_tab);
    depth_col->setContentsMargins(0, 0, 0, 0);
    depth_col->addWidget(ui->groupBox_13); // "Depth"
    depth_col->addStretch();
    // Paint scorre per conto suo, come la pagina immagini della colonna strumenti: una
    // QTabWidget è alta quanto la sua pagina più alta, e Paint (~600 px) dava la sua altezza
    // anche a Depth, i cui bottoni non avevano così mai bisogno di rimpicciolirsi
    QScrollArea *paint_scroll = new QScrollArea(style_tabs);
    paint_scroll->setWidget(paint_tab);
    paint_scroll->setWidgetResizable(true);
    // etichette corte: la colonna è larga 150 px, con nomi più lunghi la barra delle tab
    // passerebbe alle frecce di scorrimento
    style_tabs->addTab(paint_scroll, tr("Paint"));
    style_tabs->addTab(depth_tab, tr("Depth"));
    style_tabs->setTabToolTip(0, tr("Fill and border"));
    // stretch: tutto lo spazio in più va alle tab, non allo spacer che resta in coda al .ui
    ui->verticalLayout_20->insertWidget(0, style_tabs, 1);
}

namespace {

/**
 * @brief The bottom row's splitter: when the window narrows, the side columns give up their
 *        spare width before the canvas shrinks, and when it widens they get it back first.
 * @details With the canvas as the only stretch factor, QSplitter hands every change of width
 * to the canvas until the canvas reaches its minimum. So narrowing the window crushed the canvas
 * while the tools column kept a hundred pixels it didn't need. No stretch factor can express
 * the rule, since a stretch factor applies to growing as well. A column's spare width is what
 * it has beyond what its content needs. It grows back up to `preferred`: its starting width, or
 * the width the user last dragged it to. Below the columns' needs QSplitter's own rule applies
 * again: the canvas shrinks to its minimum, then the columns scroll.
 */
class BottomRowSplitter : public QSplitter
{
public:
    explicit BottomRowSplitter(QWidget *parent) : QSplitter(Qt::Horizontal, parent)
    {
        connect(this, &QSplitter::splitterMoved, this, [this]{ preferred = sizes(); });
    }
    QList<int> preferred; ///< per widget, as sizes(); the canvas's entry is not used
    static constexpr int CANVAS = 1;

protected:
    void resizeEvent(QResizeEvent *event) override
    {
        QList<int> s = sizes(); // ancora quelle di prima del ridimensionamento
        QSplitter::resizeEvent(event);
        const int delta = event->size().width() - event->oldSize().width();
        if(event->oldSize().width() < 0 || delta == 0 || preferred.size() != s.size())
            return;
        // stringendo si prende il margine di ogni colonna, allargando si restituisce il perso
        QList<int> room(s.size(), 0);
        int total = 0;
        for(int i = 0; i < s.size(); i++){
            if(i == CANVAS)
                continue;
            QScrollArea *area = static_cast<QScrollArea*>(widget(i));
            const int need = area->widget()->minimumSizeHint().width()
                           + style()->pixelMetric(QStyle::PM_ScrollBarExtent);
            room[i] = qMax(0, delta < 0 ? s[i] - need : preferred[i] - s[i]);
            total += room[i];
        }
        const int moved = qMin(qAbs(delta), total);
        const int sign = delta < 0 ? -1 : 1;
        int left = moved;
        for(int i = 0; i < s.size() && total > 0; i++){
            const int share = int(qint64(moved) * room[i] / total); // in proporzione al margine
            s[i] += sign * share;
            left -= share;
        }
        for(int i = 0; i < s.size() && left > 0; i++) // gli avanzi dell'arrotondamento
            if(room[i] > 0){
                s[i] += sign;
                left--;
            }
        s[CANVAS] += delta - sign * moved;
        setSizes(s);
    }
};

} // namespace

/// @details The .ui fixes each side column's width, which together with the bottom frame's
/// old 600 px minimum height made the window unusable below roughly 1600x1000: the columns
/// could neither shrink nor scroll. The columns are moved, not rebuilt, so every `ui->`
/// pointer and connection stays valid.
void Widget::BuildResizableSidebars()
{
    const int scroll_bar = style()->pixelMetric(QStyle::PM_ScrollBarExtent);
    BottomRowSplitter *split = new BottomRowSplitter(ui->bottomframe);
    QList<int> sizes;
    auto add_column = [&](QWidget *column){
        // i limiti fissi del .ui impedirebbero sia di allargare la colonna sia di farla
        // scorrere: senza minimo esplicito l'area di scorrimento usa quello del contenuto
        column->setMinimumSize(0, 0);
        column->setMaximumWidth(QWIDGETSIZE_MAX);
        QScrollArea *area = new QScrollArea;
        area->setWidget(column);
        area->setWidgetResizable(true);
        area->setFrameShape(QFrame::NoFrame);
        split->addWidget(area);
        split->setCollapsible(split->count() - 1, false);
        // posto per la barra verticale fin dall'inizio, o la sua comparsa farebbe comparire
        // anche quella orizzontale
        sizes << column->sizeHint().width() + scroll_bar;
    };
    add_column(ui->toolsFrame);
    ui->viewBox->setMinimumSize(0, 0); // era 800x500: la vista scorre già da sé
    split->addWidget(ui->viewFrame);
    split->setCollapsible(1, false);
    // lo spazio in più va alla tela, non alle colonne (che però cedono il loro margine per
    // prime: vedi BottomRowSplitter)
    split->setStretchFactor(1, 1);
    sizes << ui->viewFrame->sizeHint().width();
    add_column(ui->groupBox_3);
    add_column(ui->layersFrame);
    split->setSizes(sizes);
    split->preferred = sizes;
    // le maniglie di default sono invisibili: una riga fissa che si accende al passaggio
    // del mouse mostra dove trascinare
    split->setObjectName("sidebarSplitter");
    split->setHandleWidth(7);
    split->setStyleSheet("#sidebarSplitter::handle { background: palette(mid); margin: 0 2px; }"
                         "#sidebarSplitter::handle:hover { background: palette(highlight); margin: 0; }");
    // la lista layer e il nome erano limitati alla larghezza fissa della loro colonna
    ui->stickLayerView->setMaximumWidth(QWIDGETSIZE_MAX);
    ui->stickFigureNameText->setMaximumWidth(QWIDGETSIZE_MAX);

    ui->horizontalLayout_4->addWidget(split);
    delete ui->groupBox_9; // vuoto: teneva solo la colonna strumenti e la tela
}

/// @details See the header. Every branch either applies a brush through ApplyStyleBrush()
/// or leaves the current one alone (a cancelled dialog must not change anything) — and since
/// ApplyStyleBrush() ASSIGNS the brush, one fill (or one stroke) at a time is all a stick can
/// have: picking a gradient over a pattern drops the tile, its placement and its tiling with it.
void Widget::ChooseStyle(bool for_fill, int kind, bool replace)
{
    switch(kind){
    case STYLE_NONE:
        ApplyStyleBrush(for_fill, QBrush(Qt::NoBrush));
        break;

    case STYLE_BRUSH: {
        if(for_fill)
            break;              // non è nella combo del riempimento: nulla da timbrare lungo
        if(!brush_stroke_dialog)
            brush_stroke_dialog = new BrushStrokeDialog(brush_folder.path(), this);
        // il pennello parte da quello dello stick selezionato, non dall'ultimo scelto: la
        // finestra è un editor del contorno che c'è, non un ricordo di quello di prima
        StickData *current = view->my_animation->current_frame == nullptr ? nullptr
                           : view->my_animation->CurrentFrame()->CurrentStick();
        BrushStroke seed = current != nullptr ? current->StrokeBrush() : view->my_stroke_brush;
        if(current == nullptr){
            // niente stick da cui leggere colore/spessore/opacità: quelli della penna corrente
            seed.color = view->my_pen.brush().color();
            seed.size = qMax(1.0, view->my_pen.widthF());
            seed.opacity = ReadPaintOpacity(view->my_pen.brush());
        }
        brush_stroke_dialog->Seed(seed);
        if(brush_stroke_dialog->exec() != QDialog::Accepted)
            break;
        view->my_stroke_brush = brush_stroke_dialog->Brush();
        if(view->my_stroke_brush.stamp.isNull())
            break;              // libreria vuota: non c'è alcuna punta da adottare
        // i dab sono tinti con il COLORE della penna, quindi la penna diventa la tinta unita che
        // la finestra ha scelto: un gradiente o una tile lì sotto non verrebbe più tracciato da
        // nessuno, e il colore deve poter cambiare senza passare da "Solid" (che butta il pennello)
        ApplyStyleBrush(false, QBrush(view->my_stroke_brush.color), STYLE_BRUSH);
        break;
    }

    case STYLE_SOLID: {
        const QBrush current = for_fill ? view->my_brush : view->my_pen.brush();
        QColor color = QColorDialog::getColor(current.color(), this,
                                             for_fill ? tr("Fill color") : tr("Stroke color"),
                                             QColorDialog::ShowAlphaChannel);
        if(!color.isValid())
            break;
        // l'opacità resta quella dello slider, come nei due handler storici che questo
        // ramo ha sostituito
        color.setAlphaF((for_fill ? brush_opacity_slider->value()
                                  : pen_opacity_slider->value()) / 100.0);
        ApplyStyleBrush(for_fill, QBrush(color));
        break;
    }

    case STYLE_LINEAR:
    case STYLE_RADIAL: {
        // una finestra per tipo, tenuta in vita: riaprirla riparte dall'ultimo gradiente
        GradientDialog *&dialog = (kind == STYLE_LINEAR) ? linear_gradient_dialog
                                                         : radial_gradient_dialog;
        if(!dialog)
            dialog = new GradientDialog(kind == STYLE_LINEAR ? GradientDialog::LINEAR
                                                             : GradientDialog::RADIAL, this);
        if(dialog->exec() == QDialog::Accepted)
            ApplyStyleBrush(for_fill, QBrush(dialog->Gradient()));
        break;
    }

    case STYLE_IMAGE: {
        QBrush current = for_fill ? view->my_brush : view->my_pen.brush();
        // replace: la scelta viene dalla combo, quindi è un riempimento NUOVO e va chiesta
        // l'immagine anche se una texture c'è già (un pattern è una texture come le altre: senza
        // questo, "Immagine" scivolava nelle opzioni della tile precedente senza sostituirla).
        // Il bottone della campionatura passa false e resta la strada per rivedere le opzioni.
        if(replace || current.style() != Qt::TexturePattern){
            // la prima cosa da chiedere è QUALE immagine, non come sistemarla — le opzioni
            // arrivano subito dopo, sull'immagine appena scelta
            const QString path = QFileDialog::getOpenFileName(this, tr("Fill image"), QString(),
                                     tr("Images (*.png *.jpg *.jpeg *.bmp *.gif *.tif *.tiff *.webp)"));
            if(path.isEmpty())
                break;
            const QImage image(path);
            if(image.isNull()){
                QMessageBox::warning(this, tr("Fill image"), tr("Cannot read %1.").arg(path));
                break;
            }
            // stessa scala dei pattern, e con lo stesso motivo: un'immagine da file è grande
            // uno o due ordini di grandezza più di uno stick
            current = TextureBrush(image, texture_tile_size);
        }
        if(!texture_options_dialog)
            texture_options_dialog = new TextureOptionsDialog(this);
        // il ritaglio a istanza singola vale solo per il RIEMPIMENTO: un contorno ritagliato a
        // una tile sarebbe un frammento di contorno, non un contorno
        texture_options_dialog->Seed(current, view->my_texture_tiled, for_fill);
        if(texture_options_dialog->exec() != QDialog::Accepted)
            break;
        const QBrush chosen = texture_options_dialog->Texture();
        if(chosen.style() != Qt::TexturePattern)
            break; // nessuna immagine: non c'è niente da applicare
        if(for_fill)
            view->my_texture_tiled = texture_options_dialog->Tiled();
        ApplyStyleBrush(for_fill, chosen, STYLE_IMAGE);
        break;
    }

    case STYLE_PATTERN:
        // l'unico ramo asincrono: la finestra resta aperta e richiama ApplyStyleBrush() a
        // ogni click, quindi il bersaglio va ricordato adesso
        pattern_target_is_fill = for_fill;
        if(!pattern_library){
            pattern_library = new PatternLibrary(pattern_folder.path(), this);
            connect(pattern_library, &PatternLibrary::patternChosen, this,
                    [this](const QImage &tile, int tile_size){
                texture_tile_size = tile_size; // vale anche per il prossimo riempimento a immagine
                ApplyStyleBrush(pattern_target_is_fill, TextureBrush(tile, tile_size));
            });
            // doppio click / "Apply to stick": porta il brush corrente SULLO stick, la stessa
            // strada dei bottoni "applica" della colonna Style (undo incluso)
            connect(pattern_library, &PatternLibrary::patternApplied, this, [this]{
                view->SetGraphics(false, pattern_target_is_fill ? ATTRIBUTE_BRUSHCOLOR
                                                                : ATTRIBUTE_PENCOLOR);
            });
        }
        pattern_library->show();
        pattern_library->raise();
        break;
    }
}

/**
 * @details "No stroke" is stored as Qt::NoPen rather than as a pen holding an empty brush:
 * that is the form every paint engine and both foreign exporters already recognize
 * (StkFormat reads pen.style() == Qt::NoPen as thickness 0). Coming back out of NoPen
 * restores a solid line without touching any other pen style, in case one is ever added.
 */
void Widget::ApplyStyleBrush(bool for_fill, const QBrush &brush, int kind)
{
    if(for_fill){
        view->my_brush = brush;
    } else if(brush.style() == Qt::NoBrush){
        view->my_pen.setStyle(Qt::NoPen);
    } else {
        if(view->my_pen.style() == Qt::NoPen)
            view->my_pen.setStyle(Qt::SolidLine);
        view->my_pen.setBrush(brush);
    }
    // un contorno alla volta, come un riempimento alla volta: scegliere qualunque altro tipo di
    // bordo butta via il pennello, altrimenti i dab resterebbero sopra la nuova penna e la scelta
    // sembrerebbe non aver fatto nulla. Un contorno a pennello passa STYLE_BRUSH esplicito perché
    // la sua penna È una tinta unita e il brush da solo non potrebbe mai dirlo.
    if(!for_fill && kind != STYLE_BRUSH)
        view->my_stroke_brush.stamp = QImage();
    // una texture appena scelta arriva opaca (QBrush(tile) parte da un colore nero pieno): dalle
    // subito l'opacità che lo slider mostra già, altrimenti il cursore starebbe a metà e il
    // riempimento sarebbe pieno finché non lo si tocca
    // live=false: qui si sta ADOTTANDO un pennello, non modificando lo stick — a portarlo sullo
    // stick ci pensano i bottoni Apply (o il doppio click della libreria pattern)
    if(brush.style() == Qt::TexturePattern)
        SetPaintOpacity(for_fill, for_fill ? brush_opacity_slider->value()
                                           : pen_opacity_slider->value(), false);
    QComboBox *combo = for_fill ? fill_type_combo : stroke_type_combo;
    const int index = combo->findData(kind < 0 ? KindOfBrush(brush) : kind);
    if(index >= 0){
        QSignalBlocker mute(combo); // activated() non scatta da codice, currentIndexChanged sì
        combo->setCurrentIndex(index);
    }
    RefreshStyleSwatch(for_fill);
}

/// @details The two buttons are a fixed 20x20 in widget.ui, so a 14x14 icon leaves their
/// frame visible. The old `background-color` stylesheet is cleared because it would paint
/// over the icon — and it could only ever show a flat colour anyway.
void Widget::RefreshStyleSwatch(bool for_fill)
{
    QPushButton *button = for_fill ? ui->currentBrushColorBtn : ui->currentColorBtn;
    const QBrush brush = for_fill ? view->my_brush
                                  : (view->my_pen.style() == Qt::NoPen ? QBrush(Qt::NoBrush)
                                                                       : view->my_pen.brush());
    QPixmap swatch(14, 14);
    swatch.fill(Qt::transparent);
    QPainter painter(&swatch);
    if(brush.style() == Qt::NoBrush){
        // scatola vuota con la diagonale: la convenzione grafica per "nessun colore"
        painter.setPen(QPen(Qt::gray));
        painter.drawRect(0, 0, swatch.width()-1, swatch.height()-1);
        painter.drawLine(0, swatch.height()-1, swatch.width()-1, 0);
    } else {
        painter.setPen(Qt::NoPen);
        // una texture porta la sua opacità nell'alpha del colore del brush, che Qt non usa per
        // dipingerla: applicala qui come fa StickGraphicsItem::FillAndStroke(), sennò il
        // campione resta pieno mentre il disegno sbiadisce
        painter.setOpacity(StickGraphicsItem::FillOpacity(brush));
        painter.setBrush(brush);
        painter.drawRect(swatch.rect());
    }
    painter.end();
    button->setStyleSheet(QString());
    button->setIcon(QIcon(swatch));
    button->setIconSize(swatch.size());
}

/**
 * @details Re-opens the chooser for whatever the Border type combo currently says, rather
 * than always the colour picker: this is the button that used to BE the colour picker, so
 * it stays the one place to go back and edit the current stroke — the gradient's stops, the
 * pattern library, another image file — instead of the type having to be re-picked from the
 * combo to get its options back.
 */
void Widget::on_chooseColorBtn_clicked()
{
    ChooseStyle(false, stroke_type_combo->currentData().toInt());
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

/// @details Fill twin of on_chooseColorBtn_clicked() — see its doc: it re-opens the options
/// of the fill's CURRENT type, which for a solid fill is the colour picker as always.
void Widget::on_selectBrushColorBtn_clicked()
{
    ChooseStyle(true, fill_type_combo->currentData().toInt());
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



/**
 * @details Applies the slider's value as alpha on the current pen paint, mirroring it onto the
 * numeric spinbox. Handled by SetPaintOpacity() so the stroke and the fill cannot drift apart.
 */
void Widget::on_penOpacitySlider_valueChanged(int value)
{
    Q_UNUSED(value)
    SetPaintOpacity(false, pen_opacity_slider->value());
    pen_opacity_spinbox->setValue(pen_opacity_slider->value());
}

/// @details Fill counterpart of on_penOpacitySlider_valueChanged().
void Widget::on_fillOpacitySlider_valueChanged(int value)
{
    Q_UNUSED(value)
    SetPaintOpacity(true, brush_opacity_slider->value());
    brush_opacity_spinbox->setValue(brush_opacity_slider->value());
}

/**
 * @details One place for both sliders, and for the two brush kinds the alpha means something
 * different for:
 * - a **solid colour** — the alpha IS the colour's, which is what these sliders always wrote;
 * - a **texture** — the alpha of a texture brush's QBrush::color() is unused by Qt's rendering
 *   (setColor() is documented as having no effect on one), so it is free to carry the tile's
 *   opacity, which StickGraphicsItem::FillOpacity() reads back at paint time. Absolute, not
 *   compounding, and it persists with the brush: nothing new to serialize.
 *
 * The stroke goes through `setBrush()` rather than `setColor()`, because QPen::setColor()
 * REPLACES the pen's brush with a flat one — it would turn a textured stroke into a plain colour.
 *
 * It writes to the SELECTED STICK as well as to the current paint (@p live). The sliders used to
 * change only the current paint, so nothing happened until an Apply button was pressed — which,
 * next to the controls that do act at once (the type combos apply on Apply, but the anchoring and
 * tiling boxes are immediate), read as the opacity simply being ignored. Now they behave like
 * every other per-stick slider in the panel: one StoreUndo() at the start of a drag, coalesced by
 * a sliderFlags latch, so the whole drag is one undo step.
 *
 * - a **gradient** — its opacity is its stops', scaled relative to the most opaque one, so the
 *   transparency dialled per stop keeps its ratios and the slider still reads one number. That is
 *   also the only place it CAN live: QDataStream restores a QBrush's colour for a solid and a
 *   texture but not for a gradient (measured), so an alpha parked there would vanish on save.
 *
 * All three go through ReadPaintOpacity()/WritePaintOpacity() (stickdata.h), which is what keeps
 * this slider, its echo in Animation::UpdateStyleControls() and the paint path from disagreeing
 * about where a given kind of brush keeps the number.
 */
void Widget::SetPaintOpacity(bool for_fill, int percent, bool live)
{
    auto tinted = [percent](QBrush brush) {
        WritePaintOpacity(&brush, percent / 100.0);
        return brush;
    };
    auto fadeable = [](const QBrush &brush) {
        return brush.style() != Qt::NoBrush; // solo il vuoto non ha niente da sbiadire
    };
    // il pennello corrente: quel che erediteranno i prossimi stick e gli Apply
    if(fadeable(for_fill ? view->my_brush : view->my_pen.brush())){
        if(for_fill)
            view->my_brush = tinted(view->my_brush);
        else
            view->my_pen.setBrush(tinted(view->my_pen.brush()));
        RefreshStyleSwatch(for_fill);
    }
    if(!live || view->my_animation->current_frame == nullptr)
        return;
    // ...e lo stick selezionato, subito
    StickData *cs = view->my_animation->CurrentFrame()->CurrentStick();
    if(cs == nullptr)
        return;
    if(for_fill ? !fadeable(cs->brush)
                : (cs->pen.style() == Qt::NoPen || !fadeable(cs->pen.brush())))
        return;
    bool &latch = for_fill ? sliderFlags.fillOpacity : sliderFlags.penOpacity;
    if(!latch){
        view->StoreUndo(CMD_SIMPLE); // un solo passo di undo per tutto il trascinamento
        latch = true;
    }
    if(for_fill)
        cs->brush = tinted(cs->brush);
    else
        cs->pen.setBrush(tinted(cs->pen.brush()));
    if(cs->view)
        cs->view->Refresh(0);
    if(view->my_animation->CurrentFrame()->CurrentStickFigure())
        view->my_animation->CurrentFrame()->CurrentStickFigure()->UpdateIcon();
}

/// @details Lowers the drag latch so the NEXT drag starts a fresh undo entry — the same
/// coalescing pattern as on_imgWSlider_sliderReleased() and friends.
void Widget::on_penOpacitySlider_sliderReleased()
{
    sliderFlags.penOpacity = false;
}

/// @details Fill counterpart of on_penOpacitySlider_sliderReleased().
void Widget::on_fillOpacitySlider_sliderReleased()
{
    sliderFlags.fillOpacity = false;
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
 * lost), then creates a brand new, empty *.samlib file at the chosen path and switches
 * to it as the open library.
 */
void Widget::on_createNewLibraryBtn_clicked()
{
    on_saveCurrentLibraryBtn_clicked();
    QString file_name = QFileDialog::getSaveFileName(this,tr("create a new SAM library"),
                       lib_folder.path(),"SAM Library(*.samlib)");
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
 * @details Confirms before deleting the *.samlib file itself; the list row is removed
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
/**
 * @details Snapshots every SELECTED frame into `frame_copy_buffer` for
 * on_pasteFrame_clicked() to consume — one frame with no multi-selection, the whole block
 * with one. The buffer is pure data (Phase 2b): direct extraction, no stream and no flags,
 * and cheap in memory whatever the block's size, since a snapshot shares its images with
 * the live sticks (QImage is copy-on-write — see CLAUDE.md "Images are shared").
 */
void Widget::on_cpyFrame_clicked()
{
    qDeleteAll(frame_copy_buffer);
    frame_copy_buffer.clear();
    for(Frame *frame : SelectedFrames())
        frame_copy_buffer.append(ExtractFrameData(*frame));
}

/**
 * @details Two different pastes, told apart by how many frames were copied:
 * - a BLOCK of frames enters as frames, in its original order, right after the current one
 *   (MyView::InsertFrames(), one undo step for the lot). The buffer is handed Clone()s, so
 *   the same block can be pasted again;
 * - a single frame keeps its historical behaviour and merges instead: one new layer per
 *   stickfigure in the buffer is added to the *current* frame rather than creating a frame
 *   for it, and the last figure pasted becomes the new current selection. It is the only way
 *   to fold one frame's figures into another, so it is deliberately not unified with the above.
 */
void Widget::on_pasteFrame_clicked()
{
    if(frame_copy_buffer.isEmpty())
        return;
    view->ExitDrawMode(); // il paste aggiunge layer al frame corrente: chiudi prima il disegno
    if(frame_copy_buffer.count() > 1){
        QList<FrameData *> snapshots;
        for(FrameData *copied : frame_copy_buffer)
            snapshots.append(copied->Clone()); // l'undo stack li prende in proprietà, il buffer resta
        view->InsertFrames(view->my_animation->CurrentFrame()->frame_number + 1, snapshots);
        return;
    }
    view->StoreUndo();
    // add a new stickfigure per every stickfigure in the copy buffer
    for(StickFigureData*S:frame_copy_buffer.first()->stick_figures){
        StickFigure *added = AddStick();
        PopulateLiveStickFigure(added,*S,true,true);
        for(StickData*s:added->stick_list){
            view->my_animation->CurrentFrame()->total_sticks.append(s);
        }
        view->AddHandles(added); // mancava: senza questo i manici degli stick incollati non comparivano
        added->linked_item->setData(Qt::DisplayRole,added->name);
        added->UpdateIcon(); // mancava: l'icona del layer incollato restava quella vuota di AddStick()
        if(frame_copy_buffer.first()->stick_figures.last() == S){
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

///@brief switches to the text stick tool
void Widget::on_drawTextBtn_clicked()
{
    view->SetTool(DRAWTEXT);
}

