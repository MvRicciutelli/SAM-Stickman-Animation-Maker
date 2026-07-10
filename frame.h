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
    //StickFigure* addStickFigure(QListWidgetItem* item);
    QList<StickFigure*> stickFigures;
    StickFigure* stickFigureBuffer;
    StickFigure* currentStickFigure;
    /**
     * @brief Accessor for the currently selected stickfigure in this frame.
     * @return reference to `currentStickFigure`, so reads and assignments
     *         (`CurrentStickFigure() = someFigure`) both work like the raw member did.
     *
     * Replaces the `CURRENTSTICKFIGURE` macro (Phase 5).
     */
    StickFigure*& CurrentStickFigure() { return currentStickFigure; }
    /**
     * @brief Accessor for the currently selected stick within `CurrentStickFigure()`.
     * @return reference to that stickfigure's `currentStick`.
     *
     * Replaces the `CS` macro (Phase 5). Same as the macro, this dereferences
     * `currentStickFigure` without a null check — callers are responsible for that,
     * exactly as before.
     */
    StickData*& CurrentStick() { return currentStickFigure->currentStick; }
    StickData *selectStick(QPointF point);
    StickData *selectStick(StickFigure* S, StickData* s= nullptr);
    StickFigure* removeStickFigure(StickFigure* toRemove);
    QIcon* frameIcon;
    int numOfItems = 0;
    QPixmap * iconImg;
    //QImage * renderImg;
    QGraphicsScene * scene;
    int frameNumber = 0;
    QListWidgetItem * linkedItem;
    void updateIcon();
    QList <StickData*> totalSticks;
    StickFigure *addStickFigure( QListWidget *myListWidget, QString name = "");
    void updateRender();
    void clearFrame();
    void normalizeDepth();
    void refresh(int mode = 0);
    void stickfigureToBottom(StickFigure *S);
    void stickfigureToTop(StickFigure *S);
    float minMaxZ(int option);
    void stickfigureAboveBelow(StickFigure *toMove, StickFigure *previous, int mode);
    void cleanTotalSticks();
};
Q_DECLARE_METATYPE(Frame*);

/**
 * @brief Streams a live frame by extracting its data first (file save path).
 * @param stream destination stream.
 * @param myFrame live frame to write.
 * @return the stream, for chaining.
 */
QDataStream & operator<< (QDataStream& stream, const Frame& myFrame);

/**
 * @brief Streams data into a live frame WITHOUT attaching sticks to the scene
 *        (file load path — moveToFrame() attaches the displayed frame afterwards).
 * @param stream source stream.
 * @param myFrame live frame to fill; must start empty.
 * @return the stream, for chaining.
 */
QDataStream & operator>> (QDataStream& stream, Frame& myFrame);

/**
 * @brief Snapshots a live frame into a pure-data record (undo/copy source).
 * @param source_frame live frame to copy from.
 * @return new FrameData; caller takes ownership.
 */
FrameData *ExtractFrameData(const Frame &source_frame);

/**
 * @brief Rebuilds a live frame's content from a data record (restore/paste/load target).
 * @param target_frame live frame to fill; must start with no stickfigures
 *        (fresh from setUpFrame() or just cleared by clearFrame()).
 * @param source_data record to copy from (left untouched).
 * @param attach_to_scene forwarded to PopulateLiveStickFigure() for every figure.
 * @param update_icons forwarded to PopulateLiveStickFigure() for every figure.
 */
void PopulateLiveFrame(Frame *target_frame, const FrameData &source_data,
                       bool attach_to_scene, bool update_icons);
#endif // FRAME_H
