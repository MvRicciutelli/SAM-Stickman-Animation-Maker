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
    StickFigure* stick_figure_buffer = nullptr;
    StickFigure* current_stick_figure = nullptr;
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
     * @return reference to that stickfigure's `current_stick`, or a reference to a
     *         static null placeholder when no stickfigure is current.
     *
     * Replaces the `CS` macro (Phase 5). Unlike the macro, this no longer dereferences
     * a null `current_stick_figure`: several UI handlers (slider/spinbox callbacks,
     * advanced tabs) call it *before* checking `CurrentStickFigure()`, and after an
     * undo/redo restore the current figure can legitimately be null — the placeholder
     * turns every one of those latent crashes into a harmless "no current stick".
     * Assignments made through the placeholder are deliberately discarded.
     */
    StickData*& CurrentStick() {
        static StickData* no_current_figure = nullptr;
        if(current_stick_figure == nullptr){
            no_current_figure = nullptr;
            return no_current_figure;
        }
        return current_stick_figure->current_stick;
    }
    StickData *SelectStick(StickFigure* S, StickData* s= nullptr);
    StickFigure* RemoveStickFigure(StickFigure* to_remove);
    QIcon* frame_icon = nullptr;
    int num_of_items = 0;
    QPixmap * icon_img = nullptr;
    //QImage * render_img;
    QGraphicsScene * scene = nullptr;
    int frame_number = 0;
    /**
     * @brief Optional user-given frame name, empty by default. Shown next to the frame
     *        number in the thumbnail's label band (UpdateLabel()) and set by
     *        double-clicking the row (Widget::on_frameListWidget_itemDoubleClicked).
     *        Travels through undo/copy as FrameData::name.
     */
    QString name;
    QListWidgetItem * linked_item = nullptr;
    void UpdateIcon();

    /**
     * @brief Repaints just the thumbnail's label band ("<number> <name>") and pushes the
     *        result back onto `linked_item`, without re-rendering the frame content.
     *
     * Split out of UpdateIcon() because the label changes far more often than the picture:
     * a rename, and every renumbering of the frames after an insertion/deletion
     * (Animation::SetupFrame(), MyView::DeleteFrame()) — a full UpdateIcon() per shifted
     * frame would re-render the whole scene for each of them.
     */
    void UpdateLabel();
    QList <StickData*> total_sticks;
    StickFigure *AddStickFigure( QListWidget *my_list_widget, QString name = "");
    void UpdateRender();
    void ClearFrame();
    void NormalizeDepth();
    void Refresh(int mode = 0);
    void StickfigureToBottom(StickFigure *S);
    void StickfigureToTop(StickFigure *S);
    float MinMaxZ(int option);
    void StickfigureAboveBelow(StickFigure *to_move, StickFigure *previous, int mode);
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
