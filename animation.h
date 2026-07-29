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
     *        operator>> resets it after reading, so re-saving a version-1 file writes
     *        version 2 rather than a v1 header with v2 content.
     *
     * 2 appends the per-frame names (Frame::name) after the frame blocks — a version-1
     * file simply has no such trailer and still loads.
     */
    qint32 version = 2;
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
    void LoadAnimation(QString name);
    void ClearAnimation();
    void UpdateSliders();
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
};
Q_DECLARE_METATYPE(Animation*)
QDataStream &operator<<(QDataStream& stream, const Animation& my_animation);
QDataStream &operator>>(QDataStream& stream,Animation& my_animation);
#endif // ANIMATION_H
