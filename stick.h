#ifndef STICK_H
#define STICK_H
#include <QGraphicsItem>
#include <math.h>
#include <style.h>
#include <QPainter>
#include <QGraphicsScene>
#include <QLabel>
#define STEPCHILD -1
#define STKTYPE 90000
#define MAXDEPTH 99.9f
#define MINDEPTH -99.9f

extern bool onionRender;
extern QLabel *dbgLbl;
class StickFigure;
class stick:public QGraphicsItem
{

public:
    enum StickType{
        LINE = 5,
        CIRCLE = 6,
        IMAGE = 7,
        RECT = 16,
        TAPER = 18,
        TRAPEZOID = 17
    };
    enum { Type = STKTYPE };
//-------- methods ----------//

    int type() const override { return Type; }
    stick(QLineF *line);
    stick(stick *S);
    stick();
    ~stick();
    stick * parent = nullptr;
    QLineF myLine;
    QPointF getP2(QLineF* line);
    QRectF boundingRect() const ;
    QRectF br;
    QImage calcImg();
    void paint(QPainter *painter, const QStyleOptionGraphicsItem *option, QWidget *widget);
    void refresh(int mode);
    void endRotation();
    void setLine(QLineF* line);
    void rotate(QPointF* point);
    void manipulate(QPointF* point);
    bool isMaster() const;
//-------- variables----------//
    bool highlight = false;
    bool selected = false;
    bool stepchild = false;
    //bool master = false;
    bool hardTop = false;
    bool hardBottom = false;
    int parentIdx = 0;
    int rotationSpinboxBuffer = 0;
    int scaleSpinBoxBuffer = 100;
    int version = 1;
    int stickType = StickType::LINE;
    float imgAngle = 0;
    float imgWScale = 1;
    float imgHScale = 1;
    float scaleBuffer = 1;
    float widthBuffer = 1;
    int imgAngleOffset = 0;
    int order = 0;
    float angleBuffer = 0; //usato per disegnare lo stick nei refresh
    float angleBuffer2 = 0; //usato nella rotazione di tutto lo stickfigure
    QPointF imgOffset = QPointF(0,0);
    QRectF imgRect;
    QList<stick*> children;
    QList<void*> linkedStickFigure;
    QList<QString> imgNameList;
    QPen Pen;
    QBrush Brush;
    qreal Z;
    QImage* stickImg;
    QGraphicsEllipseItem* stickHandle[2];
    QList<QImage*>stickImgList;
    StickFigure* myStickFigure = nullptr;
    QRectF updateBr(int mode);
    void rotate(float angle);
    void scale(float scaleFactor);
    void addImage(QImage *imgToAdd, QString name);
    void populateImageListWidget();
    void removeImgFromList(int idx);
    float measureDistanceFromLine(QPointF p, QLineF l);
private:
    //-------- methods----------//

    void  calcAngle();
    //-------- variables----------//


    QRectF constructBr();
};
void sceneRemover(QGraphicsScene *sceneToClear);
QRectF calcImgRect(QLineF l,QSizeF s);
#endif // STICK_H
