#ifndef ANIMATION_H
#define ANIMATION_H
#include <frame.h>
#include <QGraphicsItem>
#include <QListWidgetItem>
class Animation
{
public:
    Animation();
    int version = 1;
    QList<Frame*> frame_list;
    Frame *frame_buffer;
    Frame * AddFrame(QListWidgetItem* item, int pos);
    Frame* current_frame;
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
    QGraphicsScene * scene;
    Frame *DeleteFrame(Frame* frame);
    Frame *SetupFrame(int pos);
    void SaveAnimation(QString file_name);
    void LoadAnimation(QString name);
    void ClearAnimation();
    void UpdateSliders();
    void UpdateTab(int t);
    int width;
    int height;
};
Q_DECLARE_METATYPE(Animation*)
QDataStream &operator<<(QDataStream& stream, const Animation& my_animation);
QDataStream &operator>>(QDataStream& stream,Animation& my_animation);
#endif // ANIMATION_H
