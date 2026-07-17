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

// --- Tool ids stored in `tool` and set by SetTool(); the toolbar button slots in
// --- widget.cpp map each button to one of these. mousePressEvent/mouseMoveEvent/
// --- ChangeTool() all switch() on the active id.
#define DRAW 5          ///< draw a straight segment (2 clicks: origin, then free end)
#define DRAWCIRCLE 6    ///< draw a circle-typed stick (StickData::CIRCLE)
#define ELONGATE 19     ///< drag the free end to change only a stick's length
#define DRAWIMG 7       ///< draw an image stick from image_draw_buffer (StickData::IMAGE)
#define MANIPULATE 8    ///< drag the free end, carrying the stick's children along
#define ROTATE 9        ///< rotate the whole selected figure around its master
#define SCALE 10        ///< scale the whole selected figure
#define NOTOOL 4        ///< default: click to select, drag to rotate a single stick/subtree
#define CLAMP 20        ///< like NOTOOL but rotates only the clicked stick (no children)
#define MOVESCENE 11    ///< translate every figure in the frame together
#define ROTATESCENE 12  ///< rotate every figure in the frame around the canvas centre
#define SCALESCENE 13   ///< scale every figure in the frame around the canvas centre
#define JOIN 14         ///< weld the current figure onto the one clicked next (MergeStickFigures)
#define SPLIT 15        ///< break the current figure at the selected stick (SplitStickFigures)
#define DRAWRECT 16     ///< draw a rectangle-typed stick (StickData::RECT)
#define DRAWTRAPEZOID 17 ///< draw a trapezoid-typed stick (StickData::TRAPEZOID)
#define DRAWTAPER 18    ///< draw a tapered stick (StickData::TAPER)

// --- Layer reorder directions passed to MoveStickFigureZ().
#define MODE_DOWN 1     ///< move the selected layer one step down (towards the back)
#define MODE_UP 0       ///< move the selected layer one step up (towards the front)
#define MODE_TOP 2      ///< send the selected layer all the way to the front
#define MODE_BOTTOM 3   ///< send the selected layer all the way to the back

// --- Structural tag carried by every undo entry; StoreUndo() records which kind of
// --- change is about to happen and RestoreFrameSnapshot() dispatches on it.
#define CMD_SIMPLE 0        ///< in-place edit, current frame stays (move/scale/rotate/add-remove stick)
#define CMD_ADDFRAME 1      ///< a frame was created (undo must delete it again)
#define CMD_DELETEFRAME 2   ///< a frame was removed (undo must recreate it)
#define CMD_MOVETOFRAME 3   ///< a frame's whole content changed, no frame added/removed

// --- Direction passed to RestoreFrameSnapshot(), telling apart the two halves of the
// --- add/delete-frame pair (an undo of an add == a redo of a delete, and vice versa).
#define MODE_REDO 0
#define MODE_UNDO 1

// --- Which graphic attribute SetGraphics() copies from my_pen/my_brush onto a stick.
#define ATTRIBUTE_PENCOLOR 0
#define ATTRIBUTE_PENWIDTH 1
#define ATTRIBUTE_BRUSHCOLOR 2
#define ATTRIBUTE_BRUSHSIZE 3


/**
 * @brief The animation canvas: a QGraphicsView that owns the whole document and turns
 *        mouse/keyboard gestures into edits of the live Animation.
 *
 * MyView is the hub of the app. It owns the `Animation` (frames → figures → sticks,
 * see animation.h / frame.h / StickFigure.h / stickdata.h) and is the one place raw
 * input becomes a model change: the `mousePressEvent`/`mouseMoveEvent`/
 * `mouseReleaseEvent`/`keyPressEvent`/`wheelEvent` overrides switch on the active
 * `tool` and drive the matching StickFigure/StickData operation, wrapping each gesture
 * in a `StoreUndo()`/undo_stack entry.
 *
 * The surrounding UI talks to it from two sides: the toolbar/panels in widget.cpp
 * (`view->...`) trigger tools and edits, and the menu in mymainwindow.cpp handles
 * file/export. Only sticks actually on screen get a `StickGraphicsItem` view, attached
 * through `FramePresenter::AttachView()`; everything MyView stores for undo/copy/library
 * is pure data (`FrameData`/`StickFigureData`/`StickData`). See CLAUDE.md for the full
 * data-model and undo design.
 */
class MyView : public QGraphicsView
{
    Q_OBJECT
public:
    /**
     * @brief Builds the empty canvas: creates the Animation, the default pen/brush, the
     *        undo stack and the autosave hook.
     * @param parent parent widget (Qt ownership).
     */
    explicit MyView(QWidget *parent = nullptr);

    /**
     * @brief Tears the document down: detaches every handle, then deletes all frames,
     *        the Animation and the copy/library buffers.
     */
    ~MyView();

    unsigned int tool = NOTOOL; ///< active tool id (one of the DRAW/ROTATE/... constants)

    /**
     * @brief Leaves the current tool cleanly, then activates a new one.
     * @param new_tool one of the tool-id constants above.
     *
     * Calls ChangeTool() first so a half-drawn stick or tool-specific UI is cancelled
     * before the switch. Called from the toolbar button slots in widget.cpp.
     */
    void SetTool(int new_tool);

    Animation * my_animation; ///< the document root: owns the frame list (animation.h)

    /**
     * @brief Deletes the selected figure layer from the current frame and its list row.
     *
     * Refuses to remove the last remaining layer (a frame must keep at least one
     * figure). Also reached from keyPressEvent() when Delete is pressed on a master
     * stick, and from the JOIN tool after a merge. Selection is moved to a surviving
     * layer so a following Move-Layer works on the right row.
     */
    void DeleteStickFigure();

    QPen    my_pen;   ///< current pen pushed onto newly drawn sticks and applied by SetGraphics()
    QBrush  my_brush; ///< current brush, same role as my_pen; both edited from widget.cpp's panels
    StickFigureData* copy_stick_figure_buffer = nullptr; ///< owned copy-paste buffer (pure data since Phase 2b)

    /**
     * @brief Switches the on-screen frame: clears the scene/layer list, repopulates
     *        them from `frame`, refreshes onion skins and selection.
     * @param frame the frame to display; becomes current_frame.
     *
     * The single routine every frame change goes through — the frame list slot in
     * widget.cpp, undo/redo (RestoreFrameSnapshot()), file load (mymainwindow.cpp) and
     * playback all call it. Attaches a live view to any stick that doesn't have one yet
     * (a freshly loaded frame), via FramePresenter::AttachView().
     */
    void MoveToFrame(Frame *frame);

    /**
     * @brief Rebuilds the semi-transparent "ghosts" of the previous frames.
     *
     * Clears the old onion pixmaps, then renders the N frames before the current one
     * (N = onion_skin_sb spinbox) onto faded pixmaps. Works on Clone()d StickData so it
     * never steals a live stick's `view` back-pointer (same pattern as
     * StickFigure::UpdateIcon()). Skipped during playback.
     */
    void UpdateOnionSkins();
    QList<QGraphicsPixmapItem*> onion_skins; ///< pixmaps currently drawn as onion skins (owned while shown)

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
    void StoreUndo(int command = CMD_SIMPLE);

    /**
     * @brief Removes a frame from the animation and renumbers the ones after it.
     * @param frame frame to delete (its list row is taken first).
     *
     * The list-widget/renumber half of a frame deletion; the model half lives in
     * Animation::DeleteFrame(). Used by the delete-frame button (widget.cpp) and by
     * RestoreFrameSnapshot() when undoing an add / redoing a delete.
     */
    void DeleteFrame(Frame *frame);

    /**
     * @brief Inserts a new empty frame at a position.
     * @param pos index where the frame is created.
     * @return the created Frame.
     *
     * Thin pass-through to Animation::SetupFrame(); kept as a member so undo restore
     * and the add-frame button share one entry point.
     */
    Frame *SetUpFrame(int pos);

    /**
     * @brief Converts a viewport point to scene coordinates, clamped to the canvas.
     * @param my_coord in/out: the point to clamp (kept within my_rect + a 10% margin).
     * @param starting_point raw viewport position from the mouse event.
     *
     * Every mouse handler runs the cursor through this so an edit can't drag a stick
     * arbitrarily far off the page. Writes the global `coord` as a side effect.
     */
    void MapMyCoords(QPointF &my_coord, QPointF starting_point);

    /**
     * @brief Wipes the undo/redo history (QUndoStack::clear()).
     *
     * Called when a new document replaces the current one — file load and
     * New-Blank-Animation in mymainwindow.cpp — so stale frames can't be restored onto
     * a different animation.
     */
    void ClearUndo();

    /**
     * @brief Loads a stick-figure library file into the buffer and its list widget.
     * @param file_name .stck-style library file to read.
     * @param mode 0 (default) rebuilds the widget entries; non-zero loads data only
     *        (legacy path, currently unused).
     */
    void LoadLibrary(QString file_name, int mode = 0);

    /**
     * @brief Copies the current figure into the library as a data record.
     * @return the live source figure, or nullptr if it has no sticks.
     */
    StickFigure *AddStickFigureToLibrary();
    QList<QGraphicsPixmapItem*> preview_list; ///< rendered frames shown one-by-one during playback (PreparePreview/DisplayPreview)

    /**
     * @brief Removes the selected entry from the library (widget + data buffer).
     */
    void RemoveFromLibrary();

    /**
     * @brief Applies the current pen/brush attribute to one stick or the whole figure.
     * @param all false: only the current stick; true: every stick in the current figure.
     * @param attribute which property to copy (ATTRIBUTE_PENCOLOR/PENWIDTH/BRUSHCOLOR).
     *
     * Reads my_pen/my_brush (set by widget.cpp's colour/width controls) and pushes the
     * chosen attribute onto the model, then refreshes the affected views and icon.
     */
    void SetGraphics(bool all, int attribute);

    /**
     * @brief Splits the current figure at the selected stick into a new figure.
     *
     * The selected stick and its subtree become a new "_split" figure layer; the rest
     * stays. Refuses to split at the master. The model work is done by SplitStickFigures()
     * (StickFigure.cpp); this method handles the layer list, handles and z-order list.
     */
    void SplitStickFigure();

    /**
     * @brief Empties the current library (widget + buffer) and starts a fresh one.
     */
    void ClearCurrentLib();

    /**
     * @brief Re-applies the canvas size (W/H) to every frame, optionally repositioning.
     * @param option REPO (recentre only), REPOSCALE (recentre + rescale) or NOREPO.
     *
     * Called from MyMainWindow::GetAnimationParams() after the user changes the
     * animation dimensions in the options dialog.
     */
    void SizeChange(int option);

    /**
     * @brief Uniformly scales one frame's figures about the canvas centre.
     * @param F frame to scale.
     * @param scale_amount linear scale factor.
     * @param R reference rect whose centre is the scaling pivot.
     *
     * Helper for SizeChange()'s REPOSCALE path. NOTE: "Rescale animation" currently has
     * a width-only scale-factor bug (see CLAUDE.md "Known unresolved items").
     */
    void ScaleFrame(Frame *F, float scale_amount, QRectF R);

    /**
     * @brief Pre-renders every frame to a pixmap for smooth playback.
     *
     * Fills preview_list with one flattened image per frame (rendered from Clone()d
     * data, so live views are untouched). DisplayPreview() then flips through them.
     */
    void PreparePreview();

    /**
     * @brief Shows the pre-rendered frame at `idx` during playback.
     * @param idx index into preview_list; the previous frame's pixmap is removed first.
     */
    void DisplayPreview(int idx);

    /**
     * @brief Selects the stick/figure nearest the last click and syncs the layer list.
     *
     * The picking routine behind the NOTOOL/CLAMP/ELONGATE/MANIPULATE tools: asks the
     * Animation which stick is under `coord`, then mirrors that into the layer list
     * widget selection and the image list. No-op on an empty frame.
     */
    void ArrowSelection();

    /**
     * @brief Moves the selected figure layer in the Z (drawing) order.
     * @param increment signed step (used for MODE_UP/MODE_DOWN).
     * @param mode MODE_UP / MODE_DOWN / MODE_TOP / MODE_BOTTOM.
     *
     * Keeps the model z-order (Frame::Stickfigure...()) and the layer list widget in
     * sync. Driven by the move-layer buttons in widget.cpp.
     */
    void MoveStickFigureZ(int increment, int mode);

    /**
     * @brief Renumbers the current frame's `order` fields and refreshes num_of_items.
     * @param f retained for call-site compatibility; ignored (always uses the current frame).
     *
     * Run by StoreUndo() (and a couple of widget.cpp edits) right before snapshotting,
     * so the saved z-order matches the on-screen stacking.
     */
    void UpdateFrameOrder(Frame *f);

    /**
     * @brief Adds every stick's endpoint handles of a figure to the scene.
     * @param S figure whose handles become visible (master gets a green origin handle).
     */
    void AddHandles(StickFigure*S);

    /**
     * @brief Removes a figure's endpoint handles from the scene (without deleting them).
     * @param S figure whose handles are hidden.
     */
    void RemoveHandles(StickFigure*S);
public slots:
    /**
     * @brief Writes the current library buffer to a file.
     * @param file_name destination path.
     */
    void SaveLibrary(QString file_name);
signals:
    /**
     * @brief Fired when it's time to autosave; wired to MyMainWindow::AutoSave() in
     *        MyMainWindow::DoConnections(). Emitted from the undo_stack indexChanged
     *        hook every auto_save_interval operations.
     */
    void  AutoSaveTrigger();


    // QWidget interface
protected:
    virtual void mouseReleaseEvent(QMouseEvent *event) override;
    virtual void mouseMoveEvent(QMouseEvent *event) override;
    virtual void mousePressEvent(QMouseEvent *event) override;
    virtual void keyPressEvent(QKeyEvent *event) override;
private:
    /**
     * @brief Performs one click of the 2-click drawing gesture.
     * @param point click position in scene coordinates.
     * @param mode the active DRAW* tool id (decides the resulting StickData type).
     *
     * First click starts a stick (StickFigure::StartDrawing() + AttachView()); second
     * click finishes it (EndDrawing()) and files it in the frame's z-order list.
     */
    void DrawCmd(QPointF* point, int mode);

    /**
     * @brief Cleanup run when leaving a tool (called by SetTool before switching).
     *
     * Mostly cancels a half-finished draw so an unfinished stick doesn't survive a tool
     * change, and frees the image draw buffer when leaving DRAWIMG.
     */
    void ChangeTool();

    // QWidget interface


protected:
    virtual void wheelEvent(QWheelEvent *event) override;
};

#endif // MYVIEW_H
