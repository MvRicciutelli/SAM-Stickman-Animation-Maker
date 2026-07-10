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
    StickData *DrawStick(QPointF from, QPointF to)
    {
        StickFigure *figure = CurrentStickFigure();
        view->storeUndo();
        figure->startDrawing(&from, view->myPen, view->myBrush);
        StickData *drawn = figure->currentStick;
        FramePresenter::AttachView(drawn, scene);
        drawn->view->selected = true;
        drawn->hardTop = hardTopCheck->isChecked();
        drawn->hardBottom = hardBottomCheck->isChecked();
        drawn->Pen = view->myPen;
        figure->endDrawing(&to);
        scene->addItem(drawn->view->stickHandle[0]);
        scene->addItem(drawn->view->stickHandle[1]);
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
        StickData source;
        source.myLine = QLineF(1, 2, 3, 4);
        source.Z = 5.5;
        source.stickType = StickData::LINE;
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
        StickData loaded;
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
        StickData source;
        source.myLine = QLineF(10, 20, 30, 40);
        source.Z = 1;
        source.stickType = StickData::CIRCLE;
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
        StickData loaded;
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
        StickData source;
        source.stickType = StickData::IMAGE;
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
        StickData loaded;
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

    // Phase 6: DataCloneIsIndependent above only exercises LINE sticks (no images) —
    // this covers the deep-image-copy path of StickData::CopyFrom() through the
    // StickFigureData::Clone() wrapper, pure data, no scene/view involved
    void StickFigureDataCloneDeepCopiesImages()
    {
        StickFigureData source;
        StickData *imgStick = new StickData();
        imgStick->stickType = StickData::IMAGE;
        imgStick->myLine = QLineF(0, 0, 0, 50);
        QImage *img = new QImage(4, 4, QImage::Format_ARGB32);
        img->fill(Qt::red);
        imgStick->stickImgList << img;
        imgStick->stickImg = img;
        source.stickList << imgStick;
        source.masterStick = imgStick;

        StickFigureData *copy = source.Clone();
        QCOMPARE(copy->stickList.count(), 1);
        QCOMPARE(copy->stickList[0]->stickImgList.count(), 1);
        QVERIFY(copy->stickList[0]->stickImg != source.stickList[0]->stickImg); // deep copy, not a shared pointer
        QCOMPARE(*copy->stickList[0]->stickImg, *source.stickList[0]->stickImg); // same pixels

        // mutating the clone's image must not affect the source's
        copy->stickList[0]->stickImg->fill(Qt::blue);
        QCOMPARE(source.stickList[0]->stickImg->pixelColor(0, 0), QColor(Qt::red));

        delete copy;
    }

    // Phase 6: FrameData::Clone() remaps totalSticks and currentStickFigure onto the
    // cloned figures by index — this is easy to get wrong (stale pointers into the
    // source) and had no direct pure-data test before
    void FrameDataCloneRemapsReferences()
    {
        FrameData source;
        source.frameNumber = 3;
        source.numOfItems = 2;

        StickFigureData *figureA = new StickFigureData();
        StickData *stickA = new StickData();
        stickA->myLine = QLineF(0, 0, 0, 50);
        figureA->stickList << stickA;
        figureA->masterStick = stickA;
        figureA->currentStick = stickA;
        source.stickFigures << figureA;

        StickFigureData *figureB = new StickFigureData();
        StickData *stickB = new StickData();
        stickB->myLine = QLineF(0, 50, 0, 100);
        figureB->stickList << stickB;
        figureB->masterStick = stickB;
        source.stickFigures << figureB;

        // deliberately out of figure order, to prove the remap is index-based, not positional
        source.totalSticks << stickB << stickA;
        source.currentStickFigure = figureA;

        FrameData *copy = source.Clone();
        QCOMPARE(copy->stickFigures.count(), 2);
        QVERIFY(copy->stickFigures[0] != figureA);
        QCOMPARE(copy->totalSticks.count(), 2);
        QCOMPARE(copy->totalSticks[0], copy->stickFigures[1]->stickList[0]); // was stickB
        QCOMPARE(copy->totalSticks[1], copy->stickFigures[0]->stickList[0]); // was stickA
        QCOMPARE(copy->currentStickFigure, copy->stickFigures[0]);

        QLineF original_line = stickA->myLine;
        copy->stickFigures[0]->stickList[0]->myLine.translate(10, 10);
        QCOMPARE(stickA->myLine, original_line);

        delete copy;
    }

    // Phase 6: FrameData's operator>> rebuilds totalSticks from each stick's `order`
    // field, not from insertion/figure order (see framedata.cpp) — this is the one
    // reordering behavior FrameSnapshotRoundTrip (which uses sequential orders via a
    // live frame) never actually exercises
    void FrameDataRoundTrip()
    {
        FrameData source;
        source.frameNumber = 5;
        source.numOfItems = 2;

        StickFigureData *figureA = new StickFigureData();
        StickData *stickA = new StickData();
        stickA->myLine = QLineF(0, 0, 0, 50);
        stickA->order = 1; // second in z-order despite being the first figure added
        figureA->stickList << stickA;
        figureA->masterStick = stickA;
        figureA->currentStick = stickA;
        source.stickFigures << figureA;

        StickFigureData *figureB = new StickFigureData();
        StickData *stickB = new StickData();
        stickB->myLine = QLineF(0, 50, 0, 100);
        stickB->order = 0; // first in z-order despite being the second figure added
        figureB->stickList << stickB;
        figureB->masterStick = stickB;
        source.stickFigures << figureB;

        source.currentStickFigure = figureA;

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

        QCOMPARE(loaded.stickFigures.count(), 2);
        QCOMPARE(loaded.frameNumber, source.frameNumber);
        QCOMPARE(loaded.numOfItems, source.numOfItems);
        QCOMPARE(loaded.totalSticks.count(), 2);
        QCOMPARE(loaded.totalSticks[0]->myLine, stickB->myLine); // order 0
        QCOMPARE(loaded.totalSticks[1]->myLine, stickA->myLine); // order 1
        QVERIFY(loaded.currentStickFigure != nullptr);
        QCOMPARE(loaded.currentStickFigure->currentStick->myLine, stickA->myLine);
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
        for (StickData *s : pasted->stickList)
            CurrentFrame()->totalSticks.append(s);
        view->addHandles(pasted);

        QCOMPARE(pasted->stickList.count(), 2);
        QCOMPARE(pasted->stickList[1]->parent, pasted->stickList[0]);
        // pasted sticks really live on the scene (attach_to_scene = true)
        QCOMPARE(pasted->stickList[0]->view->scene(), scene);
    }

    // the imgListWidget cleanup moved from ~stick to explicit calls at the live-delete
    // sites (Phase 1b): deleting the on-screen IMAGE stick empties its widget rows,
    // while scratch snapshots (undo buffer) never touch them
    void DeleteImageStickClearsImageListWidget()
    {
        AddFrame();
        StickData *drawn = DrawStick(QPointF(500, 100), QPointF(500, 200));
        // upgrade to IMAGE like drawCmd's DRAWIMG branch (myview.cpp:486)
        drawn->stickType = StickData::IMAGE;
        QImage *image = new QImage(8, 8, QImage::Format_ARGB32);
        image->fill(Qt::green);
        drawn->view->addImage(image, "img");
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

        view->undo_stack.undo();
        QCOMPARE(CurrentFrame()->stickFigures[figure_index]->stickList[0]->myLine, original_line);

        view->undo_stack.redo();
        QCOMPARE(CurrentFrame()->stickFigures[figure_index]->stickList[0]->myLine, moved_line);
    }

    void UndoAddFrame()
    {
        int frames_before = view->myAnimation->frameList.count();
        int current_number_before = CurrentFrame()->frameNumber;

        AddFrame();
        QCOMPARE(view->myAnimation->frameList.count(), frames_before + 1);

        view->undo_stack.undo();
        QCOMPARE(view->myAnimation->frameList.count(), frames_before);
        QCOMPARE(CurrentFrame()->frameNumber, current_number_before);

        view->undo_stack.redo();
        QCOMPARE(view->myAnimation->frameList.count(), frames_before + 1);
    }

    // isolates the CMD_ADDFRAME+undo path specifically: RestoreFrameSnapshot() calls
    // Animation::deleteFrame() on the added frame (~Frame -> delete each StickFigure ->
    // ~StickFigure()), which is a DIFFERENT code path from clearFrame() (used on the frame
    // being restored INTO, not the one being removed). Built the image stick directly,
    // bypassing DrawStick()/storeUndo(), so only ONE undo command (CMD_ADDFRAME) exists —
    // undoing it must delete the whole added frame, sticks included, in a single step.
    void UndoAddFrameFreesImageStickListWidgetEntries()
    {
        AddFrame();
        StickFigure *figure = CurrentStickFigure();
        StickData *img_stick = new StickData();
        img_stick->myLine = QLineF(700, 100, 700, 200);
        // attacca la view MENTRE il tipo è ancora LINE (default): il costruttore di stick
        // chiama updateBr(0) subito, che per un IMAGE deferenzierebbe stickImg prima che
        // addImage() lo assegni — stesso ordine di myView::drawCmd() (myview.cpp)
        StickGraphicsItem *img_stick_view = FramePresenter::AttachView(img_stick, scene);
        img_stick_view->myStickFigure = figure;
        img_stick->stickType = StickData::IMAGE;
        QImage *image = new QImage(8, 8, QImage::Format_ARGB32);
        image->fill(Qt::blue);
        img_stick_view->addImage(image, "leak-check"); // also registers an entry in imgListWidget
        figure->stickList.append(img_stick);
        figure->masterStick = img_stick;
        figure->currentStick = img_stick;
        CurrentFrame()->totalSticks.append(img_stick);
        QCOMPARE(imgListWidget->count(), 1);

        view->undo_stack.undo(); // CMD_ADDFRAME+undo: deletes the frame img_stick lives in
        QCOMPARE(imgListWidget->count(), 0);
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

        view->undo_stack.undo();
        QCOMPARE(view->myAnimation->frameList.count(), frames_before);
        QCOMPARE(CurrentFrame()->frameNumber, deleted_number);
        QCOMPARE(CurrentFrame()->totalSticks.count(), sticks_before);

        view->undo_stack.redo();
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

        view->undo_stack.undo();
        QCOMPARE(CurrentFrame()->frameNumber, origin_number);
    }

    void ChopAndWeld()
    {
        AddFrame();
        StickFigure *figure = CurrentStickFigure();
        StickData *drawn = DrawStick(QPointF(100, 100), QPointF(100, 200));
        QCOMPARE(figure->stickList.count(), 1);

        // replicates Widget::on_splitStick_clicked() (widget.cpp:1198)
        view->storeUndo();
        view->removeHandles(figure);
        StickData *added = figure->chop(figure->currentStick);
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
        QList<StickData *> welded = figure->weld(drawn);
        for (StickData *s : welded)
            CurrentFrame()->totalSticks.removeAll(s);

        QCOMPARE(figure->stickList.count(), 1);
        QCOMPARE(drawn->myLine.p2(), QPointF(100, 200));
    }

    void MergeTwoStickFigures()
    {
        AddFrame();
        StickFigure *main_figure = CurrentStickFigure();
        StickData *main_stick = DrawStick(QPointF(100, 100), QPointF(100, 200));
        StickFigure *to_join = AddStick("joined");
        StickData *join_master = DrawStick(QPointF(200, 200), QPointF(200, 300));
        QCOMPARE(CurrentStickFigure(), to_join);

        // replicates the JOIN branch of myView::mousePressEvent() (myview.cpp:314)
        view->removeHandles(main_figure);
        mergeStickFigures(main_figure, main_stick, to_join);
        view->addHandles(main_figure);

        QCOMPARE(main_figure->stickList.count(), 3); // original + invisible linker + clone
        StickData *linker = main_figure->stickList[1];
        StickData *joined_clone = main_figure->stickList[2];
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
        for (StickData *s : CurrentStickFigure()->stickList)
            if (!CurrentFrame()->totalSticks.contains(s))
                CurrentFrame()->totalSticks.append(s);
        QCOMPARE(CurrentFrame()->stickFigures.count(), 1);
        QCOMPARE(CurrentStickFigure(), main_figure);
    }

    void SplitStickFigure()
    {
        AddFrame();
        StickFigure *figure = CurrentStickFigure();
        StickData *master_stick = DrawStick(QPointF(100, 100), QPointF(100, 200));
        StickData *child_stick = DrawStick(QPointF(100, 200), QPointF(150, 250));
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
        StickData *old_master = DrawStick(QPointF(200, 100), QPointF(200, 200));
        StickData *new_master = DrawStick(QPointF(200, 200), QPointF(200, 300));
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

    // exercises the exact destructor cascade Widget::~Widget()'s `delete view` triggers
    // (myView -> Animation -> Frame -> StickFigure -> stick) with an IMAGE stick present,
    // on a throwaway myView instance — proves removing undoFlag's guard from
    // ~StickFigure() didn't turn app shutdown into a crash: scene->removeItem() and
    // RemoveImagesFromListWidget() now run unconditionally, and this is where they'd
    // fault if `scene`/`imgListWidget` weren't still valid at that point.
    void ViewDestructorHandlesImageStickWithoutCrash()
    {
        myView *throwaway_view = new myView();
        throwaway_view->setScene(scene);
        throwaway_view->myAnimation->scene = scene;
        Frame *throwaway_frame = throwaway_view->setUpFrame(0);
        throwaway_view->moveToFrame(throwaway_frame);
        StickFigure *figure = throwaway_frame->addStickFigure(myStickFigureWidgetList);

        StickData *img_stick = new StickData();
        img_stick->myLine = QLineF(50, 50, 50, 150);
        // vedi UndoAddFrameFreesImageStickListWidgetEntries: attacca la view prima di
        // passare a IMAGE, altrimenti updateBr(0) nel costruttore deferenzia stickImg==nullptr
        StickGraphicsItem *img_stick_view = FramePresenter::AttachView(img_stick, scene);
        img_stick_view->myStickFigure = figure;
        img_stick->stickType = StickData::IMAGE;
        QImage *image = new QImage(8, 8, QImage::Format_ARGB32);
        image->fill(Qt::green);
        img_stick_view->addImage(image, "shutdown-check");
        figure->stickList.append(img_stick);
        figure->masterStick = img_stick;
        figure->currentStick = img_stick;
        QCOMPARE(imgListWidget->count(), 1);

        delete throwaway_view; // same cascade as Widget::~Widget()'s `delete view`
        QCOMPARE(imgListWidget->count(), 0);
    }

    // Prep A for REFACTOR_PLAN.md Phase 2c: synthetic QTest mouse events driven through the
    // REAL mousePressEvent/mouseMoveEvent/mouseReleaseEvent overrides, not the DrawStick()/
    // storeUndo() helpers used elsewhere that call model methods directly. This is the one
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
        view->setTool(DRAW);
        int sticks_before = CurrentStickFigure()->stickList.count();

        QPoint p1 = view->mapFromScene(QPointF(100, 100));
        QPoint p2 = view->mapFromScene(QPointF(100, 200));
        QTest::mouseClick(view->viewport(), Qt::LeftButton, Qt::NoModifier, p1);
        QTest::mouseClick(view->viewport(), Qt::LeftButton, Qt::NoModifier, p2);

        QCOMPARE(CurrentStickFigure()->stickList.count(), sticks_before + 1);
        StickData *drawn = CurrentStickFigure()->stickList.last();
        QVERIFY(drawn->view && scene->items().contains(drawn->view));
        QCOMPARE(drawn->stickType, (int)StickData::LINE);

        view->setTool(NOTOOL);
    }

    // exercises a real drag gesture (mousePress + several mouseMove + mouseRelease) through
    // the NOTOOL/CLAMP rotate path, then undo/redo — the multi-event-gesture shape
    // FrameUndoCommand's design depends on (storeUndo() captures "before" at press time;
    // "after" is captured lazily on the first undo — see myview.cpp), now driven by real
    // QTest-synthesized events end to end instead of calling the model methods directly.
    void DragRotateViaRealMouseEventsIsUndoable()
    {
        AddFrame();
        view->resize(800, 600);
        DrawStick(QPointF(400, 100), QPointF(400, 200));
        int figure_index = CurrentFrame()->stickFigures.indexOf(CurrentStickFigure());
        QLineF original_line = CurrentStickFigure()->stickList[0]->myLine;

        view->setTool(NOTOOL);
        QTest::mousePress(view->viewport(), Qt::LeftButton, Qt::NoModifier,
                           view->mapFromScene(QPointF(400, 200)));
        for (int i = 1; i <= 5; i++) {
            QTest::mouseMove(view->viewport(),
                              view->mapFromScene(QPointF(400 + i * 10, 200 - i * 5)));
        }
        QTest::mouseRelease(view->viewport(), Qt::LeftButton, Qt::NoModifier,
                             view->mapFromScene(QPointF(450, 175)));

        QLineF dragged_line = CurrentFrame()->stickFigures[figure_index]->stickList[0]->myLine;
        QVERIFY(dragged_line != original_line);

        view->undo_stack.undo();
        QCOMPARE(CurrentFrame()->stickFigures[figure_index]->stickList[0]->myLine, original_line);

        view->undo_stack.redo();
        QCOMPARE(CurrentFrame()->stickFigures[figure_index]->stickList[0]->myLine, dragged_line);

        view->setTool(NOTOOL);
    }
};

QTEST_MAIN(TestCharacterization)
#include "tst_characterization.moc"
