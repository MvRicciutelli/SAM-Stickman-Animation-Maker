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
/**
 * @brief Deep-copies a stick's persistent data (plain field copy since Phase 1a).
 * @param dest freshly-constructed record to fill.
 * @param src record to copy from; tree pointers are not copied.
 */
void CloneStick(StickData* dest, StickData*src);
#endif // StickFigure_H
