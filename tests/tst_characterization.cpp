// Characterization tests (REFACTOR_PLAN.md, Phase 0).
// They assert the CURRENT behavior of the model/undo/serialization code, driving it exactly the
// way the UI handlers in widget.cpp / myview.cpp do — each helper cites the call site it
// replicates. If a later refactor phase changes observable behavior, a failure here means
// "behavior changed", not necessarily "behavior is wrong".
#include <QtTest>
#include <QGraphicsScene>
#include <QGraphicsRectItem>
#include <QTabWidget>
#include "myview.h"
#include "uiItems.h"
#include "framepresenter.h"

extern QGraphicsScene *scene;
extern QGraphicsRectItem *my_rect;
extern QGraphicsRectItem *limit_rect;
extern QPointF onion_offset;
extern MyView *V;
extern int W;
extern int H;
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
        QCOMPARE(branch->stick_list[0]->my_line, join_master->my_line);
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
};

QTEST_MAIN(TestCharacterization)
#include "tst_characterization.moc"
