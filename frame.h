#ifndef FRAME_H
#define FRAME_H
#include <StickFigure.h>
#include <framedata.h>
#include <QGraphicsScene>
class Frame
{
public:
    Frame();
    ~Frame();
    //StickFigure* AddStickFigure(QListWidgetItem* item);
    QList<StickFigure*> stick_figures;
    StickFigure* stick_figure_buffer;
    StickFigure* current_stick_figure;
    /**
     * @brief Accessor for the currently selected stickfigure in this frame.
     * @return reference to `current_stick_figure`, so reads and assignments
     *         (`CurrentStickFigure() = someFigure`) both work like the raw member did.
     *
     * Replaces the `CURRENTSTICKFIGURE` macro (Phase 5).
     */
    StickFigure*& CurrentStickFigure() { return current_stick_figure; }
    /**
     * @brief Accessor for the currently selected stick within `CurrentStickFigure()`.
     * @return reference to that stickfigure's `current_stick`.
     *
     * Replaces the `CS` macro (Phase 5). Same as the macro, this dereferences
     * `current_stick_figure` without a null check — callers are responsible for that,
     * exactly as before.
     */
    StickData*& CurrentStick() { return current_stick_figure->current_stick; }
    StickData *SelectStick(QPointF point);
    StickData *SelectStick(StickFigure* S, StickData* s= nullptr);
    StickFigure* RemoveStickFigure(StickFigure* toRemove);
    QIcon* frame_icon;
    int num_of_items = 0;
    QPixmap * icon_img;
    //QImage * renderImg;
    QGraphicsScene * scene;
    int frame_number = 0;
    QListWidgetItem * linked_item;
    void UpdateIcon();
    QList <StickData*> total_sticks;
    StickFigure *AddStickFigure( QListWidget *my_list_widget, QString name = "");
    void UpdateRender();
    void ClearFrame();
    void NormalizeDepth();
    void Refresh(int mode = 0);
    void StickfigureToBottom(StickFigure *S);
    void StickfigureToTop(StickFigure *S);
    float MinMaxZ(int option);
    void StickfigureAboveBelow(StickFigure *toMove, StickFigure *previous, int mode);
    void CleanTotalSticks();
};
Q_DECLARE_METATYPE(Frame*);

/**
 * @brief Streams a live frame by extracting its data first (file save path).
 * @param stream destination stream.
 * @param my_frame live frame to write.
 * @return the stream, for chaining.
 */
QDataStream & operator<< (QDataStream& stream, const Frame& my_frame);

/**
 * @brief Streams data into a live frame WITHOUT attaching sticks to the scene
 *        (file load path — MoveToFrame() attaches the displayed frame afterwards).
 * @param stream source stream.
 * @param my_frame live frame to fill; must start empty.
 * @return the stream, for chaining.
 */
QDataStream & operator>> (QDataStream& stream, Frame& my_frame);

/**
 * @brief Snapshots a live frame into a pure-data record (undo/copy source).
 * @param source_frame live frame to copy from.
 * @return new FrameData; caller takes ownership.
 */
FrameData *ExtractFrameData(const Frame &source_frame);

/**
 * @brief Rebuilds a live frame's content from a data record (restore/paste/load target).
 * @param target_frame live frame to fill; must start with no stickfigures
 *        (fresh from SetUpFrame() or just cleared by ClearFrame()).
 * @param source_data record to copy from (left untouched).
 * @param attach_to_scene forwarded to PopulateLiveStickFigure() for every figure.
 * @param update_icons forwarded to PopulateLiveStickFigure() for every figure.
 */
void PopulateLiveFrame(Frame *target_frame, const FrameData &source_data,
                       bool attach_to_scene, bool update_icons);
#endif // FRAME_H
