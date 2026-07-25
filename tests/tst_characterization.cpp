// Characterization tests (REFACTOR_PLAN.md, Phase 0).
// They assert the CURRENT behavior of the model/undo/serialization code, driving it exactly the
// way the UI handlers in widget.cpp / myview.cpp do — each helper cites the call site it
// replicates. If a later refactor phase changes observable behavior, a failure here means
// "behavior changed", not necessarily "behavior is wrong".
#include <QtTest>
#include <cmath>
#include <QGraphicsScene>
#include <QGraphicsRectItem>
#include <QTabWidget>
#include <QTemporaryDir>
#include <QtEndian>
#include "myview.h"
#include "uiItems.h"
#include "framepresenter.h"
#include "stkformat.h"

extern QGraphicsScene *scene;
extern QGraphicsRectItem *my_rect;
extern QGraphicsRectItem *limit_rect;
extern QPointF onion_offset;
extern MyView *V;
extern qint32 W;
extern qint32 H;
extern FrameData *stick_library_buffer;

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

        fig->SetMaster(gc);
        QCOMPARE(fig->master_stick, gc); // the change actually happened

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
        onion_offset = QPointF(-W / 10.0, -H / 10.0);

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

        QString path = QDir::temp().filePath("marionetta_phase0_test.mAnim");
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
        QString path = QDir::temp().filePath("marionetta_phase0_test.marlib");
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

    // Bug 1: rotating the whole figure by dragging the MASTER must carry the stepchild
    // limbs too (they hang off the master's origin). The old master-drag filtered out
    // stepchildren, so those limbs stayed put while the rest turned.
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
        QVERIFY2(qAbs(step->my_line.angle()  - step_a0)  > 1.0, "stepchild limb was left behind by the master-drag rotation");
        view->SetTool(NOTOOL);
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
};

QTEST_MAIN(TestCharacterization)
#include "tst_characterization.moc"
