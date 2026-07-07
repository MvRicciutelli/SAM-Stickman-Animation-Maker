#ifndef StickFigure_H
#define StickFigure_H
#include <QPointF>
#include <QLineF>
#include <QtMath>
#include <stick.h>
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
    QList<stick*>stickList;
    void setLineFromPoint(QPointF *point);
    QLineF lineBuffer;
    stick* currentStick = nullptr;
    stick* masterStick = nullptr;
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
    void cloneStickFigure(StickFigure *target, StickFigure *source);

    void rotateStickFigure(QPointF *coord, QList<stick*>sticks, bool completeRotation);
    void scale(QPointF *coord);
    void traslate(qreal dx, qreal dy);

    QList<stick*> weld(stick *master);
    stick* chop(stick *origin);
    void setMaster(stick *toMaster);
    void setDirectChainAsMaster(stick *toMaster);
    void setStepChildAsMaster(stick *toMaster);
    void invertMaster(stick *toMaster);
    void stepDownMaster(stick *toMaster);
    void updateBoundingRects();
    void elongate(QPointF newEndPoint, stick *myStick);
    float maxZ();
    float minZ();
    float measureDistanceFromLine(QPointF p, QLineF l);
private:

    QPointF p0;
    QPointF p1;
    stick* stickBuffer = nullptr;
    int selectOrigin( QPointF * point);
    /**
     * @brief Parent picked for the stick being drawn (tree pointer, so data-typed).
     *
     * Bridge note (until Phase 2c): every StickData in a live figure is actually a
     * stick, so the rare places that need the view side downcast explicitly.
     */
    StickData* parentBuffer = nullptr;
    QGraphicsScene * myScene = nullptr;


};
// to use QVariant in order to insert a stickfigure pointer into the widget list
Q_DECLARE_METATYPE(StickFigure*)
QDataStream &operator<<(QDataStream& stream, const StickFigure& myStickFigure);
QDataStream &operator>>(QDataStream& stream,StickFigure& myStickFigure);
void cloneStickFigure(StickFigure *dest, StickFigure *src);
void mergeStickFigures(StickFigure* mainStickFigure, stick* mainStick,StickFigure* toJoin);
void splitStickFigures(StickFigure* split, stick* origin,StickFigure* branch);
/**
 * @brief Deep-copies a stick's persistent data (plain field copy since Phase 1a).
 * @param dest freshly-constructed stick to fill.
 * @param src stick to copy from; tree pointers are not copied.
 */
void cloneStick(stick* dest, stick*src);
#endif // StickFigure_H
