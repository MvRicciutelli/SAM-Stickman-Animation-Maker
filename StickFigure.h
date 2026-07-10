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
    void  startDrawing(QPointF* point, QPen pen, QBrush brush);
    void  previewDrawing(QPointF* point);
    void  endDrawing(QPointF* point);
    void  returnLine();
    void  cancelDrawing();
    void deleteStick(int idx);
    void saveStickFigure(QString name);
    int drawCount;
    int stickNum = 0;
    int version = 1;
    int selectStick( QPointF * point);
    QList<StickData*>stickList;
    void setLineFromPoint(QPointF *point);
    QLineF lineBuffer;
    StickData* currentStick = nullptr;
    StickData* masterStick = nullptr;
    qreal baseZ;
    void highlight(bool setting);
    QListWidgetItem* linkedItem;
    QGraphicsScene * scene;
    void updateZ(float value);
    QIcon*stickFigureIcon = nullptr;
    QPixmap * iconImg = nullptr;
    QPointF originPosBuffer;
    float sceneRotationBuffer = 0;
    QString name;

    void updateIcon();
    bool selectingOrigin = false;
    void loadStickFigure(QString name);
    void refresh(int mode = 0);

    void rotateStickFigure(QPointF *coord, QList<StickGraphicsItem*>sticks, bool completeRotation);
    void scale(QPointF *coord);
    void traslate(qreal dx, qreal dy);

    QList<StickData*> weld(StickData *master);
    StickData* chop(StickData *origin);
    void setMaster(StickData *toMaster);
    void setDirectChainAsMaster(StickData *toMaster);
    void setStepChildAsMaster(StickData *toMaster);
    void invertMaster(StickData *toMaster);
    void stepDownMaster(StickData *toMaster);
    void updateBoundingRects();
    void elongate(QPointF newEndPoint, StickData *myStick);
    float maxZ();
    float minZ();
    float measureDistanceFromLine(QPointF p, QLineF l);
private:

    QPointF p0;
    QPointF p1;
    StickData* stickBuffer = nullptr; ///< in-progress stick's data; a view is attached separately by the caller (see startDrawing())
    int selectOrigin( QPointF * point);
    StickData* parentBuffer = nullptr;
    QGraphicsScene * myScene = nullptr;


};
// to use QVariant in order to insert a stickfigure pointer into the widget list
Q_DECLARE_METATYPE(StickFigure*)

/**
 * @brief Streams a live figure by extracting its data first (.stck save path).
 * @param stream destination stream.
 * @param myStickFigure live figure to write.
 * @return the stream, for chaining.
 */
QDataStream &operator<<(QDataStream& stream, const StickFigure& myStickFigure);

/**
 * @brief Streams data into a live figure, attaching it to the scene (.stck load path).
 * @param stream source stream.
 * @param myStickFigure live figure to fill; must have a valid scene member.
 * @return the stream, for chaining.
 */
QDataStream &operator>>(QDataStream& stream,StickFigure& myStickFigure);

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
 *        moveToFrame() attaches the displayed frame afterwards).
 * @param update_icon true renders the layer icon at the end (skipped on file load,
 *        where only the displayed frame's icons matter — historical perf fix).
 */
void PopulateLiveStickFigure(StickFigure *target_figure, const StickFigureData &source_data,
                             bool attach_to_scene, bool update_icon);

void mergeStickFigures(StickFigure* mainStickFigure, StickData* mainStick,StickFigure* toJoin);
void splitStickFigures(StickFigure* split, StickData* origin,StickFigure* branch);
/**
 * @brief Deep-copies a stick's persistent data (plain field copy since Phase 1a).
 * @param dest freshly-constructed record to fill.
 * @param src record to copy from; tree pointers are not copied.
 */
void cloneStick(StickData* dest, StickData*src);
#endif // StickFigure_H
