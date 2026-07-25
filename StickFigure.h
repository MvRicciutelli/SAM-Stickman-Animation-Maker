#ifndef StickFigure_H
#define StickFigure_H
#include <QPointF>
#include <QLineF>
#include <QtMath>
#include <stick.h>
#include <stickfiguredata.h>
#include <QGraphicsScene>
#include <QListWidgetItem>

class StickFigure
{
public:
    ~StickFigure();
    StickFigure(QListWidgetItem* item);
    StickFigure();
    void  StartDrawing(QPointF* point, QPen pen, QBrush brush);
    void  PreviewDrawing(QPointF* point);
    void  EndDrawing(QPointF* point);
    void  ReturnLine();
    void  CancelDrawing();
    void DeleteStick(int idx);
    void SaveStickFigure(QString name);
    int draw_count = 0;
    int stick_num = 0;
    int version = 1;

    /**
     * @brief Raised by MyView::mousePressEvent() at the start of a rotate gesture;
     *        RotateStickFigure() consumes it on the first mouse move to record the
     *        starting angle (so the first delta is 0). One gesture at a time exists
     *        app-wide, hence static — formerly the free global
     *        `start_stick_figure_rotation` (myview.cpp), now scoped to the class
     *        that reads it.
     */
    static bool start_stick_figure_rotation;
    /**
     * @brief Same handshake as start_stick_figure_rotation, for the scale gesture:
     *        raised on mouse press, consumed by Scale() to record the starting
     *        distance. Formerly the free global `start_stick_figure_scale`.
     */
    static bool start_stick_figure_scale;
    int SelectStick( QPointF * point);
    QList<StickData*>stick_list;
    void SetLineFromPoint(QPointF *point);
    QLineF line_buffer;
    StickData* current_stick = nullptr;
    StickData* master_stick = nullptr;
    qreal base_z = 0;
    void Highlight(bool setting);
    QListWidgetItem* linked_item = nullptr;
    QGraphicsScene * scene = nullptr;
    void UpdateZ(float value);
    QIcon*stick_figure_icon = nullptr;
    QPixmap * icon_img = nullptr;
    QPointF origin_pos_buffer;
    float scene_rotation_buffer = 0;
    /**
     * @brief Persistent offset of the rotation/scale pivot handle from the master's
     *        origin (master_stick->my_line.p1()). The handle scene position is always
     *        master.p1 + pivot_offset, so it follows the figure when the master is
     *        dragged; only a select-tool Ctrl-drag rewrites it. (0,0) = handle on the
     *        master origin (the default). See MyView pivot handling.
     */
    QPointF pivot_offset = QPointF(0, 0);
    QString name;

    void UpdateIcon();
    bool selecting_origin = false;
    void LoadStickFigure(QString name);
    void Refresh(int mode = 0);

    void RotateStickFigure(QPointF *coord, QList<StickGraphicsItem*>sticks, bool complete_rotation);
    void Scale(QPointF *coord);
    void Traslate(qreal dx, qreal dy);

    QList<StickData*> Weld(StickData *master);
    StickData* Chop(StickData *origin);
    void SetMaster(StickData *to_master);
    void SetDirectChainAsMaster(StickData *to_master);
    void SetStepChildAsMaster(StickData *to_master);
    void InvertMaster(StickData *to_master);
    void StepDownMaster(StickData *to_master);
    void UpdateBoundingRects();
    void Elongate(QPointF new_end_point, StickData *my_stick);
    float MaxZ();
    float MinZ();
    float MeasureDistanceFromLine(QPointF p, QLineF l);
private:

    QPointF p0;
    QPointF p1;
    StickData* stick_buffer = nullptr; ///< in-progress stick's data; a view is attached separately by the caller (see StartDrawing())
    int SelectOrigin( QPointF * point);
    StickData* parent_buffer = nullptr;
    QGraphicsScene * my_scene = nullptr;


};
// to use QVariant in order to insert a stickfigure pointer into the widget list
Q_DECLARE_METATYPE(StickFigure*)

/**
 * @brief Streams a live figure by extracting its data first (.stck save path).
 * @param stream destination stream.
 * @param my_stick_figure live figure to write.
 * @return the stream, for chaining.
 */
QDataStream &operator<<(QDataStream& stream, const StickFigure& my_stick_figure);

/**
 * @brief Streams data into a live figure, attaching it to the scene (.stck load path).
 * @param stream source stream.
 * @param my_stick_figure live figure to fill; must have a valid scene member.
 * @return the stream, for chaining.
 */
QDataStream &operator>>(QDataStream& stream,StickFigure& my_stick_figure);

/**
 * @brief Snapshots a live figure into a pure-data record (undo/copy/library source).
 * @param source_figure live figure to copy from.
 * @return new StickFigureData; caller takes ownership.
 */
StickFigureData *ExtractStickFigureData(const StickFigure &source_figure);

/**
 * @brief Rebuilds a live figure from a data record (paste/restore/load target).
 * @param target_figure live figure to fill; existing sticks are freed first.
 * @param source_data record to copy from (left untouched).
 * @param attach_to_scene true adds a view for each rebuilt stick to the figure's scene
 *        member (paste and .stck load); false leaves them detached (file load, where
 *        MoveToFrame() attaches the displayed frame afterwards).
 * @param update_icon true renders the layer icon at the end (skipped on file load,
 *        where only the displayed frame's icons matter — historical perf fix).
 */
void PopulateLiveStickFigure(StickFigure *target_figure, const StickFigureData &source_data,
                             bool attach_to_scene, bool update_icon);

void MergeStickFigures(StickFigure* main_stick_figure, StickData* main_stick,StickFigure* to_join);
void SplitStickFigures(StickFigure* split, StickData* origin,StickFigure* branch);

// ------------------------------------------------------------- IK (myview.cpp DRAW_IK) ----
// Pure-model half of the inverse-kinematics feature: chain bookkeeping and the FABRIK
// solve itself. MyView (myview.h/cpp) owns the tool state and hit-testing that decide
// *when* these run; nothing here touches a scene or a view beyond calling Refresh() on
// one already resolved via StickData::view, the same non-owning back-pointer every
// other interactive stick operation (Rotate/Manipulate/Scale) already uses.

/**
 * @brief True if `descendant` is `ancestor` itself or one of its descendants.
 * @param ancestor candidate ancestor.
 * @param descendant stick to test; walked upward via `parent`.
 * @return whether the walk from `descendant` reaches `ancestor`.
 */
bool IsStickDescendantOf(const StickData *ancestor, const StickData *descendant);

/**
 * @brief Finds the chain a stick belongs to.
 * @param stick any member of a chain (IK_START, IK_NODE or IK_TARGET_ON/OFF).
 * @return the chain's IK_START stick, or nullptr if `stick` has no IK role at all.
 */
StickData *IkChainStart(StickData *stick);

/**
 * @brief Whether `stick` is part of a chain whose target is currently toggled ON.
 * @param stick stick to test (any role, including IK_NONE).
 * @return true only for a chain member (any role) whose target's role is IK_TARGET_ON.
 */
bool StickIsInActiveIkChain(const StickData *stick);

/**
 * @brief Commits a validated chain: marks every stick between `chain_start` and
 *        `target` (inclusive) with its role and wires the start/target pointers.
 * @param chain_start the stick clicked first (already IK_START; see myview.cpp's
 *        DRAW_IK case for why the role is set at the first click, not here).
 * @param target the stick clicked second; becomes IK_TARGET_ON.
 *
 * Caller (myview.cpp) has already validated `target` via IsStickDescendantOf() plus
 * the stepchild-continuity walk FABRIK's joint-sharing assumption depends on — see
 * MyView::FindIkTargetCandidate().
 */
void CommitIkChain(StickData *chain_start, StickData *target);

/**
 * @brief Reverts every stick of the chain starting at `chain_start` to IK_NONE and
 *        clears the start/target pointers, without deleting any stick.
 * @param chain_start the chain's IK_START stick; no-op if null or not actually IK_START.
 */
void ClearIkChain(StickData *chain_start);

/**
 * @brief Flips a chain between IK_TARGET_ON and IK_TARGET_OFF.
 * @param target the chain's target stick (IK_TARGET_ON or IK_TARGET_OFF); no-op otherwise.
 */
void ToggleIkChain(StickData *target);

/**
 * @brief Garbage-collects every IK chain in a figure: keeps only sticks that belong to
 *        a fully structurally-valid chain (a distinct IK_START ancestor of an
 *        IK_TARGET_ON/OFF through an unbroken parent path, with start<->target pointers
 *        agreeing), and resets every other IK-roled stick back to IK_NONE.
 * @param figure figure to sanitize.
 *
 * The single validity/"delete the chain if conditions are insufficient" pass the spec's
 * split/merge rules end with. Tree-restructuring ops (Chop/Weld/MergeStickFigures/
 * SplitStickFigures) apply their specific role inheritance first, then call this to drop
 * anything the mutation left inconsistent (e.g. a start welded onto its own target, or a
 * chain the split cut in half). Pointer-safe: it only reads live sticks in stick_list.
 */
void SanitizeFigureIkChains(StickFigure *figure);

/**
 * @brief FABRIK solve: reshapes the chain anchored at `chain_start` so its target's
 *        free end reaches `desired_target_pos` (clamped to the chain's combined
 *        length when unreachable), up to 10 iterations or a 2-unit tolerance.
 * @param chain_start the chain's IK_START stick; no-op if it has no target.
 * @param desired_target_pos the position the user is dragging the target's free end to.
 *
 * Follows the specified technique literally: each forward-pass step swaps a segment's
 * P1/P2 so QLineF::setLength() (which only ever keeps P1 fixed) can rescale from the
 * FAR joint instead of the near one, then swaps back; the backward pass needs no swap
 * since P1 already plays the fixed role. Never calls Rotate()/Manipulate() on a chain
 * stick — those cascade to children with their own (incompatible) assumptions; a
 * solved stick's off-chain children are instead translated rigidly by its own
 * before/after delta, mirroring Manipulate()'s existing child-cascade rule.
 */
void SolveIkChain(StickData *chain_start, const QPointF &desired_target_pos);
/**
 * @brief Deep-copies a stick's persistent data (plain field copy since Phase 1a).
 * @param dest freshly-constructed record to fill.
 * @param src record to copy from; tree pointers are not copied.
 */
void CloneStick(StickData* dest, StickData*src);
#endif // StickFigure_H
