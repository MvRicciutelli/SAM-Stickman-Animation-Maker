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
    QList<Frame*> frameList;
    Frame *frameBuffer;
    Frame * addFrame(QListWidgetItem* item, int pos);
    Frame* currentFrame;
    void updateSelection(stick* item);
    void updateSelection(StickFigure* item);
    void updateSelection(QPointF point);
    QGraphicsScene * scene;
    Frame *deleteFrame(Frame* frame);
    Frame *setupFrame(int pos);
    void saveAnimation(QString fileName);
    void loadAnimation(QString name);
    void clearAnimation();
    void updateSliders();
    void updateTab(int t);
    int width;
    int height;
};
Q_DECLARE_METATYPE(Animation*)
QDataStream &operator<<(QDataStream& stream, const Animation& myAnimation);
QDataStream &operator>>(QDataStream& stream,Animation& myAnimation);
#endif // ANIMATION_H
