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

extern QGraphicsScene *scene;
extern QGraphicsRectItem *myRect;
extern QGraphicsRectItem *limitRect;
extern QPointF onionOffset;
extern myView *V;
extern int W;
extern int H;
extern FrameData *stickLibraryBuffer;

class TestCharacterization : public QObject
{
    Q_OBJECT

    myView *view = nullptr;

    Frame *CurrentFrame() { return view->myAnimation->currentFrame; }
    StickFigure *CurrentStickFigure() { return CurrentFrame()->currentStickFigure; }

    // replicates Widget::addStick() (widget.cpp:421)
    StickFigure *AddStick(const QString &name = QString())
    {
        return CurrentFrame()->addStickFigure(myStickFigureWidgetList, name);
    }

    // replicates Widget::addFrame() (widget.cpp:487)
    Frame *AddFrame()
    {
        view->storeUndo(CMD_ADDFRAME);
        int pos = 0;
        if (view->myAnimation->frameList.count() >= 1)
            pos = CurrentFrame()->frameNumber + 1;
        Frame *added_frame = view->setUpFrame(pos);
        view->moveToFrame(added_frame);
        AddStick();
        return added_frame;
    }

    // replicates myView::drawCmd() for a LINE stick, press + release (myview.cpp:449) —
    // drawCmd itself is private and only reachable through mouse events
    stick *DrawStick(QPointF from, QPointF to)
    {
        StickFigure *figure = CurrentStickFigure();
        view->storeUndo();
        figure->startDrawing(&from, view->myPen, view->myBrush);
        stick *drawn = figure->currentStick;
        drawn->selected = true;
        drawn->hardTop = hardTopCheck->isChecked();
        drawn->hardBottom = hardBottomCheck->isChecked();
        drawn->Pen = view->myPen;
        scene->addItem(drawn);
        figure->endDrawing(&to);
        scene->addItem(drawn->stickHandle[0]);
        scene->addItem(drawn->stickHandle[1]);
        CurrentFrame()->updateIcon();
        view->myAnimation->updateTab(drawn->stickType);
        CurrentFrame()->totalSticks.append(drawn);
        view->myAnimation->updateSelection(drawn);
        return drawn;
    }

private slots:
    void initTestCase()
    {
        // mirrors the parts of Widget::Widget() (widget.cpp:143) the model layer depends on,
        // without the file I/O, the config dialogs and the library autoload
        myStickFigureWidgetList = new QListWidget;
        myFrameWidgetList = new QListWidget;
        myLibraryListWidget = new QListWidget;
        myCurrentLibraryWidget = new QListWidget;
        imgListWidget = new QListWidget;
        onionSkinSB = new QSpinBox;   // value 0 -> updateOnionSkins() is a no-op
        fpsSpinBox = new QSpinBox;
        imgRotationSlider = new QSlider;
        imgHOffsetSlider = new QSlider;
        imgVOffsetSlider = new QSlider;
        imgWidthSlider = new QSlider;
        imgHeightSlider = new QSlider;
        imgHOffsetSpinbox = new QSpinBox;
        imgVOffsetSpinbox = new QSpinBox;
        imgWidthSpinbox = new QSpinBox;
        imgHeightSpinbox = new QSpinBox;
        imgRotationSpinbox = new QSpinBox;
        penOpacitySpinbox = new QSpinBox;
        brushOpacitySpinbox = new QSpinBox;
        lineEditThicknessWidthSpinbox = new QSpinBox;
        stickFigureScaleSpinbox = new QSpinBox;
        stickFigureRotationSpinbox = new QSpinBox;
        circleSquashSpinbox = new QSpinBox;
        elongateSpinbox = new QDoubleSpinBox;
        elongateSpinbox->setRange(0, W);
        depthSpinbox = new QDoubleSpinBox;
        penOpacitySlider = new QSlider;
        brushOpacitySlider = new QSlider;
        depthSlider = new QSlider;
        taperWSlider = new QSlider;
        taperHSlider = new QSlider;
        advancedRectSlider = new QSlider;
        editLineThicknessSlider = new QSlider;
        editLineThicknessSlider->setRange(0, 100);
        circleSquashSlider = new QSlider;
        circleSquashSlider->setRange(-100, 100);
        hardTopCheck = new QCheckBox;
        hardBottomCheck = new QCheckBox;
        dbgLbl = new QLabel;

        scene = new QGraphicsScene;
        view = new myView;
        V = view;
        view->setScene(scene);
        view->myAnimation->scene = scene;

        advancedImgTab = new QWidget;
        advancedLineTab = new advancedLineWidget;
        advancedCircleTab = new advancedCircleWidget;
        advancedTaperTab = new advancedTaperWidget;
        advancedRectTab = new advancedTab;
        myTabWidget = new QTabWidget;
        myTabWidget->addTab(new QWidget, "tools");
        myTabWidget->addTab(new QWidget, "placeholder"); // updateTab() swaps index 1

        myRect = new QGraphicsRectItem(0, 0, W, H);
        myRect->setZValue(-100);
        myRect->setPen(Qt::NoPen);
        scene->addItem(myRect);
        limitRect = new QGraphicsRectItem(-W / 10.0, -H / 10.0, W * 1.1, H * 1.1);
        scene->setSceneRect(limitRect->rect());
        onionOffset = QPointF(-W / 10.0, -H / 10.0);

        AddFrame();
        CurrentFrame()->currentStickFigure = CurrentFrame()->stickFigures[0];
    }

    void StickLineRoundTrip()
    {
        stick source;
        source.myLine = QLineF(1, 2, 3, 4);
        source.Z = 5.5;
        source.stickType = stick::LINE;
        source.stepchild = true;
        source.parentIdx = 7;
        source.order = 3;
        source.Pen = QPen(Qt::red);
        source.Pen.setWidth(9);
        source.Brush = QBrush(Qt::blue);

        QByteArray bytes;
        {
            QDataStream out(&bytes, QIODevice::WriteOnly);
            out << source;
        }
        stick loaded;
        {
            QDataStream in(&bytes, QIODevice::ReadOnly);
            in >> loaded;
        }
        QCOMPARE(loaded.myLine, source.myLine);
        QCOMPARE(loaded.Z, source.Z);
        QCOMPARE(loaded.version, source.version);
        QCOMPARE(loaded.stickType, source.stickType);
        QCOMPARE(loaded.stepchild, source.stepchild);
        QCOMPARE(loaded.parentIdx, source.parentIdx);
        QCOMPARE(loaded.order, source.order);
        QCOMPARE(loaded.Pen, source.Pen);
        QCOMPARE(loaded.Brush, source.Brush);
    }

    void StickCircleRoundTrip()
    {
        stick source;
        source.myLine = QLineF(10, 20, 30, 40);
        source.Z = 1;
        source.stickType = stick::CIRCLE;
        source.imgAngle = 33.5f;
        source.imgAngleOffset = 42; // regression: used to reset on every save/load/clone
        source.imgHScale = 1.5f;
        source.imgWScale = 0.5f;
        source.imgOffset = QPointF(3, 4);
        source.hardTop = true;
        source.hardBottom = true;

        QByteArray bytes;
        {
            QDataStream out(&bytes, QIODevice::WriteOnly);
            out << source;
        }
        stick loaded;
        {
            QDataStream in(&bytes, QIODevice::ReadOnly);
            in >> loaded;
        }
        QCOMPARE(loaded.stickType, source.stickType);
        QCOMPARE(loaded.imgAngle, source.imgAngle);
        QCOMPARE(loaded.imgAngleOffset, source.imgAngleOffset);
        QCOMPARE(loaded.imgHScale, source.imgHScale);
        QCOMPARE(loaded.imgWScale, source.imgWScale);
        QCOMPARE(loaded.imgOffset, source.imgOffset);
        QCOMPARE(loaded.hardTop, source.hardTop);
        QCOMPARE(loaded.hardBottom, source.hardBottom);
    }

    // regression (found during Phase 1a): the stickImg index must be written as a 4-byte
    // int — Qt6's QList::indexOf returns qsizetype, and streaming it uncast wrote 8 bytes
    // while operator>> reads 4, desyncing every IMAGE-stick save
    void StickImageRoundTrip()
    {
        stick source;
        source.stickType = stick::IMAGE;
        source.myLine = QLineF(0, 0, 0, 50);
        source.Z = 2;
        QImage *first_image = new QImage(8, 8, QImage::Format_ARGB32);
        first_image->fill(Qt::red);
        QImage *second_image = new QImage(8, 8, QImage::Format_ARGB32);
        second_image->fill(Qt::blue);
        source.stickImgList << first_image << second_image;
        source.imgNameList << "red" << "blue";
        source.stickImg = second_image;
        source.imgAngleOffset = 15;

        QByteArray bytes;
        {
            QDataStream out(&bytes, QIODevice::WriteOnly);
            out << source;
        }
        stick loaded;
        {
            QDataStream in(&bytes, QIODevice::ReadOnly);
            in >> loaded;
        }
        QCOMPARE(loaded.stickImgList.count(), 2);
        QCOMPARE(loaded.imgNameList, source.imgNameList);
        QCOMPARE(loaded.stickImg, loaded.stickImgList[1]); // displayed-image index survives
        QCOMPARE(*loaded.stickImgList[0], *first_image);
        QCOMPARE(*loaded.stickImgList[1], *second_image);
        QCOMPARE(loaded.imgAngleOffset, source.imgAngleOffset);
        // no manual cleanup: ~StickData frees the images, ~stick only the handles
    }

    // figure-level round trip on pure data: scratch objects need no scene and no flags
    void StickFigureDataRoundTrip()
    {
        StickFigureData source;
        source.name = "figura";
        StickData *master = new StickData();
        master->myLine = QLineF(0, 0, 0, 100);
        source.stickList << master;
        source.masterStick = master;
        StickData *child = new StickData();
        child->myLine = QLineF(0, 100, 50, 100);
        child->parent = master;
        master->children << child;
        source.stickList << child;
        StickData *step_child = new StickData();
        step_child->myLine = QLineF(0, 0, -50, 0);
        step_child->parent = master;
        step_child->stepchild = true;
        master->children << step_child;
        source.stickList << step_child;

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

        QCOMPARE(loaded.stickList.count(), 3);
        QCOMPARE(loaded.name, source.name);
        QCOMPARE(loaded.masterStick, loaded.stickList[0]);
        QVERIFY(loaded.stickList[0]->parent == nullptr);
        QCOMPARE(loaded.stickList[1]->parent, loaded.stickList[0]);
        QCOMPARE(loaded.stickList[2]->parent, loaded.stickList[0]);
        QVERIFY(loaded.stickList[2]->stepchild);
        QVERIFY(!loaded.stickList[1]->stepchild);
        QVERIFY(loaded.stickList[0]->children.contains(loaded.stickList[1]));
        QVERIFY(loaded.stickList[0]->children.contains(loaded.stickList[2]));
        for (int i = 0; i < 3; i++)
            QCOMPARE(loaded.stickList[i]->myLine, source.stickList[i]->myLine);
        // the destructors free the sticks — no scratch-cleanup choreography left
    }

    void DataCloneIsIndependent()
    {
        StickFigureData source;
        StickData *master = new StickData();
        master->myLine = QLineF(0, 0, 0, 100);
        source.stickList << master;
        source.masterStick = master;
        StickData *child = new StickData();
        child->myLine = QLineF(0, 100, 50, 100);
        child->parent = master;
        master->children << child;
        source.stickList << child;

        StickFigureData *copy = source.Clone();
        QCOMPARE(copy->stickList.count(), 2);
        QVERIFY(copy->stickList[0] != source.stickList[0]);
        QCOMPARE(copy->masterStick, copy->stickList[0]);
        QCOMPARE(copy->stickList[1]->parent, copy->stickList[0]);
        QVERIFY(copy->stickList[0]->children.contains(copy->stickList[1]));

        QLineF original_line = source.stickList[0]->myLine;
        copy->stickList[0]->myLine.translate(5, 5);
        QCOMPARE(source.stickList[0]->myLine, original_line);
        delete copy;
    }

    void CopyPasteStickFigure()
    {
        AddFrame();
        DrawStick(QPointF(100, 100), QPointF(100, 200));
        DrawStick(QPointF(100, 200), QPointF(150, 250));
        StickFigure *source = CurrentStickFigure();
        QCOMPARE(source->stickList.count(), 2);

        // replicates Widget::on_copyStickFigureBtn_clicked() (widget.cpp)
        delete view->copyStickFigureBuffer;
        view->copyStickFigureBuffer = ExtractStickFigureData(*source);

        StickFigureData *buffered = view->copyStickFigureBuffer;
        QCOMPARE(buffered->stickList.count(), 2);
        QCOMPARE(buffered->stickList[0]->myLine, source->stickList[0]->myLine);
        QCOMPARE(buffered->stickList[1]->myLine, source->stickList[1]->myLine);
        QCOMPARE(buffered->stickList[1]->parent, buffered->stickList[0]);
        QCOMPARE(buffered->masterStick, buffered->stickList[0]);

        // deep copy: mutating the buffer leaves the source untouched
        QLineF source_line = source->stickList[0]->myLine;
        buffered->stickList[0]->myLine.translate(5, 5);
        QCOMPARE(source->stickList[0]->myLine, source_line);
        buffered->stickList[0]->myLine.translate(-5, -5);

        // replicates Widget::on_pasteStickFigureBtn_clicked() (widget.cpp)
        view->storeUndo(CMD_SIMPLE);
        StickFigure *pasted = AddStick();
        PopulateLiveStickFigure(pasted, *buffered, true, true);
        pasted->refresh(0);
        for (stick *s : pasted->stickList)
            CurrentFrame()->totalSticks.append(s);
        view->addHandles(pasted);

        QCOMPARE(pasted->stickList.count(), 2);
        QCOMPARE(pasted->stickList[1]->parent, pasted->stickList[0]);
        // pasted sticks really live on the scene (attach_to_scene = true)
        QCOMPARE(pasted->stickList[0]->scene(), scene);
    }

    // the imgListWidget cleanup moved from ~stick to explicit calls at the live-delete
    // sites (Phase 1b): deleting the on-screen IMAGE stick empties its widget rows,
    // while scratch snapshots (undo buffer) never touch them
    void DeleteImageStickClearsImageListWidget()
    {
        AddFrame();
        stick *drawn = DrawStick(QPointF(500, 100), QPointF(500, 200));
        // upgrade to IMAGE like drawCmd's DRAWIMG branch (myview.cpp:486)
        drawn->stickType = stick::IMAGE;
        QImage *image = new QImage(8, 8, QImage::Format_ARGB32);
        image->fill(Qt::green);
        drawn->addImage(image, "img");
        QCOMPARE(imgListWidget->count(), 1);

        // snapshotting the frame into the undo buffer (scratch path) must not touch the widget
        view->storeUndo(CMD_SIMPLE);
        QCOMPARE(imgListWidget->count(), 1);

        // live deletion via the delete-key flow (myview.cpp:394)
        StickFigure *figure = CurrentStickFigure();
        int idx = figure->stickList.indexOf(drawn);
        CurrentFrame()->totalSticks.removeAll(drawn);
        figure->deleteStick(idx);
        figure->currentStick = nullptr; // the key handler moves selection to the parent (none here)
        QCOMPARE(figure->stickList.count(), 0);
        QCOMPARE(imgListWidget->count(), 0);
    }

    // the exact snapshot storeUndo() takes since Phase 2b (plain data extraction),
    // plus the FrameData byte round-trip the file paths use
    void FrameSnapshotRoundTrip()
    {
        AddFrame();
        DrawStick(QPointF(10, 10), QPointF(10, 110));
        DrawStick(QPointF(10, 110), QPointF(60, 160));
        Frame *live_frame = CurrentFrame();
        view->updateFrameOrder(live_frame); // storeUndo() does this before snapshotting

        FrameData *snapshot = ExtractFrameData(*live_frame);
        QCOMPARE(snapshot->stickFigures.count(), live_frame->stickFigures.count());
        QCOMPARE(snapshot->frameNumber, live_frame->frameNumber);
        QCOMPARE(snapshot->numOfItems, live_frame->numOfItems);
        QCOMPARE(snapshot->totalSticks.count(), live_frame->totalSticks.count());
        for (int i = 0; i < live_frame->stickFigures.count(); i++) {
            StickFigure *live_figure = live_frame->stickFigures[i];
            StickFigureData *snapshot_figure = snapshot->stickFigures[i];
            QCOMPARE(snapshot_figure->stickList.count(), live_figure->stickList.count());
            for (int j = 0; j < live_figure->stickList.count(); j++)
                QCOMPARE(snapshot_figure->stickList[j]->myLine, live_figure->stickList[j]->myLine);
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
        QCOMPARE(loaded.stickFigures.count(), snapshot->stickFigures.count());
        QCOMPARE(loaded.frameNumber, snapshot->frameNumber);
        QCOMPARE(loaded.numOfItems, snapshot->numOfItems);
        QCOMPARE(loaded.totalSticks.count(), snapshot->totalSticks.count());
        for (int j = 0; j < snapshot->totalSticks.count(); j++)
            QCOMPARE(loaded.totalSticks[j]->order, snapshot->totalSticks[j]->order);
        QVERIFY(loaded.currentStickFigure != nullptr);

        delete snapshot; // pure data: plain delete, no scratch choreography
    }

    void UndoSimpleEdit()
    {
        AddFrame();
        DrawStick(QPointF(100, 100), QPointF(100, 200));
        int figure_index = CurrentFrame()->stickFigures.indexOf(CurrentStickFigure());
        QLineF original_line = CurrentStickFigure()->stickList[0]->myLine;

        // a "simple" edit: storeUndo(CMD_SIMPLE) + direct model mutation
        view->storeUndo(CMD_SIMPLE);
        CurrentStickFigure()->traslate(10, 20);
        QLineF moved_line = CurrentStickFigure()->stickList[0]->myLine;
        QVERIFY(moved_line != original_line);

        view->undoRedo(MODE_UNDO);
        QCOMPARE(CurrentFrame()->stickFigures[figure_index]->stickList[0]->myLine, original_line);

        view->undoRedo(MODE_REDO);
        QCOMPARE(CurrentFrame()->stickFigures[figure_index]->stickList[0]->myLine, moved_line);
    }

    void UndoAddFrame()
    {
        int frames_before = view->myAnimation->frameList.count();
        int current_number_before = CurrentFrame()->frameNumber;

        AddFrame();
        QCOMPARE(view->myAnimation->frameList.count(), frames_before + 1);

        view->undoRedo(MODE_UNDO);
        QCOMPARE(view->myAnimation->frameList.count(), frames_before);
        QCOMPARE(CurrentFrame()->frameNumber, current_number_before);

        view->undoRedo(MODE_REDO);
        QCOMPARE(view->myAnimation->frameList.count(), frames_before + 1);
    }

    void UndoDeleteFrame()
    {
        AddFrame();
        DrawStick(QPointF(50, 50), QPointF(50, 150));
        int frames_before = view->myAnimation->frameList.count();
        int sticks_before = CurrentFrame()->totalSticks.count();
        int deleted_number = CurrentFrame()->frameNumber;

        // replicates Widget::on_deleteFrameBtn_clicked() (widget.cpp:559)
        view->storeUndo(CMD_DELETEFRAME);
        view->deleteFrame(CurrentFrame());
        view->moveToFrame(CurrentFrame());
        QCOMPARE(view->myAnimation->frameList.count(), frames_before - 1);

        view->undoRedo(MODE_UNDO);
        QCOMPARE(view->myAnimation->frameList.count(), frames_before);
        QCOMPARE(CurrentFrame()->frameNumber, deleted_number);
        QCOMPARE(CurrentFrame()->totalSticks.count(), sticks_before);

        view->undoRedo(MODE_REDO);
        QCOMPARE(view->myAnimation->frameList.count(), frames_before - 1);
    }

    void UndoMoveToFrame()
    {
        AddFrame();
        int origin_number = CurrentFrame()->frameNumber;
        Frame *other_frame = view->myAnimation->frameList[0];
        QVERIFY(other_frame != CurrentFrame());

        // replicates Widget::on_frameListWidget_itemClicked() (widget.cpp:568)
        view->storeUndo(CMD_MOVETOFRAME);
        view->moveToFrame(other_frame);
        QCOMPARE(CurrentFrame()->frameNumber, 0);

        view->undoRedo(MODE_UNDO);
        QCOMPARE(CurrentFrame()->frameNumber, origin_number);
    }

    void ChopAndWeld()
    {
        AddFrame();
        StickFigure *figure = CurrentStickFigure();
        stick *drawn = DrawStick(QPointF(100, 100), QPointF(100, 200));
        QCOMPARE(figure->stickList.count(), 1);

        // replicates Widget::on_splitStick_clicked() (widget.cpp:1198)
        view->storeUndo();
        view->removeHandles(figure);
        stick *added = figure->chop(figure->currentStick);
        view->addHandles(figure);
        CurrentFrame()->totalSticks.append(added);

        QCOMPARE(figure->stickList.count(), 2);
        QCOMPARE(drawn->myLine.p2(), QPointF(100, 150));
        QCOMPARE(added->myLine.p1(), QPointF(100, 150));
        QCOMPARE(added->myLine.p2(), QPointF(100, 200));
        QCOMPARE(added->parent, drawn);

        // replicates Widget::on_joinStick_clicked() (widget.cpp:1186), welding from the master
        figure->currentStick = drawn;
        view->storeUndo();
        QList<stick *> welded = figure->weld(drawn);
        for (stick *s : welded)
            CurrentFrame()->totalSticks.removeAll(s);

        QCOMPARE(figure->stickList.count(), 1);
        QCOMPARE(drawn->myLine.p2(), QPointF(100, 200));
    }

    void MergeTwoStickFigures()
    {
        AddFrame();
        StickFigure *main_figure = CurrentStickFigure();
        stick *main_stick = DrawStick(QPointF(100, 100), QPointF(100, 200));
        StickFigure *to_join = AddStick("joined");
        stick *join_master = DrawStick(QPointF(200, 200), QPointF(200, 300));
        QCOMPARE(CurrentStickFigure(), to_join);

        // replicates the JOIN branch of myView::mousePressEvent() (myview.cpp:314)
        view->removeHandles(main_figure);
        mergeStickFigures(main_figure, main_stick, to_join);
        view->addHandles(main_figure);

        QCOMPARE(main_figure->stickList.count(), 3); // original + invisible linker + clone
        stick *linker = main_figure->stickList[1];
        stick *joined_clone = main_figure->stickList[2];
        QCOMPARE(linker->parent, main_stick);
        QCOMPARE(linker->Pen.style(), Qt::NoPen);
        QCOMPARE(joined_clone->parent, linker);
        QCOMPARE(joined_clone->myLine, join_master->myLine);
        QCOMPARE(main_figure->masterStick, main_stick);
        QVERIFY(main_stick->children.contains(linker));
        QVERIFY(main_stick->children.contains(joined_clone));

        // the JOIN flow then deletes the merged-in layer via deleteStickFigure()
        myStickFigureWidgetList->clearSelection();
        to_join->linkedItem->setSelected(true);
        view->deleteStickFigure();
        for (stick *s : CurrentStickFigure()->stickList)
            if (!CurrentFrame()->totalSticks.contains(s))
                CurrentFrame()->totalSticks.append(s);
        QCOMPARE(CurrentFrame()->stickFigures.count(), 1);
        QCOMPARE(CurrentStickFigure(), main_figure);
    }

    void SplitStickFigure()
    {
        AddFrame();
        StickFigure *figure = CurrentStickFigure();
        stick *master_stick = DrawStick(QPointF(100, 100), QPointF(100, 200));
        stick *child_stick = DrawStick(QPointF(100, 200), QPointF(150, 250));
        QCOMPARE(child_stick->parent, master_stick);
        QCOMPARE(CurrentStickFigure()->currentStick, child_stick);
        int figures_before = CurrentFrame()->stickFigures.count();

        view->splitStickFigure(); // replicates Widget::on_splitBtn_clicked() (widget.cpp:1180)

        QCOMPARE(CurrentFrame()->stickFigures.count(), figures_before + 1);
        StickFigure *branch = CurrentStickFigure();
        QVERIFY(branch != figure);
        QCOMPARE(branch->name, figure->name + "_split");
        QCOMPARE(branch->stickList.count(), 1);
        QCOMPARE(figure->stickList.count(), 1);
        QCOMPARE(branch->masterStick, branch->stickList[0]);
        QCOMPARE(branch->stickList[0]->myLine, QLineF(100, 200, 150, 250));
        QCOMPARE(figure->stickList[0], master_stick);
    }

    void SetMasterOnDirectChain()
    {
        AddFrame();
        StickFigure *figure = CurrentStickFigure();
        stick *old_master = DrawStick(QPointF(200, 100), QPointF(200, 200));
        stick *new_master = DrawStick(QPointF(200, 200), QPointF(200, 300));
        QCOMPARE(figure->masterStick, old_master);
        QCOMPARE(figure->currentStick, new_master);

        // replicates Widget::on_setMasterNodeBtn_clicked() (widget.cpp:1213)
        view->storeUndo();
        figure->setMaster(new_master);

        QCOMPARE(figure->masterStick, new_master);
        QVERIFY(new_master->parent == nullptr);
        QCOMPARE(old_master->parent, new_master);
        // the parent chain between old and new master is inverted (p1/p2 swapped)
        QCOMPARE(old_master->myLine, QLineF(200, 200, 200, 100));
        // the new master keeps its own direction
        QCOMPARE(new_master->myLine, QLineF(200, 200, 200, 300));
        QVERIFY(new_master->children.contains(old_master));
    }

    void AnimationFileRoundTrip()
    {
        AddFrame();
        DrawStick(QPointF(300, 100), QPointF(300, 200));
        QLineF drawn_line = CurrentStickFigure()->stickList[0]->myLine;

        QList<int> figures_per_frame;
        QList<int> sticks_per_frame;
        for (Frame *f : view->myAnimation->frameList) {
            figures_per_frame << f->stickFigures.count();
            int sticks = 0;
            for (StickFigure *s : f->stickFigures)
                sticks += s->stickList.count();
            sticks_per_frame << sticks;
        }
        int frame_count = view->myAnimation->frameList.count();

        QString path = QDir::temp().filePath("marionetta_phase0_test.mAnim");
        view->myAnimation->saveAnimation(path);

        // replicates the load flow in mymainwindow.cpp: clear undo, load, land on frame 0
        view->clearUndo();
        view->myAnimation->loadAnimation(path);
        QCOMPARE(view->myAnimation->frameList.count(), frame_count);
        view->moveToFrame(view->myAnimation->frameList[0]);

        for (int i = 0; i < frame_count; i++) {
            Frame *f = view->myAnimation->frameList[i];
            QCOMPARE(f->stickFigures.count(), figures_per_frame[i]);
            int sticks = 0;
            for (StickFigure *s : f->stickFigures)
                sticks += s->stickList.count();
            QCOMPARE(sticks, sticks_per_frame[i]);
        }
        Frame *last_frame = view->myAnimation->frameList[frame_count - 1];
        QCOMPARE(last_frame->stickFigures.last()->stickList[0]->myLine, drawn_line);

        QFile::remove(path);
    }

    void LibraryRoundTrip()
    {
        AddFrame();
        DrawStick(QPointF(400, 100), QPointF(400, 200));
        StickFigure *source = CurrentStickFigure();
        source->name = "libfig";

        // replicates the add-to-library flow (myview.cpp:1386)
        QVERIFY(view->addStickFigureToLibrary() != nullptr);
        QCOMPARE(stickLibraryBuffer->stickFigures.count(), 1);
        QCOMPARE(myCurrentLibraryWidget->count(), 1);

        // save + reload the library file: this is the path where a missing placeholder int
        // used to desync the whole byte stream (see Frame's operator<< libFlag comment)
        QString path = QDir::temp().filePath("marionetta_phase0_test.marlib");
        view->saveLibrary(path);
        view->loadLibrary(path);

        QCOMPARE(stickLibraryBuffer->stickFigures.count(), 1);
        StickFigureData *reloaded = stickLibraryBuffer->stickFigures[0];
        QCOMPARE(reloaded->name, QString("libfig"));
        QCOMPARE(reloaded->stickList.count(), 1);
        QCOMPARE(reloaded->stickList[0]->myLine, source->stickList[0]->myLine);
        QCOMPARE(myCurrentLibraryWidget->count(), 1);

        // the widget entry carries the data record (what the add-to-scene handler casts)
        StickFigureData *from_item = qvariant_cast<StickFigureData*>(
            myCurrentLibraryWidget->item(0)->data(Qt::UserRole));
        QCOMPARE(from_item, reloaded);

        QFile::remove(path);
    }
};

QTEST_MAIN(TestCharacterization)
#include "tst_characterization.moc"
