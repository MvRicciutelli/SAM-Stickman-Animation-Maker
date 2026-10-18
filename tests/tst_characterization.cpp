// Characterization tests (REFACTOR_PLAN.md, Phase 0).
// They assert the CURRENT behavior of the model/undo/serialization code, driving it exactly the
// way the UI handlers in widget.cpp / myview.cpp do — each helper cites the call site it
// replicates. If a later refactor phase changes observable behavior, a failure here means
// "behavior changed", not necessarily "behavior is wrong".
#include <QtTest>
#include <cmath>
#include <QFontDatabase>
#include <QGraphicsScene>
#include <QGraphicsRectItem>
#include <QTabWidget>
#include <QTemporaryDir>
#include <QtEndian>
#include <QTemporaryFile>
#include "myview.h"
#include "gradientdialog.h"
#include "uiItems.h"
#include "framepresenter.h"
#include "stkformat.h"
#include "abrbrush.h"
#include "brushpicker.h"
#include "mfvideo.h"
#include "imageexport.h"

extern QGraphicsScene *scene;
extern QGraphicsRectItem *my_rect;
extern QGraphicsRectItem *limit_rect;
extern MyView *V;
extern qint32 W;
extern qint32 H;
extern FrameData *stick_library_buffer;

// QMessageBox's static helpers are modal and would hang a headless run: close whatever modal
// box appears, and count them so a test can also require that the user WAS told.
struct ModalDismisser
{
    QTimer timer;
    int dismissed = 0;
    ModalDismisser()
    {
        timer.setInterval(10);
        QObject::connect(&timer, &QTimer::timeout, [this]{
            if(QWidget *modal = QApplication::activeModalWidget()){
                dismissed++;
                modal->close();
            }
        });
        timer.start();
    }
};

class TestCharacterization : public QObject
{
    Q_OBJECT

    MyView *view = nullptr;

    Frame *CurrentFrame() { return view->my_animation->current_frame; }
    StickFigure *CurrentStickFigure() { return CurrentFrame()->current_stick_figure; }

    // replicates Widget::AddStick() (widget.cpp:421)
    StickFigure *AddStick(const QString &name = QString())
    {
        return CurrentFrame()->AddStickFigure(my_stick_figure_widget_list, name);
    }

    // replicates Widget::AddFrame() (widget.cpp:487)
    Frame *AddFrame()
    {
        view->StoreUndo(CMD_ADDFRAME);
        int pos = 0;
        if (view->my_animation->frame_list.count() >= 1)
            pos = CurrentFrame()->frame_number + 1;
        Frame *added_frame = view->SetUpFrame(pos);
        view->MoveToFrame(added_frame);
        AddStick();
        return added_frame;
    }

    // replicates MyView::DrawCmd() for a LINE stick, press + release (myview.cpp:449) —
    // DrawCmd itself is private and only reachable through mouse events
    StickData *DrawStick(QPointF from, QPointF to)
    {
        StickFigure *figure = CurrentStickFigure();
        view->StoreUndo();
        figure->StartDrawing(&from, view->my_pen, view->my_brush);
        StickData *drawn = figure->current_stick;
        FramePresenter::AttachView(drawn, scene);
        drawn->view->selected = true;
        drawn->hard_top = hard_top_check->isChecked();
        drawn->hard_bottom = hard_bottom_check->isChecked();
        drawn->pen = view->my_pen;
        figure->EndDrawing(&to);
        scene->addItem(drawn->view->stick_handle[0]);
        scene->addItem(drawn->view->stick_handle[1]);
        CurrentFrame()->UpdateIcon();
        view->my_animation->UpdateTab(drawn->stick_type);
        CurrentFrame()->total_sticks.append(drawn);
        view->my_animation->UpdateSelection(drawn);
        return drawn;
    }

    // Group D bug 2: changing the master stick reverses the spine (p1<->p2) of the sticks
    // on the parent chain. A POLYGON/MULTILINE in that chain must keep its shape in scene
    // space (its vertices are stored normalized to the reversed spine's frame; before the
    // fix they flipped/reshaped). Builds master(LINE)->middle(vertex)->grandchild(LINE),
    // makes the grandchild master (SetDirectChainAsMaster -> reverses master+middle) and
    // checks the middle stick's scene vertices are unchanged. (Not a test slot: the
    // ChangeMasterKeeps*Shape wrappers drive it.)
    void ChangeMasterKeepsVertexShape(qint32 middle_type)
    {
        AddFrame();
        StickFigure *fig = CurrentStickFigure();
        // wipe whatever AddStick/DrawStick may have left; build a clean linear chain
        qDeleteAll(fig->stick_list);
        fig->stick_list.clear();

        StickData *master = new StickData();
        master->stick_type = StickData::LINE;
        master->my_line = QLineF(100, 100, 100, 200);
        master->parent = nullptr;
        master->stepchild = false;

        StickData *mid = new StickData();
        mid->stick_type = middle_type;
        mid->my_line = QLineF(100, 200, 200, 200);
        mid->parent = master;
        mid->stepchild = false;
        // choose scene vertices, then store them normalized to mid's current frame
        QList<QPointF> want;
        if (middle_type == StickData::MULTILINE)
            want << QPointF(130, 180) << QPointF(170, 220); // interior only (endpoints are p1/p2)
        else
            want << QPointF(120, 180) << QPointF(180, 180) << QPointF(150, 240);
        QTransform inv = mid->PolygonTransform().inverted();
        for (const QPointF &p : want)
            mid->polygon_points.append(inv.map(p));
        if (middle_type == StickData::MULTILINE) {
            mid->vertex_widths << 3 << 4 << 5 << 6;        // per-vertex incl. endpoints
            mid->vertex_curvatures << 0.1f << 0.2f << 0.3f << 0.4f;
            // con un ramo: P1->(130,180)->P2 è la linea principale, (170,220) pende da P1.
            // Lo scambio p1<->p2 rovescia gli indici: i LATI devono restare gli stessi
            mid->vertex_parents = {-1, 0, 0, 1};
        }

        StickData *gc = new StickData();
        gc->stick_type = StickData::LINE;
        gc->my_line = QLineF(200, 200, 200, 300);
        gc->parent = mid;
        gc->stepchild = false;

        master->children << mid << gc; // children is flattened (all descendants)
        mid->children << gc;

        fig->stick_list << master << mid << gc;
        fig->master_stick = master;
        fig->current_stick = master;

        // scene positions of the middle stick's full vertex path, BEFORE the master change
        auto scene_verts = [&](StickData *s) {
            QList<QPointF> v;
            QTransform t = s->PolygonTransform();
            if (s->stick_type == StickData::MULTILINE) v << s->my_line.p1();
            for (const QPointF &p : s->polygon_points) v << t.map(p);
            if (s->stick_type == StickData::MULTILINE) v << s->my_line.p2();
            return v;
        };
        QList<QPointF> before = scene_verts(mid);
        // a multiline's edges as unordered pairs of scene points (rounded, to compare them)
        auto scene_edges = [&](StickData *s) {
            QList<QPointF> v = scene_verts(s);
            QList<qint32> parents = s->MultilineParents();
            QSet<QString> edges;
            auto key = [](QPointF p) { return QString("%1,%2").arg(qRound(p.x())).arg(qRound(p.y())); };
            for (int i = 1; i < v.count(); i++) {
                QString a = key(v[i]), b = key(v[parents[i]]);
                edges.insert(a < b ? a + "|" + b : b + "|" + a);
            }
            return edges;
        };
        QSet<QString> edges_before = scene_edges(mid);

        fig->SetMaster(gc);
        QCOMPARE(fig->master_stick, gc); // the change actually happened

        if (middle_type == StickData::MULTILINE)
            QCOMPARE(scene_edges(mid), edges_before); // the branch still hangs from the same vertex
        QList<QPointF> after = scene_verts(mid);
        // the shape occupies the same scene points (order may reverse for a multiline —
        // same visual polyline), so compare as an unordered set within tolerance
        QCOMPARE(after.count(), before.count());
        for (const QPointF &b : before) {
            bool found = false;
            for (const QPointF &a : after)
                if (QLineF(a, b).length() < 0.5) { found = true; break; }
            QVERIFY2(found, "a vertex moved: shape flipped/reshaped on master change");
        }

        // ...and img_angle must keep tracking the (reversed) spine, exactly as a fresh
        // draw/Manipulate leaves it. StickGraphicsItem::Rotate() rewrites img_angle from
        // the drag direction on the first rotate; if the master change left it 180° off
        // (static render still fine, because polygon_points were baked against it) the
        // vertices snap on that first rotate (Group D follow-up).
        float expected = -atan2(mid->my_line.dx(), mid->my_line.dy()) * 180.0 / M_PI;
        float diff = std::fmod(std::abs(mid->img_angle - expected), 360.0f);
        if (diff > 180.0f) diff = 360.0f - diff;
        QVERIFY2(diff < 0.5f, "img_angle no longer tracks the spine: vertices jump on the first rotate");
    }

private slots:
    void initTestCase()
    {
        // mirrors the parts of Widget::Widget() (widget.cpp:143) the model layer depends on,
        // without the file I/O, the config dialogs and the library autoload
        my_stick_figure_widget_list = new QListWidget;
        my_frame_widget_list = new QListWidget;
        my_library_list_widget = new QListWidget;
        my_current_library_widget = new QListWidget;
        img_list_widget = new QListWidget;
        img_buttons_row = new QWidget;  // nell'app è il frame dei bottoni immagine (widget.cpp)
        onion_skin_sb = new QSpinBox;   // value 0 -> UpdateOnionSkins() is a no-op
        fps_spin_box = new QSpinBox;
        img_rotation_slider = new QSlider;
        img_h_offset_slider = new QSlider;
        img_v_offset_slider = new QSlider;
        img_width_slider = new QSlider;
        img_height_slider = new QSlider;
        img_h_offset_spinbox = new QSpinBox;
        img_v_offset_spinbox = new QSpinBox;
        img_width_spinbox = new QSpinBox;
        img_height_spinbox = new QSpinBox;
        img_rotation_spinbox = new QSpinBox;
        pen_opacity_spinbox = new QSpinBox;
        brush_opacity_spinbox = new QSpinBox;
        line_edit_thickness_width_spinbox = new QSpinBox;
        stick_figure_scale_spinbox = new QSpinBox;
        stick_figure_rotation_spinbox = new QSpinBox;
        circle_squash_spinbox = new QSpinBox;
        elongate_spinbox = new QDoubleSpinBox;
        elongate_spinbox->setRange(0, W);
        depth_spinbox = new QDoubleSpinBox;
        pen_opacity_slider = new QSlider;
        brush_opacity_slider = new QSlider;
        depth_slider = new QSlider;
        taper_w_slider = new QSlider;
        taper_h_slider = new QSlider;
        advanced_rect_slider = new QSlider;
        edit_line_thickness_slider = new QSlider;
        edit_line_thickness_slider->setRange(0, 100);
        circle_squash_slider = new QSlider;
        circle_squash_slider->setRange(-100, 100);
        hard_top_check = new QCheckBox;
        hard_bottom_check = new QCheckBox;
        dbg_lbl = new QLabel;

        scene = new QGraphicsScene;
        view = new MyView;
        V = view;
        view->setScene(scene);
        view->my_animation->scene = scene;

        advanced_img_tab = new QWidget;
        advanced_vertex_tab = new QWidget;
        vertex_width_row = new QWidget(advanced_vertex_tab);
        vertex_width_slider = new QSlider;
        vertex_curve_slider = new QSlider;
        advanced_line_tab = new AdvancedLineWidget;
        advanced_circle_tab = new AdvancedCircleWidget;
        advanced_taper_tab = new AdvancedTaperWidget;
        advanced_rect_tab = new AdvancedTab;
        my_tab_widget = new QTabWidget;
        my_tab_widget->addTab(new QWidget, "tools");
        my_tab_widget->addTab(new QWidget, "placeholder"); // UpdateTab() swaps index 1

        my_rect = new QGraphicsRectItem(0, 0, W, H);
        my_rect->setZValue(-100);
        my_rect->setPen(Qt::NoPen);
        scene->addItem(my_rect);
        limit_rect = new QGraphicsRectItem(-W / 10.0, -H / 10.0, W * 1.1, H * 1.1);
        scene->setSceneRect(limit_rect->rect());

        AddFrame();
        CurrentFrame()->current_stick_figure = CurrentFrame()->stick_figures[0];
    }

    void StickLineRoundTrip()
    {
        StickData source;
        source.my_line = QLineF(1, 2, 3, 4);
        source.z = 5.5;
        source.stick_type = StickData::LINE;
        source.stepchild = true;
        source.parent_idx = 7;
        source.order = 3;
        source.pen = QPen(Qt::red);
        source.pen.setWidth(9);
        source.brush = QBrush(Qt::blue);

        QByteArray bytes;
        {
            QDataStream out(&bytes, QIODevice::WriteOnly);
            out << source;
        }
        StickData loaded;
        {
            QDataStream in(&bytes, QIODevice::ReadOnly);
            in >> loaded;
        }
        QCOMPARE(loaded.my_line, source.my_line);
        QCOMPARE(loaded.z, source.z);
        QCOMPARE(loaded.version, source.version);
        QCOMPARE(loaded.stick_type, source.stick_type);
        QCOMPARE(loaded.stepchild, source.stepchild);
        QCOMPARE(loaded.parent_idx, source.parent_idx);
        QCOMPARE(loaded.order, source.order);
        QCOMPARE(loaded.pen, source.pen);
        QCOMPARE(loaded.brush, source.brush);
    }

    void StickCircleRoundTrip()
    {
        StickData source;
        source.my_line = QLineF(10, 20, 30, 40);
        source.z = 1;
        source.stick_type = StickData::CIRCLE;
        source.img_angle = 33.5f;
        source.img_angle_offset = 42; // regression: used to reset on every save/load/clone
        source.img_h_scale = 1.5f;
        source.img_w_scale = 0.5f;
        source.img_offset = QPointF(3, 4);
        source.hard_top = true;
        source.hard_bottom = true;

        QByteArray bytes;
        {
            QDataStream out(&bytes, QIODevice::WriteOnly);
            out << source;
        }
        StickData loaded;
        {
            QDataStream in(&bytes, QIODevice::ReadOnly);
            in >> loaded;
        }
        QCOMPARE(loaded.stick_type, source.stick_type);
        QCOMPARE(loaded.img_angle, source.img_angle);
        QCOMPARE(loaded.img_angle_offset, source.img_angle_offset);
        QCOMPARE(loaded.img_h_scale, source.img_h_scale);
        QCOMPARE(loaded.img_w_scale, source.img_w_scale);
        QCOMPARE(loaded.img_offset, source.img_offset);
        QCOMPARE(loaded.hard_top, source.hard_top);
        QCOMPARE(loaded.hard_bottom, source.hard_bottom);
    }

    // regression (found during Phase 1a): the stick_img index must be written as a 4-byte
    // int — Qt6's QList::indexOf returns qsizetype, and streaming it uncast wrote 8 bytes
    // while operator>> reads 4, desyncing every IMAGE-stick save
    void StickImageRoundTrip()
    {
        StickData source;
        source.stick_type = StickData::IMAGE;
        source.my_line = QLineF(0, 0, 0, 50);
        source.z = 2;
        QImage *first_image = new QImage(8, 8, QImage::Format_ARGB32);
        first_image->fill(Qt::red);
        QImage *second_image = new QImage(8, 8, QImage::Format_ARGB32);
        second_image->fill(Qt::blue);
        source.stick_img_list << first_image << second_image;
        source.img_name_list << "red" << "blue";
        source.stick_img = second_image;
        source.img_angle_offset = 15;

        QByteArray bytes;
        {
            QDataStream out(&bytes, QIODevice::WriteOnly);
            out << source;
        }
        StickData loaded;
        {
            QDataStream in(&bytes, QIODevice::ReadOnly);
            in >> loaded;
        }
        QCOMPARE(loaded.stick_img_list.count(), 2);
        QCOMPARE(loaded.img_name_list, source.img_name_list);
        QCOMPARE(loaded.stick_img, loaded.stick_img_list[1]); // displayed-image index survives
        QCOMPARE(*loaded.stick_img_list[0], *first_image);
        QCOMPARE(*loaded.stick_img_list[1], *second_image);
        QCOMPARE(loaded.img_angle_offset, source.img_angle_offset);
        // no manual cleanup: ~StickData frees the images, ~stick only the handles
    }

    // FREEHAND is an IMAGE-like raster stick (its painted picture lives in stick_img like
    // an IMAGE's); the IsImageType() gates in the stream operators must serialize it the
    // same way. Guards the freehand-draw-mode feature's persistence: a saved drawing must
    // reload byte-identical.
    void StickFreehandRoundTrip()
    {
        StickData source;
        source.stick_type = StickData::FREEHAND;
        source.my_line = QLineF(20, 20, 20, 60);
        source.z = 3;
        QImage *drawing = new QImage(12, 9, QImage::Format_ARGB32);
        drawing->fill(Qt::transparent);
        drawing->setPixelColor(4, 3, QColor(10, 200, 30, 255)); // un pixel dipinto
        source.stick_img_list << drawing;
        source.img_name_list << "freehand";
        source.stick_img = drawing;
        source.img_angle = 0;
        source.img_offset = QPointF(0, 0);

        QByteArray bytes;
        {
            QDataStream out(&bytes, QIODevice::WriteOnly);
            out << source;
        }
        StickData loaded;
        {
            QDataStream in(&bytes, QIODevice::ReadOnly);
            in >> loaded;
        }
        QCOMPARE(loaded.stick_type, (qint32)StickData::FREEHAND);
        QVERIFY(loaded.IsImageType());
        QCOMPARE(loaded.stick_img_list.count(), 1);
        QVERIFY(loaded.stick_img != nullptr);
        QCOMPARE(*loaded.stick_img, *drawing);           // il disegno sopravvive intatto
        QCOMPARE(loaded.stick_img->pixelColor(4, 3), QColor(10, 200, 30, 255));
    }

    // TEXT stick: content/font/justification/spacing/wrap must survive a save-load (and a
    // Clone(), which copy/paste and undo go through), and a font this system doesn't have
    // must come back as Arial with the missing family reported in missing_text_fonts —
    // the only branch in the text feature that isn't plain field copying.
    void StickTextRoundTripAndFontFallback()
    {
        // svuota SUBITO: un residuo farebbe scattare il prompt (modale!) dentro il primo
        // MyView::LoadLibrary() dei test successivi
        missing_text_fonts.clear();
        // una famiglia davvero installata: in un ambiente headless il font DB è ridotto,
        // quindi la si chiede al sistema invece di codificarne una
        const QStringList installed = QFontDatabase::families();
        const QString real_family = installed.isEmpty() ? QString("Arial") : installed.first();

        StickData source;
        source.stick_type = StickData::TEXT;
        source.my_line = QLineF(100, 100, 300, 100);
        source.text = "first line\nsecond line";
        source.text_font = QFont(real_family, 21);
        // grassetto/corsivo/spaziatura lettere/stretch orizzontale viaggiano DENTRO il
        // QFont (il motivo per cui il campo è un QFont e non una stringa + un corpo)
        source.text_font.setBold(true);
        source.text_font.setItalic(true);
        source.text_font.setUnderline(true);
        source.text_font.setStrikeOut(true);
        source.text_font.setLetterSpacing(QFont::PercentageSpacing, 130);
        source.text_font.setStretch(150);
        source.text_align = Qt::AlignJustify;
        source.text_line_spacing = 1.75f;
        source.text_v_stretch = 175;           // stretch verticale: campo a sé (QFont non l'ha)
        source.text_word_wrap = false;
        source.img_offset = QPointF(-8, 12);   // posizionamento: campi dell'immagine riusati
        source.img_angle_offset = 30;

        QByteArray bytes;
        {
            QDataStream out(&bytes, QIODevice::WriteOnly);
            out << source;
        }
        StickData loaded;
        {
            QDataStream in(&bytes, QIODevice::ReadOnly);
            in >> loaded;
        }
        QCOMPARE(loaded.stick_type, (qint32)StickData::TEXT);
        QCOMPARE(loaded.text, source.text);
        QCOMPARE(loaded.text_font.family(), source.text_font.family());
        QCOMPARE(loaded.text_font.pointSize(), 21);
        QVERIFY(loaded.text_font.bold());
        QVERIFY(loaded.text_font.italic());
        QVERIFY(loaded.text_font.underline());
        QVERIFY(loaded.text_font.strikeOut());
        QCOMPARE(loaded.text_font.letterSpacingType(), QFont::PercentageSpacing);
        QCOMPARE(loaded.text_font.letterSpacing(), 130.0);
        QCOMPARE(loaded.text_font.stretch(), 150);
        QCOMPARE(loaded.text_align, source.text_align);
        QCOMPARE(loaded.text_line_spacing, source.text_line_spacing);
        QCOMPARE(loaded.text_v_stretch, source.text_v_stretch);
        QCOMPARE(loaded.text_word_wrap, false);
        QCOMPARE(loaded.img_offset, source.img_offset);
        QCOMPARE(loaded.img_angle_offset, source.img_angle_offset);

        StickData *clone = source.Clone();
        QCOMPARE(clone->text, source.text);
        QCOMPARE(clone->text_font, source.text_font);
        QCOMPARE(clone->text_align, source.text_align);
        QCOMPARE(clone->text_line_spacing, source.text_line_spacing);
        QCOMPARE(clone->text_v_stretch, source.text_v_stretch);
        QCOMPARE(clone->text_word_wrap, source.text_word_wrap);
        delete clone;

        // un record salvato prima che lo stretch verticale esistesse (version 2) resta
        // leggibile: il campo non è nel flusso e conserva il suo default
        StickData old_record;
        old_record.CopyFrom(source);
        old_record.version = 2;
        QByteArray old_bytes;
        {
            QDataStream out(&old_bytes, QIODevice::WriteOnly);
            out << old_record;
        }
        StickData old_loaded;
        {
            QDataStream in(&old_bytes, QIODevice::ReadOnly);
            in >> old_loaded;
        }
        // letto, il record sale al formato corrente: un nuovo salvataggio scrive ogni campo
        // (rimasto alla 2, avrebbe di nuovo saltato quelli nati dopo)
        QCOMPARE(old_loaded.version, StickData::CURRENT_VERSION);
        QCOMPARE(old_loaded.text, source.text);
        QCOMPARE(old_loaded.text_v_stretch, qint32(100)); // default, non letto dal file

        // font inesistente: fallback ad Arial + segnalazione per il prompt (solo se il font
        // DB è utilizzabile: senza font installati la sostituzione è disattivata per scelta)
        QVERIFY(missing_text_fonts.isEmpty()); // la famiglia reale non ha segnalato nulla
        if(installed.isEmpty())
            return;
        StickData missing;
        missing.stick_type = StickData::TEXT;
        missing.text = "x";
        missing.text_font = QFont("NoSuchFontFamily_1234", 14);
        QByteArray missing_bytes;
        {
            QDataStream out(&missing_bytes, QIODevice::WriteOnly);
            out << missing;
        }
        StickData substituted;
        {
            QDataStream in(&missing_bytes, QIODevice::ReadOnly);
            in >> substituted;
        }
        QCOMPARE(substituted.text_font.family(), QString("Arial"));
        QCOMPARE(substituted.text_font.pointSize(), 14); // il corpo NON viene toccato
        QVERIFY(missing_text_fonts.contains("NoSuchFontFamily_1234"));
        missing_text_fonts.clear();
    }

    // The TEXT paint path (stick.cpp): the paragraph is laid out along the stick and lands
    // on the side the local frame points to, the bounding rect grows to cover it, and an
    // EMPTY text stick paints its bare skeleton instead — the rule the 2-click drawing
    // preview relies on. Rendered offscreen the way Frame::UpdateIcon() does.
    void TextStickRendersTextAndPreviewSkeleton()
    {
        if(QFontDatabase::families().isEmpty())
            QSKIP("no font installed: nothing to lay out");
        StickData text_stick;
        text_stick.stick_type = StickData::TEXT;
        text_stick.my_line = QLineF(10, 40, 210, 40); // orizzontale
        text_stick.img_angle = -90;                   // convenzione -atan2(dx,dy) per questa direzione
        text_stick.pen = QPen(Qt::black);
        text_stick.text_font = QFont(QFontDatabase::families().first(), 20);

        // pixel non bianchi sotto la spina: il testo corre lungo lo stick, dal lato +y locale
        auto ink_below_line = [](StickData *data){
            QGraphicsScene render_scene;
            render_scene.setSceneRect(0, 0, 300, 200);
            render_scene.addItem(new StickGraphicsItem(data)); // la scena possiede la view
            QImage img(300, 200, QImage::Format_ARGB32);
            img.fill(Qt::white);
            QPainter painter(&img);
            render_scene.render(&painter);
            painter.end();
            int painted = 0;
            for(int y = 46; y < img.height(); y++)
                for(int x = 0; x < img.width(); x++)
                    if(img.pixelColor(x, y) != QColor(Qt::white))
                        painted++;
            return painted;
        };

        StickGraphicsItem measure(&text_stick); // solo per boundingRect (nessuna scena serve)
        QRectF skeleton_br = measure.boundingRect();
        QCOMPARE(ink_below_line(&text_stick), 0); // testo vuoto: solo la spina, niente sotto

        text_stick.text = "the quick brown fox jumps over the lazy dog";
        measure.Refresh(0);
        QVERIFY(measure.boundingRect().height() > skeleton_br.height()); // il testo entra nel br
        QVERIFY(ink_below_line(&text_stick) > 0);                        // ed è davvero dipinto
    }

    // LoadAbr() decodes a Photoshop .abr v2 sampled brush into an ARGB stamp whose
    // alpha is the coverage (255 - stored value). Builds a minimal 2x2 raw brush
    // in memory — the crop/format math is the non-trivial part worth pinning.
    void AbrSampledBrushParses()
    {
        QByteArray bytes;
        {
            QDataStream s(&bytes, QIODevice::WriteOnly);
            s.setByteOrder(QDataStream::BigEndian);
            s << (qint16)2 << (qint16)1;        // version 2, one brush
            // --- one sampled brush block ---
            s << (qint16)2;                     // type = sampled
            s << (qint32)48;                    // block_size (bytes after this field)
            s << (qint32)0;                     // misc
            s << (qint16)25;                    // spacing
            s << (qint32)3;                     // name: 3 UTF-16 units ("AB" + null)
            s << (quint16)'A' << (quint16)'B' << (quint16)0;
            s << (qint8)1;                      // antialiasing
            for(int k = 0; k < 4; k++) s << (qint16)0;          // short_bounds (legacy)
            s << (qint32)0 << (qint32)0 << (qint32)2 << (qint32)2; // bounds t,l,b,r -> 2x2
            s << (qint16)8;                     // depth
            s << (qint8)0;                      // compression = raw
            const uchar px[4] = {0, 64, 128, 255};             // row-major grayscale
            s.writeRawData(reinterpret_cast<const char*>(px), 4);
        }

        QTemporaryFile f;
        QVERIFY(f.open());
        f.write(bytes);
        f.flush();

        QList<AbrBrush> brushes = LoadAbr(f.fileName());
        QCOMPARE(brushes.count(), 1);
        QCOMPARE(brushes[0].name, QStringLiteral("AB"));
        QCOMPARE(brushes[0].stamp.width(), 2);
        QCOMPARE(brushes[0].stamp.height(), 2);
        // coverage = value: 0->0 (none), 255->255 (full)
        QCOMPARE(qAlpha(brushes[0].stamp.pixel(0, 0)), 0);
        QCOMPARE(qAlpha(brushes[0].stamp.pixel(1, 1)), 255);
    }

    // LoadPat() decodes a Photoshop .pat tile: one 8-bit plane per channel, each in its own
    // Virtual Memory Array, interleaved back into an ARGB32 image. Built here to match the
    // framing REAL packs use, which is what the first cut of the reader got wrong:
    //   - the '8BPT' signature and an int16 (not int32) version
    //   - a VMA "channels" field of 24, which is Photoshop's channel CAPACITY and not the
    //     number of planes that follow, so the arrays must be read to the list's end
    //   - RGB and CMYK records side by side (packs are full of CMYK)
    // Mirrors AbrSampledBrushParses() for the sibling format.
    void PatPatternParses()
    {
        // one Virtual Memory Array holding a 2x1 8-bit plane, uncompressed
        auto write_plane = [](QDataStream &s, uchar a, uchar b){
            s << (qint32)1;                 // is_written
            s << (qint32)(23 + 2);          // length: the 23-byte header + 2 samples
            s << (qint32)8;                 // depth
            s << (qint32)0 << (qint32)0 << (qint32)1 << (qint32)2; // bounds t,l,b,r -> 2x1
            s << (qint16)8;                 // pixel_depth
            s << (qint8)0;                  // compression = raw
            const uchar px[2] = {a, b};
            s.writeRawData(reinterpret_cast<const char*>(px), 2);
        };
        // record header + VMA list wrapper around a body built by the caller
        auto write_record = [](QDataStream &s, qint32 mode, const QString &name,
                               const QByteArray &vma_body){
            s << (qint32)1;                 // record version 1 (nessun prefisso di lunghezza)
            s << mode;
            s << (qint16)1 << (qint16)2;    // point height, width
            s << (qint32)(name.length() + 1); // nome: unità UTF-16, terminatore incluso
            for(QChar c : name)
                s << (quint16)c.unicode();
            s << (quint16)0;
            s << (quint8)0;                 // pattern id: pascal string vuota
            s << (qint32)3;                 // VMA list version 3
            s << (qint32)vma_body.size();
            s.writeRawData(vma_body.constData(), vma_body.size());
        };

        QByteArray rgb_body;
        {
            QDataStream s(&rgb_body, QIODevice::WriteOnly);
            s.setByteOrder(QDataStream::BigEndian);
            s << (qint32)0 << (qint32)0 << (qint32)1 << (qint32)2; // rettangolo del tile
            s << (qint32)24;                // CAPACITÀ, non conteggio: come i file veri
            write_plane(s, 255, 0);         // R
            write_plane(s, 0, 128);         // G
            write_plane(s, 0, 0);           // B
            for(int slot = 0; slot < 21; slot++)
                s << (qint32)0;             // gli slot restanti, non scritti
        }
        QByteArray cmyk_body;
        {
            QDataStream s(&cmyk_body, QIODevice::WriteOnly);
            s.setByteOrder(QDataStream::BigEndian);
            s << (qint32)0 << (qint32)0 << (qint32)1 << (qint32)2;
            s << (qint32)24;
            // CMYK memorizzato INVERTITO: 255 = nessun inchiostro
            write_plane(s, 255, 0);         // C: pixel 0 senza ciano, pixel 1 ciano pieno
            write_plane(s, 255, 255);       // M: niente
            write_plane(s, 255, 255);       // Y: niente
            write_plane(s, 255, 255);       // K: niente
            for(int slot = 0; slot < 20; slot++)
                s << (qint32)0;
        }

        QByteArray bytes;
        {
            QDataStream s(&bytes, QIODevice::WriteOnly);
            s.setByteOrder(QDataStream::BigEndian);
            s.writeRawData("8BPT", 4);      // firma del contenitore
            s << (qint16)1;                 // version, a 16 bit
            s << (qint32)2;                 // due pattern
            write_record(s, 3, QStringLiteral("AB"), rgb_body);   // RGB
            write_record(s, 4, QStringLiteral("CM"), cmyk_body);  // CMYK
        }

        QTemporaryFile f;
        QVERIFY(f.open());
        f.write(bytes);
        f.flush();

        QList<PatPattern> patterns = LoadPat(f.fileName());
        QCOMPARE(patterns.count(), 2);
        QCOMPARE(patterns[0].name, QStringLiteral("AB"));
        QCOMPARE(patterns[0].tile.width(), 2);
        QCOMPARE(patterns[0].tile.height(), 1);
        // i piani si reinterlacciano nell'ordine R,G,B; nessun alpha scritto -> opaco
        QCOMPARE(patterns[0].tile.pixel(0, 0), qRgba(255, 0, 0, 255));
        QCOMPARE(patterns[0].tile.pixel(1, 0), qRgba(0, 128, 0, 255));
        // CMYK: senza inchiostro è bianco, ciano pieno è (0,255,255)
        QCOMPARE(patterns[1].name, QStringLiteral("CM"));
        QCOMPARE(patterns[1].tile.pixel(0, 0), qRgba(255, 255, 255, 255));
        QCOMPARE(patterns[1].tile.pixel(1, 0), qRgba(0, 255, 255, 255));

        // un contenitore senza la firma non è un .pat: nessun tile, nessun crash
        QByteArray unsigned_bytes = bytes;
        unsigned_bytes.replace(0, 4, "XXXX");
        QTemporaryFile bad;
        QVERIFY(bad.open());
        bad.write(unsigned_bytes);
        bad.flush();
        QVERIFY(LoadPat(bad.fileName()).isEmpty());
    }

    // The pattern/gradient/image fills need no StickData field and no serialization code of
    // their own: they live inside the QPen/QBrush that operator<</operator>> already write
    // whole. This pins that — if the fill ever grows a hand-rolled format, this test says so.
    void GradientAndTextureFillsRoundTrip()
    {
        QImage tile(2, 2, QImage::Format_ARGB32);
        tile.fill(qRgba(10, 20, 30, 255));
        tile.setPixel(1, 1, qRgba(200, 100, 50, 128));

        QLinearGradient gradient(QPointF(0, 0), QPointF(1, 1));
        gradient.setCoordinateMode(QGradient::ObjectMode); // come lo costruisce GradientDialog
        gradient.setSpread(QGradient::ReflectSpread);
        gradient.setStops({ QGradientStop(0.0, QColor(255, 0, 0)),
                            QGradientStop(0.4, QColor(0, 255, 0, 90)),
                            QGradientStop(1.0, QColor(0, 0, 255)) });

        StickData source;
        source.stick_type = StickData::RECT;
        source.my_line = QLineF(0, 0, 0, 100);
        source.brush = QBrush(gradient);        // riempimento: gradiente
        source.pen = QPen(QBrush(tile), 5.0);   // contorno: tile (pattern/immagine)

        QByteArray bytes;
        {
            QDataStream out(&bytes, QIODevice::WriteOnly);
            out << source;
        }
        StickData loaded;
        {
            QDataStream in(&bytes, QIODevice::ReadOnly);
            in >> loaded;
        }

        QCOMPARE(loaded.brush.style(), Qt::LinearGradientPattern);
        QVERIFY(loaded.brush.gradient() != nullptr);
        QCOMPARE(loaded.brush.gradient()->stops(), gradient.stops());
        QCOMPARE(loaded.brush.gradient()->spread(), QGradient::ReflectSpread);
        QCOMPARE(loaded.brush.gradient()->coordinateMode(), QGradient::ObjectMode);
        QCOMPARE(loaded.pen.brush().style(), Qt::TexturePattern);
        QCOMPARE(loaded.pen.brush().textureImage().size(), tile.size());
        QCOMPARE(loaded.pen.brush().textureImage().pixel(1, 1), tile.pixel(1, 1));
        QCOMPARE(loaded.pen.widthF(), 5.0);

        // Clone() deep-copies the same fields with no stream involved
        StickData *clone = source.Clone();
        QCOMPARE(clone->brush.style(), Qt::LinearGradientPattern);
        QCOMPARE(clone->brush.gradient()->stops(), gradient.stops());
        QCOMPARE(clone->pen.brush().style(), Qt::TexturePattern);
        delete clone;
    }

    // StickData::brush_follows_stick anchors a texture/gradient to the stick or to the scene.
    // What it has to compensate for differs per stick type AND per brush style, which is the
    // part worth pinning:
    //   - LINE/POLYGON/MULTILINE paint in scene coordinates, the rest inside the stick's own
    //     rotated frame, so the two families need OPPOSITE corrections;
    //   - a texture needs the whole frame, while an ObjectMode gradient is already anchored
    //     to the shape's bounding box and needs the rotation ONLY — composing the
    //     translation too slides it off the shape and the fill goes flat — taken about the
    //     CENTRE of that box, since the transform of an ObjectMode gradient acts in the box's
    //     own 0..1 space whose origin is its top-left corner (a corner-pivoted rotation swings
    //     the axis off the shape: that was the "gradient turns the wrong way" bug).
    void BrushAnchoringCompensatesPerTypeAndStyle()
    {
        QLinearGradient gradient(QPointF(0, 0), QPointF(1, 0));
        gradient.setCoordinateMode(QGradient::ObjectMode);
        gradient.setStops({ QGradientStop(0.0, Qt::red), QGradientStop(1.0, Qt::blue) });
        QImage tile(4, 4, QImage::Format_ARGB32);
        tile.fill(Qt::green);

        // --- LINE: dipinge in coordinate di scena, quindi "segue" richiede una correzione
        StickData line;
        line.stick_type = StickData::LINE;
        line.my_line = QLineF(30, 40, 30, 140);
        line.img_angle = 25;
        StickGraphicsItem line_view(&line);

        line.brush_follows_stick = true;
        QTransform textured = line_view.AlignBrush(QBrush(tile)).transform();
        QVERIFY(!textured.isIdentity());
        QCOMPARE(textured.dx(), 30.0);          // la texture porta anche la traslazione
        QCOMPARE(textured.dy(), 40.0);
        QTransform graded = line_view.AlignBrush(QBrush(gradient)).transform();
        QVERIFY(!graded.isIdentity());
        // il gradiente ObjectMode prende SOLO la rotazione, e attorno al centro del box 0..1:
        // (0.5,0.5) resta fermo...
        QPointF box_centre = graded.map(QPointF(0.5, 0.5));
        QVERIFY(qAbs(box_centre.x() - 0.5) < 1e-9);
        QVERIFY(qAbs(box_centre.y() - 0.5) < 1e-9);
        // ...mentre un punto sull'asse del gradiente gira per davvero
        QVERIFY(QLineF(graded.map(QPointF(1.0, 0.5)), QPointF(1.0, 0.5)).length() > 0.1);
        // e la traslazione dello stick NON entra: sarebbe il difetto che appiattisce il fill
        QVERIFY(qAbs(graded.dx()) < 1.0);
        QVERIFY(qAbs(graded.dy()) < 1.0);

        line.brush_follows_stick = false;       // la texture è già ancorata alla scena
        QVERIFY(line_view.AlignBrush(QBrush(tile)).transform().isIdentity());
        // il gradiente NO: un ObjectMode è ancorato al bounding box della FORMA per
        // definizione, quindi non seguire lo stick richiede di riesprimerlo in LogicalMode
        // (nessuna scena qui -> il rettangolo di ancoraggio è il quadrato unitario, quindi il
        // transform resta l'identità, ma il coordinateMode è la prova del cambio)
        const QBrush unpinned = line_view.AlignBrush(QBrush(gradient));
        QVERIFY(unpinned.gradient() != nullptr);
        QCOMPARE(unpinned.gradient()->coordinateMode(), QGradient::LogicalMode);
        QCOMPARE(unpinned.gradient()->stops(), gradient.stops()); // stessi colori, altro ancoraggio

        // --- RECT: il painter è già nel frame locale, quindi il caso è esattamente opposto
        StickData rect;
        rect.stick_type = StickData::RECT;
        rect.my_line = QLineF(30, 40, 30, 140);
        rect.img_angle = 25;
        StickGraphicsItem rect_view(&rect);

        rect.brush_follows_stick = true;        // segue per costruzione
        QVERIFY(rect_view.AlignBrush(QBrush(tile)).transform().isIdentity());
        QVERIFY(rect_view.AlignBrush(QBrush(gradient)).transform().isIdentity());
        QCOMPARE(rect_view.AlignBrush(QBrush(gradient)).gradient()->coordinateMode(),
                 QGradient::ObjectMode);        // e resta ObjectMode: è ciò che lo fa seguire
        rect.brush_follows_stick = false;
        QVERIFY(!rect_view.AlignBrush(QBrush(tile)).transform().isIdentity());
        QVERIFY(!rect_view.AlignBrush(QBrush(gradient)).transform().isIdentity());
        QCOMPARE(rect_view.AlignBrush(QBrush(gradient)).gradient()->coordinateMode(),
                 QGradient::LogicalMode);

        // --- una tinta unita non ha geometria da ancorare: mai toccata, in nessun caso
        QVERIFY(rect_view.AlignBrush(QBrush(Qt::green)).transform().isIdentity());
        QVERIFY(line_view.AlignBrush(QBrush(Qt::NoBrush)).transform().isIdentity());

        // --- e il flag è persistente (record version 4)
        StickData source;
        source.stick_type = StickData::RECT;
        source.brush = QBrush(gradient);
        source.brush_follows_stick = false;
        QByteArray bytes;
        {
            QDataStream out(&bytes, QIODevice::WriteOnly);
            out << source;
        }
        StickData loaded;
        {
            QDataStream in(&bytes, QIODevice::ReadOnly);
            in >> loaded;
        }
        QCOMPARE(loaded.brush_follows_stick, false);
        StickData *clone = source.Clone();
        QCOMPARE(clone->brush_follows_stick, false);
        delete clone;
    }

    // A texture's scale/angle/offset live in QBrush::transform() and nowhere else — no
    // StickData field, so they persist for free. TextureXf is the editable form of that
    // matrix, and the numeric options dialog and the on-canvas arrow handle BOTH go through
    // it, so a lossy conversion would silently corrupt a texture on every edit.
    void TextureTransformRoundTripsThroughTextureXf()
    {
        QImage tile(8, 4, QImage::Format_ARGB32);
        tile.fill(Qt::magenta);

        TextureXf source;
        source.scale = 0.375;
        source.angle = -117.5;
        source.offset = QPointF(-23.5, 61.25);

        QBrush brush(tile);
        WriteTextureXf(&brush, source);
        const TextureXf back = ReadTextureXf(brush);
        QVERIFY(qAbs(back.scale - source.scale) < 1e-9);
        QVERIFY(qAbs(back.angle - source.angle) < 1e-9);
        QCOMPARE(back.offset, source.offset);

        // uno scale zero renderebbe la matrice singolare (Qt non disegnerebbe nulla, e
        // l'angolo non sarebbe più recuperabile): va clampato, non accettato
        TextureXf degenerate;
        degenerate.scale = 0;
        degenerate.angle = 40;
        QBrush clamped(tile);
        WriteTextureXf(&clamped, degenerate);
        QVERIFY(clamped.transform().isInvertible());
        QVERIFY(qAbs(ReadTextureXf(clamped).angle - 40.0) < 1e-6);

        // un brush senza transform legge l'identità, non una matrice degenere
        const TextureXf plain = ReadTextureXf(QBrush(tile));
        QCOMPARE(plain.scale, 1.0);
        QCOMPARE(plain.angle, 0.0);
        QCOMPARE(plain.offset, QPointF(0, 0));

        // il transform viaggia col brush attraverso lo stream, senza codice dedicato
        StickData stick;
        stick.stick_type = StickData::RECT;
        stick.brush = brush;
        stick.texture_tiled = false;      // questo invece è un campo (record version 5)
        QByteArray bytes;
        {
            QDataStream out(&bytes, QIODevice::WriteOnly);
            out << stick;
        }
        StickData loaded;
        {
            QDataStream in(&bytes, QIODevice::ReadOnly);
            in >> loaded;
        }
        QCOMPARE(loaded.texture_tiled, false);
        const TextureXf streamed = ReadTextureXf(loaded.brush);
        QVERIFY(qAbs(streamed.scale - source.scale) < 1e-9);
        QVERIFY(qAbs(streamed.angle - source.angle) < 1e-9);
        QCOMPARE(streamed.offset, source.offset);
        StickData *cloned = stick.Clone();
        QCOMPARE(cloned->texture_tiled, false);
        QVERIFY(qAbs(ReadTextureXf(cloned->brush).angle - source.angle) < 1e-9);
        delete cloned;
    }

    // The three Style-column check boxes depend on what the SELECTED STICK is painted with, and
    // that changes with no selection change at all: an Apply button swaps a solid fill for a
    // gradient while the same stick stays selected. UpdateSliders() only runs on a selection
    // change, so they kept the enablement computed while the fill was still solid and stayed
    // greyed out — which reads exactly like a check box that ignores clicks. Pins the refresh.
    void ApplyingAGradientEnablesTheStyleCheckBoxes()
    {
        AddFrame();
        DrawStick(QPointF(200, 100), QPointF(200, 200));
        StickData *stick = CurrentStickFigure()->stick_list.last();
        stick->stick_type = StickData::RECT;
        stick->brush = QBrush(Qt::green);      // tinta unita: niente da ancorare né da puntare
        stick->pen = QPen(Qt::black, 2);
        CurrentStickFigure()->current_stick = stick;

        // il binario di test non linka widget.cpp, quindi le caselle sono nulle: creale qui, che
        // è l'unico modo di verificare l'abilitazione senza la UI vera. I globali vanno rimessi a
        // nullptr da un DISTRUTTORE e non a mano in fondo: un QVERIFY che fallisce ritorna
        // subito, e le caselle sono sullo stack — lasciarli appesi faceva segfaultare ogni test
        // successivo (UpdateSliders() li dereferenzia), mascherando l'errore vero
        QCheckBox follows, fill_arrow, stroke_arrow;
        struct Restore {
            ~Restore(){
                brush_follows_check = nullptr;
                fill_texture_arrow_check = nullptr;
                stroke_texture_arrow_check = nullptr;
            }
        } restore_globals;
        brush_follows_check = &follows;
        fill_texture_arrow_check = &fill_arrow;
        stroke_texture_arrow_check = &stroke_arrow;

        view->my_animation->UpdateStyleControls();
        QVERIFY(!follows.isEnabled());         // tinta unita da entrambi i lati
        QVERIFY(!fill_arrow.isEnabled());
        QVERIFY(!stroke_arrow.isEnabled());

        // scegli un gradiente come pennello CORRENTE e premi Apply, senza toccare la selezione:
        // è la sequenza che lasciava le caselle spente
        QLinearGradient gradient(QPointF(0, 0), QPointF(1, 0));
        gradient.setCoordinateMode(QGradient::ObjectMode);
        gradient.setStops({ QGradientStop(0.0, Qt::red), QGradientStop(1.0, Qt::blue) });
        view->my_brush = QBrush(gradient);
        view->SetGraphics(false, ATTRIBUTE_BRUSHCOLOR); // copia call site: on_setBrushColorBtn_clicked()

        QCOMPARE(CurrentFrame()->CurrentStick()->brush.style(), Qt::LinearGradientPattern);
        QVERIFY(follows.isEnabled());          // il riempimento ha una geometria da ancorare
        QVERIFY(fill_arrow.isEnabled());
        QVERIFY(!stroke_arrow.isEnabled());    // il contorno è ancora una tinta unita

        // e la freccia esiste per davvero, non solo la casella: un gradiente non tiene la sua
        // collocazione in QBrush::transform() ma nella propria geometria
        view->show_fill_texture_arrow = true;
        QPointF tail, tip;
        QVERIFY(view->TextureArrowPoints(true, &tail, &tip));
        QVERIFY(QLineF(tail, tip).length() > 1.0);
        view->show_fill_texture_arrow = false;
    }

    // The arrow handle drives BOTH brush kinds, through completely different fields: a texture's
    // placement is QBrush::transform(), a gradient's is its own geometry (start/finalStop, or
    // centre/radius/focus). Editing a gradient through its transform is not an option — for an
    // ObjectMode gradient that transform acts in the 0..1 box space, so a scene-unit offset would
    // mean that many whole bounding boxes. This pins the gradient half round-tripping.
    void GradientPlacementRoundTripsThroughTextureXf()
    {
        QLinearGradient linear(QPointF(0.2, 0.5), QPointF(0.8, 0.5)); // asse orizzontale centrato
        linear.setCoordinateMode(QGradient::ObjectMode);
        linear.setSpread(QGradient::ReflectSpread);
        linear.setStops({ QGradientStop(0.0, Qt::red), QGradientStop(1.0, Qt::blue) });

        // l'asse parte centrato: offset nullo, angolo 0, lunghezza 0.6
        const TextureXf read_linear = ReadGradientXf(QBrush(linear));
        QVERIFY(qAbs(read_linear.offset.x()) < 1e-9);
        QVERIFY(qAbs(read_linear.offset.y()) < 1e-9);
        QVERIFY(qAbs(read_linear.angle) < 1e-9);
        QVERIFY(qAbs(read_linear.scale - 0.6) < 1e-9);

        TextureXf moved;
        moved.offset = QPointF(0.15, -0.05);
        moved.angle = 33.0;
        moved.scale = 0.4;
        QBrush brush(linear);
        WriteGradientXf(&brush, moved);
        const TextureXf back = ReadGradientXf(brush);
        QVERIFY(QLineF(back.offset, moved.offset).length() < 1e-9);
        QVERIFY(qAbs(back.angle - moved.angle) < 1e-9);
        QVERIFY(qAbs(back.scale - moved.scale) < 1e-9);
        // e tutto ciò che NON è geometria sopravvive alla ricostruzione: perdere il
        // coordinateMode sgancerebbe di nascosto il riempimento (vedi AlignBrush)
        QVERIFY(brush.gradient() != nullptr);
        QCOMPARE(brush.gradient()->coordinateMode(), QGradient::ObjectMode);
        QCOMPARE(brush.gradient()->spread(), QGradient::ReflectSpread);
        QCOMPARE(brush.gradient()->stops(), linear.stops());

        // radiale: la scala è il DIAMETRO, e l'angolo ri-PUNTA il fuoco senza avvicinarlo
        QRadialGradient radial(QPointF(0.5, 0.5), 0.25, QPointF(0.6, 0.5), 0.0);
        radial.setCoordinateMode(QGradient::ObjectMode);
        radial.setStops({ QGradientStop(0.0, Qt::white), QGradientStop(1.0, Qt::black) });
        QBrush radial_brush(radial);
        QVERIFY(qAbs(ReadGradientXf(radial_brush).scale - 0.5) < 1e-9);
        TextureXf aimed;
        aimed.angle = 90.0;
        aimed.scale = 0.5;
        WriteGradientXf(&radial_brush, aimed);
        const QRadialGradient *turned =
                static_cast<const QRadialGradient*>(radial_brush.gradient());
        QVERIFY(qAbs(turned->centerRadius() - 0.25) < 1e-9);
        // distanza del fuoco invariata (0.1), direzione ora a 90 gradi in senso orario
        QVERIFY(qAbs(QLineF(turned->center(), turned->focalPoint()).length() - 0.1) < 1e-9);
        QVERIFY(qAbs(turned->focalPoint().y() - (turned->center().y() + 0.1)) < 1e-9);

        // una tinta unita non ha un gradiente: entrambe le direzioni sono inerti
        QBrush solid(Qt::green);
        WriteGradientXf(&solid, moved);
        QCOMPARE(solid.style(), Qt::SolidPattern);
        QCOMPARE(ReadGradientXf(solid).scale, 1.0);
    }

    // A texture brush ignores its QBrush::color() when rendering, which leaves that colour's
    // ALPHA as a field the brush already has, already streams, and otherwise never uses — so the
    // opacity sliders write the texture's opacity there, exactly as they write the colour itself
    // for a solid fill. Rendered, because the claim is that it reaches the canvas; and the
    // OUTLINE must not fade with it, which is the reason FillAndStroke() splits the passes.
    void TextureFillHonoursOpacityWithoutFadingTheOutline()
    {
        QImage tile(4, 4, QImage::Format_ARGB32);
        tile.fill(Qt::blue);

        StickData rect;
        rect.stick_type = StickData::RECT;
        rect.my_line = QLineF(100, 60, 100, 140);
        rect.img_w_scale = 60;
        rect.pen = QPen(Qt::black, 4);
        rect.brush = QBrush(tile);

        // il riempimento al centro, il contorno sul bordo destro della forma (100+30 = 130)
        auto sample = [](StickData *data, const QPoint &at){
            QGraphicsScene render_scene;
            render_scene.setSceneRect(0, 0, 200, 200);
            render_scene.addItem(new StickGraphicsItem(data)); // la scena possiede la view
            QImage img(200, 200, QImage::Format_ARGB32);
            img.fill(Qt::white);
            QPainter painter(&img);
            render_scene.render(&painter, QRectF(0, 0, 200, 200), QRectF(0, 0, 200, 200));
            painter.end();
            return img.pixelColor(at);
        };
        const QPoint inside(100, 100), on_outline(130, 100);

        QCOMPARE(StickGraphicsItem::FillOpacity(rect.brush), 1.0); // QBrush(tile) nasce opaco
        const QColor opaque_fill = sample(&rect, inside);
        const QColor opaque_edge = sample(&rect, on_outline);
        QCOMPARE(opaque_fill, QColor(Qt::blue));
        QCOMPARE(opaque_edge, QColor(Qt::black));

        // mezza opacità, scritta dove la scrivono gli slider: l'alpha del colore del brush
        QColor tint = rect.brush.color();
        tint.setAlphaF(0.5);
        rect.brush.setColor(tint);
        QVERIFY(qAbs(StickGraphicsItem::FillOpacity(rect.brush) - 0.5) < 0.01);
        const QColor faded_fill = sample(&rect, inside);
        // blu al 50% su bianco: schiarito verso il bianco, ma ancora blu
        QVERIFY(faded_fill != QColor(Qt::blue));
        QVERIFY(faded_fill.blue() > faded_fill.red());
        QVERIFY(faded_fill.red() > 80);
        // ...e il contorno è rimasto nero pieno: FillAndStroke lo disegna fuori dal passaggio
        // sbiadito, altrimenti sarebbe grigio
        QCOMPARE(sample(&rect, on_outline), opaque_edge);

        // l'alpha viaggia col brush attraverso lo stream, come tutto il resto di un QBrush
        QByteArray bytes;
        {
            QDataStream out(&bytes, QIODevice::WriteOnly);
            out << rect;
        }
        StickData loaded;
        {
            QDataStream in(&bytes, QIODevice::ReadOnly);
            in >> loaded;
        }
        QVERIFY(qAbs(StickGraphicsItem::FillOpacity(loaded.brush) - 0.5) < 0.01);
        // una tinta unita e un gradiente non passano da qui: la loro trasparenza è nel colore
        // e nei singoli stop
        QCOMPARE(StickGraphicsItem::FillOpacity(QBrush(QColor(0, 0, 0, 10))), 1.0);
    }

    // Two pieces of Style-column state that used to survive a change of fill and quietly apply to
    // the next one: the opacity, which the sliders wrote only into the CURRENT paint (so nothing
    // happened to the selected stick until an Apply press — indistinguishable from the opacity
    // being ignored), and StickData::texture_tiled, which only the image dialog could set and
    // which then rode along onto every later fill, a library pattern included.
    void OpacityAndTilingApplyToTheSelectedStickAndDoNotLeak()
    {
        AddFrame();
        DrawStick(QPointF(250, 120), QPointF(250, 220));
        StickData *stick = CurrentStickFigure()->stick_list.last();
        stick->stick_type = StickData::RECT;
        stick->img_w_scale = 40;
        QImage tile(8, 8, QImage::Format_ARGB32);
        tile.fill(Qt::darkCyan);
        stick->brush = QBrush(tile);
        stick->pen = QPen(Qt::black, 2);
        CurrentStickFigure()->current_stick = stick;

        // --- l'opacità arriva sullo stick, non solo sul pennello corrente. Replica di
        // Widget::SetPaintOpacity(true, percent, true) (widget.cpp), il corpo dello slider.
        auto set_fill_opacity = [this](int percent, bool &latch){
            StickData *cs = CurrentFrame()->CurrentStick();
            QVERIFY(cs != nullptr);
            QVERIFY(cs->brush.style() == Qt::TexturePattern);
            if(!latch){
                view->StoreUndo(CMD_SIMPLE); // un solo passo per tutto il trascinamento
                latch = true;
            }
            QColor tint = cs->brush.color();
            tint.setAlphaF(percent / 100.0);
            cs->brush.setColor(tint);
        };
        bool drag_latch = false;
        const int undo_before = view->undo_stack.index();
        for(int percent : { 90, 70, 40 })   // un trascinamento: tre valueChanged di fila
            set_fill_opacity(percent, drag_latch);
        drag_latch = false;                // sliderReleased

        stick = CurrentFrame()->CurrentStick();
        QVERIFY(qAbs(StickGraphicsItem::FillOpacity(stick->brush) - 0.4) < 0.01);
        QCOMPARE(view->undo_stack.index(), undo_before + 1); // UN solo passo, non tre
        view->undo_stack.undo();
        QVERIFY(qAbs(StickGraphicsItem::FillOpacity(CurrentFrame()->CurrentStick()->brush) - 1.0)
                < 0.01);
        view->undo_stack.redo();

        // --- e il tiling non cola sul riempimento successivo. Il flag è per-stick, ma
        // SetGraphics() lo copia da my_texture_tiled: dopo un'immagine non ripetuta, un pattern
        // applicato dopo veniva ritagliato a una sola tile senza che nulla lo dicesse.
        stick = CurrentFrame()->CurrentStick();
        view->my_texture_tiled = false;              // com'era dopo un'immagine con Tile spento
        view->my_brush = QBrush(tile);
        view->SetGraphics(false, ATTRIBUTE_BRUSHCOLOR);
        QCOMPARE(CurrentFrame()->CurrentStick()->texture_tiled, false);

        // "il riempimento viene ritagliato a una sola istanza" si legge da SingleInstancePath().
        // Su una COPIA dei soli campi che le servono: avvolgere lo stick vivo in una seconda
        // StickGraphicsItem gli ruberebbe il back-pointer view (vedi CLAUDE.md)
        auto clips_to_one_tile = [](const StickData *from){
            StickData probe;
            probe.stick_type = from->stick_type;
            probe.my_line = from->my_line;
            probe.img_angle = from->img_angle;
            probe.brush_follows_stick = from->brush_follows_stick;
            probe.texture_tiled = from->texture_tiled;
            StickGraphicsItem probe_view(&probe);
            return !probe_view.SingleInstancePath(from->brush).isEmpty();
        };
        QVERIFY(clips_to_one_tile(CurrentFrame()->CurrentStick()));

        // la casella della colonna Style è la via per rimetterlo: replica del suo slot
        // (Widget::BuildStyleCombos()), che è ciò che prima non esisteva per un pattern
        view->my_texture_tiled = true;
        StickData *current = CurrentFrame()->CurrentStick();
        view->StoreUndo(CMD_SIMPLE);
        current->texture_tiled = true;
        QVERIFY(!clips_to_one_tile(current)); // niente da ritagliare: la tile ripete
    }

    // The opacity sliders used to be DISABLED for a gradient, on the grounds that its transparency
    // belongs per stop. It does — but that is where the number can live, not a reason there cannot
    // be one: WritePaintOpacity() scales the stops relative to the most opaque, so the ratios the
    // user dialled survive and the slider still reads one value. It has to be the stops and not
    // QBrush::color(), which is where a solid and a texture keep it: QDataStream restores that
    // colour for those two but rebuilds a gradient from its stops and drops it, so an alpha parked
    // there would vanish on save. That is the half this pins hardest.
    void GradientOpacityLivesInTheStopsAndTheSliderIsEnabled()
    {
        QLinearGradient gradient(QPointF(0, 0), QPointF(1, 0));
        gradient.setCoordinateMode(QGradient::ObjectMode);
        gradient.setSpread(QGradient::ReflectSpread);
        // due stop già non uniformi: il rapporto 255:128 è quel che deve sopravvivere
        gradient.setStops({ QGradientStop(0.0, QColor(255, 0, 0, 255)),
                            QGradientStop(1.0, QColor(0, 0, 255, 128)) });
        QBrush brush(gradient);
        QCOMPARE(ReadPaintOpacity(brush), 1.0);       // il picco è pieno

        WritePaintOpacity(&brush, 0.5);
        QVERIFY(qAbs(ReadPaintOpacity(brush) - 0.5) < 0.01);
        QVERIFY(brush.gradient() != nullptr);
        const QGradientStops faded = brush.gradient()->stops();
        QCOMPARE(faded.size(), 2);
        QVERIFY(qAbs(faded[0].second.alphaF() - 0.5) < 0.01);
        QVERIFY(qAbs(faded[1].second.alphaF() - 0.25) < 0.02); // metà di metà: rapporto invariato
        // e nulla che non sia un alpha è cambiato
        QCOMPARE(brush.gradient()->coordinateMode(), QGradient::ObjectMode);
        QCOMPARE(brush.gradient()->spread(), QGradient::ReflectSpread);
        QCOMPARE(faded[0].second.red(), 255);
        // assoluta, non composta: riscrivere lo stesso valore non sbiadisce due volte
        WritePaintOpacity(&brush, 0.5);
        QVERIFY(qAbs(brush.gradient()->stops()[1].second.alphaF() - 0.25) < 0.02);

        // il salvataggio: è qui che l'alpha del COLORE del brush si perdeva
        StickData rect;
        rect.stick_type = StickData::RECT;
        rect.my_line = QLineF(100, 60, 100, 140);
        rect.brush = brush;
        QColor decoy = rect.brush.color();            // la strada vecchia, per contrasto
        decoy.setAlphaF(0.5);
        rect.brush.setColor(decoy);
        QByteArray bytes;
        {
            QDataStream out(&bytes, QIODevice::WriteOnly);
            out << rect;
        }
        StickData loaded;
        {
            QDataStream in(&bytes, QIODevice::ReadOnly);
            in >> loaded;
        }
        QVERIFY(qAbs(ReadPaintOpacity(loaded.brush) - 0.5) < 0.02);  // gli stop ci sono ancora
        QCOMPARE(loaded.brush.color().alphaF(), 1.0);                // il colore no: era una via cieca

        // ...mentre una tinta unita e una texture tengono l'opacità nel colore, e lo stesso
        // accessore le legge: un solo concetto per tutti e tre i tipi di pennello
        QBrush solid(QColor(0, 200, 0));
        WritePaintOpacity(&solid, 0.3);
        QVERIFY(qAbs(ReadPaintOpacity(solid) - 0.3) < 0.01);
        QCOMPARE(solid.color().green(), 200);
        QImage tile(4, 4, QImage::Format_ARGB32);
        tile.fill(Qt::blue);
        QBrush texture(tile);
        WritePaintOpacity(&texture, 0.3);
        QVERIFY(qAbs(ReadPaintOpacity(texture) - 0.3) < 0.01);
        QVERIFY(qAbs(StickGraphicsItem::FillOpacity(texture) - 0.3) < 0.01);
        // un gradiente non passa dall'opacità del painter: Qt legge già l'alpha degli stop, e
        // sbiadirlo una seconda volta lo eleverebbe al quadrato
        QCOMPARE(StickGraphicsItem::FillOpacity(brush), 1.0);

        // e lo slider non è più spento su un gradiente. Le caselle/slider sono nulli nel binario
        // di test (non linka widget.cpp): creali qui e rimettili a nullptr da un DISTRUTTORE, o un
        // QVERIFY che fallisce li lascia appesi allo stack e fa segfaultare i test successivi
        AddFrame();
        DrawStick(QPointF(300, 120), QPointF(300, 220));
        StickData *stick = CurrentStickFigure()->stick_list.last();
        stick->stick_type = StickData::RECT;
        stick->img_w_scale = 40;
        stick->pen = QPen(Qt::black, 2);
        stick->brush = brush;                          // gradiente al 50%
        CurrentStickFigure()->current_stick = stick;

        QSlider fill_slider, stroke_slider;
        QSpinBox fill_box, stroke_box;
        struct Restore {
            ~Restore(){
                brush_opacity_slider = nullptr; brush_opacity_spinbox = nullptr;
                pen_opacity_slider = nullptr;   pen_opacity_spinbox = nullptr;
            }
        } restore_globals;
        for(QSlider *s : { &fill_slider, &stroke_slider }) s->setRange(0, 100);
        for(QSpinBox *b : { &fill_box, &stroke_box })      b->setRange(0, 100);
        brush_opacity_slider = &fill_slider;  brush_opacity_spinbox = &fill_box;
        pen_opacity_slider = &stroke_slider;  pen_opacity_spinbox = &stroke_box;

        view->my_animation->UpdateStyleControls();
        QVERIFY(fill_slider.isEnabled());              // prima era spento per un gradiente
        QCOMPARE(fill_slider.value(), 50);             // e l'eco legge il picco degli stop
        QCOMPARE(fill_box.value(), 50);

        // il vuoto resta l'unico caso senza nulla da sbiadire
        stick->brush = QBrush(Qt::NoBrush);
        view->my_animation->UpdateStyleControls();
        QVERIFY(!fill_slider.isEnabled());
    }

    // StickGraphicsItem::TextureToScene() is what the tile -> scene mapping actually is, and
    // both the non-tiling clip and the arrow handle depend on it agreeing with AlignBrush():
    // for a scene-anchored texture the brush transform IS the whole mapping (AlignBrush has
    // cancelled the stick's frame out), for a stick-anchored one the stick's frame composes on
    // top. Also pins that texture_tiled only ever acts on a texture.
    void TextureToSceneMatchesTheFrameAlignBrushChose()
    {
        QImage tile(4, 4, QImage::Format_ARGB32);
        tile.fill(Qt::cyan);
        QBrush brush(tile);
        TextureXf xf;
        xf.scale = 2.0;
        WriteTextureXf(&brush, xf);

        StickData rect;
        rect.stick_type = StickData::RECT;
        rect.my_line = QLineF(60, 80, 60, 180);
        rect.img_angle = 0;                    // nessuna rotazione: la traslazione si legge diretta
        StickGraphicsItem rect_view(&rect);

        rect.brush_follows_stick = false;
        QCOMPARE(rect_view.TextureToScene(brush), brush.transform());

        rect.brush_follows_stick = true;
        const QTransform followed = rect_view.TextureToScene(brush);
        // la tile parte dall'origine dello stick (P1) e vale il doppio dei suoi pixel
        QCOMPARE(followed.map(QPointF(0, 0)), QPointF(60, 80));
        QCOMPARE(followed.map(QPointF(4, 4)), QPointF(68, 88));

        // un gradiente non ha una tile da mappare, e una tinta unita nemmeno
        QLinearGradient gradient(QPointF(0, 0), QPointF(1, 0));
        gradient.setCoordinateMode(QGradient::ObjectMode);
        QVERIFY(rect_view.TextureToScene(QBrush(gradient)).isIdentity());
        QVERIFY(rect_view.TextureToScene(QBrush(Qt::red)).isIdentity());
    }

    // brush_follows_stick == false has to mean the fill STAYS PUT while the stick moves over it,
    // and for a gradient that is not something a brush transform can deliver: ObjectMode means
    // "relative to the painted shape's bounding box", so the box travels with the stick and the
    // gradient with it however the transform is composed. Compensating the rotation fixes only
    // the DIRECTION — the gradient still visibly rode along, which is the bug this pins.
    // Rendered, not asserted on a matrix: the claim is about what lands on the canvas.
    void UnanchoredGradientStaysPutWhenTheStickMoves()
    {
        QLinearGradient gradient(QPointF(0, 0), QPointF(1, 0)); // asse orizzontale, nero->bianco
        gradient.setCoordinateMode(QGradient::ObjectMode);
        gradient.setStops({ QGradientStop(0.0, Qt::black), QGradientStop(1.0, Qt::white) });

        StickData rect;
        rect.stick_type = StickData::RECT;
        rect.my_line = QLineF(100, 100, 100, 180);  // lungo 80, verso il basso
        rect.img_w_scale = 80;                      // largo 80: la forma ruota su una zona ampia
        rect.brush = QBrush(gradient);
        rect.pen = QPen(Qt::NoPen);                 // solo il riempimento: nessun contorno a sporcare

        // (80,120) cade dentro la forma sia a 0 che a 90 gradi (footprint x 60..140 / y 100..180
        // contro x 20..100 / y 60..140), quindi è il punto su cui la domanda ha senso
        const QPoint probe(80, 120);
        auto colour_at_probe = [&probe](StickData *data){
            QGraphicsScene render_scene;
            render_scene.setSceneRect(0, 0, 200, 200);
            render_scene.addItem(new StickGraphicsItem(data)); // la scena possiede la view
            QImage img(200, 200, QImage::Format_ARGB32);
            img.fill(Qt::red);                                 // rosso = nulla dipinto qui
            QPainter painter(&img);
            render_scene.render(&painter, QRectF(0, 0, 200, 200), QRectF(0, 0, 200, 200));
            painter.end();
            return img.pixelColor(probe);
        };

        rect.brush_follows_stick = false;
        rect.img_angle = 0;
        const QColor flat_still = colour_at_probe(&rect);
        rect.img_angle = 90;
        const QColor turned_still = colour_at_probe(&rect);
        // il punto è coperto in entrambi i casi (altrimenti il test non misura nulla)...
        QVERIFY(flat_still != QColor(Qt::red));
        QVERIFY(turned_still != QColor(Qt::red));
        // ...e il gradiente è fermo nella scena: stesso punto, stesso colore
        QCOMPARE(turned_still, flat_still);

        // controprova: con l'ancoraggio allo stick lo stesso punto DEVE cambiare colore, sennò
        // il test sopra passerebbe anche per un gradiente diventato una tinta unita
        rect.brush_follows_stick = true;
        rect.img_angle = 0;
        const QColor flat_follow = colour_at_probe(&rect);
        rect.img_angle = 90;
        const QColor turned_follow = colour_at_probe(&rect);
        QVERIFY(qAbs(qGray(turned_follow.rgb()) - qGray(flat_follow.rgb())) > 20);
    }

    // Qt has no non-repeating brush style, so StickData::texture_tiled is enforced by clipping
    // the fill to the one instance the texture transform places. Rendered rather than asserted
    // on a matrix, because the whole point is what reaches the canvas: with tiling ON the fill
    // covers the shape, with it OFF only the tile-sized corner does — and the STROKE, which is
    // outside the clip, must survive either way.
    void NonTilingTextureFillPaintsOneInstanceOnly()
    {
        QImage tile(10, 10, QImage::Format_ARGB32);
        tile.fill(Qt::blue);

        StickData rect;
        rect.stick_type = StickData::RECT;
        rect.my_line = QLineF(100, 40, 100, 140);  // 100 di lunghezza, verso il basso
        rect.img_angle = 0;
        rect.img_w_scale = 60;                     // largo 60: molte ripetizioni di una tile da 10
        rect.brush = QBrush(tile);                 // scala 1 -> la tile resta 10x10
        rect.pen = QPen(Qt::black, 3);
        rect.brush_follows_stick = true;

        auto blue_pixels = [](StickData *data){
            QGraphicsScene render_scene;
            render_scene.setSceneRect(0, 0, 200, 200);
            render_scene.addItem(new StickGraphicsItem(data)); // la scena possiede la view
            QImage img(200, 200, QImage::Format_ARGB32);
            img.fill(Qt::white);
            QPainter painter(&img);
            render_scene.render(&painter, QRectF(0, 0, 200, 200), QRectF(0, 0, 200, 200));
            painter.end();
            int blue = 0, black = 0;
            for(int y = 0; y < img.height(); y++)
                for(int x = 0; x < img.width(); x++){
                    const QColor c = img.pixelColor(x, y);
                    if(c == QColor(Qt::blue)) blue++;
                    else if(c == QColor(Qt::black)) black++;
                }
            return QPair<int,int>(blue, black);
        };

        rect.texture_tiled = true;
        const QPair<int,int> tiled = blue_pixels(&rect);
        rect.texture_tiled = false;
        const QPair<int,int> single = blue_pixels(&rect);

        // ripetuta, la tile copre tutto il rettangolo (60 x 100, meno il bordo)
        QVERIFY(tiled.first > 4000);
        // una sola istanza: al massimo i 100 px della tile, e comunque qualcosa
        QVERIFY(single.first > 0);
        QVERIFY(single.first <= 100);
        // il contorno sta FUORI dal clip e sopravvive identico in entrambi i casi
        QVERIFY(tiled.second > 0);
        QCOMPARE(single.second, tiled.second);
    }

    // POLYGON sticks persist their normalized-local vertex list after the shared img
    // block; both the stream operators and Clone()/CopyFrom() must carry it
    void StickPolygonRoundTrip()
    {
        StickData source;
        source.stick_type = StickData::POLYGON;
        source.my_line = QLineF(10, 10, 10, 110);
        source.img_angle = 12.5f;
        source.img_angle_offset = 30;
        source.img_offset = QPointF(5, -3);
        source.polygon_points << QPointF(0.1, 0.2) << QPointF(-0.5, 0.75) << QPointF(0.33, -0.25);
        source.vertex_curvatures << 0.25f << 0.0f << 0.75f; // curvatura per vertice (POLYGON)

        QByteArray bytes;
        {
            QDataStream out(&bytes, QIODevice::WriteOnly);
            out << source;
        }
        StickData loaded;
        {
            QDataStream in(&bytes, QIODevice::ReadOnly);
            in >> loaded;
        }
        QCOMPARE(loaded.stick_type, source.stick_type);
        QCOMPARE(loaded.img_angle, source.img_angle);
        QCOMPARE(loaded.img_angle_offset, source.img_angle_offset);
        QCOMPARE(loaded.img_offset, source.img_offset);
        QCOMPARE(loaded.polygon_points, source.polygon_points);
        QCOMPARE(loaded.vertex_curvatures, source.vertex_curvatures);

        StickData *clone = source.Clone();
        QCOMPARE(clone->polygon_points, source.polygon_points);
        delete clone;

        // PolygonTransform() must be a bijection between stored vertices and the scene
        QTransform t = source.PolygonTransform();
        for (const QPointF &pt : source.polygon_points) {
            QPointF back = t.inverted().map(t.map(pt));
            QVERIFY(qAbs(back.x() - pt.x()) < 1e-9);
            QVERIFY(qAbs(back.y() - pt.y()) < 1e-9);
        }
    }

    // MULTILINE persists interior vertices plus the parallel per-vertex width and
    // curvature lists (virtual indexing: interior count + the two endpoints)
    void StickMultilineRoundTrip()
    {
        StickData source;
        source.stick_type = StickData::MULTILINE;
        source.my_line = QLineF(0, 0, 0, 100);
        source.img_angle = 0;
        source.polygon_points << QPointF(0.2, 0.3) << QPointF(-0.1, 0.6);
        source.vertex_widths << 6.0f << 8.0f << 10.0f << 4.0f;
        source.vertex_curvatures << 0.0f << 0.5f << 1.0f << 0.0f;
        source.pen.setWidthF(6.0);

        QByteArray bytes;
        {
            QDataStream out(&bytes, QIODevice::WriteOnly);
            out << source;
        }
        StickData loaded;
        {
            QDataStream in(&bytes, QIODevice::ReadOnly);
            in >> loaded;
        }
        QCOMPARE(loaded.stick_type, source.stick_type);
        QCOMPARE(loaded.polygon_points, source.polygon_points);
        QCOMPARE(loaded.vertex_widths, source.vertex_widths);
        QCOMPARE(loaded.vertex_curvatures, source.vertex_curvatures);

        StickData *clone = source.Clone();
        QCOMPARE(clone->polygon_points, source.polygon_points);
        QCOMPARE(clone->vertex_widths, source.vertex_widths);
        QCOMPARE(clone->vertex_curvatures, source.vertex_curvatures);
        delete clone;
    }

    // figure-level round trip on pure data: scratch objects need no scene and no flags
    void StickFigureDataRoundTrip()
    {
        StickFigureData source;
        source.name = "figura";
        StickData *master = new StickData();
        master->my_line = QLineF(0, 0, 0, 100);
        source.stick_list << master;
        source.master_stick = master;
        StickData *child = new StickData();
        child->my_line = QLineF(0, 100, 50, 100);
        child->parent = master;
        master->children << child;
        source.stick_list << child;
        StickData *step_child = new StickData();
        step_child->my_line = QLineF(0, 0, -50, 0);
        step_child->parent = master;
        step_child->stepchild = true;
        master->children << step_child;
        source.stick_list << step_child;
        source.current_stick = child;

        QByteArray bytes;
        {
            QDataStream out(&bytes, QIODevice::WriteOnly);
            out << source;
        }
        StickFigureData loaded;
        {
            QDataStream in(&bytes, QIODevice::ReadOnly);
            in >> loaded;
        }

        QCOMPARE(loaded.stick_list.count(), 3);
        QCOMPARE(loaded.name, source.name);
        QCOMPARE(loaded.master_stick, loaded.stick_list[0]);
        QCOMPARE(loaded.current_stick, loaded.stick_list[1]); // regression: used to always reset to stick_list[0]
        QVERIFY(loaded.stick_list[0]->parent == nullptr);
        QCOMPARE(loaded.stick_list[1]->parent, loaded.stick_list[0]);
        QCOMPARE(loaded.stick_list[2]->parent, loaded.stick_list[0]);
        QVERIFY(loaded.stick_list[2]->stepchild);
        QVERIFY(!loaded.stick_list[1]->stepchild);
        QVERIFY(loaded.stick_list[0]->children.contains(loaded.stick_list[1]));
        QVERIFY(loaded.stick_list[0]->children.contains(loaded.stick_list[2]));
        for (int i = 0; i < 3; i++)
            QCOMPARE(loaded.stick_list[i]->my_line, source.stick_list[i]->my_line);
        // the destructors free the sticks — no scratch-cleanup choreography left
    }

    void DataCloneIsIndependent()
    {
        StickFigureData source;
        StickData *master = new StickData();
        master->my_line = QLineF(0, 0, 0, 100);
        source.stick_list << master;
        source.master_stick = master;
        StickData *child = new StickData();
        child->my_line = QLineF(0, 100, 50, 100);
        child->parent = master;
        master->children << child;
        source.stick_list << child;

        StickFigureData *copy = source.Clone();
        QCOMPARE(copy->stick_list.count(), 2);
        QVERIFY(copy->stick_list[0] != source.stick_list[0]);
        QCOMPARE(copy->master_stick, copy->stick_list[0]);
        QCOMPARE(copy->stick_list[1]->parent, copy->stick_list[0]);
        QVERIFY(copy->stick_list[0]->children.contains(copy->stick_list[1]));

        QLineF original_line = source.stick_list[0]->my_line;
        copy->stick_list[0]->my_line.translate(5, 5);
        QCOMPARE(source.stick_list[0]->my_line, original_line);
        delete copy;
    }

    // Phase 6: DataCloneIsIndependent above only exercises LINE sticks (no images) —
    // this covers the deep-image-copy path of StickData::CopyFrom() through the
    // StickFigureData::Clone() wrapper, pure data, no scene/view involved
    void StickFigureDataCloneDeepCopiesImages()
    {
        StickFigureData source;
        StickData *imgStick = new StickData();
        imgStick->stick_type = StickData::IMAGE;
        imgStick->my_line = QLineF(0, 0, 0, 50);
        QImage *img = new QImage(4, 4, QImage::Format_ARGB32);
        img->fill(Qt::red);
        imgStick->stick_img_list << img;
        imgStick->stick_img = img;
        source.stick_list << imgStick;
        source.master_stick = imgStick;

        StickFigureData *copy = source.Clone();
        QCOMPARE(copy->stick_list.count(), 1);
        QCOMPARE(copy->stick_list[0]->stick_img_list.count(), 1);
        QVERIFY(copy->stick_list[0]->stick_img != source.stick_list[0]->stick_img); // deep copy, not a shared pointer
        QCOMPARE(*copy->stick_list[0]->stick_img, *source.stick_list[0]->stick_img); // same pixels

        // mutating the clone's image must not affect the source's
        copy->stick_list[0]->stick_img->fill(Qt::blue);
        QCOMPARE(source.stick_list[0]->stick_img->pixelColor(0, 0), QColor(Qt::red));

        delete copy;
    }

    // Phase 6: FrameData::Clone() remaps total_sticks and current_stick_figure onto the
    // cloned figures by index — this is easy to get wrong (stale pointers into the
    // source) and had no direct pure-data test before
    void FrameDataCloneRemapsReferences()
    {
        FrameData source;
        source.frame_number = 3;
        source.num_of_items = 2;

        StickFigureData *figureA = new StickFigureData();
        StickData *stickA = new StickData();
        stickA->my_line = QLineF(0, 0, 0, 50);
        figureA->stick_list << stickA;
        figureA->master_stick = stickA;
        figureA->current_stick = stickA;
        source.stick_figures << figureA;

        StickFigureData *figureB = new StickFigureData();
        StickData *stickB = new StickData();
        stickB->my_line = QLineF(0, 50, 0, 100);
        figureB->stick_list << stickB;
        figureB->master_stick = stickB;
        source.stick_figures << figureB;

        // deliberately out of figure order, to prove the remap is index-based, not positional
        source.total_sticks << stickB << stickA;
        source.current_stick_figure = figureA;

        FrameData *copy = source.Clone();
        QCOMPARE(copy->stick_figures.count(), 2);
        QVERIFY(copy->stick_figures[0] != figureA);
        QCOMPARE(copy->total_sticks.count(), 2);
        QCOMPARE(copy->total_sticks[0], copy->stick_figures[1]->stick_list[0]); // was stickB
        QCOMPARE(copy->total_sticks[1], copy->stick_figures[0]->stick_list[0]); // was stickA
        QCOMPARE(copy->current_stick_figure, copy->stick_figures[0]);

        QLineF original_line = stickA->my_line;
        copy->stick_figures[0]->stick_list[0]->my_line.translate(10, 10);
        QCOMPARE(stickA->my_line, original_line);

        delete copy;
    }

    // Phase 6: FrameData's operator>> rebuilds total_sticks from each stick's `order`
    // field, not from insertion/figure order (see framedata.cpp) — this is the one
    // reordering behavior FrameSnapshotRoundTrip (which uses sequential orders via a
    // live frame) never actually exercises
    void FrameDataRoundTrip()
    {
        FrameData source;
        source.frame_number = 5;
        source.num_of_items = 2;

        StickFigureData *figureA = new StickFigureData();
        StickData *stickA = new StickData();
        stickA->my_line = QLineF(0, 0, 0, 50);
        stickA->order = 1; // second in z-order despite being the first figure added
        figureA->stick_list << stickA;
        figureA->master_stick = stickA;
        figureA->current_stick = stickA;
        source.stick_figures << figureA;

        StickFigureData *figureB = new StickFigureData();
        StickData *stickB = new StickData();
        stickB->my_line = QLineF(0, 50, 0, 100);
        stickB->order = 0; // first in z-order despite being the second figure added
        figureB->stick_list << stickB;
        figureB->master_stick = stickB;
        source.stick_figures << figureB;

        source.current_stick_figure = figureA;

        QByteArray bytes;
        {
            QDataStream out(&bytes, QIODevice::WriteOnly);
            out << source;
        }
        FrameData loaded;
        {
            QDataStream in(&bytes, QIODevice::ReadOnly);
            in >> loaded;
        }

        QCOMPARE(loaded.stick_figures.count(), 2);
        QCOMPARE(loaded.frame_number, source.frame_number);
        QCOMPARE(loaded.num_of_items, source.num_of_items);
        QCOMPARE(loaded.total_sticks.count(), 2);
        QCOMPARE(loaded.total_sticks[0]->my_line, stickB->my_line); // order 0
        QCOMPARE(loaded.total_sticks[1]->my_line, stickA->my_line); // order 1
        QVERIFY(loaded.current_stick_figure != nullptr);
        QCOMPARE(loaded.current_stick_figure->current_stick->my_line, stickA->my_line);
    }

    void CopyPasteStickFigure()
    {
        AddFrame();
        DrawStick(QPointF(100, 100), QPointF(100, 200));
        DrawStick(QPointF(100, 200), QPointF(150, 250));
        StickFigure *source = CurrentStickFigure();
        QCOMPARE(source->stick_list.count(), 2);

        // replicates Widget::on_copyStickFigureBtn_clicked() (widget.cpp)
        delete view->copy_stick_figure_buffer;
        view->copy_stick_figure_buffer = ExtractStickFigureData(*source);

        StickFigureData *buffered = view->copy_stick_figure_buffer;
        QCOMPARE(buffered->stick_list.count(), 2);
        QCOMPARE(buffered->stick_list[0]->my_line, source->stick_list[0]->my_line);
        QCOMPARE(buffered->stick_list[1]->my_line, source->stick_list[1]->my_line);
        QCOMPARE(buffered->stick_list[1]->parent, buffered->stick_list[0]);
        QCOMPARE(buffered->master_stick, buffered->stick_list[0]);
        QCOMPARE(buffered->current_stick, buffered->stick_list[1]); // last-drawn stick is current

        // deep copy: mutating the buffer leaves the source untouched
        QLineF source_line = source->stick_list[0]->my_line;
        buffered->stick_list[0]->my_line.translate(5, 5);
        QCOMPARE(source->stick_list[0]->my_line, source_line);
        buffered->stick_list[0]->my_line.translate(-5, -5);

        // replicates Widget::on_pasteStickFigureBtn_clicked() (widget.cpp)
        view->StoreUndo(CMD_SIMPLE);
        StickFigure *pasted = AddStick();
        PopulateLiveStickFigure(pasted, *buffered, true, true);
        pasted->Refresh(0);
        for (StickData *s : pasted->stick_list)
            CurrentFrame()->total_sticks.append(s);
        view->AddHandles(pasted);

        QCOMPARE(pasted->stick_list.count(), 2);
        QCOMPARE(pasted->stick_list[1]->parent, pasted->stick_list[0]);
        // regression: PopulateLiveStickFigure used to always reset current_stick to
        // stick_list[0], discarding what ExtractStickFigureData had just captured
        QCOMPARE(pasted->current_stick, pasted->stick_list[1]);
        // pasted sticks really live on the scene (attach_to_scene = true)
        QCOMPARE(pasted->stick_list[0]->view->scene(), scene);
    }

    // the img_list_widget cleanup moved from ~stick to explicit calls at the live-delete
    // sites (Phase 1b): deleting the on-screen IMAGE stick empties its widget rows,
    // while scratch snapshots (undo buffer) never touch them
    void DeleteImageStickClearsImageListWidget()
    {
        AddFrame();
        StickData *drawn = DrawStick(QPointF(500, 100), QPointF(500, 200));
        // upgrade to IMAGE like DrawCmd's DRAWIMG branch (myview.cpp:486)
        drawn->stick_type = StickData::IMAGE;
        QImage *image = new QImage(8, 8, QImage::Format_ARGB32);
        image->fill(Qt::green);
        drawn->view->AddImage(image, "img");
        QCOMPARE(img_list_widget->count(), 1);

        // snapshotting the frame into the undo buffer (scratch path) must not touch the widget
        view->StoreUndo(CMD_SIMPLE);
        QCOMPARE(img_list_widget->count(), 1);

        // live deletion via the delete-key flow (myview.cpp:394)
        StickFigure *figure = CurrentStickFigure();
        int idx = figure->stick_list.indexOf(drawn);
        CurrentFrame()->total_sticks.removeAll(drawn);
        figure->DeleteStick(idx);
        figure->current_stick = nullptr; // the key handler moves selection to the parent (none here)
        QCOMPARE(figure->stick_list.count(), 0);
        QCOMPARE(img_list_widget->count(), 0);
    }

    // the exact snapshot StoreUndo() takes since Phase 2b (plain data extraction),
    // plus the FrameData byte round-trip the file paths use
    void FrameSnapshotRoundTrip()
    {
        AddFrame();
        DrawStick(QPointF(10, 10), QPointF(10, 110));
        DrawStick(QPointF(10, 110), QPointF(60, 160));
        Frame *live_frame = CurrentFrame();
        view->UpdateFrameOrder(live_frame); // StoreUndo() does this before snapshotting

        FrameData *snapshot = ExtractFrameData(*live_frame);
        QCOMPARE(snapshot->stick_figures.count(), live_frame->stick_figures.count());
        QCOMPARE(snapshot->frame_number, live_frame->frame_number);
        QCOMPARE(snapshot->num_of_items, live_frame->num_of_items);
        QCOMPARE(snapshot->total_sticks.count(), live_frame->total_sticks.count());
        for (int i = 0; i < live_frame->stick_figures.count(); i++) {
            StickFigure *live_figure = live_frame->stick_figures[i];
            StickFigureData *snapshot_figure = snapshot->stick_figures[i];
            QCOMPARE(snapshot_figure->stick_list.count(), live_figure->stick_list.count());
            for (int j = 0; j < live_figure->stick_list.count(); j++)
                QCOMPARE(snapshot_figure->stick_list[j]->my_line, live_figure->stick_list[j]->my_line);
        }

        // byte round-trip: same layout as the pre-refactor shared operators
        QByteArray bytes;
        {
            QDataStream out(&bytes, QIODevice::WriteOnly);
            out << *snapshot;
        }
        FrameData loaded;
        {
            QDataStream in(&bytes, QIODevice::ReadOnly);
            in >> loaded;
        }
        QCOMPARE(loaded.stick_figures.count(), snapshot->stick_figures.count());
        QCOMPARE(loaded.frame_number, snapshot->frame_number);
        QCOMPARE(loaded.num_of_items, snapshot->num_of_items);
        QCOMPARE(loaded.total_sticks.count(), snapshot->total_sticks.count());
        for (int j = 0; j < snapshot->total_sticks.count(); j++)
            QCOMPARE(loaded.total_sticks[j]->order, snapshot->total_sticks[j]->order);
        QVERIFY(loaded.current_stick_figure != nullptr);

        delete snapshot; // pure data: plain delete, no scratch choreography
    }

    void UndoSimpleEdit()
    {
        AddFrame();
        DrawStick(QPointF(100, 100), QPointF(100, 200));
        int figure_index = CurrentFrame()->stick_figures.indexOf(CurrentStickFigure());
        QLineF original_line = CurrentStickFigure()->stick_list[0]->my_line;

        // a "simple" edit: StoreUndo(CMD_SIMPLE) + direct model mutation
        view->StoreUndo(CMD_SIMPLE);
        CurrentStickFigure()->Traslate(10, 20);
        QLineF moved_line = CurrentStickFigure()->stick_list[0]->my_line;
        QVERIFY(moved_line != original_line);

        view->undo_stack.undo();
        QCOMPARE(CurrentFrame()->stick_figures[figure_index]->stick_list[0]->my_line, original_line);

        view->undo_stack.redo();
        QCOMPARE(CurrentFrame()->stick_figures[figure_index]->stick_list[0]->my_line, moved_line);
    }

    void UndoAddFrame()
    {
        int frames_before = view->my_animation->frame_list.count();
        int current_number_before = CurrentFrame()->frame_number;

        AddFrame();
        QCOMPARE(view->my_animation->frame_list.count(), frames_before + 1);

        view->undo_stack.undo();
        QCOMPARE(view->my_animation->frame_list.count(), frames_before);
        QCOMPARE(CurrentFrame()->frame_number, current_number_before);

        view->undo_stack.redo();
        QCOMPARE(view->my_animation->frame_list.count(), frames_before + 1);
    }

    // isolates the CMD_ADDFRAME+undo path specifically: RestoreFrameSnapshot() calls
    // Animation::DeleteFrame() on the added frame (~Frame -> delete each StickFigure ->
    // ~StickFigure()), which is a DIFFERENT code path from ClearFrame() (used on the frame
    // being restored INTO, not the one being removed). Built the image stick directly,
    // bypassing DrawStick()/StoreUndo(), so only ONE undo command (CMD_ADDFRAME) exists —
    // undoing it must delete the whole added frame, sticks included, in a single step.
    void UndoAddFrameFreesImageStickListWidgetEntries()
    {
        AddFrame();
        StickFigure *figure = CurrentStickFigure();
        StickData *img_stick = new StickData();
        img_stick->my_line = QLineF(700, 100, 700, 200);
        // attacca la view MENTRE il tipo è ancora LINE (default): il costruttore di stick
        // chiama UpdateBr(0) subito, che per un IMAGE deferenzierebbe stick_img prima che
        // AddImage() lo assegni — stesso ordine di MyView::DrawCmd() (myview.cpp)
        StickGraphicsItem *img_stick_view = FramePresenter::AttachView(img_stick, scene);
        img_stick_view->my_stick_figure = figure;
        img_stick->stick_type = StickData::IMAGE;
        QImage *image = new QImage(8, 8, QImage::Format_ARGB32);
        image->fill(Qt::blue);
        img_stick_view->AddImage(image, "leak-check"); // also registers an entry in img_list_widget
        figure->stick_list.append(img_stick);
        figure->master_stick = img_stick;
        figure->current_stick = img_stick;
        CurrentFrame()->total_sticks.append(img_stick);
        QCOMPARE(img_list_widget->count(), 1);

        view->undo_stack.undo(); // CMD_ADDFRAME+undo: deletes the frame img_stick lives in
        QCOMPARE(img_list_widget->count(), 0);
    }

    void UndoDeleteFrame()
    {
        AddFrame();
        DrawStick(QPointF(50, 50), QPointF(50, 150));
        int frames_before = view->my_animation->frame_list.count();
        int sticks_before = CurrentFrame()->total_sticks.count();
        int deleted_number = CurrentFrame()->frame_number;

        // replicates Widget::on_deleteFrameBtn_clicked() (widget.cpp:559)
        view->StoreUndo(CMD_DELETEFRAME);
        view->DeleteFrame(CurrentFrame());
        view->MoveToFrame(CurrentFrame());
        QCOMPARE(view->my_animation->frame_list.count(), frames_before - 1);

        view->undo_stack.undo();
        QCOMPARE(view->my_animation->frame_list.count(), frames_before);
        QCOMPARE(CurrentFrame()->frame_number, deleted_number);
        QCOMPARE(CurrentFrame()->total_sticks.count(), sticks_before);

        view->undo_stack.redo();
        QCOMPARE(view->my_animation->frame_list.count(), frames_before - 1);
    }

    void UndoMoveToFrame()
    {
        AddFrame();
        int origin_number = CurrentFrame()->frame_number;
        Frame *other_frame = view->my_animation->frame_list[0];
        QVERIFY(other_frame != CurrentFrame());

        // replicates Widget::on_frameListWidget_itemClicked() (widget.cpp:568)
        view->StoreUndo(CMD_MOVETOFRAME);
        view->MoveToFrame(other_frame);
        QCOMPARE(CurrentFrame()->frame_number, 0);

        view->undo_stack.undo();
        QCOMPARE(CurrentFrame()->frame_number, origin_number);
    }

    // MyView::GenerateInbetweens(): N frames appear between the two keyframes with every
    // stick at its linearly interpolated pose, the keyframes themselves are untouched, and
    // the whole batch is ONE undo step (FrameBatchUndoCommand, myview.cpp).
    void GenerateInbetweensInterpolatesAndIsOneUndoStep()
    {
        AddFrame();
        StickData *start_stick = DrawStick(QPointF(100, 100), QPointF(100, 200));
        Frame *start_frame = CurrentFrame();
        AddFrame(); // secondo keyframe: stessa struttura (1 figura, 1 stick), posa diversa
        StickData *end_stick = DrawStick(QPointF(200, 100), QPointF(200, 300));
        // un riempimento a gradiente su entrambi i keyframe: non ha punto intermedio
        // lineare, quindi InterpolateStickData() deve LASCIARLO STARE invece di appiattirlo
        // a tinta unita (QPen::setColor rimpiazzerebbe il brush del pen — vedi IsSolidFill())
        QLinearGradient gradient(0, 0, 1, 1);
        gradient.setColorAt(0, Qt::red);
        gradient.setColorAt(1, Qt::blue);
        start_stick->brush = QBrush(gradient);
        end_stick->brush = QBrush(gradient);
        start_stick->pen.setBrush(QBrush(gradient));
        end_stick->pen.setBrush(QBrush(gradient));
        int start_index = start_frame->frame_number;
        QCOMPARE(view->my_animation->frame_list[start_index + 1], CurrentFrame()); // adiacenti
        int frames_before = view->my_animation->frame_list.count();

        QCOMPARE(view->GenerateInbetweens(start_index, 3), QString()); // nessun rifiuto
        QCOMPARE(view->my_animation->frame_list.count(), frames_before + 3);

        // il secondo dei tre frame generati sta a t = 2/4, cioè esattamente a metà strada
        Frame *middle = view->my_animation->frame_list[start_index + 2];
        StickData *middle_stick = middle->stick_figures[0]->stick_list[0];
        QCOMPARE(middle_stick->my_line, QLineF(150, 100, 150, 250));
        QCOMPARE(middle->frame_number, start_index + 2);
        // il gradiente è sopravvissuto all'interpolazione, su entrambi i lati
        QCOMPARE(middle_stick->brush.style(), Qt::LinearGradientPattern);
        QCOMPARE(middle_stick->pen.brush().style(), Qt::LinearGradientPattern);
        // i due keyframe restano intatti
        QCOMPARE(start_stick->my_line, QLineF(100, 100, 100, 200));
        QCOMPARE(end_stick->my_line, QLineF(200, 100, 200, 300));

        view->undo_stack.undo(); // un solo passo deve togliere tutti e tre
        QCOMPARE(view->my_animation->frame_list.count(), frames_before);
        QCOMPARE(CurrentFrame()->frame_number, start_index);
        view->undo_stack.redo();
        QCOMPARE(view->my_animation->frame_list.count(), frames_before + 3);
        QCOMPARE(view->my_animation->frame_list[start_index + 2]
                 ->stick_figures[0]->stick_list[0]->my_line, QLineF(150, 100, 150, 250));

        // i test slot condividono una sola animazione e i successivi danno per scontato che
        // AddFrame() aggiunga in coda (cioè che il frame corrente sia l'ultimo): qui il
        // corrente è un frame generato, che di frame dopo di sé ne ha ancora
        view->MoveToFrame(view->my_animation->frame_list.last());
    }

    // structural mismatch between the two keyframes: nothing is generated and the reason
    // comes back as text (Widget::on_generateInbetweenBtn_clicked() shows it)
    void GenerateInbetweensRefusesMismatchedFrames()
    {
        AddFrame();
        DrawStick(QPointF(10, 10), QPointF(10, 60));
        Frame *start_frame = CurrentFrame();
        AddFrame();
        DrawStick(QPointF(20, 10), QPointF(20, 60));
        DrawStick(QPointF(20, 60), QPointF(60, 60)); // un secondo stick: 1 contro 2
        int frames_before = view->my_animation->frame_list.count();

        QVERIFY(!view->GenerateInbetweens(start_frame->frame_number, 2).isEmpty());
        QCOMPARE(view->my_animation->frame_list.count(), frames_before); // niente è cambiato
        // e nemmeno un frame inesistente oltre l'ultimo
        QVERIFY(!view->GenerateInbetweens(view->my_animation->frame_list.count() - 1, 1).isEmpty());
        QCOMPARE(view->my_animation->frame_list.count(), frames_before);
    }

    // MyView::InsertFrames()/DeleteFrames(): a contiguous BLOCK of frames enters or leaves
    // in ONE undo step (FrameBatchUndoCommand, myview.cpp), in its original order, and the
    // snapshots belong to the undo entry (so a paste can be repeated from the same buffer).
    // Replicates what Widget::on_cpyFrame_clicked()/on_pasteFrame_clicked() and
    // on_deleteFrameBtn_clicked() do for a multi-frame selection in the frame list.
    void MultiFramePasteAndDeleteAreOneUndoStep()
    {
        AddFrame();
        DrawStick(QPointF(30, 30), QPointF(30, 80));
        int first_index = CurrentFrame()->frame_number;
        AddFrame();
        DrawStick(QPointF(40, 30), QPointF(40, 90));
        int frames_before = view->my_animation->frame_list.count();

        // "copia i due frame selezionati" (on_cpyFrame_clicked): il buffer è puro dato
        QList<FrameData *> copy_buffer;
        copy_buffer.append(ExtractFrameData(*view->my_animation->frame_list[first_index]));
        copy_buffer.append(ExtractFrameData(*view->my_animation->frame_list[first_index + 1]));
        // "incolla" dopo il frame corrente: all'undo stack vanno dei Clone(), il buffer resta
        QList<FrameData *> snapshots;
        for (FrameData *copied : copy_buffer)
            snapshots.append(copied->Clone());
        int paste_at = CurrentFrame()->frame_number + 1;
        view->InsertFrames(paste_at, snapshots);

        QCOMPARE(view->my_animation->frame_list.count(), frames_before + 2);
        // ordine originale conservato, e ogni frame incollato è numerato per la sua posizione
        QCOMPARE(view->my_animation->frame_list[paste_at]->frame_number, paste_at);
        QCOMPARE(view->my_animation->frame_list[paste_at + 1]->frame_number, paste_at + 1);
        QCOMPARE(view->my_animation->frame_list[paste_at]->stick_figures[0]->stick_list[0]->my_line,
                 QLineF(30, 30, 30, 80));
        QCOMPARE(view->my_animation->frame_list[paste_at + 1]->stick_figures[0]->stick_list[0]->my_line,
                 QLineF(40, 30, 40, 90));

        view->undo_stack.undo(); // un solo passo deve togliere entrambi
        QCOMPARE(view->my_animation->frame_list.count(), frames_before);
        view->undo_stack.redo();
        QCOMPARE(view->my_animation->frame_list.count(), frames_before + 2);

        // la sequenza che segfaultava: LASCIARE il blocco (le righe della lista layer del
        // primo frame incollato muoiono lì, in MoveToFrame) e solo POI annullare. Il ciclo
        // di cancellazione passava ad Animation::DeleteFrame() un QListWidgetItem già
        // distrutto, perché è DeleteFrame stessa a riassegnare current_frame a ogni giro —
        // vedi MyView::ClearLayerRows(). Serve un blocco IN CODA: il current riassegnato
        // ricade sull'ultimo frame rimasto, cioè sul prossimo che il ciclo cancella
        view->MoveToFrame(view->my_animation->frame_list[0]);
        view->undo_stack.undo();
        QCOMPARE(view->my_animation->frame_list.count(), frames_before);
        view->undo_stack.redo();
        QCOMPARE(view->my_animation->frame_list.count(), frames_before + 2);

        // ...e ora cancellali insieme, che è l'altra direzione dello stesso comando
        QVERIFY(view->DeleteFrames(paste_at, 2));
        QCOMPARE(view->my_animation->frame_list.count(), frames_before);
        view->undo_stack.undo();
        QCOMPARE(view->my_animation->frame_list.count(), frames_before + 2);
        QCOMPARE(view->my_animation->frame_list[paste_at]->stick_figures[0]->stick_list[0]->my_line,
                 QLineF(30, 30, 30, 80));
        view->undo_stack.redo();
        QCOMPARE(view->my_animation->frame_list.count(), frames_before);
        // un blocco che comprende TUTTI i frame è rifiutato senza toccare niente:
        // un'animazione ha sempre almeno un frame
        QVERIFY(!view->DeleteFrames(0, view->my_animation->frame_list.count()));
        QCOMPARE(view->my_animation->frame_list.count(), frames_before);

        qDeleteAll(copy_buffer);
        // i test successivi danno per scontato che il frame corrente sia l'ultimo
        view->MoveToFrame(view->my_animation->frame_list.last());
    }

    // MoveToFrame(): a frame switch leaves EXACTLY the new frame selected in the frame list.
    // The list is ContiguousSelection in the app (Widget's ctor — the test binary doesn't link
    // widget.cpp, hence the guard below), and in any mode other than SingleSelection
    // QListWidgetItem::setSelected() does NOT clear the previous selection, so every switch
    // used to leave the frame it came from selected too (most obvious when duplicating a frame).
    void MoveToFrameSelectsOnlyTheNewFrame()
    {
        // ripristina la modalità da un DISTRUTTORE: un QVERIFY che fallisce esce subito, e
        // lasciarla cambiata falserebbe gli slot successivi (stessa ragione del guard in
        // ApplyingAGradientEnablesTheStyleCheckBoxes)
        struct Restore {
            QListWidget *list;
            QAbstractItemView::SelectionMode previous;
            ~Restore() { list->setSelectionMode(previous); }
        } restore{my_frame_widget_list, my_frame_widget_list->selectionMode()};
        my_frame_widget_list->setSelectionMode(QAbstractItemView::ContiguousSelection);

        AddFrame();
        QCOMPARE(my_frame_widget_list->selectedItems().count(), 1);
        AddFrame(); // il frame lasciato non deve restare selezionato accanto al nuovo
        QCOMPARE(my_frame_widget_list->selectedItems().count(), 1);
        QCOMPARE(my_frame_widget_list->selectedItems().first(), CurrentFrame()->linked_item);
        QCOMPARE(my_frame_widget_list->currentItem(), CurrentFrame()->linked_item);
    }

    void ChopAndWeld()
    {
        AddFrame();
        StickFigure *figure = CurrentStickFigure();
        StickData *drawn = DrawStick(QPointF(100, 100), QPointF(100, 200));
        QCOMPARE(figure->stick_list.count(), 1);

        // replicates Widget::on_splitStick_clicked() (widget.cpp:1198)
        view->StoreUndo();
        view->RemoveHandles(figure);
        StickData *added = figure->Chop(figure->current_stick);
        view->AddHandles(figure);
        CurrentFrame()->total_sticks.append(added);

        QCOMPARE(figure->stick_list.count(), 2);
        QCOMPARE(drawn->my_line.p2(), QPointF(100, 150));
        QCOMPARE(added->my_line.p1(), QPointF(100, 150));
        QCOMPARE(added->my_line.p2(), QPointF(100, 200));
        QCOMPARE(added->parent, drawn);

        // replicates Widget::on_joinStick_clicked() (widget.cpp:1186), welding from the master
        figure->current_stick = drawn;
        view->StoreUndo();
        QList<StickData *> welded = figure->Weld(drawn);
        for (StickData *s : welded)
            CurrentFrame()->total_sticks.removeAll(s);

        QCOMPARE(figure->stick_list.count(), 1);
        QCOMPARE(drawn->my_line.p2(), QPointF(100, 200));
    }

    void MergeTwoStickFigures()
    {
        AddFrame();
        StickFigure *main_figure = CurrentStickFigure();
        StickData *main_stick = DrawStick(QPointF(100, 100), QPointF(100, 200));
        StickFigure *to_join = AddStick("joined");
        StickData *join_master = DrawStick(QPointF(200, 200), QPointF(200, 300));
        QCOMPARE(CurrentStickFigure(), to_join);

        // replicates the JOIN branch of MyView::mousePressEvent() (myview.cpp:314)
        view->RemoveHandles(main_figure);
        MergeStickFigures(main_figure, main_stick, to_join);
        view->AddHandles(main_figure);

        QCOMPARE(main_figure->stick_list.count(), 3); // original + invisible linker + clone
        StickData *linker = main_figure->stick_list[1];
        StickData *joined_clone = main_figure->stick_list[2];
        QCOMPARE(linker->parent, main_stick);
        QCOMPARE(linker->pen.style(), Qt::NoPen);
        QCOMPARE(joined_clone->parent, linker);
        QCOMPARE(joined_clone->my_line, join_master->my_line);
        QCOMPARE(main_figure->master_stick, main_stick);
        QVERIFY(main_stick->children.contains(linker));
        QVERIFY(main_stick->children.contains(joined_clone));

        // the JOIN flow then deletes the merged-in layer via DeleteStickFigure()
        my_stick_figure_widget_list->clearSelection();
        to_join->linked_item->setSelected(true);
        view->DeleteStickFigure();
        for (StickData *s : CurrentStickFigure()->stick_list)
            if (!CurrentFrame()->total_sticks.contains(s))
                CurrentFrame()->total_sticks.append(s);
        QCOMPARE(CurrentFrame()->stick_figures.count(), 1);
        QCOMPARE(CurrentStickFigure(), main_figure);
    }

    void SplitStickFigure()
    {
        AddFrame();
        StickFigure *figure = CurrentStickFigure();
        StickData *master_stick = DrawStick(QPointF(100, 100), QPointF(100, 200));
        StickData *child_stick = DrawStick(QPointF(100, 200), QPointF(150, 250));
        QCOMPARE(child_stick->parent, master_stick);
        QCOMPARE(CurrentStickFigure()->current_stick, child_stick);
        int figures_before = CurrentFrame()->stick_figures.count();

        view->SplitStickFigure(); // replicates Widget::on_splitBtn_clicked() (widget.cpp:1180)

        QCOMPARE(CurrentFrame()->stick_figures.count(), figures_before + 1);
        StickFigure *branch = CurrentStickFigure();
        QVERIFY(branch != figure);
        QCOMPARE(branch->name, figure->name + "_split");
        QCOMPARE(branch->stick_list.count(), 1);
        QCOMPARE(figure->stick_list.count(), 1);
        QCOMPARE(branch->master_stick, branch->stick_list[0]);
        QCOMPARE(branch->stick_list[0]->my_line, QLineF(100, 200, 150, 250));
        QCOMPARE(figure->stick_list[0], master_stick);
    }

    void MergeThenSplitRemovesOrphanedLinker()
    {
        AddFrame();
        StickFigure *main_figure = CurrentStickFigure();
        StickData *main_stick = DrawStick(QPointF(100, 100), QPointF(100, 200));
        StickFigure *to_join = AddStick("joined");
        StickData *join_master = DrawStick(QPointF(200, 200), QPointF(200, 300));
        // il merge CLONA gli stick di to_join e DeleteStickFigure() libera gli originali:
        // join_master diventa dangling — la geometria va fotografata ORA (prima questo
        // confronto leggeva memoria liberata e passava per pura fortuna di heap)
        QLineF join_master_line = join_master->my_line;

        // replicates the JOIN branch of MyView::mousePressEvent() (myview.cpp:314)
        view->RemoveHandles(main_figure);
        MergeStickFigures(main_figure, main_stick, to_join);
        view->AddHandles(main_figure);
        my_stick_figure_widget_list->clearSelection();
        to_join->linked_item->setSelected(true);
        view->DeleteStickFigure();
        for (StickData *s : CurrentStickFigure()->stick_list)
            if (!CurrentFrame()->total_sticks.contains(s))
                CurrentFrame()->total_sticks.append(s);

        QCOMPARE(CurrentStickFigure(), main_figure);
        QCOMPARE(main_figure->stick_list.count(), 3); // main + invisible linker + joined clone
        StickData *linker = main_figure->stick_list[1];
        QVERIFY(linker->is_linker);
        StickData *joined_clone = main_figure->stick_list[2];

        // split the joined-in branch back off (replicates Widget::on_splitBtn_clicked())
        main_figure->current_stick = joined_clone;
        view->SplitStickFigure();

        // regression test: the now-childless linker must not survive the split, or its
        // endpoint handles get re-added to main_figure's scene forever by AddHandles() -
        // visually a "copy of the master handle" stuck on the now-disconnected figure
        QCOMPARE(main_figure->stick_list.count(), 1);
        QCOMPARE(main_figure->stick_list[0], main_stick);
        for (StickData *s : main_figure->stick_list)
            QVERIFY(!s->is_linker);

        StickFigure *branch = CurrentStickFigure();
        QVERIFY(branch != main_figure);
        QCOMPARE(branch->stick_list.count(), 1);
        QCOMPARE(branch->master_stick, branch->stick_list[0]);
        QCOMPARE(branch->stick_list[0]->my_line, join_master_line);
    }

    void SetMasterOnDirectChain()
    {
        AddFrame();
        StickFigure *figure = CurrentStickFigure();
        StickData *old_master = DrawStick(QPointF(200, 100), QPointF(200, 200));
        StickData *new_master = DrawStick(QPointF(200, 200), QPointF(200, 300));
        QCOMPARE(figure->master_stick, old_master);
        QCOMPARE(figure->current_stick, new_master);

        // replicates Widget::on_setMasterNodeBtn_clicked() (widget.cpp:1213)
        view->StoreUndo();
        figure->SetMaster(new_master);

        QCOMPARE(figure->master_stick, new_master);
        QVERIFY(new_master->parent == nullptr);
        QCOMPARE(old_master->parent, new_master);
        // the parent chain between old and new master is inverted (p1/p2 swapped)
        QCOMPARE(old_master->my_line, QLineF(200, 200, 200, 100));
        // the new master keeps its own direction
        QCOMPARE(new_master->my_line, QLineF(200, 200, 200, 300));
        QVERIFY(new_master->children.contains(old_master));
    }

    void AnimationFileRoundTrip()
    {
        AddFrame();
        DrawStick(QPointF(300, 100), QPointF(300, 200));
        QLineF drawn_line = CurrentStickFigure()->stick_list[0]->my_line;

        QList<int> figures_per_frame;
        QList<int> sticks_per_frame;
        for (Frame *f : view->my_animation->frame_list) {
            figures_per_frame << f->stick_figures.count();
            int sticks = 0;
            for (StickFigure *s : f->stick_figures)
                sticks += s->stick_list.count();
            sticks_per_frame << sticks;
        }
        int frame_count = view->my_animation->frame_list.count();
        // i nomi dei frame stanno nel trailer del formato v2 (Animation::operator<<)
        QString frame_name = "keyframe A";
        view->my_animation->frame_list[frame_count - 1]->name = frame_name;

        QString path = QDir::temp().filePath("sam_phase0_test.sam");
        view->my_animation->SaveAnimation(path);

        // replicates the load flow in mymainwindow.cpp: clear undo, load, land on frame 0
        view->ClearUndo();
        view->my_animation->LoadAnimation(path);
        QCOMPARE(view->my_animation->frame_list.count(), frame_count);
        view->MoveToFrame(view->my_animation->frame_list[0]);

        for (int i = 0; i < frame_count; i++) {
            Frame *f = view->my_animation->frame_list[i];
            QCOMPARE(f->stick_figures.count(), figures_per_frame[i]);
            int sticks = 0;
            for (StickFigure *s : f->stick_figures)
                sticks += s->stick_list.count();
            QCOMPARE(sticks, sticks_per_frame[i]);
        }
        Frame *last_frame = view->my_animation->frame_list[frame_count - 1];
        QCOMPARE(last_frame->stick_figures.last()->stick_list[0]->my_line, drawn_line);
        QCOMPARE(last_frame->name, frame_name);

        QFile::remove(path);
    }

    void LibraryRoundTrip()
    {
        AddFrame();
        DrawStick(QPointF(400, 100), QPointF(400, 200));
        StickFigure *source = CurrentStickFigure();
        source->name = "libfig";

        // replicates the add-to-library flow (myview.cpp:1386)
        QVERIFY(view->AddStickFigureToLibrary() != nullptr);
        QCOMPARE(stick_library_buffer->stick_figures.count(), 1);
        QCOMPARE(my_current_library_widget->count(), 1);

        // save + reload the library file: this is the path where a missing placeholder int
        // used to desync the whole byte stream (see Frame's operator<< libFlag comment)
        QString path = QDir::temp().filePath("sam_phase0_test.samlib");
        view->SaveLibrary(path);
        view->LoadLibrary(path);

        QCOMPARE(stick_library_buffer->stick_figures.count(), 1);
        StickFigureData *reloaded = stick_library_buffer->stick_figures[0];
        QCOMPARE(reloaded->name, QString("libfig"));
        QCOMPARE(reloaded->stick_list.count(), 1);
        QCOMPARE(reloaded->stick_list[0]->my_line, source->stick_list[0]->my_line);
        QCOMPARE(my_current_library_widget->count(), 1);

        // the widget entry carries the data record (what the add-to-scene handler casts)
        StickFigureData *from_item = qvariant_cast<StickFigureData*>(
            my_current_library_widget->item(0)->data(Qt::UserRole));
        QCOMPARE(from_item, reloaded);

        QFile::remove(path);
    }

    // ------------------------------------------------------------------------------------
    // Files saved by a NEWER build: every versioned record is refused, never misread.
    // A version above the supported one used to be taken for "the newest I know" and its extra
    // fields were read as the start of the next record, desyncing the stream. These pin that each
    // loader (stick record, .stck, .sam, .samlib) refuses it, says why through file_load_error,
    // and — the part that keeps the program alive — leaves what is open untouched.
    // ------------------------------------------------------------------------------------

    void NewerStickRecordVersionIsRefused()
    {
        StickData newer;
        newer.version = StickData::CURRENT_VERSION + 1;
        QByteArray bytes;
        { QDataStream out(&bytes, QIODevice::WriteOnly); out << newer; }

        file_load_error.clear();
        StickData loaded;
        QDataStream in(bytes);
        in >> loaded;
        QVERIFY(in.status() != QDataStream::Ok);
        QVERIFY(file_load_error.contains(QString::number(StickData::CURRENT_VERSION + 1)));
        QVERIFY(file_load_error.contains(QString::number(StickData::CURRENT_VERSION)));
        file_load_error.clear();

        // the newest version this build knows must still read (counterproof for an off-by-one)
        StickData current;
        QByteArray current_bytes;
        { QDataStream out(&current_bytes, QIODevice::WriteOnly); out << current; }
        QDataStream in_current(current_bytes);
        StickData loaded_current;
        in_current >> loaded_current;
        QCOMPARE(in_current.status(), QDataStream::Ok);
        QVERIFY(file_load_error.isEmpty());
    }

    void NewerStickFigureFileIsRefusedWithoutAnyLiveChange()
    {
        AddFrame();
        DrawStick(QPointF(120, 100), QPointF(120, 200));
        StickFigure *figure = CurrentStickFigure();
        const QString path = QDir::temp().filePath("sam_newer_figure.stck");

        figure->SaveStickFigure(path);
        StickFigureData *ok = LoadStickFigureData(path);
        QVERIFY(ok != nullptr);
        QVERIFY(file_load_error.isEmpty());
        delete ok;

        // the figure record itself newer
        figure->version = StickFigureData::CURRENT_VERSION + 1;
        figure->SaveStickFigure(path);
        figure->version = StickFigureData::CURRENT_VERSION;
        QVERIFY(LoadStickFigureData(path) == nullptr);
        QVERIFY(!file_load_error.isEmpty());

        // one of its STICKS newer, under a figure header that is fine
        figure->stick_list[0]->version = StickData::CURRENT_VERSION + 1;
        figure->SaveStickFigure(path);
        figure->stick_list[0]->version = StickData::CURRENT_VERSION;
        QVERIFY(LoadStickFigureData(path) == nullptr);
        QVERIFY(!file_load_error.isEmpty());

        // an unreadable path is a refusal too, not a crash
        QVERIFY(LoadStickFigureData(QDir::temp().filePath("sam_no_such_dir/x.stck")) == nullptr);
        QVERIFY(!file_load_error.isEmpty());

        // and a refusal must not poison the next, valid load
        figure->SaveStickFigure(path);
        StickFigureData *again = LoadStickFigureData(path);
        QVERIFY(again != nullptr);
        QVERIFY(file_load_error.isEmpty());
        delete again;
        QFile::remove(path);
    }

    void NewerAnimationFileIsRefusedAndTheOpenAnimationIsKept()
    {
        AddFrame();
        DrawStick(QPointF(200, 100), QPointF(200, 220));
        Animation *animation = view->my_animation;
        const QString path = QDir::temp().filePath("sam_newer_animation.sam");

        const QList<Frame*> frames_before = animation->frame_list;
        Frame *current_before = animation->current_frame;
        const int items_before = scene->items().count();
        const qint32 width_before = W;
        auto expect_untouched = [&]{
            QVERIFY(!file_load_error.isEmpty());
            QCOMPARE(animation->frame_list, frames_before);   // same Frame objects, not rebuilt
            QVERIFY(animation->current_frame == current_before);
            QCOMPARE(scene->items().count(), items_before);
            QCOMPARE(W, width_before);
            QCOMPARE(animation->version, Animation::CURRENT_VERSION);
        };

        // 1) the file's own header is newer
        animation->version = Animation::CURRENT_VERSION + 1;
        animation->SaveAnimation(path);
        animation->version = Animation::CURRENT_VERSION;
        QVERIFY(!animation->LoadAnimation(path));
        expect_untouched();

        // 2) the header is fine but one STICK record is newer — what a build that added a stick
        //    field without bumping the file header writes. Only discoverable mid-stream, i.e.
        //    after the point where ClearAnimation() used to have wiped the document.
        StickData *stick = CurrentStickFigure()->stick_list[0];
        stick->version = StickData::CURRENT_VERSION + 1;
        animation->SaveAnimation(path);
        stick->version = StickData::CURRENT_VERSION;
        QVERIFY(!animation->LoadAnimation(path));
        expect_untouched();

        // 3) not a file at all
        QVERIFY(!animation->LoadAnimation(QDir::temp().filePath("sam_no_such_dir/x.sam")));
        expect_untouched();

        // a refusal must not poison the next, valid load
        animation->SaveAnimation(path);
        QVERIFY(animation->LoadAnimation(path));
        QVERIFY(file_load_error.isEmpty());
        QVERIFY(!animation->frame_list.isEmpty());
        view->ReportMissingTextFonts(); // drops the load-time image pool, like the real handler
        view->MoveToFrame(animation->frame_list.last()); // later slots expect AddFrame() to append
        QFile::remove(path);
    }

    void NewerLibraryIsRefusedKeptAndNeverOverwritten()
    {
        AddFrame();
        DrawStick(QPointF(300, 100), QPointF(300, 200));
        QVERIFY(view->AddStickFigureToLibrary("keep me") != nullptr);
        const QString path = QDir::temp().filePath("sam_newer_library.samlib");

        // build a library file whose stick is newer, then put the buffer back to normal
        StickData *library_stick = stick_library_buffer->stick_figures.last()->stick_list[0];
        library_stick->version = StickData::CURRENT_VERSION + 1;
        view->SaveLibrary(path);
        library_stick->version = StickData::CURRENT_VERSION;
        QFile written(path);
        QVERIFY(written.open(QIODevice::ReadOnly));
        const QByteArray newer_bytes = written.readAll();
        written.close();

        FrameData *buffer_before = stick_library_buffer;
        const int figures_before = stick_library_buffer->stick_figures.count();
        const int rows_before = my_current_library_widget->count();

        ModalDismisser dismisser;
        QVERIFY(!view->LoadLibrary(path));
        QTRY_VERIFY(dismisser.dismissed >= 1);          // the user was told
        QVERIFY(stick_library_buffer == buffer_before);  // the open library is exactly as it was
        QCOMPARE(stick_library_buffer->stick_figures.count(), figures_before);
        QCOMPARE(my_current_library_widget->count(), rows_before);

        // "Save current library" writes the buffer to the selected row's file: it must not be
        // allowed to replace the newer file with another library's contents
        const int shown = dismisser.dismissed;
        view->SaveLibrary(path);
        QTRY_VERIFY(dismisser.dismissed > shown);       // and it says so
        QFile after(path);
        QVERIFY(after.open(QIODevice::ReadOnly));
        QCOMPARE(after.readAll(), newer_bytes);
        after.close();

        // a file that loads fine lifts nothing but is itself unaffected by the refusal list
        const QString good = QDir::temp().filePath("sam_good_library.samlib");
        view->SaveLibrary(good);
        QVERIFY(view->LoadLibrary(good));
        QVERIFY(file_load_error.isEmpty());
        QFile::remove(good);
        QFile::remove(path);
    }

    // exercises the exact destructor cascade Widget::~Widget()'s `delete view` triggers
    // (MyView -> Animation -> Frame -> StickFigure -> stick) with an IMAGE stick present,
    // on a throwaway MyView instance — proves removing undoFlag's guard from
    // ~StickFigure() didn't turn app shutdown into a crash: scene->removeItem() and
    // RemoveImagesFromListWidget() now run unconditionally, and this is where they'd
    // fault if `scene`/`img_list_widget` weren't still valid at that point.
    void ViewDestructorHandlesImageStickWithoutCrash()
    {
        MyView *throwaway_view = new MyView();
        throwaway_view->setScene(scene);
        throwaway_view->my_animation->scene = scene;
        Frame *throwaway_frame = throwaway_view->SetUpFrame(0);
        throwaway_view->MoveToFrame(throwaway_frame);
        StickFigure *figure = throwaway_frame->AddStickFigure(my_stick_figure_widget_list);

        StickData *img_stick = new StickData();
        img_stick->my_line = QLineF(50, 50, 50, 150);
        // vedi UndoAddFrameFreesImageStickListWidgetEntries: attacca la view prima di
        // passare a IMAGE, altrimenti UpdateBr(0) nel costruttore deferenzia stick_img==nullptr
        StickGraphicsItem *img_stick_view = FramePresenter::AttachView(img_stick, scene);
        img_stick_view->my_stick_figure = figure;
        img_stick->stick_type = StickData::IMAGE;
        QImage *image = new QImage(8, 8, QImage::Format_ARGB32);
        image->fill(Qt::green);
        img_stick_view->AddImage(image, "shutdown-check");
        figure->stick_list.append(img_stick);
        figure->master_stick = img_stick;
        figure->current_stick = img_stick;
        QCOMPARE(img_list_widget->count(), 1);

        delete throwaway_view; // same cascade as Widget::~Widget()'s `delete view`
        QCOMPARE(img_list_widget->count(), 0);
    }

    // Prep A for REFACTOR_PLAN.md Phase 2c: synthetic QTest mouse events driven through the
    // REAL mousePressEvent/mouseMoveEvent/mouseReleaseEvent overrides, not the DrawStick()/
    // StoreUndo() helpers used elsewhere that call model methods directly. This is the one
    // code path (interactive drawing/dragging) no other test in this suite exercises, and
    // exactly the surface Phase 2c's StickGraphicsItem/FramePresenter split touches most.
    //
    // view->mapFromScene(...) is used instead of hand-computed pixel coordinates because the
    // scene-to-viewport transform depends on QGraphicsView's default centering/alignment,
    // which this test doesn't need to (and shouldn't) hardcode.
    void DrawViaRealMouseEvents()
    {
        AddFrame();
        view->resize(800, 600);
        view->SetTool(DRAW);
        int sticks_before = CurrentStickFigure()->stick_list.count();

        QPoint p1 = view->mapFromScene(QPointF(100, 100));
        QPoint p2 = view->mapFromScene(QPointF(100, 200));
        QTest::mouseClick(view->viewport(), Qt::LeftButton, Qt::NoModifier, p1);
        QTest::mouseClick(view->viewport(), Qt::LeftButton, Qt::NoModifier, p2);

        QCOMPARE(CurrentStickFigure()->stick_list.count(), sticks_before + 1);
        StickData *drawn = CurrentStickFigure()->stick_list.last();
        QVERIFY(drawn->view && scene->items().contains(drawn->view));
        QCOMPARE(drawn->stick_type, (int)StickData::LINE);
        // regression: a brand-new figure's first stick (= its master) used to keep the
        // StickGraphicsItem constructor's default red handle brush forever, since
        // DrawCmd() never painted it green the way StartDrawing()'s own doc comment
        // says it should - only a later, unrelated AddHandles() call (SetMaster/merge/
        // split/paste) happened to fix it up
        QCOMPARE(drawn, CurrentStickFigure()->master_stick);
        QCOMPARE(drawn->view->stick_handle[0]->brush().color(), QColor(Qt::green));

        view->SetTool(NOTOOL);
    }

    // exercises a real drag gesture (mousePress + several mouseMove + mouseRelease) through
    // the NOTOOL/CLAMP rotate path, then undo/redo — the multi-event-gesture shape
    // FrameUndoCommand's design depends on (StoreUndo() captures "before" at press time;
    // "after" is captured lazily on the first undo — see myview.cpp), now driven by real
    // QTest-synthesized events end to end instead of calling the model methods directly.
    void DragRotateViaRealMouseEventsIsUndoable()
    {
        AddFrame();
        view->resize(800, 600);
        DrawStick(QPointF(400, 100), QPointF(400, 200));
        int figure_index = CurrentFrame()->stick_figures.indexOf(CurrentStickFigure());
        QLineF original_line = CurrentStickFigure()->stick_list[0]->my_line;

        view->SetTool(NOTOOL);
        QTest::mousePress(view->viewport(), Qt::LeftButton, Qt::NoModifier,
                           view->mapFromScene(QPointF(400, 200)));
        for (int i = 1; i <= 5; i++) {
            QTest::mouseMove(view->viewport(),
                              view->mapFromScene(QPointF(400 + i * 10, 200 - i * 5)));
        }
        QTest::mouseRelease(view->viewport(), Qt::LeftButton, Qt::NoModifier,
                             view->mapFromScene(QPointF(450, 175)));

        QLineF dragged_line = CurrentFrame()->stick_figures[figure_index]->stick_list[0]->my_line;
        QVERIFY(dragged_line != original_line);

        view->undo_stack.undo();
        QCOMPARE(CurrentFrame()->stick_figures[figure_index]->stick_list[0]->my_line, original_line);

        view->undo_stack.redo();
        QCOMPARE(CurrentFrame()->stick_figures[figure_index]->stick_list[0]->my_line, dragged_line);

        view->SetTool(NOTOOL);
    }

    // The texture arrow handle: the on-canvas editor for a texture's offset/angle/scale, which
    // before it existed had no handle at all. Driven through the REAL mouse events, because the
    // interesting half is the pick PRIORITY — the spec puts the arrow below the transformation
    // box and below the stick's own endpoint handles, so a press has to fall through to the
    // ordinary rotate whenever the arrow lies over one of those.
    void TextureArrowDragsOffsetAngleAndScale()
    {
        AddFrame();
        view->resize(800, 600);
        // uno stick RECT lontano dal centro, con un riempimento a texture da manovrare
        DrawStick(QPointF(300, 300), QPointF(300, 400));
        StickData *stick = CurrentStickFigure()->stick_list.last();
        stick->stick_type = StickData::RECT;
        QImage tile(20, 20, QImage::Format_ARGB32);
        tile.fill(Qt::yellow);
        stick->brush = QBrush(tile);
        CurrentStickFigure()->current_stick = stick;
        view->SetTool(NOTOOL);

        // spenta, la freccia non esiste: né disegnata né afferrabile
        QPointF tail, tip;
        view->show_fill_texture_arrow = false;
        QVERIFY(!view->TextureArrowPoints(true, &tail, &tip));

        view->show_fill_texture_arrow = true;
        QVERIFY(view->TextureArrowPoints(true, &tail, &tip));
        // offset 0 -> coda ACCANTO allo stick, non su P1 né sul punto medio nudo: la freccia si
        // afferra per ULTIMA, quindi una coda su una maniglia di stick — o, per uno stick
        // diritto il cui bounding box è degenere, su una maniglia laterale della transformation
        // box, che gli cade addosso — resterebbe coperta per sempre e l'offset non si potrebbe
        // mai trascinare. È la proprietà a contare, non la formula: coda E punta devono stare
        // fuori dal raggio di presa di tutto ciò che ha priorità più alta.
        QCOMPARE(tail, view->TextureArrowAnchor(stick, 0.0));
        for(const QPointF &grab : { stick->my_line.p1(), stick->my_line.p2(),
                                    stick->my_line.pointAt(0.5) }){
            QVERIFY(QLineF(tail, grab).length() > 10.0);  // 10 == handle_size, il raggio di presa
            QVERIFY(QLineF(tip, grab).length() > 10.0);
        }
        // scala 1 e tile da 20 px -> la freccia è lunga esattamente una tile
        QVERIFY(qAbs(QLineF(tail, tip).length() - 20.0) < 1e-6);
        // il contorno è una tinta unita: nessuna seconda freccia
        QVERIFY(!view->TextureArrowPoints(false, &tail, &tip));

        // --- trascina la PUNTA: angolo e scala insieme
        const int undo_before = view->undo_stack.index();
        QTest::mousePress(view->viewport(), Qt::LeftButton, Qt::NoModifier,
                          view->mapFromScene(tip));
        QTest::mouseMove(view->viewport(), view->mapFromScene(tip + QPointF(30, 30)));
        QTest::mouseRelease(view->viewport(), Qt::LeftButton, Qt::NoModifier,
                            view->mapFromScene(tip + QPointF(30, 30)));
        TextureXf dragged = ReadTextureXf(stick->brush);
        QVERIFY(dragged.scale > 1.5);                   // allungata
        QVERIFY(qAbs(dragged.angle) > 5.0);             // e ruotata
        QCOMPARE(dragged.offset, QPointF(0, 0));        // la punta non tocca l'offset
        // tutto il trascinamento è UN solo passo di undo (StoreUndo() una volta, al press)
        QCOMPARE(view->undo_stack.index(), undo_before + 1);
        view->undo_stack.undo();
        QVERIFY(qAbs(ReadTextureXf(CurrentStickFigure()->stick_list.last()->brush).scale - 1.0) < 1e-6);
        view->undo_stack.redo();
        stick = CurrentStickFigure()->stick_list.last();

        // --- trascina la CODA: solo l'offset
        CurrentStickFigure()->current_stick = stick;
        QVERIFY(view->TextureArrowPoints(true, &tail, &tip));
        const TextureXf before_move = ReadTextureXf(stick->brush);
        QTest::mousePress(view->viewport(), Qt::LeftButton, Qt::NoModifier,
                          view->mapFromScene(tail));
        QTest::mouseMove(view->viewport(), view->mapFromScene(tail + QPointF(-40, 25)));
        QTest::mouseRelease(view->viewport(), Qt::LeftButton, Qt::NoModifier,
                            view->mapFromScene(tail + QPointF(-40, 25)));
        const TextureXf moved = ReadTextureXf(CurrentStickFigure()->stick_list.last()->brush);
        QVERIFY(QLineF(moved.offset, before_move.offset).length() > 20.0);
        QVERIFY(qAbs(moved.scale - before_move.scale) < 1e-6); // la coda non tocca la scala
        QVERIFY(qAbs(moved.angle - before_move.angle) < 1e-6); // né l'angolo

        // --- priorità: con la coda spinta SOPRA una maniglia dello stick, il press NON deve
        // afferrare la freccia — deve cadere nella rotazione normale, come se la freccia non
        // ci fosse. Frame nuovo, così il gesto parte dallo stesso stato pulito di
        // DragRotateViaRealMouseEventsIsUndoable() invece che da quello lasciato dai due
        // trascinamenti qui sopra.
        AddFrame();
        DrawStick(QPointF(500, 200), QPointF(500, 300));
        stick = CurrentStickFigure()->stick_list.last();
        stick->stick_type = StickData::RECT;
        QBrush on_handle(tile);
        TextureXf at_p2;
        at_p2.offset = stick->my_line.p2() - view->TextureArrowAnchor(stick, 0.0);
        WriteTextureXf(&on_handle, at_p2);
        stick->brush = on_handle;
        CurrentStickFigure()->current_stick = stick;
        view->SetTool(NOTOOL);
        QVERIFY(view->TextureArrowPoints(true, &tail, &tip));
        QCOMPARE(tail, stick->my_line.p2());
        const QLineF line_before = stick->my_line;
        QTest::mousePress(view->viewport(), Qt::LeftButton, Qt::NoModifier,
                          view->mapFromScene(tail));
        // più di un movimento: la rotazione normale si INIZIALIZZA sul primo (come in
        // DragRotateViaRealMouseEventsIsUndoable), quindi con uno solo lo stick non gira e il
        // test non distinguerebbe "priorità rispettata" da "il press non ha fatto nulla"
        for(int i = 1; i <= 5; i++)
            QTest::mouseMove(view->viewport(), view->mapFromScene(tail + QPointF(9*i, -4*i)));
        QTest::mouseRelease(view->viewport(), Qt::LeftButton, Qt::NoModifier,
                            view->mapFromScene(tail + QPointF(45, -20)));
        StickData *after = CurrentStickFigure()->stick_list.last();
        QCOMPARE(ReadTextureXf(after->brush).offset, at_p2.offset); // la texture non si è mossa
        QVERIFY(after->my_line != line_before);                     // lo stick sì
        view->show_fill_texture_arrow = false;
        view->SetTool(NOTOOL);
    }

    // Full DRAWPOLY gesture through the real mouse/keyboard event overrides: two clicks
    // for the base line, three vertex clicks, Enter to finish. Also pins the per-vertex
    // undo granularity (each vertex click is its own StoreUndo()) and that stored
    // vertices are normalized-local (their scene mapping matches the click position).
    void DrawPolygonViaRealMouseEvents()
    {
        AddFrame();
        view->resize(800, 600);
        view->SetTool(DRAWPOLY);
        int sticks_before = CurrentStickFigure()->stick_list.count();

        QTest::mouseClick(view->viewport(), Qt::LeftButton, Qt::NoModifier,
                          view->mapFromScene(QPointF(100, 100)));
        QTest::mouseClick(view->viewport(), Qt::LeftButton, Qt::NoModifier,
                          view->mapFromScene(QPointF(100, 200)));
        QCOMPARE(CurrentStickFigure()->stick_list.count(), sticks_before + 1);
        StickData *drawn = CurrentStickFigure()->stick_list.last();
        QCOMPARE(drawn->stick_type, (qint32)StickData::POLYGON);

        QTest::mouseClick(view->viewport(), Qt::LeftButton, Qt::NoModifier,
                          view->mapFromScene(QPointF(200, 150)));
        QTest::mouseClick(view->viewport(), Qt::LeftButton, Qt::NoModifier,
                          view->mapFromScene(QPointF(250, 250)));
        QTest::mouseClick(view->viewport(), Qt::LeftButton, Qt::NoModifier,
                          view->mapFromScene(QPointF(150, 300)));
        QCOMPARE(drawn->polygon_points.count(), 3);

        // i vertici sono in coordinate locali normalizzate: rimappati in scena devono
        // ricadere dove si è cliccato
        QPointF first_vertex_scene = drawn->PolygonTransform().map(drawn->polygon_points[0]);
        QVERIFY(QLineF(first_vertex_scene, QPointF(200, 150)).length() < 0.01);

        QTest::keyClick(view, Qt::Key_Return); // chiude la sessione

        // granularità per-vertice: un undo toglie SOLO l'ultimo vertice (il frame viene
        // ricostruito dallo snapshot, quindi lo stick va ripescato dalla lista)
        view->undo_stack.undo();
        StickData *restored = CurrentFrame()->stick_figures.last()->stick_list.last();
        QCOMPARE(restored->stick_type, (qint32)StickData::POLYGON);
        QCOMPARE(restored->polygon_points.count(), 2);

        view->undo_stack.redo();
        restored = CurrentFrame()->stick_figures.last()->stick_list.last();
        QCOMPARE(restored->polygon_points.count(), 3);

        view->SetTool(NOTOOL);
    }

    // Group D bug 1: clicking an existing polygon's edge with DRAWPOLY adds exactly ONE
    // vertex and does NOT leave an extend-session open (it used to; the user's next click
    // then silently added a second vertex -> "two vertices per click", polygons only).
    void ExistingPolygonEdgeClickAddsOneVertexNoSession()
    {
        AddFrame();
        view->resize(800, 600);
        view->SetTool(DRAWPOLY);
        QTest::mouseClick(view->viewport(), Qt::LeftButton, Qt::NoModifier, view->mapFromScene(QPointF(100, 100)));
        QTest::mouseClick(view->viewport(), Qt::LeftButton, Qt::NoModifier, view->mapFromScene(QPointF(100, 200)));
        QTest::mouseClick(view->viewport(), Qt::LeftButton, Qt::NoModifier, view->mapFromScene(QPointF(200, 150)));
        QTest::mouseClick(view->viewport(), Qt::LeftButton, Qt::NoModifier, view->mapFromScene(QPointF(250, 250)));
        QTest::mouseClick(view->viewport(), Qt::LeftButton, Qt::NoModifier, view->mapFromScene(QPointF(150, 300)));
        QTest::keyClick(view, Qt::Key_Return); // chiude la sessione di disegno
        StickData *poly = CurrentStickFigure()->stick_list.last();
        QCOMPARE(poly->polygon_points.count(), 3);

        // click on the edge midpoint between vertex0 (200,150) and vertex1 (250,250)
        QTest::mouseClick(view->viewport(), Qt::LeftButton, Qt::NoModifier, view->mapFromScene(QPointF(225, 200)));
        QCOMPARE(poly->polygon_points.count(), 4); // esattamente un vertice aggiunto

        // NESSUNA sessione aperta: un click lontano NON deve aggiungere un secondo vertice
        // al poligono (avvia semmai un nuovo poligono, che è un altro stick)
        QTest::mouseClick(view->viewport(), Qt::LeftButton, Qt::NoModifier, view->mapFromScene(QPointF(600, 500)));
        QCOMPARE(poly->polygon_points.count(), 4);

        view->SetTool(NOTOOL);
    }

    // DRAWMULTI end to end: every click extends the path (first/last click = P1/P2,
    // the clicks between become interior vertices), Enter finalizes, Esc cancels the
    // whole stick (deleting it, since by then it is already committed to the figure)
    void DrawMultilineViaRealMouseEvents()
    {
        AddFrame();
        view->resize(800, 600);
        view->SetTool(DRAWMULTI);
        int sticks_before = CurrentStickFigure()->stick_list.count();

        QTest::mouseClick(view->viewport(), Qt::LeftButton, Qt::NoModifier,
                          view->mapFromScene(QPointF(100, 100)));
        QTest::mouseClick(view->viewport(), Qt::LeftButton, Qt::NoModifier,
                          view->mapFromScene(QPointF(180, 120)));
        QTest::mouseClick(view->viewport(), Qt::LeftButton, Qt::NoModifier,
                          view->mapFromScene(QPointF(240, 80)));
        QTest::mouseClick(view->viewport(), Qt::LeftButton, Qt::NoModifier,
                          view->mapFromScene(QPointF(300, 140)));
        QTest::keyClick(view, Qt::Key_Return);

        QCOMPARE(CurrentStickFigure()->stick_list.count(), sticks_before + 1);
        StickData *drawn = CurrentStickFigure()->stick_list.last();
        QCOMPARE(drawn->stick_type, (qint32)StickData::MULTILINE);
        QCOMPARE(drawn->polygon_points.count(), 2); // i click intermedi
        QCOMPARE(drawn->vertex_widths.count(), 4);
        QVERIFY(QLineF(drawn->my_line.p1(), QPointF(100, 100)).length() < 0.01);
        QVERIFY(QLineF(drawn->my_line.p2(), QPointF(300, 140)).length() < 0.01);
        // il secondo click è diventato un vertice interno, fermo dove si è cliccato
        QPointF first_interior = drawn->PolygonTransform().map(drawn->polygon_points[0]);
        QVERIFY(QLineF(first_interior, QPointF(180, 120)).length() < 0.01);

        // Esc a metà disegno: lo stick già committato viene eliminato del tutto
        int count_before_cancel = CurrentStickFigure()->stick_list.count();
        QTest::mouseClick(view->viewport(), Qt::LeftButton, Qt::NoModifier,
                          view->mapFromScene(QPointF(400, 100)));
        QTest::mouseClick(view->viewport(), Qt::LeftButton, Qt::NoModifier,
                          view->mapFromScene(QPointF(450, 150)));
        QTest::mouseClick(view->viewport(), Qt::LeftButton, Qt::NoModifier,
                          view->mapFromScene(QPointF(500, 100)));
        QTest::keyClick(view, Qt::Key_Escape);
        QCOMPARE(CurrentStickFigure()->stick_list.count(), count_before_cancel);

        view->SetTool(NOTOOL);
    }

    // DRAWMULTI branches, end to end: a click on a vertex handle makes the next clicks grow
    // a branch from that vertex — mid-session, and on a finished line when the handle is
    // clicked without being dragged (a drag still moves the vertex, and leaves no session
    // open). Rendered, the branch hangs from its vertex: drawn as the flat chain, the line
    // would run a fake edge from the branch tip to P2 through the "unpainted" probe
    void DrawBranchedMultilineViaRealMouseEvents()
    {
        AddFrame();
        view->resize(800, 600);
        view->SetTool(DRAWMULTI);
        auto click = [this](QPointF p) {
            QTest::mouseClick(view->viewport(), Qt::LeftButton, Qt::NoModifier, view->mapFromScene(p));
        };
        auto index_of = [](const QPolygonF &v, QPointF p) {
            for (int i = 0; i < v.count(); i++)
                if (QLineF(v[i], p).length() < 0.01) return i;
            return -1;
        };
        auto top_command = [this]() { return view->undo_stack.command(view->undo_stack.index() - 1); };

        click(QPointF(100, 100));
        click(QPointF(200, 100));
        click(QPointF(300, 100));
        click(QPointF(200, 100)); // maniglia del vertice interno: origine del ramo, nessun dato toccato
        click(QPointF(200, 200));
        click(QPointF(200, 300)); // il ramo prosegue dal suo ultimo punto
        QTest::keyClick(view, Qt::Key_Return);

        StickData *ml = CurrentStickFigure()->stick_list.last();
        QCOMPARE(ml->stick_type, (qint32)StickData::MULTILINE);
        QVERIFY(QLineF(ml->my_line.p1(), QPointF(100, 100)).length() < 0.01);
        QVERIFY(QLineF(ml->my_line.p2(), QPointF(300, 100)).length() < 0.01);
        QPolygonF verts = ml->view->SceneVertices();
        QCOMPARE(verts.count(), 5);
        QCOMPARE(ml->vertex_widths.count(), 5);
        QList<qint32> parents = ml->MultilineParents();
        int junction = index_of(verts, QPointF(200, 100));
        int b1 = index_of(verts, QPointF(200, 200));
        int b2 = index_of(verts, QPointF(200, 300));
        QVERIFY(junction > 0 && b1 > 0 && b2 > 0);
        QCOMPARE(parents[junction], 0);
        QCOMPARE(parents[verts.count() - 1], junction); // main line: P1 -> (200,100) -> P2
        QCOMPARE(parents[b1], junction);
        QCOMPARE(parents[b2], b1);

        {
            StickData copy;
            copy.CopyFrom(*ml);
            copy.pen = QPen(Qt::black, 8, Qt::SolidLine, Qt::FlatCap);
            copy.vertex_widths = {8, 8, 8, 8, 8};
            QGraphicsScene render_scene;
            render_scene.setSceneRect(0, 0, 400, 400);
            render_scene.addItem(new StickGraphicsItem(&copy)); // la scena possiede la view
            QImage img(400, 400, QImage::Format_ARGB32);
            img.fill(Qt::white);
            QPainter painter(&img);
            render_scene.render(&painter, QRectF(0, 0, 400, 400), QRectF(0, 0, 400, 400));
            painter.end();
            auto painted = [&img](int x, int y) { return img.pixelColor(x, y) != QColor(Qt::white); };
            QVERIFY2(painted(200, 250), "the branch is not drawn");
            QVERIFY2(painted(150, 100) && painted(250, 100), "the main line is not drawn");
            QVERIFY2(!painted(250, 200), "drawn as a flat chain: a fake edge runs from the branch tip to P2");
        }

        // one undo removes the last branch point, redo brings it back
        view->undo_stack.undo();
        ml = CurrentStickFigure()->stick_list.last();
        QCOMPARE(ml->view->SceneVertices().count(), 4);
        QCOMPARE(index_of(ml->view->SceneVertices(), QPointF(200, 300)), -1);
        view->undo_stack.redo();
        ml = CurrentStickFigure()->stick_list.last();
        QCOMPARE(ml->view->SceneVertices().count(), 5);

        // finished line: clicking the P1 handle without dragging reopens the session (and
        // takes no undo step), the next click branches from P1, Esc keeps the line
        int sticks = CurrentStickFigure()->stick_list.count();
        const QUndoCommand *top = top_command();
        click(QPointF(100, 100));
        QCOMPARE(top_command(), top);
        click(QPointF(100, 200));
        QTest::keyClick(view, Qt::Key_Escape);
        QCOMPARE(CurrentStickFigure()->stick_list.count(), sticks);
        ml = CurrentStickFigure()->stick_list.last();
        verts = ml->view->SceneVertices();
        QCOMPARE(verts.count(), 6);
        int b3 = index_of(verts, QPointF(100, 200));
        QVERIFY(b3 > 0);
        QCOMPARE(ml->MultilineParents()[b3], 0);
        QVERIFY(QLineF(ml->my_line.p2(), QPointF(300, 100)).length() < 0.01);

        // a real drag still moves the vertex, as one undo step, and opens no session: a
        // click far away then starts a NEW line instead of adding to this one
        QTest::mousePress(view->viewport(), Qt::LeftButton, Qt::NoModifier, view->mapFromScene(QPointF(200, 200)));
        QTest::mouseMove(view->viewport(), view->mapFromScene(QPointF(230, 230)));
        QTest::mouseMove(view->viewport(), view->mapFromScene(QPointF(260, 240)));
        QTest::mouseRelease(view->viewport(), Qt::LeftButton, Qt::NoModifier, view->mapFromScene(QPointF(260, 240)));
        verts = ml->view->SceneVertices();
        QVERIFY(index_of(verts, QPointF(260, 240)) > 0);
        QCOMPARE(index_of(verts, QPointF(200, 200)), -1);
        QVERIFY(top_command() != top);
        click(QPointF(600, 500));
        QCOMPARE(ml->view->SceneVertices().count(), 6);
        QTest::keyClick(view, Qt::Key_Escape); // annulla la nuova linea appena iniziata
        QCOMPARE(CurrentStickFigure()->stick_list.count(), sticks);

        view->SetTool(NOTOOL);
    }

    // Deleting multiline vertices, end to end: Alt+click works mid-session too (it used to
    // be ignored there, and a plain handle click reopens a session, so it looked broken),
    // a vertex takes the branches growing out of it along (branches of branches too), and
    // Ctrl-selected vertices go with the Delete key — the whole stick once one vertex would
    // be left
    void DeleteMultilineVerticesWithAltAndDeleteKey()
    {
        AddFrame();
        view->resize(800, 600);
        view->SetTool(DRAWMULTI);
        auto click = [this](QPointF p, Qt::KeyboardModifiers mods = Qt::NoModifier) {
            QTest::mouseClick(view->viewport(), Qt::LeftButton, mods, view->mapFromScene(p));
        };
        auto index_of = [](const QPolygonF &v, QPointF p) {
            for (int i = 0; i < v.count(); i++)
                if (QLineF(v[i], p).length() < 0.01) return i;
            return -1;
        };
        int sticks = CurrentStickFigure()->stick_list.count();
        click(QPointF(100, 100));
        click(QPointF(200, 100));
        click(QPointF(300, 100));
        click(QPointF(200, 100)); // branch from J...
        click(QPointF(200, 200));
        click(QPointF(200, 300));
        click(QPointF(200, 200)); // ...and a branch of the branch, from B1
        click(QPointF(260, 200));
        StickData *ml = CurrentStickFigure()->stick_list.last();
        QCOMPARE(ml->view->SceneVertices().count(), 6);

        // mid-session Alt+click on B1: B1, the rest of its branch and its own branch go
        click(QPointF(200, 200), Qt::AltModifier);
        QPolygonF verts = ml->view->SceneVertices();
        QCOMPARE(verts.count(), 3);
        for (QPointF gone : {QPointF(200, 200), QPointF(200, 300), QPointF(260, 200)})
            QCOMPARE(index_of(verts, gone), -1);
        // the session is still open and resumes the main line
        click(QPointF(400, 100));
        QTest::keyClick(view, Qt::Key_Return);
        QVERIFY(QLineF(ml->my_line.p2(), QPointF(400, 100)).length() < 0.01);
        QCOMPARE(ml->view->SceneVertices().count(), 4);

        // finished line: Alt+click on J closes the main line over it
        click(QPointF(200, 100), Qt::AltModifier);
        verts = ml->view->SceneVertices();
        QCOMPARE(verts.count(), 3);
        QCOMPARE(index_of(verts, QPointF(200, 100)), -1);

        // Ctrl-select + Delete: one vertex...
        view->SetTool(NOTOOL);
        click(QPointF(300, 100), Qt::ControlModifier);
        QCOMPARE(ml->view->selected_vertices.count(), 1);
        QTest::keyClick(view, Qt::Key_Delete);
        QCOMPARE(CurrentStickFigure()->stick_list.count(), sticks + 1); // a vertex, not the stick
        QCOMPARE(ml->view->SceneVertices().count(), 2);
        QVERIFY(ml->view->selected_vertices.isEmpty());
        // ...one undo brings it back
        view->undo_stack.undo();
        ml = CurrentStickFigure()->stick_list.last();
        QCOMPARE(ml->view->SceneVertices().count(), 3);
        // two vertices of a three-vertex line: one would be left, so the stick goes whole
        click(QPointF(300, 100), Qt::ControlModifier);
        click(QPointF(400, 100), Qt::ControlModifier | Qt::ShiftModifier);
        QCOMPARE(ml->view->selected_vertices.count(), 2);
        QTest::keyClick(view, Qt::Key_Delete);
        QCOMPARE(CurrentStickFigure()->stick_list.count(), sticks);
    }

    // The "Vertex cap" choice (MyView::SetVertexCap(), what the multiline panel's combo
    // calls): the vertices drawn next get it, every vertex a branch involves comes out round
    // whatever it is, and with vertices Ctrl-selected it also changes theirs, as one undo step
    void MultilineVertexCapDrawingAndSelection()
    {
        struct Restore { MyView *v; ~Restore() { v->my_vertex_cap = StickData::CAP_ROUND; } } restore{view};
        AddFrame();
        view->resize(800, 600);
        auto click = [this](QPointF p, Qt::KeyboardModifiers mods = Qt::NoModifier) {
            QTest::mouseClick(view->viewport(), Qt::LeftButton, mods, view->mapFromScene(p));
        };
        const qint32 S = StickData::CAP_SHARP, R = StickData::CAP_ROUND, B = StickData::CAP_BEVEL;
        view->SetVertexCap(S);
        view->SetTool(DRAWMULTI);
        click(QPointF(100, 100));
        click(QPointF(200, 100));
        click(QPointF(300, 150));
        QTest::keyClick(view, Qt::Key_Return);
        StickData *ml = CurrentStickFigure()->stick_list.last();
        QCOMPARE(ml->MultilineCaps(), (QList<qint32>{S, S, S}));

        // a branch from J: J and the branch vertex round, the rest untouched
        click(QPointF(200, 100)); // handle click without drag: reopens the session from J
        click(QPointF(200, 200));
        QTest::keyClick(view, Qt::Key_Return);
        QCOMPARE(ml->MultilineCaps(), (QList<qint32>{S, R, R, S}));

        // Ctrl-select P2 and pick Bevel: P2 changes, one undo puts it back
        view->SetTool(NOTOOL);
        click(QPointF(300, 150), Qt::ControlModifier);
        QCOMPARE(ml->view->selected_vertices.count(), 1);
        view->SetVertexCap(B);
        QCOMPARE(ml->MultilineCaps(), (QList<qint32>{S, R, R, B}));
        view->undo_stack.undo();
        ml = CurrentStickFigure()->stick_list.last();
        QCOMPARE(ml->MultilineCaps(), (QList<qint32>{S, R, R, S}));
        QCOMPARE(view->my_vertex_cap, B); // the choice itself is not undone: it is the drawing setting

        // and the next line is drawn with it
        view->SetTool(DRAWMULTI);
        click(QPointF(100, 400));
        click(QPointF(200, 400));
        click(QPointF(300, 450));
        QTest::keyClick(view, Qt::Key_Return);
        QCOMPARE(CurrentStickFigure()->stick_list.last()->MultilineCaps(), (QList<qint32>{B, B, B}));
        view->SetTool(NOTOOL);
    }

    void ChangeMasterKeepsPolygonShape()   { ChangeMasterKeepsVertexShape(StickData::POLYGON); }
    void ChangeMasterKeepsMultilineShape() { ChangeMasterKeepsVertexShape(StickData::MULTILINE); }

    // Group D bug 3: the scale tool must scale a MULTILINE's per-vertex thickness
    // (vertex_widths) along with its shape, and a POLYGON's outline pen. Replicates the
    // buffers mousePressEvent's SCALE case caches, then calls StickGraphicsItem::Scale().
    void ScaleToolScalesMultilineThicknessAndPolygonPen()
    {
        AddFrame();
        view->resize(800, 600);
        view->SetTool(DRAWMULTI);
        QTest::mouseClick(view->viewport(), Qt::LeftButton, Qt::NoModifier, view->mapFromScene(QPointF(100, 100)));
        QTest::mouseClick(view->viewport(), Qt::LeftButton, Qt::NoModifier, view->mapFromScene(QPointF(180, 120)));
        QTest::mouseClick(view->viewport(), Qt::LeftButton, Qt::NoModifier, view->mapFromScene(QPointF(300, 140)));
        QTest::keyClick(view, Qt::Key_Return);
        StickData *ml = CurrentStickFigure()->stick_list.last();
        QCOMPARE(ml->stick_type, (qint32)StickData::MULTILINE);
        QList<float> widths_before = ml->vertex_widths;
        QVERIFY(!widths_before.isEmpty());

        // as mousePressEvent's SCALE case does before a scale drag
        ml->view->scale_buffer = ml->my_line.length();
        ml->view->width_buffer = ml->pen.widthF();
        ml->view->Scale(2.0f);
        for (int i = 0; i < widths_before.count(); i++)
            QVERIFY2(qAbs(ml->vertex_widths[i] - widths_before[i] * 2.0f) < 0.01f,
                     "multiline vertex thickness did not scale");

        view->SetTool(NOTOOL);
        // POLYGON outline is data->pen, which Scale() already scaled before this fix
        view->SetTool(DRAWPOLY);
        QTest::mouseClick(view->viewport(), Qt::LeftButton, Qt::NoModifier, view->mapFromScene(QPointF(400, 100)));
        QTest::mouseClick(view->viewport(), Qt::LeftButton, Qt::NoModifier, view->mapFromScene(QPointF(400, 200)));
        QTest::mouseClick(view->viewport(), Qt::LeftButton, Qt::NoModifier, view->mapFromScene(QPointF(450, 150)));
        QTest::mouseClick(view->viewport(), Qt::LeftButton, Qt::NoModifier, view->mapFromScene(QPointF(500, 250)));
        QTest::mouseClick(view->viewport(), Qt::LeftButton, Qt::NoModifier, view->mapFromScene(QPointF(420, 300)));
        QTest::keyClick(view, Qt::Key_Return);
        StickData *pg = CurrentStickFigure()->stick_list.last();
        QCOMPARE(pg->stick_type, (qint32)StickData::POLYGON);
        float pen_before = pg->pen.widthF();
        pg->view->scale_buffer = pg->my_line.length();
        pg->view->width_buffer = pg->pen.widthF();
        pg->view->Scale(2.0f);
        QVERIFY2(qAbs(pg->pen.widthF() - pen_before * 2.0f) < 0.01f, "polygon outline pen did not scale");

        view->SetTool(NOTOOL);
    }

    // multi-selection plumbing: selection set + center handle, global color change on
    // every selected figure without breaking the selection, and undo restoring both
    // the model AND the selection (tracked as row indices inside FrameUndoCommand)
    void MultiSelectionOperations()
    {
        AddFrame();
        DrawStick(QPointF(100, 100), QPointF(100, 200));
        StickFigure *fig_a = CurrentStickFigure();
        AddStick();
        DrawStick(QPointF(300, 100), QPointF(300, 200));
        StickFigure *fig_b = CurrentStickFigure();
        QVERIFY(fig_a != fig_b);

        view->SetMultiSelectionList(QList<StickFigure*>() << fig_a << fig_b);
        QVERIFY(view->MultiSelectionActive());
        QPointF center = view->MultiSelectionCenter();
        QVERIFY(QLineF(center, QPointF(200, 150)).length() < 0.01);

        QColor original = fig_a->stick_list[0]->pen.color();
        view->my_pen.setColor(Qt::red);
        view->SetGraphics(true, ATTRIBUTE_PENCOLOR);
        QCOMPARE(fig_a->stick_list[0]->pen.color(), QColor(Qt::red));
        QCOMPARE(fig_b->stick_list[0]->pen.color(), QColor(Qt::red));
        QVERIFY(view->MultiSelectionActive()); // l'operazione non rompe la selezione

        view->undo_stack.undo();
        // il frame è stato ricostruito: ripesca le figure per indice
        StickFigure *restored_a = CurrentFrame()->stick_figures[CurrentFrame()->stick_figures.count()-2];
        QCOMPARE(restored_a->stick_list[0]->pen.color(), original);
        QVERIFY(view->MultiSelectionActive()); // la selezione è tracciata dall'undo

        view->ClearMultiSelection();
        QVERIFY(!view->MultiSelectionActive());
        view->SetTool(NOTOOL);
    }

    // ============================= DRAW_IK (inverse kinematics) =============================
    // helper: a 3-stick straight chain master(100,100->100,200) -> child(->100,300) ->
    // grandchild(->100,400), each DrawStick() call's `from` matching the previous
    // stick's `to` so SelectOrigin() attaches it as a proper (non-stepchild) child
    void BuildIkChainFixture(StickData **master, StickData **child, StickData **grandchild)
    {
        AddFrame();
        *master = DrawStick(QPointF(100, 100), QPointF(100, 200));
        *child = DrawStick(QPointF(100, 200), QPointF(100, 300));
        *grandchild = DrawStick(QPointF(100, 300), QPointF(100, 400));
    }

    // commits master..grandchild as a chain without going through the 2-click UI flow;
    // sets the start role first, matching the real order (mousePressEvent's first
    // click sets IK_START before the second click calls CommitIkChain())
    void CommitIkChainFixture(StickData *master, StickData *grandchild)
    {
        master->ik_role = StickData::IK_START;
        CommitIkChain(master, grandchild);
    }

    // 2 real clicks: near the master's start handle, then on the grandchild — commits
    // a chain covering all 3 sticks with the roles the spec describes
    void DrawIkChainViaRealMouseEvents()
    {
        StickData *master, *child, *grandchild;
        BuildIkChainFixture(&master, &child, &grandchild);
        view->resize(800, 600);
        view->SetTool(DRAW_IK);

        QTest::mouseClick(view->viewport(), Qt::LeftButton, Qt::NoModifier,
                          view->mapFromScene(QPointF(100, 100))); // master's start handle
        QTest::mouseClick(view->viewport(), Qt::LeftButton, Qt::NoModifier,
                          view->mapFromScene(QPointF(100, 350))); // somewhere on the grandchild segment

        QCOMPARE(master->ik_role, (qint32)StickData::IK_START);
        QCOMPARE(child->ik_role, (qint32)StickData::IK_NODE);
        QCOMPARE(grandchild->ik_role, (qint32)StickData::IK_TARGET_ON);
        QCOMPARE(master->ik_target, grandchild);
        QCOMPARE(grandchild->ik_start, master);
        QVERIFY(StickIsInActiveIkChain(child));

        view->SetTool(NOTOOL);
    }

    // an invalid second click (not a descendant of the pending start) leaves the
    // session open instead of committing garbage; a valid click afterward still works
    void DrawIkChainRejectsInvalidTargetThenAcceptsValid()
    {
        StickData *master, *child, *grandchild;
        BuildIkChainFixture(&master, &child, &grandchild);
        AddStick(); // uno stickfigure estraneo, per fornire un bersaglio non discendente
        StickData *unrelated = DrawStick(QPointF(400, 100), QPointF(400, 200));
        // DRAW_IK opera sempre su CurrentStickFigure() (stessa convenzione di
        // DRAWPOLY/DRAWMULTI): il secondo AddStick() l'ha spostata sullo stickfigure
        // estraneo, quindi va rimessa su quello della catena prima di usare il tool
        view->my_animation->UpdateSelection(master);
        view->resize(800, 600);
        view->SetTool(DRAW_IK);

        QTest::mouseClick(view->viewport(), Qt::LeftButton, Qt::NoModifier,
                          view->mapFromScene(QPointF(100, 100)));
        QCOMPARE(master->ik_role, (qint32)StickData::IK_START);

        QTest::mouseClick(view->viewport(), Qt::LeftButton, Qt::NoModifier,
                          view->mapFromScene(QPointF(400, 150))); // stick estraneo: non valido
        QCOMPARE(unrelated->ik_role, (qint32)StickData::IK_NONE);
        QCOMPARE(master->ik_target, static_cast<StickData*>(nullptr)); // sessione ancora aperta

        QTest::mouseClick(view->viewport(), Qt::LeftButton, Qt::NoModifier,
                          view->mapFromScene(QPointF(100, 350))); // ora un click valido
        QCOMPARE(grandchild->ik_role, (qint32)StickData::IK_TARGET_ON);
        QCOMPARE(master->ik_target, grandchild);

        view->SetTool(NOTOOL);
    }

    // Esc mid-creation reverts the pending start back to IK_NONE (spec)
    void DrawIkChainEscapeAbortsAndRevertsStart()
    {
        StickData *master, *child, *grandchild;
        BuildIkChainFixture(&master, &child, &grandchild);
        Q_UNUSED(child); Q_UNUSED(grandchild);
        view->resize(800, 600);
        view->SetTool(DRAW_IK);

        QTest::mouseClick(view->viewport(), Qt::LeftButton, Qt::NoModifier,
                          view->mapFromScene(QPointF(100, 100)));
        QCOMPARE(master->ik_role, (qint32)StickData::IK_START);

        QTest::keyClick(view, Qt::Key_Escape);
        QCOMPARE(master->ik_role, (qint32)StickData::IK_NONE);

        view->SetTool(NOTOOL);
    }

    // clicking an existing target toggles it, Alt+click clears the whole chain
    void DrawIkChainToggleAndClear()
    {
        StickData *master, *child, *grandchild;
        BuildIkChainFixture(&master, &child, &grandchild);
        view->resize(800, 600);
        view->SetTool(DRAW_IK);
        QTest::mouseClick(view->viewport(), Qt::LeftButton, Qt::NoModifier,
                          view->mapFromScene(QPointF(100, 100)));
        QTest::mouseClick(view->viewport(), Qt::LeftButton, Qt::NoModifier,
                          view->mapFromScene(QPointF(100, 350)));
        QCOMPARE(grandchild->ik_role, (qint32)StickData::IK_TARGET_ON);

        QTest::mouseClick(view->viewport(), Qt::LeftButton, Qt::NoModifier,
                          view->mapFromScene(QPointF(100, 400))); // sul target handle: toggle
        QCOMPARE(grandchild->ik_role, (qint32)StickData::IK_TARGET_OFF);
        QVERIFY(!StickIsInActiveIkChain(master));

        QTest::mouseClick(view->viewport(), Qt::LeftButton, Qt::NoModifier,
                          view->mapFromScene(QPointF(100, 400))); // di nuovo: riaccende
        QCOMPARE(grandchild->ik_role, (qint32)StickData::IK_TARGET_ON);

        QTest::mouseClick(view->viewport(), Qt::LeftButton, Qt::AltModifier,
                          view->mapFromScene(QPointF(100, 400))); // Alt+click: cancella la catena
        QCOMPARE(master->ik_role, (qint32)StickData::IK_NONE);
        QCOMPARE(child->ik_role, (qint32)StickData::IK_NONE);
        QCOMPARE(grandchild->ik_role, (qint32)StickData::IK_NONE);

        view->SetTool(NOTOOL);
    }

    // A limb whose sticks are stepchildren by INHERITANCE (they branch off the master's
    // origin, so EndDrawing() propagates stepchild=true down the whole subtree) but are
    // geometrically continuous (each p1 sits on its parent's p2) must still allow an IK
    // target. The old continuity test keyed on the stepchild FLAG and rejected every such
    // node, so on a real articulated figure "you cannot click any node to be the target"
    // (Group D follow-up). FindIkTargetCandidate now compares the real joints instead.
    void IkTargetSelectableOnContinuousStepchildLimb()
    {
        AddFrame();
        StickFigure *fig = CurrentStickFigure();
        qDeleteAll(fig->stick_list);
        fig->stick_list.clear();

        StickData *master = new StickData();
        master->stick_type = StickData::LINE;
        master->my_line = QLineF(100, 100, 100, 200);
        master->parent = nullptr; master->stepchild = false;

        StickData *thigh = new StickData();   // branches from the master's ORIGIN -> stepchild
        thigh->stick_type = StickData::LINE;
        thigh->my_line = QLineF(100, 100, 250, 100);
        thigh->parent = master; thigh->stepchild = true;

        StickData *shin = new StickData();    // continuation of thigh (p1 exactly on thigh's p2)
        shin->stick_type = StickData::LINE;
        shin->my_line = QLineF(250, 100, 400, 100);
        shin->parent = thigh; shin->stepchild = true; // inherited flag, but geometrically continuous

        master->children << thigh << shin; // children is flattened (all descendants)
        thigh->children << shin;
        fig->stick_list << master << thigh << shin;
        fig->master_stick = master;
        fig->current_stick = master;

        view->resize(800, 600);
        view->SetTool(DRAW_IK);
        // first click at the shared origin (100,100): master.p1 and thigh.p1 coincide, the
        // tie-break prefers the stepchild so the chain can start at the limb root
        QTest::mouseClick(view->viewport(), Qt::LeftButton, Qt::NoModifier,
                          view->mapFromScene(QPointF(100, 100)));
        QCOMPARE(thigh->ik_role, (qint32)StickData::IK_START);
        // second click on the shin segment: it must commit as the target
        QTest::mouseClick(view->viewport(), Qt::LeftButton, Qt::NoModifier,
                          view->mapFromScene(QPointF(325, 100)));
        QCOMPARE(shin->ik_role, (qint32)StickData::IK_TARGET_ON);
        QCOMPARE(thigh->ik_target, shin);

        view->SetTool(NOTOOL);
    }

    // Two stepchild limbs share the master's origin, so the first click cannot tell which
    // limb the user means (the old tie-break picked one arbitrarily and the target on the
    // OTHER limb was then unreachable — "can't select the target when a branch sits at the
    // next stick", Group D follow-up). The start must be re-derived from the target's
    // branch on the second click.
    void IkStartResolvesToTargetsBranchAtSharedOrigin()
    {
        AddFrame();
        StickFigure *fig = CurrentStickFigure();
        qDeleteAll(fig->stick_list); fig->stick_list.clear();
        auto mk = [&](QLineF l, StickData *par, bool sc){
            StickData *s = new StickData(); s->stick_type = StickData::LINE; s->my_line = l; s->parent = par; s->stepchild = sc;
            return s; };
        StickData *M   = mk(QLineF(100,100,100,300), nullptr, false);
        StickData *L1a = mk(QLineF(100,100, 50,150),  M, true);   // limb 1 (would win the old tie-break)
        StickData *L1b = mk(QLineF( 50,150, 20,200),  L1a, true);
        StickData *L2a = mk(QLineF(100,100,150,150),  M, true);   // limb 2 (the user's target is here)
        StickData *L2b = mk(QLineF(150,150,180,200),  L2a, true);
        M->children << L1a << L1b << L2a << L2b; L1a->children << L1b; L2a->children << L2b;
        fig->stick_list << M << L1a << L1b << L2a << L2b;
        fig->master_stick = M; fig->current_stick = M;

        view->resize(800,600); view->SetTool(DRAW_IK);
        QTest::mouseClick(view->viewport(), Qt::LeftButton, Qt::NoModifier, view->mapFromScene(QPointF(100,100)));
        QTest::mouseClick(view->viewport(), Qt::LeftButton, Qt::NoModifier, view->mapFromScene(QPointF(165,175)));
        QCOMPARE(L2b->ik_role, (qint32)StickData::IK_TARGET_ON); // the clicked limb's tip is the target
        QCOMPARE(L2a->ik_role, (qint32)StickData::IK_START);     // start re-derived onto limb 2's root
        QCOMPARE(L1a->ik_role, (qint32)StickData::IK_NONE);      // provisional start on limb 1 was released
        view->SetTool(NOTOOL);
    }

    // a plain continuous chain with a branch hanging off a MIDDLE stick must still let the
    // target past the branch be selected (a mid-chain branch does not break the parent line)
    void IkTargetSelectableAcrossMidChainBranch()
    {
        AddFrame();
        StickFigure *fig = CurrentStickFigure();
        qDeleteAll(fig->stick_list); fig->stick_list.clear();
        auto mk = [&](QLineF l, StickData *par){
            StickData *s = new StickData(); s->stick_type = StickData::LINE; s->my_line = l; s->parent = par; s->stepchild = false;
            return s; };
        StickData *M = mk(QLineF(100,100,100,200), nullptr);
        StickData *A = mk(QLineF(100,200,100,300), M);
        StickData *C = mk(QLineF(100,300,200,300), A); // branch off A's endpoint
        StickData *B = mk(QLineF(100,300,100,400), A); // chain continues from A's endpoint
        StickData *T = mk(QLineF(100,400,100,500), B);
        M->children << A << C << B << T; A->children << C << B << T; B->children << T;
        fig->stick_list << M << A << C << B << T;
        fig->master_stick = M; fig->current_stick = M;

        view->resize(800,600); view->SetTool(DRAW_IK);
        QTest::mouseClick(view->viewport(), Qt::LeftButton, Qt::NoModifier, view->mapFromScene(QPointF(100,100)));
        QTest::mouseClick(view->viewport(), Qt::LeftButton, Qt::NoModifier, view->mapFromScene(QPointF(100,450)));
        QCOMPARE(T->ik_role, (qint32)StickData::IK_TARGET_ON);
        QCOMPARE(M->ik_role, (qint32)StickData::IK_START);
        view->SetTool(NOTOOL);
    }

    // FABRIK on a stepchild chain whose TARGET is not the chain end: the continuation after
    // the target (attached to the target's free end P2, but flagged stepchild by inheritance)
    // must follow the target's P2, not its P1. The old flag-based delta moved it by the P1
    // (origin) delta, so it trailed the previous stick with an offset (Group D follow-up).
    void SolveIkChainDragsPostTargetContinuationByFreeEnd()
    {
        AddFrame();
        StickFigure *fig = CurrentStickFigure();
        qDeleteAll(fig->stick_list); fig->stick_list.clear();
        auto mk = [&](QLineF l, StickData *par, bool sc){
            StickData *s = new StickData(); s->stick_type = StickData::LINE; s->my_line = l; s->parent = par; s->stepchild = sc;
            return s; };
        StickData *thigh = mk(QLineF(100,100,200,100), nullptr, false); // start/anchor
        StickData *shin  = mk(QLineF(200,100,300,100), thigh, true);    // node
        StickData *foot  = mk(QLineF(300,100,400,100), shin,  true);    // target
        StickData *toe   = mk(QLineF(400,100,450,100), foot,  true);    // continuation AFTER the target
        thigh->children << shin << foot << toe; shin->children << foot << toe; foot->children << toe;
        fig->stick_list << thigh << shin << foot << toe;
        fig->master_stick = thigh; fig->current_stick = thigh;

        thigh->ik_role = StickData::IK_START;
        CommitIkChain(thigh, foot);
        QCOMPARE(foot->ik_role, (qint32)StickData::IK_TARGET_ON);

        SolveIkChain(thigh, QPointF(300, 250)); // pull the target out of line: the chain bends
        // the continuation stays welded to the target's free end (no offset)
        QVERIFY2(QLineF(toe->my_line.p1(), foot->my_line.p2()).length() < 0.5,
                 "post-target continuation trailed the previous stick instead of the target's P2");
    }

    // clicking near the SECOND-LAST stick's end handle (the joint it shares with the last
    // stick) must target the second-last, not snap to the last. Target selection ranks by
    // the endpoint handle (P2), so the stick whose free end is nearest the click wins even
    // when the click also grazes the last stick's segment (Group D follow-up, adjustment 3).
    void IkTargetPicksStickByEndHandleNotSegment()
    {
        AddFrame();
        StickFigure *fig = CurrentStickFigure();
        qDeleteAll(fig->stick_list); fig->stick_list.clear();
        auto mk = [&](QLineF l, StickData *par, bool sc){
            StickData *s = new StickData(); s->stick_type = StickData::LINE; s->my_line = l; s->parent = par; s->stepchild = sc;
            return s; };
        StickData *thigh = mk(QLineF(100,100,200,100), nullptr, false); // master/start
        StickData *shin  = mk(QLineF(200,100,300,100), thigh, true);
        StickData *foot  = mk(QLineF(300,100,400,100), shin,  true);    // second-last (desired target)
        StickData *toe   = mk(QLineF(400,100,500,100), foot,  true);    // last
        thigh->children << shin << foot << toe; shin->children << foot << toe; foot->children << toe;
        fig->stick_list << thigh << shin << foot << toe;
        fig->master_stick = thigh; fig->current_stick = thigh;

        view->resize(800, 600);
        view->SetTool(DRAW_IK);
        QTest::mouseClick(view->viewport(), Qt::LeftButton, Qt::NoModifier, view->mapFromScene(QPointF(100,100)));
        // click just past foot's end handle (400,100): grazes toe's segment, but foot's P2 is nearest
        QTest::mouseClick(view->viewport(), Qt::LeftButton, Qt::NoModifier, view->mapFromScene(QPointF(405,100)));
        QCOMPARE(foot->ik_role, (qint32)StickData::IK_TARGET_ON); // second-last chosen
        QCOMPARE(toe->ik_role,  (qint32)StickData::IK_NONE);      // last NOT snapped to
        view->SetTool(NOTOOL);
    }

    // Dragging the MASTER's end rotates only what hangs off its P2: the stepchild limbs
    // hang off its origin, which is also the pivot, so they stay put and still attached.
    // (Reverses an earlier "Bug 1" fix that rotated the whole figure, by user request.)
    // Also pins that SetMaster() on the current master, with both children and
    // stepchildren (its early-return branch), leaves the origin handle green.
    void MasterDragRotatesStepchildChain()
    {
        AddFrame();
        StickData *master = DrawStick(QPointF(100,100), QPointF(100,200));
        StickData *child  = DrawStick(QPointF(100,200), QPointF(100,300)); // direct child
        StickData *step   = DrawStick(QPointF(100,100), QPointF(200,100)); // stepchild off master origin
        QVERIFY(step->stepchild);
        float step_a0 = step->my_line.angle(), child_a0 = child->my_line.angle();
        view->resize(800,600);
        view->SetTool(NOTOOL);
        QTest::mousePress(view->viewport(), Qt::LeftButton, Qt::NoModifier, view->mapFromScene(QPointF(100,180)));
        QTest::mouseMove(view->viewport(),  view->mapFromScene(QPointF(100,180))); // latch startingAngle
        QTest::mouseMove(view->viewport(),  view->mapFromScene(QPointF(220,220))); // rotate (closer to p2)
        QTest::mouseRelease(view->viewport(), Qt::LeftButton, Qt::NoModifier, view->mapFromScene(QPointF(220,220)));
        QVERIFY2(qAbs(child->my_line.angle() - child_a0) > 1.0, "test setup: master drag did not rotate at all");
        QVERIFY2(qAbs(step->my_line.angle()  - step_a0)  < 0.01, "stepchild limb rotated with the master-drag");
        QVERIFY(QLineF(step->my_line.p1(), master->my_line.p1()).length() < 0.01);
        view->grab(); // paint() derives the handle colour from the tree
        QCOMPARE(master->view->stick_handle[0]->brush().color(), QColor(Qt::green));
        QCOMPARE(step->view->stick_handle[0]->brush().color(), QColor(Qt::red));
        CurrentStickFigure()->SetMaster(master);
        QCOMPARE(master->view->stick_handle[0]->brush().color(), QColor(Qt::green));
        view->SetTool(NOTOOL);
    }

    // MyView::ConvertStick(): ends, IK role and pen survive; what only the old type means
    // (a MULTILINE's interior vertices, a POLYGON's outline) is dropped; the new type is
    // seeded (a POLYGON's diamond passes through both ends); the whole thing is one undo step.
    void ConvertStickKeepsEndsIkAndPaintDropsOldTypeData()
    {
        AddFrame();
        DrawStick(QPointF(100,100), QPointF(100,200));
        StickData *s = DrawStick(QPointF(100,200), QPointF(200,300));
        s->stick_type = StickData::MULTILINE;
        s->polygon_points << QPointF(0.3, 0.5);
        s->vertex_widths << 3 << 3 << 3;
        s->pen = QPen(QColor(10,20,30), 7);
        s->ik_role = StickData::IK_NODE;
        const QLineF line = s->my_line;

        view->ConvertStick(s, StickData::POLYGON);
        QCOMPARE(s->stick_type, (qint32)StickData::POLYGON);
        QCOMPARE(s->my_line, line);
        QCOMPARE(s->pen.color(), QColor(10,20,30));
        QCOMPARE(s->pen.widthF(), 7.0);
        QCOMPARE(s->ik_role, (qint32)StickData::IK_NODE);
        QVERIFY(s->vertex_widths.isEmpty());
        QCOMPARE(s->polygon_points.count(), 4);
        QVERIFY(QLineF(s->PolygonTransform().map(s->polygon_points[0]), line.p1()).length() < 0.01);
        QVERIFY(QLineF(s->PolygonTransform().map(s->polygon_points[2]), line.p2()).length() < 0.01);

        view->ConvertStick(s, StickData::MULTILINE);
        QVERIFY(s->polygon_points.isEmpty()); // the polygon's outline is gone
        QCOMPARE(s->vertex_widths.count(), 2);
        QCOMPARE(s->my_line, line);

        view->undo_stack.undo();
        QCOMPARE(CurrentStickFigure()->stick_list.last()->stick_type, (qint32)StickData::POLYGON);
    }

    // A band that encloses nothing clears the selection (CurrentStickFigure() == nullptr). It
    // must clear the layer list's CURRENT row too, not just its selected rows: the list selects
    // through currentItemChanged (Widget::on_stickLayerView_currentItemChanged), which Qt does
    // not emit when the clicked row is already current — so the deselected figure could not be
    // reselected from the list, and the draw tools then reported "no stickfigure layer present".
    // A click on a row is setCurrentItem(row) inside QListWidget; that must now be a change.
    void LayerRowReselectsAfterBandDeselect()
    {
        AddFrame();
        StickFigure *figure = CurrentStickFigure();
        QVERIFY(figure != nullptr && figure->linked_item != nullptr);
        my_stick_figure_widget_list->setCurrentItem(figure->linked_item);

        view->ApplyBandSelection({});
        QVERIFY(CurrentStickFigure() == nullptr);

        QSignalSpy changed(my_stick_figure_widget_list, &QListWidget::currentItemChanged);
        my_stick_figure_widget_list->setCurrentItem(figure->linked_item); // the user's click
        QCOMPARE(changed.count(), 1);

        view->my_animation->UpdateSelection(figure); // what the slot does; later slots expect one
    }

    // A MULTILINE turning sharper than 90° into a LONGER segment: the old outline offset each
    // vertex along the normal of the prev->next chord, which here points backwards, so the band
    // flipped at the corner and pinched to nothing mid-segment ("twists on itself"). Probes 5 px
    // either side of each segment's centre (half width 8) must be painted, and a point outside
    // the band must not, so a render that floods everything cannot pass.
    void MultilineSharpTurnKeepsAFullWidthBand()
    {
        StickData m;
        m.stick_type = StickData::MULTILINE;
        m.my_line = QLineF(200, 200, 100, 260); // P1 -> P2; the corner (300,200) is interior
        m.vertex_widths << 16 << 16 << 16;
        m.vertex_curvatures << 0 << 0 << 0;
        m.pen = QPen(Qt::black, 16, Qt::SolidLine, Qt::FlatCap);
        QGraphicsScene render_scene;
        render_scene.setSceneRect(0, 0, 400, 400);
        StickGraphicsItem *item = new StickGraphicsItem(&m); // la scena possiede la view
        render_scene.addItem(item);
        // after the view exists, so the local frame is whatever angle it settled on
        m.polygon_points << m.PolygonTransform().inverted().map(QPointF(300, 200));
        item->Refresh(0);
        QImage img(400, 400, QImage::Format_ARGB32);
        img.fill(Qt::white);
        QPainter painter(&img);
        render_scene.render(&painter, QRectF(0, 0, 400, 400), QRectF(0, 0, 400, 400));
        painter.end();
        auto painted = [&img](int x, int y){ return img.pixelColor(x, y) != QColor(Qt::white); };
        QVERIFY2(painted(250, 195) && painted(250, 205), "first segment pinched: the band twisted");
        QVERIFY2(painted(198, 224) && painted(202, 236), "second segment is not full width");
        QVERIFY(!painted(250, 180)); // outside the band
    }

    // MULTILINE branch topology at the data level: removing a fork vertex reattaches its
    // children to its parent; removing an endpoint of a one-edge main line hands that end to
    // another vertex (both ends would otherwise land on the same one); the parent list
    // survives the stream — also for a record read from an older version and saved again —
    // and an invalid (cyclic / out-of-range) list reads as the plain chain
    void MultilineBranchTopologyEdits()
    {
        StickData fork, no_p2, no_p1; // prima della scena: le view vanno distrutte per prime
        QGraphicsScene render_scene;
        auto setup = [&render_scene](StickData &m, QPointF p1, QPointF p2) {
            m.stick_type = StickData::MULTILINE;
            m.my_line = QLineF(p1, p2);
            m.vertex_widths << 4 << 4;
            m.vertex_curvatures << 0 << 0;
            StickGraphicsItem *item = new StickGraphicsItem(&m); // la scena possiede la view
            render_scene.addItem(item);
            return item;
        };
        auto index_of = [](const QPolygonF &v, QPointF p) {
            for (int i = 0; i < v.count(); i++)
                if (QLineF(v[i], p).length() < 0.01) return i;
            return -1;
        };

        // P1(0,0) - J(100,0) - P2(200,0), branch J - B(100,100)
        StickGraphicsItem *f = setup(fork, QPointF(0, 0), QPointF(100, 0));
        QVERIFY(f->AppendMultilineVertex(QPointF(200, 0)));
        QCOMPARE(f->BranchMultilineVertex(1, QPointF(100, 100)), 2);
        QCOMPARE(f->BranchMultilineVertex(3, QPointF(0, 50)), -1); // P2 grows through AppendMultilineVertex
        QCOMPARE(fork.MultilineParents(), (QList<qint32>{-1, 0, 1, 1}));
        QVERIFY(QLineF(fork.my_line.p2(), QPointF(200, 0)).length() < 0.01);
        // splitting the branch edge keeps the branch hanging from J
        QCOMPARE(f->NearestEdgeInsertIndex(QPointF(102, 50)), 2);
        QVERIFY(f->AddPolygonPoint(QPointF(100, 50), 2));
        QCOMPARE(fork.MultilineParents(), (QList<qint32>{-1, 0, 1, 2, 1}));
        // removing a branch vertex takes the rest of its branch along
        QVERIFY(f->DeletePolygonPoint(2));
        QCOMPARE(fork.MultilineParents(), (QList<qint32>{-1, 0, 1}));
        QCOMPARE(index_of(f->SceneVertices(), QPointF(100, 100)), -1);
        QCOMPARE(f->BranchMultilineVertex(1, QPointF(100, 100)), 2); // back to the fork
        QCOMPARE(fork.MultilineParents(), (QList<qint32>{-1, 0, 1, 1}));

        // stream round trip; a v6 record has no parents on the wire, but once read it is a
        // current record and a re-save keeps the branch
        QByteArray bytes;
        { QDataStream out(&bytes, QIODevice::WriteOnly); out << fork; }
        StickData loaded;
        { QDataStream in(&bytes, QIODevice::ReadOnly); in >> loaded; }
        QCOMPARE(loaded.vertex_parents, fork.vertex_parents);
        StickData old_record;
        old_record.CopyFrom(fork);
        old_record.version = 6;
        QByteArray old_bytes;
        { QDataStream out(&old_bytes, QIODevice::WriteOnly); out << old_record; }
        StickData old_loaded;
        { QDataStream in(&old_bytes, QIODevice::ReadOnly); in >> old_loaded; }
        QVERIFY(old_loaded.vertex_parents.isEmpty());
        QCOMPARE(old_loaded.polygon_points.count(), fork.polygon_points.count());
        QCOMPARE(old_loaded.version, StickData::CURRENT_VERSION);
        old_loaded.vertex_parents = fork.vertex_parents; // branched after loading
        QByteArray resaved;
        { QDataStream out(&resaved, QIODevice::WriteOnly); out << old_loaded; }
        StickData reloaded;
        { QDataStream in(&resaved, QIODevice::ReadOnly); in >> reloaded; }
        QCOMPARE(reloaded.vertex_parents, fork.vertex_parents);

        // removing the fork vertex J: the branch starting there goes too, and the main line
        // closes up over J (P2 now hangs from P1)
        QVERIFY(f->DeletePolygonPoint(1));
        QPolygonF v = f->SceneVertices();
        QCOMPARE(v.count(), 2);
        QCOMPARE(index_of(v, QPointF(100, 100)), -1);
        QVERIFY(QLineF(fork.my_line.p1(), QPointF(0, 0)).length() < 0.01);
        QVERIFY(QLineF(fork.my_line.p2(), QPointF(200, 0)).length() < 0.01);

        // one-edge main line P1(0,0)-P2(100,0) with a branch P1-B(0,100): removing P2 makes
        // the branch tip the new P2...
        StickGraphicsItem *s2 = setup(no_p2, QPointF(0, 0), QPointF(100, 0));
        QCOMPARE(s2->BranchMultilineVertex(0, QPointF(0, 100)), 1);
        QVERIFY(s2->DeletePolygonPoint(2));
        QCOMPARE(s2->SceneVertices().count(), 2);
        QVERIFY(QLineF(no_p2.my_line.p1(), QPointF(0, 0)).length() < 0.01);
        QVERIFY(QLineF(no_p2.my_line.p2(), QPointF(0, 100)).length() < 0.01);
        // ...while removing P1 would take its branch along and leave P2 alone: refused,
        // nothing changes (MyView then deletes the whole stick)
        StickGraphicsItem *s1 = setup(no_p1, QPointF(0, 0), QPointF(100, 0));
        QCOMPARE(s1->BranchMultilineVertex(0, QPointF(0, 100)), 1);
        QVERIFY(!s1->DeletePolygonPoint(0));
        QCOMPARE(s1->SceneVertices().count(), 3);
        // a branch of a branch goes with its origin: P1-J-P2, J-B1-B2, B1-C
        QVERIFY(s1->AppendMultilineVertex(QPointF(200, 0)));      // P1(0,0) B(0,100) J(100,0) P2(200,0)
        int b1 = s1->BranchMultilineVertex(2, QPointF(100, 100));
        int b2 = s1->BranchMultilineVertex(b1, QPointF(100, 200));
        s1->BranchMultilineVertex(b1, QPointF(150, 100));
        QCOMPARE(s1->SceneVertices().count(), 7);
        QVERIFY(s1->DeletePolygonPoint(b1));
        v = s1->SceneVertices();
        QCOMPARE(v.count(), 4);
        for (QPointF gone : {QPointF(100, 100), QPointF(100, 200), QPointF(150, 100)})
            QCOMPARE(index_of(v, gone), -1);
        Q_UNUSED(b2);

        // an invalid list never reaches a tree walk: it reads as the plain chain
        StickData invalid;
        invalid.stick_type = StickData::MULTILINE;
        invalid.polygon_points << QPointF(0, 0.3) << QPointF(0, 0.6);
        const QList<qint32> chain{-1, 0, 1, 2};
        invalid.vertex_parents = {-1, 2, 1, 0}; // cycle 1 <-> 2
        QCOMPARE(invalid.MultilineParents(), chain);
        invalid.vertex_parents = {-1, 0, 7, 1}; // out of range
        QCOMPARE(invalid.MultilineParents(), chain);
        invalid.vertex_parents = {-1, 0, 1};    // stale size
        QCOMPARE(invalid.MultilineParents(), chain);
    }

    // Per-vertex caps (StickData::vertex_caps), rendered on a 90° corner P1(100,200) ->
    // J(200,200) -> P2(200,100), half width 10, whose outer corner is the bottom-right one:
    // SHARP keeps the miter tip, BEVEL cuts it along the chord (200,210)-(210,200), ROUND
    // replaces it with the disc of radius 10 — and the inner side stays full for all three
    // (a forced bevel runs its inner edge through the vertex). A ROUND end gets a cap, the
    // others stop flat. Also: the historical defaults for a record without the list, and
    // the version-8 round trip
    void MultilineVertexCapsRender()
    {
        auto render = [](qint32 corner_cap, qint32 end_cap) {
            StickData m;
            m.stick_type = StickData::MULTILINE;
            m.my_line = QLineF(100, 200, 200, 100);
            m.vertex_widths << 20 << 20 << 20;
            m.vertex_curvatures << 0 << 0 << 0;
            m.vertex_caps << end_cap << corner_cap << end_cap;
            m.pen = QPen(Qt::black, 20, Qt::SolidLine, Qt::FlatCap);
            QGraphicsScene render_scene;
            render_scene.setSceneRect(0, 0, 400, 400);
            StickGraphicsItem *item = new StickGraphicsItem(&m); // la scena possiede la view
            render_scene.addItem(item);
            m.polygon_points << m.PolygonTransform().inverted().map(QPointF(200, 200));
            item->Refresh(0);
            QImage img(400, 400, QImage::Format_ARGB32_Premultiplied);
            img.fill(Qt::transparent);
            QPainter painter(&img);
            render_scene.render(&painter, QRectF(0, 0, 400, 400), QRectF(0, 0, 400, 400));
            painter.end();
            return img;
        };
        auto painted = [](const QImage &img, int x, int y) { return qAlpha(img.pixel(x, y)) > 0; };
        QImage sharp = render(StickData::CAP_SHARP, StickData::CAP_SHARP);
        QImage bevel = render(StickData::CAP_BEVEL, StickData::CAP_BEVEL);
        QImage round = render(StickData::CAP_ROUND, StickData::CAP_ROUND);
        QVERIFY2(painted(sharp, 208, 208), "SHARP lost its miter tip");
        QVERIFY2(!painted(bevel, 208, 208) && !painted(bevel, 206, 206), "BEVEL did not cut the corner");
        QVERIFY2(painted(bevel, 203, 203), "BEVEL cut inside its chord");
        QVERIFY2(!painted(round, 208, 208) && painted(round, 206, 206), "ROUND is not the disc");
        for (const QImage *img : {&sharp, &bevel, &round})
            QVERIFY2(painted(*img, 196, 196) && painted(*img, 150, 192) && painted(*img, 192, 150),
                     "the inner side of the corner is notched");
        QVERIFY2(painted(round, 94, 200) && painted(round, 200, 94), "a ROUND end has no cap");
        QVERIFY2(!painted(sharp, 94, 200) && !painted(bevel, 94, 200), "a flat end grew a cap");

        // historical defaults: sharp corners, round branch origins, ends after the pen cap
        StickData legacy;
        legacy.stick_type = StickData::MULTILINE;
        legacy.polygon_points << QPointF(0, 0.3) << QPointF(0.2, 0.5);
        legacy.vertex_parents = {-1, 0, 1, 1};               // J(1) forks: branch 2, P2 3
        legacy.pen.setCapStyle(Qt::FlatCap);
        const qint32 S = StickData::CAP_SHARP, R = StickData::CAP_ROUND;
        QCOMPARE(legacy.MultilineCaps(), (QList<qint32>{S, R, S, S}));
        legacy.pen.setCapStyle(Qt::RoundCap);
        QCOMPARE(legacy.MultilineCaps(), (QList<qint32>{R, R, R, R}));
        legacy.vertex_caps = {S, S};                          // stale size: ignored
        QCOMPARE(legacy.MultilineCaps(), (QList<qint32>{R, R, R, R}));

        // version 8 round trip; a version 7 record carries none
        StickData capped;
        capped.CopyFrom(legacy);
        capped.vertex_caps = {S, R, qint32(StickData::CAP_BEVEL), S};
        QByteArray bytes;
        { QDataStream out(&bytes, QIODevice::WriteOnly); out << capped; }
        StickData loaded;
        { QDataStream in(&bytes, QIODevice::ReadOnly); in >> loaded; }
        QCOMPARE(loaded.vertex_caps, capped.vertex_caps);
        QCOMPARE(loaded.vertex_parents, capped.vertex_parents);
        capped.version = 7;
        QByteArray old_bytes;
        { QDataStream out(&old_bytes, QIODevice::WriteOnly); out << capped; }
        StickData old_loaded;
        { QDataStream in(&old_bytes, QIODevice::ReadOnly); in >> old_loaded; }
        QVERIFY(old_loaded.vertex_caps.isEmpty());
        QCOMPARE(old_loaded.vertex_parents, capped.vertex_parents);
    }

    // A branch leaving a vertex where the main line TURNS, and turning back on it sharply:
    // joined to the incoming edge with a sharp miter (the first cut), that edge was carried
    // past the vertex — where the main line no longer goes — and stuck out as a spike. The
    // branch now starts flat at its vertex with a round disc as the join. Semi-transparent
    // on purpose: the disc lives in the bands' single path with their winding, so it neither
    // punches a hole (opposite winding cancels) nor darkens the fork (a second fill would)
    void MultilineBranchJoinHasNoSpike()
    {
        StickData m;
        m.stick_type = StickData::MULTILINE;
        m.my_line = QLineF(100, 200, 200, 100);              // P1 -> P2; the corner J (200,200) is interior
        m.vertex_widths << 16 << 16 << 16 << 16;
        m.vertex_curvatures << 0 << 0 << 0 << 0;
        m.pen = QPen(QColor(0, 0, 0, 128), 16, Qt::SolidLine, Qt::FlatCap);
        QGraphicsScene render_scene;
        render_scene.setSceneRect(0, 0, 400, 400);
        StickGraphicsItem *item = new StickGraphicsItem(&m); // la scena possiede la view
        render_scene.addItem(item);
        QTransform inv = m.PolygonTransform().inverted();
        m.polygon_points << inv.map(QPointF(200, 200)) << inv.map(QPointF(120, 260));
        m.vertex_parents = {-1, 0, 1, 1};                    // J <- P1, branch <- J, P2 <- J
        item->Refresh(0);
        QImage img(400, 400, QImage::Format_ARGB32_Premultiplied);
        img.fill(Qt::transparent);
        QPainter painter(&img);
        render_scene.render(&painter, QRectF(0, 0, 400, 400), QRectF(0, 0, 400, 400));
        painter.end();
        auto alpha = [&img](int x, int y) { return qAlpha(img.pixel(x, y)); };
        const int once = alpha(150, 200); // the straight main line, painted once
        QVERIFY(qAbs(once - 128) <= 2);
        QVERIFY2(alpha(216, 195) == 0, "a spike sticks out of the fork");
        QVERIFY2(qAbs(alpha(160, 230) - once) <= 2, "the branch is not drawn");
        QVERIFY2(qAbs(alpha(205, 195) - once) <= 2, "the join disc cancels the band: a hole at the fork");
        QVERIFY2(qAbs(alpha(195, 204) - once) <= 2, "the fork is painted twice: darker than the line");
    }

    // Bug 3: the ROTATE tool pivots on the HANDLE (master origin + pivot_offset), not on
    // the master origin. With the handle offset to (300,100), a 180° drag orbits the
    // master's origin around the handle to the mirrored point.
    void RotateToolPivotsOnHandle()
    {
        AddFrame();
        StickData *master = DrawStick(QPointF(100,100), QPointF(100,200));
        StickFigure *fig = CurrentStickFigure();
        fig->pivot_offset = QPointF(200, 0);                     // handle at (300,100)
        QPointF handle = master->my_line.p1() + fig->pivot_offset;
        QPointF p1_before = master->my_line.p1();
        view->resize(800,600);
        view->SetTool(ROTATE);
        QTest::mousePress(view->viewport(), Qt::LeftButton, Qt::NoModifier, view->mapFromScene(QPointF(400,100)));
        QTest::mouseMove(view->viewport(),  view->mapFromScene(QPointF(400,100)));
        QTest::mouseMove(view->viewport(),  view->mapFromScene(QPointF(200,100))); // 180° around (300,100)
        QTest::mouseRelease(view->viewport(), Qt::LeftButton, Qt::NoModifier, view->mapFromScene(QPointF(200,100)));
        QPointF expected = 2*handle - p1_before;                 // mirror of p1 through the handle => (500,100)
        QVERIFY2(QLineF(master->my_line.p1(), expected).length() < 5.0,
                 "ROTATE tool did not pivot on the handle");
        view->SetTool(NOTOOL);
    }

    // Bug 2: pivot_offset persists and is anchored to the master, so translating the
    // figure (master drag) carries the handle. After a translate, the ROTATE tool still
    // pivots on master.p1 + offset (now at the moved location).
    void PivotHandleFollowsMasterTranslate()
    {
        AddFrame();
        StickData *master = DrawStick(QPointF(100,100), QPointF(100,200));
        StickFigure *fig = CurrentStickFigure();
        fig->pivot_offset = QPointF(200, 0);
        fig->Traslate(100, 50);                                  // move the whole figure
        QPointF handle = master->my_line.p1() + fig->pivot_offset; // follows the master
        QPointF p1_before = master->my_line.p1();
        view->resize(800,600);
        view->SetTool(ROTATE);
        QPointF a(handle.x() + 100, handle.y());                 // east of the (moved) handle
        QPointF b(handle.x() - 100, handle.y());                 // west => 180°
        QTest::mousePress(view->viewport(), Qt::LeftButton, Qt::NoModifier, view->mapFromScene(a));
        QTest::mouseMove(view->viewport(),  view->mapFromScene(a));
        QTest::mouseMove(view->viewport(),  view->mapFromScene(b));
        QTest::mouseRelease(view->viewport(), Qt::LeftButton, Qt::NoModifier, view->mapFromScene(b));
        QPointF expected = 2*handle - p1_before;
        QVERIFY2(QLineF(master->my_line.p1(), expected).length() < 5.0,
                 "handle did not follow the master translate (offset not maintained)");
        view->SetTool(NOTOOL);
    }

    // Multi-selection: a Ctrl-dragged pivot handle must survive a tool switch (e.g. to
    // ROTATE/SCALE) — switching used to recenter it to the selection default.
    void MultiSelectionPivotSurvivesToolSwitch()
    {
        AddFrame();
        StickFigure *f1 = CurrentStickFigure();
        DrawStick(QPointF(100,100), QPointF(100,200));   // stick on figure 1
        StickFigure *f2 = AddStick();
        DrawStick(QPointF(300,100), QPointF(300,200));   // stick on figure 2 (now current)
        view->resize(800,600);
        view->SetTool(NOTOOL);
        view->AddToMultiSelection(f1);
        view->AddToMultiSelection(f2);
        QVERIFY(view->MultiSelectionActive());

        QPointF custom(500, 450);
        view->multi_pivot = custom;      // simulate a Ctrl-drag of the group handle
        view->SetTool(ROTATE);           // switching tools must NOT recenter it
        QCOMPARE(view->multi_pivot, custom);
        view->SetTool(SCALE);
        QCOMPARE(view->multi_pivot, custom);
        view->SetTool(NOTOOL);
    }

    // deleting a chain member (any of them) must dissolve the WHOLE chain, not just
    // leave the survivors with a stale IK role pointing at a freed stick
    void DeletingChainMemberClearsWholeChain()
    {
        StickData *master, *child, *grandchild;
        BuildIkChainFixture(&master, &child, &grandchild);
        CommitIkChainFixture(master, grandchild);
        QCOMPARE(grandchild->ik_role, (qint32)StickData::IK_TARGET_ON);

        int child_idx = CurrentStickFigure()->stick_list.indexOf(child);
        CurrentStickFigure()->DeleteStick(child_idx); // cancella il nodo di mezzo

        QCOMPARE(master->ik_role, (qint32)StickData::IK_NONE);
        QCOMPARE(grandchild->ik_role, (qint32)StickData::IK_NONE);
        QVERIFY(master->ik_target == nullptr);
        QVERIFY(grandchild->ik_start == nullptr);
    }

    // FABRIK: dragging the target with the select tool reshapes the whole chain, and
    // clamps to the chain's combined reach instead of stretching past it
    void SolveIkChainReshapesAndClampsReach()
    {
        StickData *master, *child, *grandchild;
        BuildIkChainFixture(&master, &child, &grandchild);
        CommitIkChainFixture(master, grandchild);
        double total_length = master->my_line.length() + child->my_line.length() + grandchild->my_line.length();

        // bersaglio ben oltre il raggio raggiungibile: il target deve fermarsi esattamente
        // a total_length dall'ancora (master->my_line.p1(), che non si muove)
        SolveIkChain(master, QPointF(100, 100) + QPointF(0, total_length * 3));
        double reached = QLineF(master->my_line.p1(), grandchild->my_line.p2()).length();
        QVERIFY(qAbs(reached - total_length) < 2.5); // tolleranza dell'algoritmo (spec: 2 unità) + margine

        // l'ancora non si è mai mossa
        QCOMPARE(master->my_line.p1(), QPointF(100, 100));

        // un bersaglio raggiungibile viene raggiunto entro la tolleranza specificata
        QPointF reachable_target(100 + total_length * 0.5, 100);
        SolveIkChain(master, reachable_target);
        double final_dist = QLineF(grandchild->my_line.p2(), reachable_target).length();
        QVERIFY(final_dist <= 2.0);

        // la catena resta continua (nessun giunto si è staccato)
        QCOMPARE(master->my_line.p2(), child->my_line.p1());
        QCOMPARE(child->my_line.p2(), grandchild->my_line.p1());
    }

    // an ACTIVE chain blocks the normal per-stick rotate drag for its START/NODE
    // members (only the target responds, and via FABRIK, not a rotation)
    void ActiveChainBlocksNormalRotateOnNonTargetMembers()
    {
        StickData *master, *child, *grandchild;
        BuildIkChainFixture(&master, &child, &grandchild);
        CommitIkChainFixture(master, grandchild);
        QLineF child_line_before = child->my_line;

        view->resize(800, 600);
        view->SetTool(NOTOOL);
        // click sul nodo di mezzo e trascina: dovrebbe essere bloccato (nessun movimento)
        QTest::mousePress(view->viewport(), Qt::LeftButton, Qt::NoModifier,
                          view->mapFromScene(QPointF(100, 300)));
        QTest::mouseMove(view->viewport(), view->mapFromScene(QPointF(250, 300)));
        QTest::mouseRelease(view->viewport(), Qt::LeftButton, Qt::NoModifier,
                            view->mapFromScene(QPointF(250, 300)));

        QCOMPARE(child->my_line, child_line_before);
        view->SetTool(NOTOOL);
    }

    // native (.stck-style, in-memory) round trip carries the chain: roles plus the
    // start/target pointers, remapped onto the copies exactly like parent/children
    void IkChainSurvivesExtractAndPopulate()
    {
        StickData *master, *child, *grandchild;
        BuildIkChainFixture(&master, &child, &grandchild);
        CommitIkChainFixture(master, grandchild);

        StickFigureData *extracted = ExtractStickFigureData(*CurrentStickFigure());
        QCOMPARE(extracted->stick_list[0]->ik_role, (qint32)StickData::IK_START);
        QCOMPARE(extracted->stick_list[1]->ik_role, (qint32)StickData::IK_NODE);
        QCOMPARE(extracted->stick_list[2]->ik_role, (qint32)StickData::IK_TARGET_ON);
        QCOMPARE(extracted->stick_list[0]->ik_target, extracted->stick_list[2]);
        QCOMPARE(extracted->stick_list[2]->ik_start, extracted->stick_list[0]);

        StickFigure fresh_figure;
        fresh_figure.scene = scene;
        PopulateLiveStickFigure(&fresh_figure, *extracted, false, false);
        QCOMPARE(fresh_figure.stick_list[0]->ik_role, (qint32)StickData::IK_START);
        QCOMPARE(fresh_figure.stick_list[0]->ik_target, fresh_figure.stick_list[2]);
        QCOMPARE(fresh_figure.stick_list[2]->ik_start, fresh_figure.stick_list[0]);
        delete extracted;
    }

    // undo: chain creation (the second click's commit) is one undoable step
    void DrawIkChainCreationIsUndoable()
    {
        StickData *master, *child, *grandchild;
        BuildIkChainFixture(&master, &child, &grandchild);
        view->resize(800, 600);
        view->SetTool(DRAW_IK);
        QTest::mouseClick(view->viewport(), Qt::LeftButton, Qt::NoModifier,
                          view->mapFromScene(QPointF(100, 100)));
        QTest::mouseClick(view->viewport(), Qt::LeftButton, Qt::NoModifier,
                          view->mapFromScene(QPointF(100, 350)));
        QCOMPARE(grandchild->ik_role, (qint32)StickData::IK_TARGET_ON);

        view->undo_stack.undo();
        // il frame è stato ricostruito dallo snapshot: ripesca gli stick per indice
        StickFigure *restored_figure = CurrentStickFigure();
        QCOMPARE(restored_figure->stick_list[2]->ik_role, (qint32)StickData::IK_NONE);

        view->undo_stack.redo();
        QCOMPARE(CurrentStickFigure()->stick_list[2]->ik_role, (qint32)StickData::IK_TARGET_ON);

        view->SetTool(NOTOOL);
    }

    // A single Undo of a 2-click chain creation must clear ALL roles (start included), so
    // both the start and the target can anchor a NEW chain. The gesture used to push two
    // undo steps, so one Undo reversed only the commit and left the start stuck on
    // IK_START (and the target on an orphan role) — neither reusable (Group D follow-up).
    void UndoingChainCreationClearsAllRolesForReuse()
    {
        StickData *master, *child, *grandchild;
        BuildIkChainFixture(&master, &child, &grandchild);
        view->resize(800, 600);
        view->SetTool(DRAW_IK);
        QTest::mouseClick(view->viewport(), Qt::LeftButton, Qt::NoModifier,
                          view->mapFromScene(QPointF(100, 100)));
        QTest::mouseClick(view->viewport(), Qt::LeftButton, Qt::NoModifier,
                          view->mapFromScene(QPointF(100, 350)));
        QCOMPARE(master->ik_role, (qint32)StickData::IK_START);
        QCOMPARE(grandchild->ik_role, (qint32)StickData::IK_TARGET_ON);

        view->undo_stack.undo(); // ONE undo must remove the whole gesture

        StickFigure *fig = CurrentStickFigure(); // frame rebuilt: old pointers are stale
        QCOMPARE(fig->stick_list[0]->ik_role, (qint32)StickData::IK_NONE); // start reusable
        QCOMPARE(fig->stick_list[2]->ik_role, (qint32)StickData::IK_NONE); // target reusable

        // and the same sticks can indeed anchor a fresh chain again
        QTest::mouseClick(view->viewport(), Qt::LeftButton, Qt::NoModifier,
                          view->mapFromScene(QPointF(100, 100)));
        QTest::mouseClick(view->viewport(), Qt::LeftButton, Qt::NoModifier,
                          view->mapFromScene(QPointF(100, 350)));
        QCOMPARE(fig->stick_list[0]->ik_role, (qint32)StickData::IK_START);
        QCOMPARE(fig->stick_list[2]->ik_role, (qint32)StickData::IK_TARGET_ON);

        view->SetTool(NOTOOL);
    }

    // ---- IK graph semantics under stick split (Chop) / merge (Weld) / figure merge+split ----

    // splitting an IK_NODE leaves both halves as IK_NODE and the chain intact (spec)
    void ChopIkNodeKeepsBothNodes()
    {
        StickData *master, *child, *grandchild;
        BuildIkChainFixture(&master, &child, &grandchild);
        StickFigure *figure = CurrentStickFigure();
        master->ik_role = StickData::IK_START;
        CommitIkChain(master, grandchild); // master(start)->child(node)->grandchild(target)
        QCOMPARE(child->ik_role, (qint32)StickData::IK_NODE);

        StickData *added = figure->Chop(child); // splitta il nodo
        QCOMPARE(child->ik_role, (qint32)StickData::IK_NODE);
        QCOMPARE(added->ik_role, (qint32)StickData::IK_NODE);
        QCOMPARE(master->ik_target, grandchild);
        QCOMPARE(grandchild->ik_start, master);
        QVERIFY(StickIsInActiveIkChain(added));
    }

    // splitting the target: the free-end child inherits the target role, the parent
    // becomes a node, and the start's target pointer follows the new effector (spec)
    void ChopIkTargetMovesRoleToChild()
    {
        StickData *master, *child, *grandchild;
        BuildIkChainFixture(&master, &child, &grandchild);
        StickFigure *figure = CurrentStickFigure();
        master->ik_role = StickData::IK_START;
        CommitIkChain(master, grandchild);

        StickData *added = figure->Chop(grandchild); // splitta il target
        QCOMPARE(added->ik_role, (qint32)StickData::IK_TARGET_ON);
        QCOMPARE(grandchild->ik_role, (qint32)StickData::IK_NODE);
        QCOMPARE(master->ik_target, added);
        QCOMPARE(added->ik_start, master);
        QVERIFY(StickIsInActiveIkChain(grandchild));
    }

    // splitting the start (beginning): parent keeps the start/anchor, child becomes a node
    void ChopIkStartKeepsRoleOnParent()
    {
        StickData *master, *child, *grandchild;
        BuildIkChainFixture(&master, &child, &grandchild);
        StickFigure *figure = CurrentStickFigure();
        master->ik_role = StickData::IK_START;
        CommitIkChain(master, grandchild);

        StickData *added = figure->Chop(master); // splitta l'inizio
        QCOMPARE(master->ik_role, (qint32)StickData::IK_START);
        QCOMPARE(added->ik_role, (qint32)StickData::IK_NODE);
        QCOMPARE(master->ik_target, grandchild);
        QVERIFY(StickIsInActiveIkChain(added));
    }

    // welding the target into its parent node makes the surviving stick the target (spec)
    void WeldTargetIntoNodeBecomesTarget()
    {
        StickData *master, *child, *grandchild;
        BuildIkChainFixture(&master, &child, &grandchild);
        StickFigure *figure = CurrentStickFigure();
        master->ik_role = StickData::IK_START;
        CommitIkChain(master, grandchild);

        QList<StickData*> welded = figure->Weld(child); // assorbe il target grandchild
        QVERIFY(welded.contains(grandchild));
        QCOMPARE(child->ik_role, (qint32)StickData::IK_TARGET_ON);
        QCOMPARE(master->ik_target, child);
        QCOMPARE(child->ik_start, master);
    }

    // welding a 2-stick chain's start onto its own target leaves no room for a chain:
    // the "check ancestry, delete if insufficient" pass dissolves it (spec)
    void WeldStartAndTargetDissolvesChain()
    {
        AddFrame();
        StickFigure *figure = CurrentStickFigure();
        StickData *start = DrawStick(QPointF(100, 100), QPointF(100, 200));
        StickData *target = DrawStick(QPointF(100, 200), QPointF(100, 300));
        start->ik_role = StickData::IK_START;
        CommitIkChain(start, target); // catena a 2 stick: start->target

        figure->Weld(start); // assorbe il target dentro lo start
        QCOMPARE(figure->stick_list.count(), 1);
        QCOMPARE(start->ik_role, (qint32)StickData::IK_NONE);
        QCOMPARE(start->ik_target, static_cast<StickData*>(nullptr));
    }

    // merging a figure that carries a chain grafts the chain (roles + pointers) into the host
    void MergeStickFiguresInheritsChain()
    {
        AddFrame();
        StickFigure *main_figure = CurrentStickFigure();
        StickData *main_stick = DrawStick(QPointF(100, 100), QPointF(100, 200));
        StickFigure *to_join = AddStick("joined");
        StickData *j_start = DrawStick(QPointF(300, 100), QPointF(300, 200));
        StickData *j_mid   = DrawStick(QPointF(300, 200), QPointF(300, 300));
        StickData *j_target= DrawStick(QPointF(300, 300), QPointF(300, 400));
        j_start->ik_role = StickData::IK_START;
        CommitIkChain(j_start, j_target);

        view->RemoveHandles(main_figure);
        MergeStickFigures(main_figure, main_stick, to_join);
        view->AddHandles(main_figure);

        // ritrova i cloni per geometria (l'ordine è: originale, linker, poi i cloni di to_join)
        StickData *start_clone = nullptr, *target_clone = nullptr;
        for(StickData *s : main_figure->stick_list){
            if(s->ik_role == StickData::IK_START) start_clone = s;
            if(s->ik_role == StickData::IK_TARGET_ON) target_clone = s;
        }
        QVERIFY(start_clone != nullptr);
        QVERIFY(target_clone != nullptr);
        QCOMPARE(start_clone->ik_target, target_clone);
        QCOMPARE(target_clone->ik_start, start_clone);
        QVERIFY(StickIsInActiveIkChain(target_clone));
    }

    // splitting a figure along a chain: a chain wholly inside the moved subtree is
    // inherited by the branch; a chain crossing the split point is broken on both sides (spec)
    void SplitStickFigureBreaksSpanningChainKeepsContained()
    {
        // contained: catena interamente sotto il punto di split
        {
            AddFrame();
            StickFigure *figure = CurrentStickFigure();
            /*master*/ DrawStick(QPointF(100, 100), QPointF(100, 200));
            StickData *b = DrawStick(QPointF(100, 200), QPointF(100, 300)); // punto di split (start della catena)
            StickData *c = DrawStick(QPointF(100, 300), QPointF(100, 400));
            StickData *d = DrawStick(QPointF(100, 400), QPointF(100, 500));
            b->ik_role = StickData::IK_START;
            CommitIkChain(b, d); // catena b->c->d, tutta nel sottoalbero di b
            Q_UNUSED(c);
            figure->current_stick = b;
            view->SplitStickFigure();
            StickFigure *branch = CurrentStickFigure();
            QVERIFY(branch != figure);
            StickData *bs = nullptr, *bt = nullptr;
            for(StickData *s : branch->stick_list){
                if(s->ik_role == StickData::IK_START) bs = s;
                if(s->ik_role == StickData::IK_TARGET_ON) bt = s;
            }
            QVERIFY(bs != nullptr); // la catena è stata ereditata dal branch
            QVERIFY(bt != nullptr);
            QCOMPARE(bs->ik_target, bt);
        }
        // spanning: start sopra il punto di split, target sotto → catena spezzata ovunque
        {
            AddFrame();
            StickFigure *figure = CurrentStickFigure();
            /*master*/ DrawStick(QPointF(500, 100), QPointF(500, 200));
            StickData *b = DrawStick(QPointF(500, 200), QPointF(500, 300)); // start
            StickData *c = DrawStick(QPointF(500, 300), QPointF(500, 400)); // punto di split (nodo)
            StickData *d = DrawStick(QPointF(500, 400), QPointF(500, 500)); // target
            b->ik_role = StickData::IK_START;
            CommitIkChain(b, d);
            figure->current_stick = c;
            view->SplitStickFigure();
            StickFigure *branch = CurrentStickFigure();
            // lato split: catena sciolta da DeleteStick(c)
            QCOMPARE(b->ik_role, (qint32)StickData::IK_NONE);
            // lato branch: nessun ruolo IK sopravvive (mancava lo start)
            for(StickData *s : branch->stick_list)
                QCOMPARE(s->ik_role, (qint32)StickData::IK_NONE);
        }
    }

    // ================= STK / .nodes conversion (STK_IMPORT_EXPORT_PLAN.md §7) =================

private:
    QTemporaryDir stk_temp_dir;

    static QString SamplePath(const QString &relative)
    {
        return QStringLiteral(TESTS_SOURCE_DIR "/data/") + relative;
    }

    QString WriteTempFile(const QByteArray &bytes)
    {
        static int counter = 0;
        QString path = stk_temp_dir.filePath(QStringLiteral("stk_case_%1.bin").arg(counter++));
        QFile file(path);
        file.open(QIODevice::WriteOnly);
        file.write(bytes);
        file.close();
        return path;
    }

    static QByteArray ReadFileBytes(const QString &path)
    {
        QFile file(path);
        file.open(QIODevice::ReadOnly);
        return file.readAll();
    }

    // raw zlib inflate via qUncompress (needs the 4-byte big-endian size-hint prefix)
    static QByteArray InflateZlib(const QByteArray &zlib_stream)
    {
        quint32 hint = quint32(zlib_stream.size()) * 30 + 1024;
        QByteArray framed;
        framed.append(char((hint >> 24) & 0xFF));
        framed.append(char((hint >> 16) & 0xFF));
        framed.append(char((hint >> 8) & 0xFF));
        framed.append(char(hint & 0xFF));
        framed.append(zlib_stream);
        return qUncompress(framed);
    }

    static void PutU16(QByteArray &out, quint16 v)
    {
        out.append(char(v & 0xFF));
        out.append(char((v >> 8) & 0xFF));
    }
    static void PutI32(QByteArray &out, qint32 v)
    {
        for (int i = 0; i < 4; i++)
            out.append(char((quint32(v) >> (8 * i)) & 0xFF));
    }
    static void PutF32(QByteArray &out, float v)
    {
        quint32 u;
        memcpy(&u, &v, 4);
        PutI32(out, qint32(u));
    }
    static void PutF64(QByteArray &out, double v)
    {
        quint64 u;
        memcpy(&u, &v, 8);
        for (int i = 0; i < 8; i++)
            out.append(char((u >> (8 * i)) & 0xFF));
    }

    // a modern-format .stk file from a hand-built payload ('z' + raw zlib stream)
    QString WriteModernStkFile(const QByteArray &payload)
    {
        QByteArray bytes;
        bytes.append(char(0x7A));
        bytes.append(qCompress(payload, 9).mid(4));
        return WriteTempFile(bytes);
    }

    // one default modern segment record (len 48, straight up, width 6) for flags 0xBF
    static QByteArray DefaultSegmentRecord()
    {
        QByteArray seg;
        PutF32(seg, 48.0f);
        PutF64(seg, -M_PI / 2);
        PutF32(seg, 6.0f);
        return seg;
    }

private slots:
    void StkImportBasicStickman()
    {
        QStringList warnings;
        QString error;
        StickFigureData *figure = StkFormat::ImportStk(
                    SamplePath("stk/basic stickman with a torso two arms two legs and a round head.stk"),
                    QPointF(400, 300), &warnings, &error);
        QVERIFY2(figure, qPrintable(error));
        QCOMPARE(figure->stick_list.count(), 6);
        QCOMPARE(figure->master_stick, figure->stick_list[0]);
        QCOMPARE(figure->current_stick, figure->master_stick);
        // torso->arm chain: attached at the master's free end, NOT a stepchild
        QCOMPARE(figure->stick_list[1]->parent, figure->master_stick);
        QVERIFY(!figure->stick_list[1]->stepchild);
        QCOMPARE(figure->stick_list[1]->my_line.p1(), figure->master_stick->my_line.p2());
        // legs: attached at the origin -> stepchildren sharing the master's P1
        QVERIFY(figure->stick_list[2]->stepchild);
        QCOMPARE(figure->stick_list[2]->parent, figure->master_stick);
        QCOMPARE(figure->stick_list[2]->my_line.p1(), figure->master_stick->my_line.p1());
        QVERIFY(figure->stick_list[3]->stepchild);
        // round head
        QCOMPARE(figure->stick_list[5]->stick_type, qint32(StickData::CIRCLE));
        // flattened all-descendants children list on the master
        QCOMPARE(figure->master_stick->children.count(), 5);
        QVERIFY(warnings.isEmpty());
        delete figure;
    }

    void StkImportSingleFeatureFiles()
    {
        QString error;
        QStringList warnings;

        StickFigureData *plain = StkFormat::ImportStk(SamplePath("stk/single stick.stk"),
                                                      QPointF(0, 0), &warnings, &error);
        QVERIFY2(plain, qPrintable(error));
        QCOMPARE(plain->stick_list.count(), 1);
        QCOMPARE(plain->stick_list[0]->stick_type, qint32(StickData::LINE));
        QCOMPARE(plain->stick_list[0]->pen.widthF(), 6.0);
        QCOMPARE(plain->stick_list[0]->pen.capStyle(), Qt::RoundCap);
        QCOMPARE(plain->stick_list[0]->pen.color(), QColor(0, 0, 0));
        QCOMPARE(plain->stick_list[0]->my_line.length(), 48.0);
        delete plain;

        StickFigureData *thick = StkFormat::ImportStk(SamplePath("stk/single stick with increased line fill.stk"),
                                                      QPointF(0, 0), &warnings, &error);
        QVERIFY2(thick, qPrintable(error));
        QCOMPARE(thick->stick_list[0]->pen.widthF(), 11.0);
        delete thick;

        StickFigureData *squared = StkFormat::ImportStk(SamplePath("stk/single squared stick.stk"),
                                                        QPointF(0, 0), &warnings, &error);
        QVERIFY2(squared, qPrintable(error));
        QCOMPARE(squared->stick_list[0]->stick_type, qint32(StickData::LINE));
        QCOMPARE(squared->stick_list[0]->pen.capStyle(), Qt::SquareCap);
        delete squared;

        StickFigureData *red = StkFormat::ImportStk(SamplePath("stk/single squared stick red color.stk"),
                                                    QPointF(0, 0), &warnings, &error);
        QVERIFY2(red, qPrintable(error));
        QCOMPARE(red->stick_list[0]->pen.color(), QColor(255, 0, 0));
        delete red;

        StickFigureData *circle = StkFormat::ImportStk(SamplePath("stk/single circle.stk"),
                                                       QPointF(0, 0), &warnings, &error);
        QVERIFY2(circle, qPrintable(error));
        QCOMPARE(circle->stick_list[0]->stick_type, qint32(StickData::CIRCLE));
        QCOMPARE(circle->stick_list[0]->brush.color(), QColor(Qt::white));
        QCOMPARE(circle->stick_list[0]->img_w_scale, 1.0f);
        delete circle;

        StickFigureData *filled = StkFormat::ImportStk(SamplePath("stk/single circle with line and fill color changed.stk"),
                                                       QPointF(0, 0), &warnings, &error);
        QVERIFY2(filled, qPrintable(error));
        QCOMPARE(filled->stick_list[0]->stick_type, qint32(StickData::CIRCLE));
        QCOMPARE(filled->stick_list[0]->brush.color(), QColor(0xBB, 0x47, 0x47));
        delete filled;

        StickFigureData *chain = StkFormat::ImportStk(SamplePath("stk/two contactened sticks.stk"),
                                                      QPointF(0, 0), &warnings, &error);
        QVERIFY2(chain, qPrintable(error));
        QCOMPARE(chain->stick_list.count(), 2);
        QCOMPARE(chain->stick_list[1]->parent, chain->stick_list[0]);
        QVERIFY(!chain->stick_list[1]->stepchild);
        QCOMPARE(chain->stick_list[1]->my_line.p1(), chain->stick_list[0]->my_line.p2());
        delete chain;
    }

    // the polyfill of this sample (3 verts, red) now imports as a POLYGON stick
    // attached to the joint of its first vertex, instead of being skipped
    void StkImportPolyfillTriangleWarns()
    {
        QStringList warnings;
        QString error;
        StickFigureData *figure = StkFormat::ImportStk(
                    SamplePath("stk/two sticks and a polygon fill forming a red triangle.stk"),
                    QPointF(0, 0), &warnings, &error);
        QVERIFY2(figure, qPrintable(error));
        QCOMPARE(figure->stick_list.count(), 3); // 2 segments + the imported polygon
        StickData *poly = figure->stick_list.last();
        QCOMPARE(poly->stick_type, qint32(StickData::POLYGON));
        QCOMPARE(poly->polygon_points.count(), 3);
        QCOMPARE(poly->brush.color().red(), 255);
        QVERIFY(poly->parent != nullptr); // welded to the joint of its first vertex
        // the triangle's vertices are the segments' joints: each mapped vertex must sit
        // on an endpoint of one of the two imported segments
        QTransform t = poly->PolygonTransform();
        for (const QPointF &pt : poly->polygon_points) {
            QPointF v = t.map(pt);
            bool on_a_joint = false;
            for (StickData *s : figure->stick_list) {
                if (s == poly)
                    continue;
                on_a_joint |= QLineF(v, s->my_line.p1()).length() < 0.1
                        || QLineF(v, s->my_line.p2()).length() < 0.1;
            }
            QVERIFY(on_a_joint);
        }
        QVERIFY(warnings.join(" ").contains("polygon"));
        delete figure;
    }

    // builds a LINE master + POLYGON child, exports, re-imports, and checks the polygon
    // survives with its shape intact (relative to the master origin — both directions
    // recenter the figure). The vertices travel as invisible carrier segments/nodes plus
    // a polyfill record, so the re-import also contains those extra invisible sticks.
    void StkAndNodesExportImportPolygonRoundTrip()
    {
        QVector<QPointF> triangle { QPointF(0, 100), QPointF(70, 150), QPointF(-10, 180) };
        auto build_figure = [&triangle](StickFigureData &figure) {
            StickData *master = new StickData();
            master->my_line = QLineF(0, 0, 0, 100);
            master->pen.setWidthF(6.0);
            master->pen.setCapStyle(Qt::RoundCap);
            figure.stick_list.append(master);
            figure.master_stick = master;
            figure.current_stick = master;

            StickData *poly = new StickData();
            poly->stick_type = StickData::POLYGON;
            poly->my_line = QLineF(0, 100, 70, 150);
            poly->img_angle = float(-atan2(poly->my_line.dx(), poly->my_line.dy()) * 180.0 / M_PI);
            poly->parent = master;
            master->children.append(poly);
            QPen no_pen;
            no_pen.setStyle(Qt::NoPen);
            poly->pen = no_pen;
            poly->brush = QBrush(QColor(200, 30, 30));
            poly->z = 1;
            QTransform inverse = poly->PolygonTransform().inverted();
            for (const QPointF &v : triangle)
                poly->polygon_points.append(inverse.map(v));
            figure.stick_list.append(poly);
        };
        // valida la geometria reimportata confrontando i vertici RELATIVI all'origine
        // del master (il recentering trasla tutto rigidamente)
        auto check_import = [&triangle](StickFigureData *imported) {
            StickData *poly = nullptr;
            int polygons = 0;
            for (StickData *s : imported->stick_list) {
                if (s->stick_type == StickData::POLYGON) {
                    poly = s;
                    polygons++;
                }
            }
            QCOMPARE(polygons, 1);
            QCOMPARE(poly->polygon_points.count(), 3);
            QCOMPARE(poly->brush.color(), QColor(200, 30, 30));
            QPointF origin = imported->master_stick->my_line.p1();
            QTransform t = poly->PolygonTransform();
            for (int i = 0; i < 3; i++) {
                QPointF v = t.map(poly->polygon_points[i]) - origin;
                QVERIFY2(QLineF(v, triangle[i]).length() < 0.1,
                         qPrintable(QString("vertex %1 drifted: (%2,%3) vs (%4,%5)")
                                    .arg(i).arg(v.x()).arg(v.y())
                                    .arg(triangle[i].x()).arg(triangle[i].y())));
            }
        };

        QStringList warnings;
        QString error;

        StickFigureData stk_figure;
        build_figure(stk_figure);
        QString stk_path = stk_temp_dir.filePath("polygon_roundtrip.stk");
        QVERIFY2(StkFormat::ExportStk(stk_path, stk_figure, StkFormat::PIVOT5, &warnings, &error),
                 qPrintable(error));
        StickFigureData *stk_back = StkFormat::ImportStk(stk_path, QPointF(0, 0), &warnings, &error);
        QVERIFY2(stk_back, qPrintable(error));
        QCOMPARE(stk_back->stick_list.count(), 6); // master + skeleton + 3 carriers + polygon
        check_import(stk_back);
        delete stk_back;

        StickFigureData nodes_figure;
        build_figure(nodes_figure);
        QString nodes_path = stk_temp_dir.filePath("polygon_roundtrip.nodes");
        QVERIFY2(StkFormat::ExportNodes(nodes_path, nodes_figure, &warnings, &error),
                 qPrintable(error));
        StickFigureData *nodes_back = StkFormat::ImportNodes(nodes_path, QPointF(0, 0), &warnings, &error);
        QVERIFY2(nodes_back, qPrintable(error));
        QCOMPARE(nodes_back->stick_list.count(), 6);
        check_import(nodes_back);
        delete nodes_back;
    }

    // a MULTILINE exports to both formats as a chain of welded segments (one per path
    // edge, thickness = mean of the edge's two vertex widths); re-importing yields the
    // chain with the vertices in place
    void StkAndNodesExportMultilineChain()
    {
        QVector<QPointF> path { QPointF(0, 0), QPointF(30, 40), QPointF(80, 10), QPointF(120, 50) };
        auto build_figure = [&path](StickFigureData &figure) {
            StickData *multi = new StickData();
            multi->stick_type = StickData::MULTILINE;
            multi->my_line = QLineF(path.first(), path.last());
            multi->img_angle = float(-atan2(multi->my_line.dx(), multi->my_line.dy()) * 180.0 / M_PI);
            multi->pen.setWidthF(6.0);
            multi->pen.setCapStyle(Qt::RoundCap);
            QTransform inverse = multi->PolygonTransform().inverted();
            for (int i = 1; i < path.count() - 1; i++)
                multi->polygon_points.append(inverse.map(path[i]));
            multi->vertex_widths << 6.0f << 8.0f << 10.0f << 4.0f;
            multi->vertex_curvatures << 0.0f << 0.0f << 0.0f << 0.0f;
            figure.stick_list.append(multi);
            figure.master_stick = multi;
            figure.current_stick = multi;
        };
        // il reimport dà una catena di 3 segmenti: verifica che i giunti ricadano sui
        // vertici del percorso (relativi al primo giunto — il recentering trasla tutto)
        auto check_chain = [&path](StickFigureData *imported) {
            QCOMPARE(imported->stick_list.count(), 3);
            QPointF origin = imported->stick_list[0]->my_line.p1();
            for (int i = 0; i < 3; i++) {
                QCOMPARE(imported->stick_list[i]->stick_type, qint32(StickData::LINE));
                QPointF joint = imported->stick_list[i]->my_line.p2() - origin;
                QVERIFY2(QLineF(joint, path[i + 1]).length() < 0.1,
                         qPrintable(QString("joint %1 drifted").arg(i)));
            }
            // spessore del primo tratto = media delle larghezze dei suoi due vertici
            QVERIFY(qAbs(imported->stick_list[0]->pen.widthF() - 7.0) < 0.51); // .nodes arrotonda a int
        };

        QStringList warnings;
        QString error;

        StickFigureData stk_figure;
        build_figure(stk_figure);
        QString stk_path = stk_temp_dir.filePath("multiline_roundtrip.stk");
        QVERIFY2(StkFormat::ExportStk(stk_path, stk_figure, StkFormat::PIVOT5, &warnings, &error),
                 qPrintable(error));
        StickFigureData *stk_back = StkFormat::ImportStk(stk_path, QPointF(0, 0), &warnings, &error);
        QVERIFY2(stk_back, qPrintable(error));
        check_chain(stk_back);
        delete stk_back;

        StickFigureData nodes_figure;
        build_figure(nodes_figure);
        QString nodes_path = stk_temp_dir.filePath("multiline_roundtrip.nodes");
        QVERIFY2(StkFormat::ExportNodes(nodes_path, nodes_figure, &warnings, &error),
                 qPrintable(error));
        StickFigureData *nodes_back = StkFormat::ImportNodes(nodes_path, QPointF(0, 0), &warnings, &error);
        QVERIFY2(nodes_back, qPrintable(error));
        check_chain(nodes_back);
        delete nodes_back;

        // with a branch (30,40)->(30,100) (vertex_parents): both formats hang its segment off
        // the joint at (30,40), not off the end of the chain
        auto build_branched = [&build_figure](StickFigureData &figure) {
            build_figure(figure);
            StickData *multi = figure.stick_list.first();
            multi->polygon_points.append(multi->PolygonTransform().inverted().map(QPointF(30, 100)));
            multi->vertex_widths.insert(3, 5.0f);
            multi->vertex_curvatures << 0.0f;
            multi->vertex_parents = {-1, 0, 1, 1, 2}; // P1, (30,40), (80,10), branch, P2
        };
        auto check_branch = [](StickFigureData *imported) {
            QCOMPARE(imported->stick_list.count(), 4);
            QPointF origin = imported->stick_list[0]->my_line.p1();
            StickData *branch = nullptr;
            for (StickData *s : imported->stick_list)
                if (QLineF(s->my_line.p2() - origin, QPointF(30, 100)).length() < 0.1)
                    branch = s;
            QVERIFY2(branch, "the branch segment is missing");
            QVERIFY2(QLineF(branch->my_line.p1() - origin, QPointF(30, 40)).length() < 0.1,
                     "the branch does not start at its vertex");
            QVERIFY(branch->parent);
            QVERIFY2(QLineF(branch->parent->my_line.p2() - origin, QPointF(30, 40)).length() < 0.1,
                     "the branch hangs from the wrong joint");
        };
        StickFigureData stk_branched;
        build_branched(stk_branched);
        QString stk_branched_path = stk_temp_dir.filePath("multiline_branched.stk");
        QVERIFY2(StkFormat::ExportStk(stk_branched_path, stk_branched, StkFormat::PIVOT5, &warnings, &error),
                 qPrintable(error));
        StickFigureData *stk_branched_back = StkFormat::ImportStk(stk_branched_path, QPointF(0, 0), &warnings, &error);
        QVERIFY2(stk_branched_back, qPrintable(error));
        check_branch(stk_branched_back);
        delete stk_branched_back;
        StickFigureData nodes_branched;
        build_branched(nodes_branched);
        QString nodes_branched_path = stk_temp_dir.filePath("multiline_branched.nodes");
        QVERIFY2(StkFormat::ExportNodes(nodes_branched_path, nodes_branched, &warnings, &error),
                 qPrintable(error));
        StickFigureData *nodes_branched_back = StkFormat::ImportNodes(nodes_branched_path, QPointF(0, 0), &warnings, &error);
        QVERIFY2(nodes_branched_back, qPrintable(error));
        check_branch(nodes_branched_back);
        delete nodes_branched_back;

        QVERIFY(warnings.join(" ").contains("multi-line"));
    }

    void StkImportStaticSampleWarns()
    {
        QStringList warnings;
        QString error;
        StickFigureData *figure = StkFormat::ImportStk(
                    SamplePath("stk/three consecutive sticks with the middle of them being a static segment.stk"),
                    QPointF(0, 0), &warnings, &error);
        QVERIFY2(figure, qPrintable(error));
        QCOMPARE(figure->stick_list.count(), 3);
        QCOMPARE(figure->stick_list[1]->parent, figure->stick_list[0]);
        QCOMPARE(figure->stick_list[2]->parent, figure->stick_list[1]);
        QVERIFY(warnings.join(" ").contains("1 static"));
        delete figure;
    }

    void StkImportRejectsCorruptFiles()
    {
        QStringList warnings;
        QString error;

        // wrong marker byte
        QVERIFY(!StkFormat::ImportStk(WriteTempFile("xjunkjunkjunk"), QPointF(0, 0), &warnings, &error));
        QVERIFY(!error.isEmpty());

        // 'z' marker but not a zlib stream
        QVERIFY(!StkFormat::ImportStk(WriteTempFile("z_not_zlib_data"), QPointF(0, 0), &warnings, &error));

        // unknown flag bits (bit 7 cleared)
        QByteArray bad_flags;
        bad_flags.append(char(0x3F));
        PutI32(bad_flags, 1);
        bad_flags.append(DefaultSegmentRecord());
        PutU16(bad_flags, 0);
        PutU16(bad_flags, 0);
        QVERIFY(!StkFormat::ImportStk(WriteModernStkFile(bad_flags), QPointF(0, 0), &warnings, &error));
        QVERIFY(error.contains("variant"));

        // absurd segment count
        QByteArray bad_count;
        bad_count.append(char(0xBF));
        PutI32(bad_count, 1000000000);
        QVERIFY(!StkFormat::ImportStk(WriteModernStkFile(bad_count), QPointF(0, 0), &warnings, &error));

        // forward parent reference
        QByteArray forward;
        forward.append(char(0xBF));
        PutI32(forward, 2);
        forward.append(DefaultSegmentRecord());
        PutU16(forward, 5); // segment 2 claims to hang off segment 5
        forward.append(DefaultSegmentRecord());
        PutU16(forward, 0);
        PutU16(forward, 0);
        QVERIFY(!StkFormat::ImportStk(WriteModernStkFile(forward), QPointF(0, 0), &warnings, &error));

        // truncated mid-segment
        QByteArray truncated;
        truncated.append(char(0xBF));
        PutI32(truncated, 2);
        truncated.append(DefaultSegmentRecord());
        QVERIFY(!StkFormat::ImportStk(WriteModernStkFile(truncated), QPointF(0, 0), &warnings, &error));
    }

    // pins the writer to the exact byte layout Pivot produced for the same content
    void StkExportMatchesPivotBytes()
    {
        StickFigureData figure;
        StickData *stick = new StickData();
        stick->my_line = QLineF(100, 100, 100, 52); // straight up, length 48
        stick->stick_type = StickData::LINE;
        stick->pen = QPen(QColor(0, 0, 0));
        stick->pen.setWidthF(6.0);
        stick->pen.setCapStyle(Qt::RoundCap);
        figure.stick_list.append(stick);
        figure.master_stick = stick;

        QStringList warnings;
        QString error;
        QString out_path = stk_temp_dir.filePath("pin_modern.stk");
        QVERIFY2(StkFormat::ExportStk(out_path, figure, StkFormat::PIVOT5, &warnings, &error),
                 qPrintable(error));

        QByteArray exported = ReadFileBytes(out_path);
        QByteArray sample = ReadFileBytes(SamplePath("stk/single stick.stk"));
        QCOMPARE(quint8(exported.at(0)), quint8(0x7A));
        QCOMPARE(InflateZlib(exported.mid(1)), InflateZlib(sample.mid(1)));
        QVERIFY(warnings.isEmpty());
    }

    // Neither foreign format has a fill beyond a flat colour, so a gradient/pattern/image
    // one is dropped on export — but to its FIRST STOP (or the tile's average), never to
    // QBrush::color(), which is an unset black for those styles and would silently blacken
    // the figure. Both exporters warn once with the count.
    void ForeignExportFlattensNonSolidFillsAndWarns()
    {
        StickFigureData figure;
        StickData *master = new StickData();
        master->my_line = QLineF(100, 100, 100, 60);
        master->stick_type = StickData::LINE;
        master->pen = QPen(QColor(0, 0, 0), 4.0);
        figure.stick_list.append(master);
        figure.master_stick = master;

        // riempimento a gradiente: il primo stop è verde, QBrush::color() sarebbe nero
        QLinearGradient gradient(QPointF(0, 0), QPointF(1, 0));
        gradient.setCoordinateMode(QGradient::ObjectMode);
        gradient.setStops({ QGradientStop(0.0, QColor(0, 200, 0)),
                            QGradientStop(1.0, QColor(0, 0, 200)) });
        StickData *filled = new StickData();
        filled->my_line = QLineF(100, 60, 100, 20);
        filled->stick_type = StickData::CIRCLE;
        filled->pen = QPen(QColor(0, 0, 0), 2.0);
        filled->brush = QBrush(gradient);
        filled->img_w_scale = 1;
        filled->parent = master;
        filled->z = 0.01;
        figure.stick_list.append(filled);

        for (int variant = 0; variant < 2; variant++) {
            QStringList warnings;
            QString error;
            const bool nodes = (variant == 1);
            const QString out_path = stk_temp_dir.filePath(nodes ? "flat_fill.nodes"
                                                                 : "flat_fill.stk");
            QVERIFY2(nodes ? StkFormat::ExportNodes(out_path, figure, &warnings, &error)
                           : StkFormat::ExportStk(out_path, figure, StkFormat::PIVOT5,
                                                  &warnings, &error),
                     qPrintable(error));
            QVERIFY(warnings.filter("flat color").count() == 1);
        }

        // e il colore appiattito è davvero il primo stop, non il nero di default
        QStringList warnings;
        QString error;
        const QString round_trip = stk_temp_dir.filePath("flat_fill_back.stk");
        QVERIFY2(StkFormat::ExportStk(round_trip, figure, StkFormat::PIVOT5, &warnings, &error),
                 qPrintable(error));
        StickFigureData *back = StkFormat::ImportStk(round_trip, QPointF(0, 0), &warnings, &error);
        QVERIFY2(back != nullptr, qPrintable(error));
        bool found_green = false;
        for (StickData *stick : back->stick_list)
            if (stick->brush.style() != Qt::NoBrush && stick->brush.color() == QColor(0, 200, 0))
                found_green = true;
        QVERIFY(found_green);
        delete back; // ~StickFigureData() owns stick_list (come negli altri test di import)

        // il nativo invece non appiattisce nulla: stesso StickData, gradiente intatto
        QByteArray bytes;
        {
            QDataStream out(&bytes, QIODevice::WriteOnly);
            out << *filled;
        }
        StickData native;
        {
            QDataStream in(&bytes, QIODevice::ReadOnly);
            in >> native;
        }
        QCOMPARE(native.brush.style(), Qt::LinearGradientPattern);
    }

    void StkExportImportRoundTrip()
    {
        StickFigureData figure;
        StickData *master = new StickData();
        master->my_line = QLineF(200, 200, 200, 150);
        master->stick_type = StickData::LINE;
        master->pen = QPen(QColor(200, 30, 30));
        master->pen.setWidthF(6.0);
        master->pen.setCapStyle(Qt::RoundCap);
        master->z = 0.00;
        figure.stick_list.append(master);
        figure.master_stick = master;

        StickData *circle = new StickData();
        circle->my_line = QLineF(200, 150, 200, 100);
        circle->stick_type = StickData::CIRCLE;
        circle->pen = QPen(QColor(0, 0, 0));
        circle->pen.setWidthF(4.0);
        circle->brush = QBrush(Qt::white);
        circle->img_w_scale = 1;
        circle->parent = master;
        circle->z = 0.01;
        figure.stick_list.append(circle);

        StickData *step = new StickData();
        step->my_line = QLineF(200, 200, 250, 200);
        step->stick_type = StickData::LINE;
        step->pen = QPen(QColor(30, 30, 200));
        step->pen.setWidthF(3.0);
        step->pen.setCapStyle(Qt::SquareCap);
        step->parent = master;
        step->stepchild = true;
        step->z = 0.02;
        figure.stick_list.append(step);

        StickData *rect = new StickData();
        rect->my_line = QLineF(200, 100, 240, 80);
        rect->stick_type = StickData::RECT;
        rect->pen = QPen(QColor(20, 120, 20));
        rect->brush = QBrush(QColor(20, 120, 20));
        rect->img_w_scale = 12;
        rect->parent = circle;
        rect->z = 0.03;
        figure.stick_list.append(rect);

        QStringList export_warnings;
        QString error;
        QString out_path = stk_temp_dir.filePath("roundtrip.stk");
        QVERIFY2(StkFormat::ExportStk(out_path, figure, StkFormat::PIVOT5, &export_warnings, &error),
                 qPrintable(error));
        QVERIFY(export_warnings.join(" ").contains("plain Pivot segments")); // the RECT

        // reimport centered on the original bounding box so coordinates line up
        QRectF bounds(QPointF(200, 80), QPointF(250, 200));
        QStringList import_warnings;
        StickFigureData *loaded = StkFormat::ImportStk(out_path, bounds.center(),
                                                       &import_warnings, &error);
        QVERIFY2(loaded, qPrintable(error));
        QCOMPARE(loaded->stick_list.count(), 4);
        for (int i = 0; i < 4; i++) {
            QVERIFY2(QLineF(loaded->stick_list[i]->my_line.p1(), figure.stick_list[i]->my_line.p1()).length() < 1e-3
                     && QLineF(loaded->stick_list[i]->my_line.p2(), figure.stick_list[i]->my_line.p2()).length() < 1e-3,
                     qPrintable(QStringLiteral("stick %1 moved").arg(i)));
        }
        QCOMPARE(loaded->stick_list[0]->stick_type, qint32(StickData::LINE));
        QCOMPARE(loaded->stick_list[0]->pen.color(), QColor(200, 30, 30));
        QCOMPARE(loaded->stick_list[1]->stick_type, qint32(StickData::CIRCLE));
        QCOMPARE(loaded->stick_list[1]->brush.color(), QColor(Qt::white));
        QVERIFY(loaded->stick_list[2]->stepchild);
        QCOMPARE(loaded->stick_list[2]->pen.capStyle(), Qt::SquareCap);
        // the RECT degrades to a square-capped line of the same width
        QCOMPARE(loaded->stick_list[3]->stick_type, qint32(StickData::LINE));
        QCOMPARE(loaded->stick_list[3]->pen.capStyle(), Qt::SquareCap);
        QCOMPARE(loaded->stick_list[3]->pen.widthF(), 12.0);
        delete loaded;
    }

    void StkImportV2Files()
    {
        QStringList warnings;
        QString error;

        StickFigureData *plain = StkFormat::ImportStk(SamplePath("stk/v2/single stick.stk"),
                                                      QPointF(0, 0), &warnings, &error);
        QVERIFY2(plain, qPrintable(error));
        QCOMPARE(plain->stick_list.count(), 1);
        QCOMPARE(plain->stick_list[0]->stick_type, qint32(StickData::LINE));
        QCOMPARE(plain->stick_list[0]->pen.widthF(), 5.0);
        // v2 angles live in [0, 2pi) but must decode to the same direction: straight up
        QVERIFY(qAbs(plain->stick_list[0]->my_line.dx()) < 1e-3);
        QVERIFY(plain->stick_list[0]->my_line.dy() < 0);
        QVERIFY(qAbs(plain->stick_list[0]->my_line.length() - 48.0) < 1e-3);
        delete plain;

        StickFigureData *stickman = StkFormat::ImportStk(
                    SamplePath("stk/v2/basic stickman with a torso two arms two legs and a round head.stk"),
                    QPointF(0, 0), &warnings, &error);
        QVERIFY2(stickman, qPrintable(error));
        QCOMPARE(stickman->stick_list.count(), 6);
        QCOMPARE(stickman->stick_list[1]->parent, stickman->master_stick); // arm at the neck
        QCOMPARE(stickman->stick_list[3]->stick_type, qint32(StickData::CIRCLE)); // the head
        QVERIFY(stickman->stick_list[4]->stepchild); // legs at the origin
        QVERIFY(stickman->stick_list[5]->stepchild);
        QCOMPARE(stickman->stick_list[0]->pen.widthF(), 15.0);
        delete stickman;

        warnings.clear();
        StickFigureData *chain = StkFormat::ImportStk(
                    SamplePath("stk/v2/three consecutive sticks with the middle of them being a static segment.stk"),
                    QPointF(0, 0), &warnings, &error);
        QVERIFY2(chain, qPrintable(error));
        QCOMPARE(chain->stick_list.count(), 3);
        QCOMPARE(chain->stick_list[2]->parent, chain->stick_list[1]);
        QVERIFY(warnings.join(" ").contains("1 static"));
        delete chain;
    }

    void StkImportRejectsCorruptV2()
    {
        QStringList warnings;
        QString error;
        QByteArray valid = ReadFileBytes(SamplePath("stk/v2/single stick.stk"));

        // size not matching 2 + 24*count
        QVERIFY(!StkFormat::ImportStk(WriteTempFile(valid + char(0)), QPointF(0, 0), &warnings, &error));

        // non-sequential segment id
        QByteArray bad_id = valid;
        bad_id[3] = char(2); // id u16 low byte at record offset 1
        QVERIFY(!StkFormat::ImportStk(WriteTempFile(bad_id), QPointF(0, 0), &warnings, &error));

        // parent joint referencing itself/forward
        QByteArray bad_parent = valid;
        bad_parent[2] = char(1); // parent u8 of segment 1 must be 0
        QVERIFY(!StkFormat::ImportStk(WriteTempFile(bad_parent), QPointF(0, 0), &warnings, &error));
    }

    void StkExportV2MatchesPivotBytesAndLimits()
    {
        StickFigureData figure;
        StickData *stick = new StickData();
        stick->my_line = QLineF(100, 100, 100, 52); // straight up, length 48
        stick->stick_type = StickData::LINE;
        stick->pen = QPen(QColor(0, 0, 0));
        stick->pen.setWidthF(5.0);
        stick->pen.setCapStyle(Qt::RoundCap);
        figure.stick_list.append(stick);
        figure.master_stick = stick;

        QStringList warnings;
        QString error;
        QString out_path = stk_temp_dir.filePath("pin_v2.stk");
        QVERIFY2(StkFormat::ExportStk(out_path, figure, StkFormat::PIVOT2, &warnings, &error),
                 qPrintable(error));
        QCOMPARE(ReadFileBytes(out_path), ReadFileBytes(SamplePath("stk/v2/single stick.stk")));

        // the u8 count/parent fields cap the v2 format at 255 sticks
        StickFigureData big;
        StickData *big_master = new StickData();
        big_master->my_line = QLineF(0, 0, 0, 50);
        big.stick_list.append(big_master);
        big.master_stick = big_master;
        for (int i = 0; i < 255; i++) {
            StickData *child = new StickData();
            child->my_line = QLineF(0, 50, 10, 60);
            child->parent = big_master;
            big.stick_list.append(child);
        }
        QVERIFY(!StkFormat::ExportStk(stk_temp_dir.filePath("too_big.stk"), big,
                                      StkFormat::PIVOT2, &warnings, &error));
        QVERIFY(error.contains("255"));
    }

    void NodesImportZedStickman()
    {
        QStringList warnings;
        QString error;
        StickFigureData *figure = StkFormat::ImportNodes(SamplePath("nodes/Zed-Stickman.nodes"),
                                                         QPointF(0, 0), &warnings, &error);
        QVERIFY2(figure, qPrintable(error));
        QCOMPARE(figure->stick_list.count(), 91); // 92 nodes minus the pure-joint root
        QCOMPARE(figure->master_stick, figure->stick_list[0]);
        QCOMPARE(figure->master_stick->children.count(), 90); // ancestor of everything

        int origin_stepchildren = 0, circles = 0, trapezoids = 0;
        for (StickData *stick : figure->stick_list) {
            if (stick->stepchild && stick->parent == figure->master_stick)
                origin_stepchildren++;
            if (stick->stick_type == StickData::CIRCLE)
                circles++;
            if (stick->stick_type == StickData::TRAPEZOID)
                trapezoids++;
        }
        QCOMPARE(origin_stepchildren, 2); // the root had 3 children: master + 2
        QCOMPARE(circles, 1);
        QCOMPARE(trapezoids, 10); // the 10 triangle nodes

        // header scale 1.5 applied to geometry and thickness (node 1: len 132.73, th 50)
        QVERIFY(qAbs(figure->master_stick->my_line.length() - 132.7297 * 1.5) < 0.5);
        QVERIFY(qAbs(figure->master_stick->pen.widthF() - 50 * 1.5) < 1e-6);

        QString joined = warnings.join(" ");
        QVERIFY(joined.contains("72 static"));
        QVERIFY(joined.contains("33 stretchy"));
        delete figure;
    }

    void NodesExportRoundTrip()
    {
        // line + white circle + origin-stepchild + taper, deliberately listing the
        // master NOT first in draw order (z) to exercise the draw_order ranks
        StickFigureData figure;
        StickData *master = new StickData();
        master->my_line = QLineF(300, 300, 300, 250);
        master->stick_type = StickData::LINE;
        master->pen = QPen(QColor(10, 20, 30));
        master->pen.setWidthF(6.0);
        master->pen.setCapStyle(Qt::RoundCap);
        master->z = 0.02; // drawn ABOVE the circle
        figure.stick_list.append(master);
        figure.master_stick = master;

        StickData *circle = new StickData();
        circle->my_line = QLineF(300, 250, 300, 200);
        circle->stick_type = StickData::CIRCLE;
        circle->pen = QPen(QColor(0, 0, 0));
        circle->pen.setWidthF(4.0);
        circle->brush = QBrush(Qt::white);
        circle->img_w_scale = 1;
        circle->parent = master;
        circle->z = 0.01;
        figure.stick_list.append(circle);

        StickData *step = new StickData();
        step->my_line = QLineF(300, 300, 350, 310);
        step->stick_type = StickData::LINE;
        step->pen = QPen(QColor(200, 10, 10, 128)); // semi-transparent
        step->pen.setWidthF(3.0);
        step->pen.setCapStyle(Qt::SquareCap);
        step->parent = master;
        step->stepchild = true;
        step->z = 0.03;
        figure.stick_list.append(step);

        StickData *taper = new StickData();
        taper->my_line = QLineF(300, 200, 340, 180);
        taper->stick_type = StickData::TAPER;
        taper->pen = QPen(QColor(20, 120, 20));
        taper->brush = QBrush(QColor(20, 120, 20));
        taper->img_w_scale = 14;
        taper->img_h_scale = 6;
        taper->parent = circle;
        taper->z = 0.04;
        figure.stick_list.append(taper);

        QStringList export_warnings;
        QString error;
        QString out_path = stk_temp_dir.filePath("roundtrip.nodes");
        QVERIFY2(StkFormat::ExportNodes(out_path, figure, &export_warnings, &error),
                 qPrintable(error));

        // the payload must be the newest dialect: bare zlib, v403 b72 little-endian
        QByteArray payload = InflateZlib(ReadFileBytes(out_path));
        QVERIFY(payload.size() > 12);
        QCOMPARE(qFromLittleEndian<qint32>(reinterpret_cast<const uchar *>(payload.constData())), 403);
        QCOMPARE(qFromLittleEndian<qint32>(reinterpret_cast<const uchar *>(payload.constData()) + 4), 72);

        QRectF bounds(QPointF(300, 180), QPointF(350, 310));
        QStringList import_warnings;
        StickFigureData *loaded = StkFormat::ImportNodes(out_path, bounds.center(),
                                                         &import_warnings, &error);
        QVERIFY2(loaded, qPrintable(error));
        QCOMPARE(loaded->stick_list.count(), 4);
        QCOMPARE(loaded->master_stick, loaded->stick_list[0]); // master survives z-reordering

        // .nodes files are depth-first pre-order, so the reimported list order is
        // master, circle, taper, then the origin-stepchild
        const int original_index[4] = {0, 1, 3, 2};
        for (int i = 0; i < 4; i++) {
            StickData *expected = figure.stick_list[original_index[i]];
            StickData *actual = loaded->stick_list[i];
            QVERIFY2(QLineF(actual->my_line.p1(), expected->my_line.p1()).length() < 1e-2
                     && QLineF(actual->my_line.p2(), expected->my_line.p2()).length() < 1e-2,
                     qPrintable(QStringLiteral("stick %1 moved").arg(i)));
        }
        StickData *loaded_master = loaded->stick_list[0];
        StickData *loaded_circle = loaded->stick_list[1];
        StickData *loaded_taper = loaded->stick_list[2];
        StickData *loaded_step = loaded->stick_list[3];
        // draw order came through the draw_order_index ranks, not the file order
        QVERIFY(loaded_circle->z < loaded_master->z);
        QVERIFY(loaded_master->z < loaded_step->z);
        QVERIFY(loaded_step->z < loaded_taper->z);
        QCOMPARE(loaded_master->pen.color(), QColor(10, 20, 30));
        QCOMPARE(loaded_master->pen.capStyle(), Qt::RoundCap);
        QCOMPARE(loaded_circle->stick_type, qint32(StickData::CIRCLE));
        QCOMPARE(loaded_circle->brush.color(), QColor(Qt::white));
        QCOMPARE(loaded_circle->pen.color(), QColor(0, 0, 0)); // outline color kept
        QVERIFY(loaded_step->stepchild);
        QCOMPARE(loaded_step->pen.color(), QColor(200, 10, 10, 128)); // alpha as-is
        QCOMPARE(loaded_step->pen.capStyle(), Qt::SquareCap);
        QCOMPARE(loaded_taper->stick_type, qint32(StickData::TAPER)); // rounded trapezoid
        QCOMPARE(loaded_taper->img_w_scale, 14.0f);
        QCOMPARE(loaded_taper->img_h_scale, 6.0f);
        delete loaded;
    }

    void NodesExportZedRoundTrip()
    {
        QStringList warnings;
        QString error;
        StickFigureData *zed = StkFormat::ImportNodes(SamplePath("nodes/Zed-Stickman.nodes"),
                                                      QPointF(500, 400), &warnings, &error);
        QVERIFY2(zed, qPrintable(error));

        QString out_path = stk_temp_dir.filePath("zed_reexport.nodes");
        QStringList export_warnings;
        QVERIFY2(StkFormat::ExportNodes(out_path, *zed, &export_warnings, &error),
                 qPrintable(error));

        // recompute the exported figure's bounding box center so coordinates line up
        QRectF bounds;
        for (StickData *stick : zed->stick_list) {
            QRectF box = QRectF(stick->my_line.p1(), stick->my_line.p2()).normalized();
            bounds = bounds.isNull() ? box : bounds.united(box);
        }
        QStringList import_warnings;
        StickFigureData *reloaded = StkFormat::ImportNodes(out_path, bounds.center(),
                                                           &import_warnings, &error);
        QVERIFY2(reloaded, qPrintable(error));
        QCOMPARE(reloaded->stick_list.count(), zed->stick_list.count());
        double worst = 0;
        for (int i = 0; i < zed->stick_list.count(); i++) {
            worst = qMax(worst, double(QLineF(reloaded->stick_list[i]->my_line.p2(),
                                              zed->stick_list[i]->my_line.p2()).length()));
        }
        QVERIFY2(worst < 0.75, qPrintable(QStringLiteral("worst endpoint drift %1 px").arg(worst)));
        delete zed;
        delete reloaded;
    }

    void NodesImportRejectsCorruptFiles()
    {
        QStringList warnings;
        QString error;

        // truncated real file
        QByteArray zed = ReadFileBytes(SamplePath("nodes/Zed-Stickman.nodes"));
        QVERIFY(!StkFormat::ImportNodes(WriteTempFile(zed.left(100)), QPointF(0, 0), &warnings, &error));
        QVERIFY(!error.isEmpty());

        // implausible version and not a zlib stream either
        QVERIFY(!StkFormat::ImportNodes(WriteTempFile("GARBAGEGARBAGEGARBAGE"), QPointF(0, 0), &warnings, &error));

        // node-count bomb: valid v224 header + root whose child_count is enormous
        QByteArray bomb = zed.left(12 + 55); // header + root node, all fields genuine
        for (int i = 0; i < 4; i++)          // overwrite the root's trailing child_count (BE)
            bomb[12 + 51 + i] = char((quint32(1000000) >> (8 * (3 - i))) & 0xFF);
        QVERIFY(!StkFormat::ImportNodes(WriteTempFile(bomb), QPointF(0, 0), &warnings, &error));
    }

    // Draw-mode brush spacing is measured along the WHOLE stroke, not per mouse-move event:
    // painting the same drag at 400% spacing must cover visibly less than at 25%. Before
    // MyView::draw_dab_carry every mouse-move stamped a dab at its own start point (moves are
    // only a few pixels apart), so any spacing above ~100% painted the same stroke as a much
    // lower one — the spinbox went over 100% but nothing changed.
    void BrushSpacingScalesDabCountAlongStroke()
    {
        AddFrame();
        view->resize(800, 600);
        // painted (non-transparent) canvas pixels left by one identical drag at a given spacing;
        // a fresh stick per run, since ExitDrawMode() bakes the stroke into the stick's image
        auto paint = [this](int spacing)->int{
            StickData *stick = DrawStick(QPointF(100, 100), QPointF(100, 200));
            view->EnterDrawMode(stick, false);
            view->draw_brush_size = 10;
            view->draw_brush_hardness = 100;   // hard edge: coverage is the dab count, not the feather
            view->draw_brush_spacing = spacing;
            QTest::mousePress(view->viewport(), Qt::LeftButton, Qt::NoModifier,
                              view->mapFromScene(QPointF(200, 300)));
            for (int x = 205; x <= 400; x += 5)
                QTest::mouseMove(view->viewport(), view->mapFromScene(QPointF(x, 300)));
            QTest::mouseRelease(view->viewport(), Qt::LeftButton, Qt::NoModifier,
                                view->mapFromScene(QPointF(400, 300)));
            int painted = 0;
            const QImage &canvas = *view->draw_canvas;
            for (int y = 0; y < canvas.height(); y++)
                for (int x = 0; x < canvas.width(); x++)
                    if (qAlpha(canvas.pixel(x, y)) != 0)
                        painted++;
            view->ExitDrawMode();
            return painted;
        };

        int dense = paint(25);
        int sparse = paint(400);
        QVERIFY2(dense > 0, "the drag painted nothing at all");
        QVERIFY2(sparse * 2 < dense, qPrintable(QStringLiteral("spacing ignored: %1 px at 400% vs "
                                                              "%2 px at 25%").arg(sparse).arg(dense)));
        view->draw_brush_spacing = 25;
        view->SetTool(NOTOOL);
    }

    // The built-in tip's two shape knobs, which every draw-panel preset (pencils, aerograph,
    // ink pen) is just a parameter set over: hardness is the fully opaque core radius,
    // roundness squashes the tip vertically inside a mask that stays square (so StampBrush()'s
    // per-dab scaling stays uniform). Hardness 100 in particular used to come out as the
    // SOFTEST tip: its gradient stop landed on 1.0 and QGradient::setColorAt() replaces a stop
    // at an existing position, so it overwrote the transparent rim.
    void RoundBrushHardnessAndRoundnessShapeTheTip()
    {
        QImage hard = MyView::MakeRoundBrush(64, 100, 100);
        QImage soft = MyView::MakeRoundBrush(64, 0, 100);
        QCOMPARE(qAlpha(hard.pixel(32, 32)), 255);  // both solid in the middle (the soft one's
        QVERIFY(qAlpha(soft.pixel(32, 32)) > 240);  // centre pixel samples half a pixel off it)
        QCOMPARE(qAlpha(hard.pixel(32, 5)), 255);   // near the rim: still solid at 100% hardness
        QVERIFY(qAlpha(soft.pixel(32, 5)) < 64);    // feathered from the centre out at 0%

        QImage flat = MyView::MakeRoundBrush(64, 100, 50);
        QCOMPARE(flat.size(), hard.size());         // square mask: uniform dab scaling still holds
        QCOMPARE(qAlpha(flat.pixel(32, 5)), 0);     // squashed to half height: top rim is empty
        QCOMPARE(qAlpha(flat.pixel(5, 32)), 255);   // ...while the width is untouched
    }

    // The "Brush" BORDER: the outline is stamped with a brush tip instead of traced by the pen,
    // through the same BrushStroke/StampAlongPath() pair draw mode stamps with. Rendered, because
    // the whole claim is about what reaches the canvas, and in three parts:
    //   1) the pen must NOT also draw its own outline (it did, until StrokeWithBrush() returned
    //      true and FillAndStroke() honoured it), which shows up as a widely spaced brush border
    //      covering far less than the continuous line;
    //   2) spacing must actually act on a border, exactly as it acts on a freehand stroke;
    //   3) the whole thing must survive a save/load — it is the one style entry that is not a
    //      QBrush, so unlike every other fill/stroke kind it needed real serialization (record
    //      version 6) rather than riding along inside QPen/QBrush's own operators.
    void BrushBorderStampsTheOutlineInsteadOfTracingIt()
    {
        StickData rect;
        rect.stick_type = StickData::RECT;
        rect.my_line = QLineF(100, 50, 100, 150);
        rect.img_angle = 0;
        rect.img_w_scale = 60;
        rect.brush = QBrush(Qt::NoBrush);          // solo il bordo: ogni pixel dipinto è suo
        rect.pen = QPen(Qt::blue, 8);
        rect.stroke_brush.stamp = MyView::MakeRoundBrush(32, 100, 100); // punta piena, bordo netto

        auto painted_pixels = [](StickData *data){
            QGraphicsScene render_scene;
            render_scene.setSceneRect(0, 0, 200, 220);
            render_scene.addItem(new StickGraphicsItem(data)); // la scena possiede la view
            QImage img(200, 220, QImage::Format_ARGB32);
            img.fill(Qt::white);
            QPainter painter(&img);
            render_scene.render(&painter, QRectF(0, 0, 200, 220), QRectF(0, 0, 200, 220));
            painter.end();
            int painted = 0;
            for(int y = 0; y < img.height(); y++)
                for(int x = 0; x < img.width(); x++)
                    if(img.pixelColor(x, y) != QColor(Qt::white))
                        painted++;
            return painted;
        };

        rect.stroke_brush.spacing = 25;
        const int dense = painted_pixels(&rect);
        rect.stroke_brush.spacing = 600;
        const int sparse = painted_pixels(&rect);
        QImage no_brush_stamp;
        qSwap(rect.stroke_brush.stamp, no_brush_stamp);
        const int plain_pen = painted_pixels(&rect);
        qSwap(rect.stroke_brush.stamp, no_brush_stamp);

        QVERIFY2(sparse > 0, "the brush border painted nothing at all");
        QVERIFY2(sparse * 2 < dense, qPrintable(QStringLiteral("border spacing ignored: %1 px at "
                                     "600%% vs %2 px at 25%%").arg(sparse).arg(dense)));
        // la penna non traccia ANCHE il suo contorno: a dab radi si copre molto meno della linea
        // continua, mentre disegnandoli entrambi il conto sarebbe almeno quello della penna
        QVERIFY2(sparse * 2 < plain_pen, qPrintable(QStringLiteral("the pen drew the outline as "
                                     "well: %1 px stamped vs %2 px with the pen alone")
                                     .arg(sparse).arg(plain_pen)));

        // ...e vale per OGNI tipo con un percorso, non solo per il rettangolo, con un
        // riempimento sotto (il caso reale): conta i soli pixel del colore della penna, così un
        // contorno tracciato si distingue dal riempimento che gli sta dietro
        const struct { const char *name; int type; } types[] = {
            { "LINE", StickData::LINE }, { "RECT", StickData::RECT },
            { "CIRCLE", StickData::CIRCLE }, { "TRAPEZOID", StickData::TRAPEZOID },
            { "TAPER", StickData::TAPER }, { "POLYGON", StickData::POLYGON },
            { "MULTILINE", StickData::MULTILINE } };
        auto pen_coloured_pixels = [](StickData *data){
            QGraphicsScene render_scene;
            render_scene.setSceneRect(0, 0, 240, 240);
            render_scene.addItem(new StickGraphicsItem(data));
            QImage img(240, 240, QImage::Format_ARGB32);
            img.fill(Qt::white);
            QPainter painter(&img);
            render_scene.render(&painter, QRectF(0, 0, 240, 240), QRectF(0, 0, 240, 240));
            painter.end();
            int n = 0;
            for(int y = 0; y < img.height(); y++)
                for(int x = 0; x < img.width(); x++)
                    if(img.pixelColor(x, y) == QColor(Qt::blue)) n++;
            return n;
        };
        for(const auto &t : types){
            StickData d;
            d.stick_type = t.type;
            d.my_line = QLineF(100, 60, 100, 160);
            d.img_angle = 0;
            d.img_w_scale = 60;
            d.img_h_scale = 60;
            d.brush = QBrush(Qt::red);                 // riempimento pieno sotto il bordo
            d.pen = QPen(Qt::blue, 8);
            if(t.type == StickData::POLYGON || t.type == StickData::MULTILINE){
                d.polygon_points = { QPointF(0.4, 0.3), QPointF(-0.4, 0.6) };
                d.vertex_curvatures = { 0, 0, 0, 0 };
                d.vertex_widths = { 8, 8, 8, 8 };
            }
            const int traced = pen_coloured_pixels(&d);
            d.stroke_brush.stamp = MyView::MakeRoundBrush(32, 100, 100);
            d.stroke_brush.spacing = 600;
            const int stamped = pen_coloured_pixels(&d);
            QVERIFY2(stamped > 0 && stamped * 2 < traced,
                     qPrintable(QStringLiteral("%1: pen outline still drawn under the dabs "
                                "(%2 px stamped vs %3 px traced)")
                                .arg(t.name).arg(stamped).arg(traced)));
        }

        // l'ONION SKIN passa di qui come tutto il resto, in grigio piatto. Lasciarlo alla penna
        // (che è ciò che faceva l'esclusione di onion_render) metteva un contorno pieno su ogni
        // fotogramma fantasma di uno stick che sulla tela è timbrato: un bordo solido visibile
        // DIETRO il pennello. Il flag è static: va rimesso da un distruttore, perché una QVERIFY
        // fallita esce subito e lo lascerebbe acceso per ogni slot successivo.
        struct OnionGuard { ~OnionGuard(){ StickGraphicsItem::onion_render = false; } } guard;
        StickData ghost;
        ghost.stick_type = StickData::RECT;
        ghost.my_line = QLineF(100, 50, 100, 150);
        ghost.img_angle = 0;
        ghost.img_w_scale = 60;
        ghost.brush = QBrush(Qt::NoBrush);
        ghost.pen = QPen(Qt::blue, 8);
        StickGraphicsItem::onion_render = true;
        const int ghost_traced = painted_pixels(&ghost);
        ghost.stroke_brush.stamp = MyView::MakeRoundBrush(32, 100, 100);
        ghost.stroke_brush.spacing = 600;
        const int ghost_stamped = painted_pixels(&ghost);
        StickGraphicsItem::onion_render = false;
        QVERIFY2(ghost_stamped > 0, "the onion skin of a brush border painted nothing");
        QVERIFY2(ghost_stamped * 2 < ghost_traced,
                 qPrintable(QStringLiteral("the onion skin traced a solid outline under the "
                            "dabs: %1 px stamped vs %2 px traced")
                            .arg(ghost_stamped).arg(ghost_traced)));

        // e il giro attraverso lo stream: version 6, nessun campo perso
        rect.stroke_brush.spacing = 40;
        rect.stroke_brush.angle = -30;
        rect.stroke_brush.size_jitter = 15;
        rect.stroke_brush.angle_jitter = 70;
        rect.stroke_brush.opacity_jitter = 55;
        QByteArray blob;
        QDataStream out(&blob, QIODevice::WriteOnly);
        out << rect;
        StickData back;
        QDataStream in(&blob, QIODevice::ReadOnly);
        in >> back;
        QCOMPARE(back.stroke_brush.stamp.size(), rect.stroke_brush.stamp.size());
        QCOMPARE(back.stroke_brush.spacing, 40);
        QCOMPARE(back.stroke_brush.angle, -30);
        QCOMPARE(back.stroke_brush.size_jitter, 15);
        QCOMPARE(back.stroke_brush.angle_jitter, 70);
        QCOMPARE(back.stroke_brush.opacity_jitter, 55);

        // colore, diametro e opacità NON sono memorizzati: StrokeBrush() li rilegge dalla penna,
        // così i controlli di colore/spessore/opacità che già esistono comandano anche qui
        back.pen = QPen(QColor(0, 128, 0, 128), 17);
        const BrushStroke assembled = back.StrokeBrush();
        QCOMPARE(assembled.color, QColor(0, 128, 0, 128));
        QCOMPARE(assembled.size, 17.0);
        QVERIFY(qAbs(assembled.opacity - 128/255.0) < 0.01);

        // un TEXT ha un contorno ma non un percorso da percorrere: la voce non gli si applica
        back.stick_type = StickData::TEXT;
        QVERIFY(back.PaintsStroke());
        QVERIFY(!back.StrokesAPath());
    }

    // A stick-anchored paint's placement is stored in the STICK's frame — a texture's offset is a
    // translation the local frame then rotates, an ObjectMode gradient's is a displacement inside a
    // box ObjectGradientTurn() turns — while the arrow handle speaks scene coordinates. Reading one
    // as the other moved the paint along the stick's angle instead of along the drag: for a stick
    // pointing UP (img_angle 180) exactly backwards, which is the report this pins. Rendered rather
    // than asserted on the stored numbers, because the claim is about what lands on the canvas.
    // Also pins that a type which never hands its brush to the painter shows no fill arrow at all.
    void TextureArrowDragMovesThePaintTheWayItWasDragged()
    {
        AddFrame();
        view->resize(800, 600);
        DrawStick(QPointF(300, 400), QPointF(300, 300));   // verso l'ALTO: il frame è girato di 180
        StickData *stick = CurrentStickFigure()->stick_list.last();
        stick->stick_type = StickData::RECT;
        // il tipo cambia dopo il disegno, e img_angle lo riallineano alla spina i percorsi che lo
        // usano (StickFigure::Elongate() e simili): qui va messo a mano, come negli altri slot
        stick->img_angle = 180;                            // convenzione -atan2(dx,dy) per il verso su
        stick->img_w_scale = 120;                          // forma larga: i due sensori ci stanno dentro
        stick->pen = QPen(Qt::NoPen);                      // solo il riempimento, nessun contorno
        // una macchia radiale: dove sta il bianco si legge in un pixel, senza ambiguità di direzione
        QRadialGradient spot(QPointF(0.5, 0.5), 0.5);
        spot.setCoordinateMode(QGradient::ObjectMode);
        spot.setStops({ QGradientStop(0.0, Qt::white), QGradientStop(1.0, Qt::black) });
        stick->brush = QBrush(spot);
        CurrentStickFigure()->current_stick = stick;
        view->SetTool(NOTOOL);
        view->show_fill_texture_arrow = true;
        // senza questo il test non misurerebbe nulla: è il frame ruotato a rendere il caso interessante
        QVERIFY2(qAbs(qAbs(stick->img_angle) - 180.0) < 0.5, "lo stick non punta in su: niente da misurare");

        // luminanza in un punto di scena, dipinta da una COPIA dei soli campi che servono:
        // avvolgere lo stick VIVO in una seconda StickGraphicsItem gli ruberebbe il back-pointer
        auto bright_at = [](const StickData *from, const QPoint &probe){
            StickData copy;
            copy.stick_type = from->stick_type;
            copy.my_line = from->my_line;
            copy.img_angle = from->img_angle;
            copy.img_w_scale = from->img_w_scale;
            copy.brush = from->brush;
            copy.brush_follows_stick = from->brush_follows_stick;
            copy.pen = QPen(Qt::NoPen);
            QGraphicsScene render_scene;
            render_scene.setSceneRect(0, 0, 600, 600);
            render_scene.addItem(new StickGraphicsItem(&copy)); // la scena possiede la view
            QImage img(600, 600, QImage::Format_ARGB32);
            img.fill(Qt::red);
            QPainter painter(&img);
            render_scene.render(&painter, QRectF(0, 0, 600, 600), QRectF(0, 0, 600, 600));
            painter.end();
            return qGray(img.pixelColor(probe).rgb());
        };
        // simmetrici rispetto al centro della forma (300,350), dentro i suoi 240..360 x 300..400
        const QPoint left(265, 350), right(335, 350);
        QVERIFY(qAbs(bright_at(stick, left) - bright_at(stick, right)) < 12); // macchia centrata

        QPointF tail, tip;
        QVERIFY(view->TextureArrowPoints(true, &tail, &tip));
        QTest::mousePress(view->viewport(), Qt::LeftButton, Qt::NoModifier,
                          view->mapFromScene(tail));
        QTest::mouseMove(view->viewport(), view->mapFromScene(tail + QPointF(40, 0)));
        QTest::mouseRelease(view->viewport(), Qt::LeftButton, Qt::NoModifier,
                            view->mapFromScene(tail + QPointF(40, 0)));
        stick = CurrentStickFigure()->stick_list.last();
        // la presa ha funzionato (altrimenti il confronto sotto passerebbe a macchia ferma)
        QVERIFY(ReadGradientXf(stick->brush).offset != QPointF(0, 0));
        // ...e la macchia è andata a DESTRA, come il trascinamento
        QVERIFY2(bright_at(stick, right) > bright_at(stick, left) + 20,
                 "il riempimento non ha seguito la freccia");
        // la freccia ridisegnata sta dove il mouse l'ha lasciata: Read e Write sono inversi
        QPointF moved_tail, moved_tip;
        QVERIFY(view->TextureArrowPoints(true, &moved_tail, &moved_tip));
        QVERIFY(QLineF(moved_tail, tail + QPointF(40, 0)).length() < 1.0);

        // un tipo che non passa il brush al painter non ha un riempimento da puntare: niente
        // freccia, per quanta geometria il pennello contenga (StickData::PaintsFill())
        for(qint32 no_fill : { qint32(StickData::LINE), qint32(StickData::MULTILINE),
                              qint32(StickData::TEXT), qint32(StickData::IMAGE),
                              qint32(StickData::FREEHAND) }){
            stick->stick_type = no_fill;
            QVERIFY2(!view->TextureArrowPoints(true, &tail, &tip),
                     qPrintable(QStringLiteral("tipo %1: freccia di riempimento mostrata")
                                    .arg(no_fill)));
        }
        stick->stick_type = StickData::RECT;               // ...e per un tipo che riempie sì
        QVERIFY(view->TextureArrowPoints(true, &tail, &tip));
    }

    // A gradient's transparency lives in its stops (see
    // GradientOpacityLivesInTheStopsAndTheSliderIsEnabled for why it can live nowhere else), so
    // per-stop alpha is the only way to dial a gradient that FADES rather than one uniformly
    // faded. The box has to write the selected stop only, and it has to be echoed back on a
    // selection change — a stale echo would slam the newly selected stop to the previous one's
    // alpha at the first nudge, which is the failure mode UpdateStyleControls() exists for.
    // One dialog class serves fill and border, linear and radial, so this covers all four.
    void GradientStopAlphaIsEditablePerStop()
    {
        for(GradientDialog::Kind kind : { GradientDialog::LINEAR, GradientDialog::RADIAL }){
            GradientDialog dialog(kind);
            QSpinBox *alpha = dialog.findChild<QSpinBox*>("gradient_stop_alpha");
            QVERIFY(alpha);
            QCOMPARE(alpha->value(), 100);          // lo stop 0 di partenza è opaco, e l'eco lo dice

            alpha->setValue(40);
            QGradientStops stops = dialog.Gradient().stops();
            QCOMPARE(stops.size(), 2);
            QVERIFY(qAbs(stops[0].second.alphaF() - 0.4) < 0.01);
            QCOMPARE(stops[1].second.alpha(), 255); // solo lo stop selezionato cambia
            QCOMPARE(stops[0].second.rgb(), QColor(Qt::white).rgb()); // e ne cambia solo l'alpha

            // l'eco: selezionare l'altro stop deve portare il SUO alpha nella casella
            QListWidget *list = dialog.findChild<QListWidget*>();
            QVERIFY(list);
            list->setCurrentRow(1);
            QCOMPARE(alpha->value(), 100);
            list->setCurrentRow(0);
            QCOMPARE(alpha->value(), 40);
        }
    }

    // A picture, a texture fill/stroke and a border-brush tip are all read back by
    // StickData::operator>> as freshly decoded, INDEPENDENT pixel buffers, so a figure that
    // exists through N frames used to cost N copies of each of its images (measured: 60
    // frames of one 11 MB picture = ~690 MB). The load-time pool gives every record that
    // reads identical pixels the same buffer instead. QImage is copy-on-write, so this is
    // invisible: sharing is only ever an allocation, never a semantic change — which the
    // detach check at the bottom is here to prove.
    void LoadedImagesShareOneBufferAcrossRecords()
    {
        ClearStickImagePool(); // stato definito: altri slot hanno già caricato immagini

        QImage tile(16, 16, QImage::Format_ARGB32);
        tile.fill(Qt::magenta);
        QImage tip(12, 12, QImage::Format_Alpha8);
        for(int y = 0; y < 12; y++)
            memset(tip.scanLine(y), y * 20, tip.bytesPerLine());

        StickData source;
        source.stick_type = StickData::IMAGE;
        source.my_line = QLineF(0, 0, 0, 40);
        QImage *picture = new QImage(24, 24, QImage::Format_ARGB32);
        picture->fill(Qt::darkCyan);
        source.stick_img_list << picture;
        source.img_name_list << "shared";
        source.stick_img = picture;
        QBrush fill(tile);
        fill.setTransform(QTransform::fromScale(0.5, 0.5).rotate(30)); // il piazzamento deve sopravvivere
        source.brush = fill;
        QPen stroke(QBrush(tile), 4);
        source.pen = stroke;
        source.stroke_brush.stamp = tip;

        QByteArray bytes;
        {
            QDataStream out(&bytes, QIODevice::WriteOnly);
            out << source;
        }
        // DUE record letti dallo stesso blocco: è la situazione di uno stesso stick presente
        // in due frame diversi, che è il caso che moltiplicava la memoria
        StickData first, second;
        {
            QDataStream in(&bytes, QIODevice::ReadOnly);
            in >> first;
        }
        {
            QDataStream in(&bytes, QIODevice::ReadOnly);
            in >> second;
        }

        QCOMPARE(*first.stick_img, *picture);              // i pixel sono ancora quelli giusti
        QVERIFY(first.stick_img != second.stick_img);      // oggetti QImage distinti, come prima
        QCOMPARE(first.stick_img->constBits(), second.stick_img->constBits()); // ...ma un solo buffer
        QCOMPARE(first.brush.textureImage().constBits(),
                 second.brush.textureImage().constBits());
        QCOMPARE(first.pen.brush().textureImage().constBits(),
                 second.pen.brush().textureImage().constBits());
        QCOMPARE(first.stroke_brush.stamp.constBits(), second.stroke_brush.stamp.constBits());
        // la texture della penna e quella del riempimento sono gli stessi pixel: una sola copia
        QCOMPARE(first.brush.textureImage().constBits(),
                 first.pen.brush().textureImage().constBits());
        // il piazzamento della texture (scala/angolo/offset) non deve essere toccato dallo scambio
        QCOMPARE(first.brush.transform(), fill.transform());
        QCOMPARE(first.pen.widthF(), stroke.widthF());

        // copy-on-write: scrivere su una copia condivisa non tocca l'altra
        first.stick_img->fill(Qt::yellow);
        QCOMPARE(second.stick_img->pixelColor(0, 0), QColor(Qt::darkCyan));
        QVERIFY(first.stick_img->constBits() != second.stick_img->constBits());

        ClearStickImagePool();
    }

    // The file half of the same problem: every record used to inline its own copy of its
    // pixels, so one picture in N frames was encoded N times (measured: a realistic 60-frame
    // animation at 218 MB, of which 1.9 MB was content). v3 writes an ImageTable once, ahead
    // of the frames, and the records index into it. Checked by SIZE — the only way to tell
    // "written once" from "written N times and read back correctly".
    void AnimationFileWritesEachDistinctImageOnce()
    {
        ClearStickImagePool();

        // un'immagine non comprimibile, così il suo peso nel file si vede. Tenuta PER VALORE:
        // le copie che finiscono negli stick sono di proprietà loro, e LoadAnimation() più
        // sotto le distrugge tutte prima che il confronto finale avvenga.
        QImage expected(200, 200, QImage::Format_ARGB32);
        quint32 noise = 12345; // LCG: rumore riproducibile, e PNG non riesce a comprimerlo
        for(int y = 0; y < 200; y++){
            QRgb *row = reinterpret_cast<QRgb*>(expected.scanLine(y));
            for(int x = 0; x < 200; x++){
                noise = noise * 1664525u + 1013904223u;
                row[x] = qRgba((noise >> 16) & 255, (noise >> 8) & 255, noise & 255, 255);
            }
        }
        qint64 one_image_bytes = 0;
        {
            QByteArray encoded;
            QDataStream out(&encoded, QIODevice::WriteOnly);
            out << expected;
            one_image_bytes = encoded.size();
        }
        QVERIFY(one_image_bytes > 20000); // se comprimesse, la misura sotto non direbbe nulla

        const QString path = QDir::temp().filePath("sam_imagetable_test.sam");
        qint64 size_with_one = 0, size_with_five = 0;
        for(int frame = 0; frame < 5; frame++){
            AddFrame();
            DrawStick(QPointF(120, 120), QPointF(120, 200));
            StickData *stick = CurrentStickFigure()->stick_list.last();
            stick->stick_type = StickData::IMAGE;
            stick->stick_img_list << new QImage(expected); // stessi pixel in ogni frame
            stick->img_name_list << "noise";
            stick->stick_img = stick->stick_img_list.last();
            view->my_animation->SaveAnimation(path);
            if(frame == 0) size_with_one = QFileInfo(path).size();
            if(frame == 4) size_with_five = QFileInfo(path).size();
        }

        // la misura: quattro frame in più con la STESSA immagine devono costare meno di una
        // sola copia in più di quell'immagine. Senza tabella ne costerebbero quattro.
        const qint64 growth = size_with_five - size_with_one;
        QVERIFY2(growth < one_image_bytes,
                 qPrintable(QStringLiteral("4 frame con la stessa immagine hanno aggiunto %1 byte, "
                                           "contro %2 per una sola copia: è stata riscritta ogni volta")
                                .arg(growth).arg(one_image_bytes)));

        // e il contenuto torna indietro intatto
        view->ClearUndo();
        view->my_animation->LoadAnimation(path);
        ClearStickImagePool();
        view->MoveToFrame(view->my_animation->frame_list[0]);
        int checked = 0;
        for(Frame *f : view->my_animation->frame_list){
            for(StickFigure *figure : f->stick_figures){
                for(StickData *s : figure->stick_list){
                    if(s->stick_type != StickData::IMAGE)
                        continue;
                    QVERIFY(s->stick_img != nullptr);
                    QCOMPARE(*s->stick_img, expected);
                    checked++;
                }
            }
        }
        QCOMPARE(checked, 5);

        QFile::remove(path);
        // gli slot successivi assumono che AddFrame() accodi in fondo (vedi la nota in
        // GenerateInbetweensInterpolatesAndIsOneUndoStep)
        view->MoveToFrame(view->my_animation->frame_list.last());
    }

    // A brush tip is a MASK: only its alpha means anything (AbrBrush::stamp's contract, and
    // TintedStamp() drops the RGB through CompositionMode_SourceIn anyway). Holding one as
    // ARGB32 therefore costs four bytes per pixel to say what one byte says — 163 MB against
    // 40 MB for a real 20-tip smoke pack. The saving is only legitimate if every consumer
    // renders identically from either format, which is what this checks.
    void BrushTipsAreAlpha8AndRenderIdentically()
    {
        const QImage tip = MyView::MakeRoundBrush(64, 70, 100);
        QCOMPARE(tip.format(), QImage::Format_Alpha8);

        BrushStroke as_alpha8;
        as_alpha8.stamp = tip;
        as_alpha8.color = QColor(200, 40, 40);
        as_alpha8.size = 24;
        BrushStroke as_argb32 = as_alpha8;
        as_argb32.stamp = tip.convertToFormat(QImage::Format_ARGB32);

        const QImage dab8 = BrushDab(as_alpha8);
        const QImage dab32 = BrushDab(as_argb32);
        QCOMPARE(dab8.size(), dab32.size());
        QCOMPARE(dab8.convertToFormat(QImage::Format_ARGB32),
                 dab32.convertToFormat(QImage::Format_ARGB32));

        // e lungo un contorno, che è la strada del bordo a pennello
        QPainterPath path;
        path.addRect(10, 10, 60, 40);
        QImage stamped8(90, 70, QImage::Format_ARGB32), stamped32(90, 70, QImage::Format_ARGB32);
        stamped8.fill(Qt::transparent);
        stamped32.fill(Qt::transparent);
        { QPainter p(&stamped8);  StampAlongPath(&p, path, as_alpha8); }
        { QPainter p(&stamped32); StampAlongPath(&p, path, as_argb32); }
        QCOMPARE(stamped8, stamped32);

        // il round trip su file riporta ARGB32 (QDataStream scrive PNG), e operator>> lo
        // riconverte: senza quella riconversione il risparmio sparirebbe al primo caricamento
        StickData source;
        source.stick_type = StickData::RECT;
        source.my_line = QLineF(0, 0, 0, 30);
        source.stroke_brush.stamp = tip;
        QByteArray bytes;
        {
            QDataStream out(&bytes, QIODevice::WriteOnly);
            out << source;
        }
        StickData loaded;
        {
            QDataStream in(&bytes, QIODevice::ReadOnly);
            in >> loaded;
        }
        QCOMPARE(loaded.stroke_brush.stamp.format(), QImage::Format_Alpha8);
        QCOMPARE(loaded.stroke_brush.stamp, tip);
        ClearStickImagePool();
    }

    // "Save brush" stores the selected tip PLUS the host's live parameters in a .mbrush
    // collection — the parameters are the whole reason the format exists, since an .abr carries
    // only a shape. What the grid shows afterwards has been through the file (Populate() reloads
    // it), so reading it back off the item is what pins the codec; deleting the last tip must
    // take the collection with it, or the combo keeps an empty entry forever.
    void UserBrushCollectionRoundTripsAndDeletes()
    {
        QTemporaryDir dir;
        QVERIFY(dir.isValid());
        BrushPicker picker(dir.path());

        BrushPreset dialled;
        dialled.size = 33;
        dialled.spacing = 77;
        dialled.hardness = 41;
        dialled.angle = -40;
        dialled.size_jitter = 19;
        dialled.opacity_jitter = 12;
        picker.SetParamsProvider([dialled]{ return dialled; });

        BrushPreset seen;
        QObject::connect(&picker, &BrushPicker::presetChosen, &picker,
                         [&seen](const BrushPreset &p){ seen = p; });
        picker.SaveBrush(QStringLiteral("Test tip"));

        const QString file = dir.path() + "/My brushes.mbrush";
        QVERIFY(QFile::exists(file));
        QCOMPARE(seen.name, QStringLiteral("Test tip"));
        QCOMPARE(seen.spacing, 77);
        QCOMPARE(seen.hardness, 41);
        QCOMPARE(seen.angle, -40);
        QCOMPARE(seen.size_jitter, 19);
        QCOMPARE(seen.opacity_jitter, 12);
        // lo stamp torna Alpha8 come ci era andato (QDataStream lo riporta ARGB32)
        QCOMPARE(picker.CurrentStamp().format(), QImage::Format_Alpha8);
        QCOMPARE(picker.CurrentStamp().size(), QSize(128, 128));

        picker.DeleteBrush(false);
        QVERIFY(!QFile::exists(file));
    }

    // mfvideo.cpp: l'mp4 lo encoda Media Foundation (l'encoder H.264 di Windows), non ffmpeg.
    // Replica quello che ImageExport::run() fa in modalita' MFVIDEO, che a sua volta arriva da
    // MyMainWindow::StartMediaFoundationRender().
    void MediaFoundationEncodesAnMp4FromAnImageSequence()
    {
        QTemporaryDir dir;
        QVERIFY(dir.isValid());

        // lati dispari di proposito: l'H.264 li rifiuta e il writer deve arrotondarli
        QStringList frames;
        for(int i = 0; i < 12; ++i){
            QImage img(321, 241, QImage::Format_ARGB32);
            img.fill(Qt::white);
            QPainter p(&img);
            p.setBrush(Qt::red);
            p.drawRect(10 * i, 100, 40, 40);
            p.end();
            const QString path = dir.filePath(QString("img_%1.png").arg(i));
            QVERIFY(img.save(path));
            frames << path;
        }

        const QString out = dir.filePath("out.mp4");
        QString error;
        QVERIFY2(WriteMp4WithMediaFoundation(out, frames, 24, &error), qPrintable(error));

        QFile f(out);
        QVERIFY(f.open(QIODevice::ReadOnly));
        const QByteArray written = f.readAll();
        f.close();
        QVERIFY(written.size() > 2000);        // un container vuoto pesa poche centinaia di byte
        QVERIFY(written.contains("ftyp"));     // e' davvero un mp4...
        QVERIFY(written.contains("avc1"));     // ...con una traccia H.264 dentro

        // un fallimento deve restare un fallimento, non un file vuoto dichiarato buono
        QVERIFY(!WriteMp4WithMediaFoundation(dir.filePath("none.mp4"), QStringList(), 24, &error));
        QVERIFY(!error.isEmpty());
    }

    // ImageExport gira in un thread mentre l'utente continua a modificare l'animazione: deve
    // renderizzare l'istantanea presa alla costruzione, non il modello vivo (che il thread della
    // UI può cambiare o liberare sotto di lui). E modo/percorso sono suoi: quando erano globali,
    // un'anteprima creata durante un export li sovrascriveva e l'export non scriveva nulla.
    // Infine l'anteprima si renderizza allo zoom della view, non alla risoluzione della tela.
    void ExportRendersTheSnapshotTakenAtConstruction()
    {
        struct Restore {
            MyView *v; QPen pen;
            ~Restore(){ v->my_pen = pen; v->setTransform(QTransform()); }
        } restore{view, view->my_pen};
        struct Reap { ImageExport *e; ~Reap(){ e->wait(); delete e; } };

        QTemporaryDir dir;
        QVERIFY(dir.isValid());
        AddFrame();
        view->my_pen = QPen(Qt::black, 20);
        StickData *stick = DrawStick(QPointF(100, 100), QPointF(100, 300));

        Reap exporter{new ImageExport(nullptr, dir.filePath("img.png"), FFMPEG)};
        view->setTransform(QTransform::fromScale(0.5, 0.5));
        Reap preview{new ImageExport(nullptr, "", PREVIEW)};
        // il modello cambia DOPO la costruzione, prima che i thread partano
        stick->my_line = QLineF(600, 100, 600, 300);

        QSignalSpy finished(exporter.e, &ImageExport::ImageExportFinished);
        exporter.e->start();
        preview.e->start();
        QVERIFY(exporter.e->wait(30000));
        QVERIFY(preview.e->wait(30000));

        QCOMPARE(finished.count(), 1);
        QCOMPARE(finished[0][0].toInt(), FFMPEG);
        QCOMPARE(exporter.e->rendered_files.count(), view->my_animation->frame_list.count());
        const QImage frame(exporter.e->rendered_files.last());
        QCOMPARE(frame.size(), QSize(W, H));
        QVERIFY2(qGray(frame.pixel(100, 200)) < 64, "the export lost the stick where it was");
        QVERIFY2(qGray(frame.pixel(600, 200)) > 192, "the export rendered the live model");

        const qreal k = qMin(1.0, 0.5 * view->devicePixelRatioF());
        QCOMPARE(preview.e->preview_scale, k);
        QCOMPARE(preview.e->preview_frames.count(), view->my_animation->frame_list.count());
        const QImage shown = preview.e->preview_frames.last();
        QCOMPARE(shown.size(), QSize(qRound(W * k), qRound(H * k)));
        QVERIFY(qGray(shown.pixel(qRound(100 * k), qRound(200 * k))) < 64);
        QVERIFY(qGray(shown.pixel(qRound(600 * k), qRound(200 * k))) > 192);
    }

    // Un onion skin si renderizza solo sull'ingombro delle figure del suo frame, non su tutta la
    // scena (1.44x la tela per fantasma): il pixmap deve restare piccolo e, posizionato col suo
    // offset, cadere esattamente dove lo stick era — il pixel sotto lo stick è dipinto, quelli
    // accanto no.
    void OnionSkinIsCroppedToTheFiguresAndStaysInPlace()
    {
        struct Restore {
            MyView *v; QPen pen;
            ~Restore(){ onion_skin_sb->setValue(0); v->my_pen = pen; }
        } restore{view, view->my_pen};
        view->my_pen = QPen(Qt::black, 12); // spesso: il pixel campionato non dipende dall'arrotondamento
        AddFrame();
        DrawStick(QPointF(300, 150), QPointF(300, 250));
        AddFrame(); // il frame precedente diventa il fantasma
        onion_skin_sb->setValue(1);
        view->UpdateOnionSkins();

        QCOMPARE(view->onion_skins.count(), 1);
        QGraphicsPixmapItem *ghost = view->onion_skins.first();
        const QImage pixels = ghost->pixmap().toImage();
        QVERIFY2(pixels.width() < W / 2 && pixels.height() < H / 2, "the ghost was not cropped");
        auto alpha_at = [&](QPointF scene_point){
            QPoint p = (scene_point - ghost->offset()).toPoint();
            return pixels.rect().contains(p) ? qAlpha(pixels.pixel(p)) : 0;
        };
        QVERIFY2(alpha_at(QPointF(300, 200)) > 0, "the ghost is not where the stick was");
        QCOMPARE(alpha_at(QPointF(300, 200) + QPointF(40, 0)), 0);
    }

    // The view-only navigation tools (ROTATEVIEW/PANVIEW) and ResetView(), driven by real mouse
    // events: they move the VIEW and nothing else — the stick, the undo stack and the scene's own
    // rect (what icons and exports render) stay as they were. The pan runs zoomed out, where the
    // whole canvas fits the viewport: there QGraphicsView re-centres the scene by itself, which
    // swallows a pan unless UnlockViewNavigation() has made room for it.
    void ViewRotateAndPanLeaveTheDocumentUntouched()
    {
        struct Restore {
            MyView *v;
            ~Restore(){ v->ResetView(); v->SetTool(NOTOOL); }
        } restore{view};
        AddFrame();
        view->resize(800, 600);
        DrawStick(QPointF(400, 100), QPointF(400, 200));
        const QLineF line = CurrentStickFigure()->stick_list[0]->my_line;
        const int undo_count = view->undo_stack.count();
        const QRectF scene_rect = scene->sceneRect();
        QWidget *vp = view->viewport();
        // QTest::mouseMove() takes no modifiers, and Shift is the point of the snap
        auto move = [vp](QPoint pos, Qt::KeyboardModifiers mods){
            QMouseEvent ev(QEvent::MouseMove, pos, vp->mapToGlobal(pos), Qt::NoButton, Qt::LeftButton, mods);
            QCoreApplication::sendEvent(vp, &ev);
        };

        const QPointF probe(400, 200);
        view->ResetView();
        const QPoint home = view->mapFromScene(probe); // where the probe sits un-navigated

        // rotation and zoom pivot on the CANVAS centre, so first push the canvas off the screen
        // centre: a pivot on the viewport centre (the first cut) then visibly moves the canvas
        const QPointF canvas_centre = my_rect->sceneBoundingRect().center();
        auto pan_by = [&](QPoint d){
            view->SetTool(PANVIEW);
            const QPoint p = vp->rect().center();
            QTest::mousePress(vp, Qt::LeftButton, Qt::NoModifier, p);
            move(p + d, Qt::NoModifier);
            QTest::mouseRelease(vp, Qt::LeftButton, Qt::NoModifier, p + d);
        };
        pan_by(QPoint(120, 80));

        view->SetTool(ROTATEVIEW);
        // a click with no drag: makes the room (scroll bars may appear and shrink the viewport)
        // and turns nothing, so the pivot measured next is the one the drag turns about
        QTest::mouseClick(vp, Qt::LeftButton, Qt::NoModifier, vp->rect().center());
        QVERIFY(qAbs(view->ViewAngle()) < 1e-9);
        const QPoint c = view->mapFromScene(canvas_centre);
        QVERIFY2(QLineF(c, vp->rect().center()).length() > 50, "the canvas is not off-centre: nothing to tell apart");
        QTest::mousePress(vp, Qt::LeftButton, Qt::NoModifier, c + QPoint(200, 0));
        move(c + QPoint(141, 141), Qt::NoModifier);
        move(c + QPoint(0, 200), Qt::NoModifier);
        QVERIFY2(qAbs(view->ViewAngle() - 90) < 0.5, "the view did not turn with the cursor");
        QVERIFY2(QLineF(view->mapFromScene(canvas_centre), c).length() <= 2,
                 "the rotation did not pivot on the canvas centre");
        const QPoint at_110 = c + QPoint(-68, 188); // ~109.9° from where the drag started
        move(at_110, Qt::NoModifier);
        QVERIFY(qAbs(view->ViewAngle() - 109.9) < 0.5);
        move(at_110, Qt::ShiftModifier);
        QVERIFY2(qAbs(view->ViewAngle() - 105) < 1e-6, "Shift did not snap to a 15° step");
        QTest::mouseRelease(vp, Qt::LeftButton, Qt::NoModifier, at_110);

        view->ResetView();
        QVERIFY(view->transform().isIdentity());
        QCOMPARE(view->sceneRect(), scene->sceneRect()); // the room for panning is gone again
        QVERIFY(QLineF(view->mapFromScene(probe), home).length() <= 1);

        view->scale(0.1, 0.1); // the whole canvas fits: the case a pan used to do nothing in
        view->SetTool(PANVIEW);
        const QPoint p0 = vp->rect().center();
        QTest::mousePress(vp, Qt::LeftButton, Qt::NoModifier, p0);
        const QPoint before = view->mapFromScene(probe); // after the press: it makes the room
        move(p0 + QPoint(30, 20), Qt::NoModifier);
        move(p0 + QPoint(60, 40), Qt::NoModifier);
        QTest::mouseRelease(vp, Qt::LeftButton, Qt::NoModifier, p0 + QPoint(60, 40));
        QCOMPARE(view->mapFromScene(probe), before + QPoint(60, 40));
        view->ResetView(); // zoom AND pan
        QVERIFY(view->transform().isIdentity());
        QVERIFY(QLineF(view->mapFromScene(probe), home).length() <= 1);

        extern float zoom_lvl; // test_globals.cpp; the middle-drag zoom reads it
        // zoom: 200 px right doubles, back to the press restores, left/down shrinks; Shift
        // snaps the ABSOLUTE level to 10% steps. The canvas centre stays where it was on screen,
        // also through a burst of small moves (the first cut drifted ~half a pixel per event).
        pan_by(QPoint(-90, 60));
        view->SetTool(ZOOMVIEW);
        const QPoint z0 = vp->rect().center();
        QTest::mousePress(vp, Qt::LeftButton, Qt::NoModifier, z0);
        const QPoint pivot = view->mapFromScene(canvas_centre); // after the press: it makes the room
        QVERIFY(QLineF(pivot, vp->rect().center()).length() > 50);
        move(z0 + QPoint(200, 0), Qt::NoModifier);
        QVERIFY2(qAbs(view->ViewZoom() - 2) < 1e-6, "dragging right did not zoom in");
        QCOMPARE(zoom_lvl, 2.0f);
        QVERIFY2(QLineF(view->mapFromScene(canvas_centre), pivot).length() <= 2,
                 "the zoom did not pivot on the canvas centre");
        for(int i = 0; i < 60; i++)
            move(z0 + QPoint(i % 2 ? 7 : -5, 0), Qt::NoModifier);
        QVERIFY2(QLineF(view->mapFromScene(canvas_centre), pivot).length() <= 2,
                 "the canvas drifted over many small zoom steps");
        move(z0, Qt::NoModifier);
        QVERIFY(qAbs(view->ViewZoom() - 1) < 1e-6);
        move(z0 + QPoint(0, 200), Qt::NoModifier); // down = out
        QVERIFY2(qAbs(view->ViewZoom() - 0.5) < 1e-6, "dragging down did not zoom out");
        move(z0 + QPoint(-30, 0), Qt::NoModifier); // 2^-0.15 = 0.9013
        QVERIFY(qAbs(view->ViewZoom() - 0.9013) < 1e-3);
        move(z0 + QPoint(-30, 0), Qt::ShiftModifier);
        QVERIFY2(qAbs(view->ViewZoom() - 0.9) < 1e-9, "Shift did not snap to a 10% step");
        QTest::mouseRelease(vp, Qt::LeftButton, Qt::NoModifier, z0 + QPoint(-30, 0));
        view->ResetView();
        QCOMPARE(zoom_lvl, 1.0f);

        QCOMPARE(CurrentStickFigure()->stick_list[0]->my_line, line);
        QCOMPARE(view->undo_stack.count(), undo_count);
        QCOMPARE(scene->sceneRect(), scene_rect);
    }
};

QTEST_MAIN(TestCharacterization)
#include "tst_characterization.moc"
