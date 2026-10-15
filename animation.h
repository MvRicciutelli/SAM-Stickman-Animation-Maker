#ifndef ANIMATION_H
#define ANIMATION_H
#include <frame.h>
#include <QGraphicsItem>
#include <QListWidgetItem>
class Animation
{
public:
    Animation();
    /**
     * @brief On-disk format version of the whole animation file, always the CURRENT one:
     *        operator>> resets it after reading, so re-saving an older file writes a
     *        complete current-version file rather than an old header with new content.
     *
     * 2 appends the per-frame names (Frame::name) after the frame blocks — a version-1
     * file simply has no such trailer and still loads.
     *
     * 3 inserts an ImageTable between the header and the frames, and the stick records index
     * into it instead of each inlining its own copy of its pixels. Unlike the v2 trailer this
     * one is NOT ignorable — it sits ahead of the data that needs it — so a v3 file does not
     * open in a build that predates it. v1 and v2 files still load here.
     */
    static constexpr qint32 CURRENT_VERSION = 3; ///< newest file format this build reads; LoadAnimation() refuses a newer header
    qint32 version = CURRENT_VERSION;
    QList<Frame*> frame_list;
    Frame *frame_buffer = nullptr;
    Frame * AddFrame(QListWidgetItem* item, int pos);
    Frame* current_frame = nullptr;
    /**
     * @brief Accessor for the currently displayed frame.
     * @return reference to `current_frame`, so both reads and assignments
     *         (`CurrentFrame() = someFrame`) work exactly like the raw member did.
     *
     * Replaces the `CURRENTFRAME` macro (Phase 5) — same expression, but as a real,
     * symbol-searchable, compiler-checked method instead of blind text substitution.
     */
    Frame*& CurrentFrame() { return current_frame; }
    void UpdateSelection(StickData* item);
    void UpdateSelection(StickFigure* item);
    void UpdateSelection(QPointF point);
    QGraphicsScene * scene = nullptr;
    Frame *DeleteFrame(Frame* frame);
    Frame *SetupFrame(int pos);
    void SaveAnimation(QString file_name);
    /**
     * @brief Replaces the open animation with the contents of a .sam file — or leaves it
     *        completely untouched when the file is refused.
     * @param name path of the file.
     * @return true when loaded. False means nothing at all was changed (frames, canvas size and
     *         scene included) and file_load_error says why: the file could not be opened, was
     *         saved by a newer version (the header's, or one of its stick/figure records — a
     *         newer build can add a record field without touching the header), or held no
     *         readable frame.
     *
     * The file is parsed into pure data FIRST and only applied once nothing was refused, which is
     * what makes the refusal safe: ClearAnimation() used to run before the first byte was read,
     * so a file that turned out unreadable had already destroyed the document.
     */
    bool LoadAnimation(QString name);
    void ClearAnimation();
    void UpdateSliders();

    /**
     * @brief Re-evaluates the three Style-column check boxes that depend on what the selected
     *        stick is PAINTED WITH: "Texture follows stick" and the two "Show texture handle".
     *
     * Called by UpdateSliders() (the selection path) and by MyView::SetGraphics() — an Apply
     * button changes a stick's fill or stroke from a solid colour to a gradient/texture without
     * any selection change, and these boxes are meaningless for the first and meaningful for the
     * second. No-op when nothing is selected, or in the test binary, where the controls are null.
     */
    void UpdateStyleControls();
    void UpdateTab(int t);
    qint32 width = 0;
    qint32 height = 0;
private:
    /**
     * @brief True only while ClearAnimation() is tearing down every frame, so
     *        DeleteFrame() skips re-selecting a "next current frame" that is itself
     *        about to be deleted. Was a file-scope global before the flag sweep —
     *        only Animation's own methods ever touched it, so it lives here now.
     */
    bool clearing_animation = false;
    /**
     * @brief The type whose panels UpdateTab() last put up (-1 = none yet). UpdateSelection()
     *        compares the clicked stick against THIS, not against the current stick: picking a
     *        draw tool swaps the panels to that tool's type without changing the current stick.
     */
    int tab_type = -1;
};
Q_DECLARE_METATYPE(Animation*)
QDataStream &operator<<(QDataStream& stream, const Animation& my_animation);
// niente operator>> : il caricamento passa da Animation::LoadAnimation(), che legge prima in dati puri
#endif // ANIMATION_H
