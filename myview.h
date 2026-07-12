#ifndef MYVIEW_H
#define MYVIEW_H


#include <QGraphicsView>
#include <QPointF>
#include <QMouseEvent>
#include <QGraphicsItem>
#include <QLineF>
#include <QUndoStack>
#include <StickFigure.h>
#include <animation.h>
#define DRAW 5
#define DRAWCIRCLE 6
#define ELONGATE 19
#define DRAWIMG 7
#define MANIPULATE 8
#define ROTATE 9
#define SCALE 10
#define NOTOOL 4
#define CLAMP 20
#define MOVESCENE 11
#define ROTATESCENE 12
#define SCALESCENE 13
#define JOIN 14
#define SPLIT 15
#define DRAWRECT 16
#define DRAWTRAPEZOID 17
#define DRAWTAPER 18
#define MODE_DOWN 1
#define MODE_UP 0
#define MODE_TOP 2
#define MODE_BOTTOM 3
#define CMD_SIMPLE 0
#define CMD_ADDFRAME 1
#define CMD_DELETEFRAME 2
#define CMD_MOVETOFRAME 3

#define MODE_REDO 0
#define MODE_UNDO 1

#define ATTRIBUTE_PENCOLOR 0
#define ATTRIBUTE_PENWIDTH 1
#define ATTRIBUTE_BRUSHCOLOR 2
#define ATTRIBUTE_BRUSHSIZE 3


class MyView : public QGraphicsView
{
    Q_OBJECT
public:
    explicit MyView(QWidget *parent = nullptr);
    ~MyView();
    unsigned int tool = NOTOOL;
    void setTool(int Tool);
    StickFigure *addStickFigure(QListWidgetItem* item);
    Animation * my_animation;
    void deleteStickFigure();
    QPen    myPen;
    QBrush  myBrush;
    StickFigureData* copyStickFigureBuffer = nullptr; ///< owned copy-paste buffer (pure data since Phase 2b)
    void moveToFrame(Frame *frame);
    void updateOnionSkins();
    QList<QGraphicsPixmapItem*> onionSkins;

    /**
     * @brief Undo/redo history, replacing the hand-rolled undoBuffer/redoBuffer
     *        (REFACTOR_PLAN.md Phase 4). Every entry is a FrameUndoCommand
     *        (myview.cpp) holding before/after `FrameData` snapshots of a single
     *        frame; `MAXUNDO` is enforced via `setUndoLimit()` in the constructor.
     */
    QUndoStack undo_stack;

    /**
     * @brief Restores a frame from a snapshot, dispatching on the command tag.
     * @param command_tag CMD_SIMPLE / CMD_ADDFRAME / CMD_DELETEFRAME / CMD_MOVETOFRAME —
     *        which structural change (if any) accompanied the edit being reverted/
     *        reapplied, and therefore whether a frame needs to be created/removed
     *        first (mirrors the pre-Phase-4 `undoRedo()` dispatch).
     * @param mode MODE_UNDO or MODE_REDO — which direction the CMD_ADDFRAME/
     *        CMD_DELETEFRAME pair goes (add un-does a delete and vice versa).
     * @param snapshot the frame content to restore; not modified or taken over.
     *
     * Called only by FrameUndoCommand::undo()/redo() (myview.cpp) — public because
     * that class lives outside MyView, not because other callers are expected.
     */
    void RestoreFrameSnapshot(int command_tag, int mode, FrameData *snapshot);

    /**
     * @brief Snapshots the current frame as the "before" state of a new edit and
     *        pushes it onto `undo_stack`.
     * @param command CMD_SIMPLE (default) / CMD_ADDFRAME / CMD_DELETEFRAME /
     *        CMD_MOVETOFRAME — see RestoreFrameSnapshot().
     *
     * Call BEFORE mutating, exactly like the pre-Phase-4 version — callers are
     * unchanged by this refactor. The "after" state is captured lazily, the first
     * time this edit is undone (see FrameUndoCommand in myview.cpp), since until
     * then the live frame already *is* the after-state and needs no separate copy.
     */
    void storeUndo(int command = CMD_SIMPLE);
    void deleteFrame(Frame *frame);
    Frame *setUpFrame(int pos);
    void mapMyCoords(QPointF &myCoord, QPointF startingPoint);
    void clearUndo();
    void loadLibrary(QString fileName, int mode = 0);
    StickFigure *addStickFigureToLibrary();
    QList<QGraphicsPixmapItem*> previewList;
    void removeFromLibrary();
    void setGraphics(bool all, int attribute);
    void splitStickFigure();
    void clearCurrentLib();
    void sizeChange(int option);
    void scaleFrame(Frame *F, float scaleAmount, QRectF R);
    void preparePreview();
    void displayPreview(int idx);
    void arrowSelection();
    void moveStickFigureZ(int increment, int mode);
    void updateFrameOrder(Frame *f);
    void addHandles(StickFigure*S);
    void removeHandles(StickFigure*S);
public slots:
    void saveLibrary(QString fileName);
signals:
    void  autoSaveTrigger();


    // QWidget interface
protected:
    virtual void mouseReleaseEvent(QMouseEvent *event) override;
    virtual void mouseMoveEvent(QMouseEvent *event) override;
    virtual void mousePressEvent(QMouseEvent *event) override;
    virtual void keyPressEvent(QKeyEvent *event) override;
private:
    void drawCmd(QPointF* point, int mode);
    void Rotate(QGraphicsLineItem* item);
    void updateRotationBuffer();
    void changeTool();

    // QWidget interface



    // QWidget interface
protected:
    virtual void wheelEvent(QWheelEvent *event) override;
};

#endif // MYVIEW_H
