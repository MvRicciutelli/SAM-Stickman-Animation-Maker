#include "stick.h"
#include <QGraphicsItem>
#include <QVector>
#include <QListWidget>
#include <StickFigure.h>
bool onionRender = false;
extern QGraphicsRectItem* myRect;
extern QPixmap * imageDrawBuffer ;
extern QListWidget* imgListWidget;
extern bool renderFlag;
extern bool playBack;
extern int HANDLESIZE;
/**
 * @details Since Phase 2c: wraps an existing StickData (non-owning) and wires the
 * back-pointer (`stick_data->view = this`) so tree-walk code holding a `StickData*`
 * can reach this view without a separate lookup table. Endpoint handles are created
 * here but attached to the scene by the caller (FramePresenter::AttachView() or the
 * drawing/paste/library code it's called from).
 */
StickGraphicsItem::StickGraphicsItem(StickData *stick_data)
{
    this->setFlag(QGraphicsItem::ItemIsSelectable,true);
    data = stick_data;
    data->view = this;
    highlight = true;
    stickHandle[0] = new QGraphicsEllipseItem();
    stickHandle[1] = new QGraphicsEllipseItem();
    stickHandle[0]->setZValue(100);stickHandle[1]->setZValue(100);
    stickHandle[0]->setBrush(QBrush(Qt::red));
    stickHandle[1]->setBrush(QBrush(Qt::red));
    br = updateBr(0);
}
/**
 * @details Frees only what this object owns: the endpoint handles. `data` is
 * non-owning (never deleted here) — whoever owns the StickData's container frees it
 * separately. Clears the back-pointer so a stray `data->view` can't outlive this
 * object; a destroy-before-detach bug becomes a null dereference instead of a
 * use-after-free, easier to notice.
 */
StickGraphicsItem::~StickGraphicsItem(){
    if(data && data->view == this)
        data->view = nullptr;
    delete stickHandle[0] ;
    delete stickHandle[1] ;
}
/**
 * @details Called right before a LIVE stick's view is detached (deleteStick, weld,
 * cancelDrawing, FramePresenter::DetachView()): empties the rows this stick's images
 * occupy in the image list widget, which mirrors the on-screen selected IMAGE stick.
 * Scratch deletions (undo/copy/library buffers never have a view at all) and render
 * clones (onion skins, icons, preview/export — see below) simply never call it.
 */
void StickGraphicsItem::RemoveImagesFromListWidget(){
    if(data->stickType != StickData::IMAGE)
        return;
    int j = 0;
    for(QImage*i:data->stickImgList)
    {
        Q_UNUSED(i);
        QListWidgetItem * item= imgListWidget->takeItem(j);
        delete  item;
        j++;
    }
}
void StickGraphicsItem::paint(QPainter *painter, const QStyleOptionGraphicsItem *option, QWidget *widget)
{
    Q_UNUSED(option);
    Q_UNUSED(widget);
    QPointF p(data->myLine.p1().x(),data->myLine.p1().y());
    this->setZValue(data->Z);
    if(!onionRender)
        painter->setPen(data->Pen);
    else{
        QPen onionSkinPen(data->Pen);
        onionSkinPen.setColor(Qt::gray);
        painter->setPen(onionSkinPen);
    }
    if(data->stickType == StickData::IMAGE){
        // ogni immagine la facciamo partire dal punto 0,0
        // la ruotiamo
        // la trasliamo al punto di p1
        // il punto p1 deve essere sempre il topleft corner
        QImage scaled = calcImg();
        // in caso di onion rendering converti l'immagine in grayscale
        if(onionRender){
            scaled.convertToFormat(QImage::Format_Grayscale8);
        }
        p = QPointF(data->myLine.p1().x(),data->myLine.p1().y());
        QPointF p2 = QPointF(-scaled.width()/2,-scaled.height()/2);
        painter->translate(p);
        painter->rotate(data->imgAngle+data->imgAngleOffset);
        QPointF p3 = QPointF(data->imgOffset);
        painter->translate(p3);
        QPixmap pixmap;
        pixmap.convertFromImage(scaled);

        painter->drawPixmap(p2,
                            pixmap);
        painter->translate(-p3.x(),-p3.y());
        painter->rotate(-data->imgAngle-data->imgAngleOffset);
        painter->translate(-p.x(),-p.y());

    }
    else if(data->stickType == StickData::RECT || data->stickType == StickData::CIRCLE){

        painter->setBrush(data->Brush);
        painter->translate(p);
        painter->rotate(data->imgAngle);
         if(data->stickType == StickData::RECT){
             QPointF c(-data->imgWScale/2,0);
            painter->drawRect(c.x(),c.y(),data->imgWScale,data->myLine.length());
         }
         else{
             QPointF c(-data->imgWScale*data->myLine.length()/2,0);
             painter->drawEllipse(c.x(),c.y(),data->myLine.length()*data->imgWScale,data->myLine.length());
         }
        painter->rotate(-data->imgAngle);
        painter->translate(-p.x(),-p.y());

    }
    else if(data->stickType == StickData::TRAPEZOID || data->stickType == StickData::TAPER){
        QPointF points[] = { QPointF(-data->imgWScale/2,0),QPointF(data->imgWScale/2,0),QPointF(data->imgHScale/2,data->myLine.length()), QPointF(-data->imgHScale/2,data->myLine.length())};

        QVector <QPointF>pVect = {points[0],points[1],points[2],points[3]};
        QPolygonF poly(pVect);
        painter->setBrush(data->Brush);
        painter->translate(p);
        painter->rotate(data->imgAngle);

        if(data->stickType == StickData::TAPER){

            QPainterPath totalPath;
            totalPath.setFillRule(Qt::WindingFill);
            QRectF pie1(QPointF(-data->imgWScale/2,-data->imgWScale/2),QSize(data->imgWScale,data->imgWScale));
            if(!data->hardBottom){
                QPainterPath piePath;
                piePath.moveTo(points[0]);
                piePath.arcTo(pie1,0,180);
                piePath.closeSubpath();
                totalPath.addPath(piePath);
            }
            totalPath.addPolygon(poly);
            QRectF pie2(QPointF(-data->imgHScale/2,data->myLine.length()-data->imgHScale/2),QSize(data->imgHScale,data->imgHScale));
            if(!data->hardTop){
                QPainterPath piePath;
                piePath.moveTo(points[2]);
                piePath.arcTo(pie2,0,-180);
                piePath.closeSubpath();
                totalPath.addPath(piePath);
            }
            painter->drawPath(totalPath.simplified());
        }
        else{
            painter->drawPolygon(points,4);
        }
        painter->rotate(-data->imgAngle);
        painter->translate(-p.x(),-p.y());

    }
    else if(data->stickType == StickData::LINE){
        painter->drawLine(data->myLine.x1(),data->myLine.y1(),data->myLine.x2(),data->myLine.y2());
    }

    stickHandle[0]->setRect(data->myLine.p1().x()-HANDLESIZE/2,data->myLine.p1().y()-HANDLESIZE/2,HANDLESIZE,HANDLESIZE);
    stickHandle[1]->setRect(data->myLine.p2().x()-HANDLESIZE/2,data->myLine.p2().y()-HANDLESIZE/2,HANDLESIZE,HANDLESIZE);
    if(this->selected){
        QColor c(Qt::yellow);
        c.setAlphaF(0.75);
        painter->setPen(QPen(c,8));

        painter->drawEllipse(data->myLine.p2().x()-HANDLESIZE*0.4,data->myLine.p2().y()-HANDLESIZE*0.4,HANDLESIZE*0.8,HANDLESIZE*0.8);
        painter->drawEllipse(data->myLine.p1().x()-HANDLESIZE*0.4,data->myLine.p1().y()-HANDLESIZE*0.4,HANDLESIZE*0.8,HANDLESIZE*0.8);
        painter->drawLine(data->myLine.x1(),data->myLine.y1(),data->myLine.x2(),data->myLine.y2());

    }
}
// bounding rectangle deve essere sempre "positivo"
QRectF StickGraphicsItem::boundingRect() const
{
    return this->br;
}

QRectF calcImgRect(QLineF l,QSizeF s){
    float f = l.length()/s.width();
    QRectF out = QRectF(QPointF//                                Upper left corner of rect =
                   (l.p1().x()-abs(l.length())*0.5, //           line center X shifted by minus half line length
                    l.p1().y()-abs(s.height()*f)*0.5), //        line center Y shifted by minus half line length (pushing top left)
                   QPointF(l.p1().x()+abs(l.length())*0.5, //    bottom right corner =
                        l.p1().y()+abs(s.height()*f)*0.5));//    line center shifted by adding half line length (pushing bottom right)
    return out;

}

QImage StickGraphicsItem::calcImg(){
    QSizeF offsetted;
    if(data->stickImg->width()>data->stickImg->height())
    {
        offsetted = QSizeF(data->myLine.length()*data->imgWScale, data->stickImg->height()*data->myLine.length()/data->stickImg->width()*data->imgHScale);
    }
    else
    {
        offsetted = QSizeF(data->myLine.length()*data->stickImg->width()*data->imgWScale/data->stickImg->height(), data->myLine.length()*data->imgHScale);
    }

    return  data->stickImg->scaled(offsetted.width(),offsetted.height());
}

/**
 * @brief StickGraphicsItem::constructBr builds the boudning rect (without taking into account scaling factors)
 * @return
 */
QRectF StickGraphicsItem::constructBr()
{

    if((data->stickType == StickData::IMAGE || data->stickType == StickData::CIRCLE||data->stickType ==StickData::RECT || data->stickType == StickData::TRAPEZOID || data->stickType == StickData::TAPER ) && data->myLine.length()>0)
    {
        float d = 0;

        if(data->stickType == StickData::IMAGE ){
            QImage scaled = calcImg();
            d = sqrt(pow(scaled.width(),2)+pow(scaled.height(),2));
        }
        else if(data->stickType !=StickData::CIRCLE){
            d = sqrt(pow(data->imgWScale,2)+pow(data->myLine.length()+2*data->imgHScale,2));
        }
        else{
            d = sqrt(pow(2*data->myLine.length()*data->imgWScale,2)+pow(data->myLine.length()*data->imgHScale,2));
        }
        return  QRectF(QPointF                                                      // Upper left corner of rect =
                       (data->myLine.p1().x()-d, //           line center X shifted by minus half line length
                        data->myLine.p1().y()-d), //          line center Y shifted by minus half line length (pushing top left)
                       QPointF(data->myLine.p1().x()+d, // bottom right corner =
                               data->myLine.p1().y()+d));
    }else
    {
        // il bounding rect sarà il bounding rect del CERCHIO di cui la linea dello stick è il RAGGIO! Questo per evitare che
        // l'immagine sia tagliata (in qualsiasi direzione)
        return  QRectF(QPointF                                                      // Upper left corner of rect =
                       (0.5*(data->myLine.p1().x()+data->myLine.p2().x())-data->myLine.length()*0.5, //  line center X shifted by minus half line length
                        0.5*(data->myLine.p1().y()+data->myLine.p2().y())-data->myLine.length()*0.5), // line center Y shifted by minus half line length (pushing top left)
                       QPointF(0.5*(data->myLine.p1().x()+data->myLine.p2().x())+data->myLine.length()*0.5, // bottom right corner =
                               0.5*(data->myLine.p1().y()+data->myLine.p2().y())+data->myLine.length()*0.5));     // line center shifted by adding half line length (pushing bottom right)
    }
}
//funzione per aggiornare il rettangolo di refresh del disegno della scena per l'item grafico
QRectF StickGraphicsItem::updateBr(int mode)
{
    qreal oldP1x;
    qreal oldP1y;
    qreal oldP2x;
    qreal oldP2y;
    qreal newP1x;
    qreal newP1y;
    qreal newP2x;
    qreal newP2y;
    qreal fP1x;
    qreal fP1y;
    qreal fP2x;
    qreal fP2y;
    QRectF newBr = constructBr();

    // 1 Assicurati che a seconda della direzione della linea, il rettangolo abbia come P1 il punto più in alto a sx
    //      e come P2 il punto più in basso a destra
    newBr.setX(newBr.x()-data->Pen.width()-HANDLESIZE);newBr.setY(newBr.y()-data->Pen.width()-HANDLESIZE); // TODO padding proporzionale alla dimensione della Penna
    newBr.setWidth(newBr.width()+data->Pen.width()+HANDLESIZE);
    newBr.setHeight(newBr.height()+data->Pen.width()+HANDLESIZE);
    this->br.getCoords(&oldP1x,&oldP1y,&oldP2x,&oldP2y);
    // Se stiamo muovendo l'oggetto, teniamo conto anche del precedente bounding rect, e li fondiamo
    // per creare un bounding rect che contenga sia quello nuovo che quello vecchio così da non generare ghosting
    if(mode == 1)
    {
        newBr.getCoords(&newP1x,&newP1y,&newP2x,&newP2y);
        if(oldP1x<=newP1x)
        {
            fP1x = oldP1x;
        }
        else
        {
            fP1x = newP1x;
        }

        if(oldP1y<=newP1y)
        {
            fP1y = oldP1y;
        }
        else
        {
            fP1y = newP1y;
        }

        if(oldP2x>=newP2x)
        {
            fP2x = oldP2x;
        }
        else
        {
            fP2x = newP2x;
        }

        if(oldP2y>=newP2y)
        {
            fP2y = oldP2y;
        }
        else
        {
            fP2y = newP2y;
        }

        newBr = QRectF(QPointF(fP1x,fP1y),QPointF(fP2x,fP2y));
    }

    return newBr;

}
//lo stick misura l'angolo effettivo dell'oggetto linea rispetto al piano di disegno
void StickGraphicsItem::calcAngle()
{
    angleBuffer = data->myLine.angle();
}

//aggiorna la linea dello stick
void StickGraphicsItem::setLine(QLineF* line)
{
    data->myLine = *line;
}
void StickGraphicsItem::refresh(int mode)
{
    prepareGeometryChange();    // necessario chiamare questa funzione per non lasciare trails ed evitare il ghosting durante la manipolazione e il disegno!
    br = updateBr(mode);
    calcAngle();
    update();
}
//comando per cambiare la rotazione di uno stick
void StickGraphicsItem::rotate(QPointF *point)
{

    //step 1 calcola l'angolo con alpha = atan2(dx/dy)
    int dx = static_cast<int>(data->myLine.p1().x()-point->x());
    int dy = static_cast<int>(-data->myLine.p1().y()+point->y());
    float DX, DY = 0;
    float angle         = atan2(dx,dy)*180/M_PI;
    bool traslationOnly = false;
    //agisci sull'oggetto linea
    QPointF pBuf        = getP2(&data->myLine);
    //setta il pivot dello stick
    //progettiamo lóffset degli stick figlio
    QPointF oldEndPos   = data->myLine.p2();
    // lo stick punta al cursore che lo trascina
    //calcola se stiamo lavorando con l'origine del master stick
    if(this->isMaster() &&
            QLineF(*point,data->myLine.p1()).length()<QLineF(*point,data->myLine.p2()).length()) // se la selezione è più prossima all'orgine che al secondo punto
    {
        oldEndPos   = data->myLine.p1();
        DX = oldEndPos.x()-point->x(); //opera una traslazione
        DY = oldEndPos.y()-point->y();
        data->myLine.translate(-DX,-DY);
        traslationOnly = true;
    }
    else
    {
        data->myLine.setAngle(-angle-90);
    }
    refresh(1);
    for(int i= 0; i<data->children.length();i++)
    {
        StickData* child = data->children[i];
        if(traslationOnly){ // inc aso di sola traslazione dello stick non ricalcolare DX e DY
            child->myLine.translate(-DX,-DY);
        }
        // se non è uno stepchild oppure lo è ma stai ruotando uno stepchild a sua volta
        else if( (child->stepchild == false) || (data->stepchild == true && child->stepchild == true)) // dall'altra estremità del master non spostare gli stick quando il master è ruotato
        {
            //effettua la traslazione dovuta alla rotazione dell'estremo libero
            DX = oldEndPos.x()-data->myLine.p2().x();
            DY = oldEndPos.y()-data->myLine.p2().y();
            child->myLine.translate(-DX,-DY);
        }
        if(child->view)
            child->view->refresh(1);
    }
    if((data->stickType == StickData::IMAGE ||data->stickType == StickData::CIRCLE|| data->stickType == StickData::RECT || data->stickType == StickData::TAPER || data->stickType == StickData::TRAPEZOID) && !traslationOnly){
        data->imgAngle = angle;
    }

}
// funzione usata per fare le rotazioni dei singoli stick in modalità stickfigure rotation
// angle è il delta di quanto lo vogliamo ruotare
void StickGraphicsItem::rotate(float angle){
    // l'angolo viene settato, quindi in realtà andiamo ad aggiungere un delta
    angle = angleBuffer2+angle;
    QPointF oldEndPos = data->myLine.p2();
    data->myLine.setAngle(angle);
    float DX = 0;
    float DY = 0;
    DX = oldEndPos.x()-data->myLine.p2().x(); //traslazione dei nodi figli
    DY = oldEndPos.y()-data->myLine.p2().y();
    refresh(1);
    for(int i= 0; i<data->children.length();i++)
    {
        StickData* child = data->children[i];
        // se non è uno stepchild oppure lo è ma stai ruotando uno stepchild a sua volta
        if( (child->stepchild == false) || (data->stepchild == true && child->stepchild == true)) // dall'altra estremità del master non spostare gli stick quando il master è ruotato
        {
            //effettua la traslazione dovuta alla rotazione dell'estremo libero
            child->myLine.translate(-DX,-DY);
        }
        if(child->view)
            child->view->refresh(1);
    }
    if(data->stickType == StickData::IMAGE ||data->stickType == StickData::CIRCLE|| data->stickType == StickData::TRAPEZOID || data->stickType == StickData::TAPER || data->stickType == StickData::RECT){
        data->imgAngle = -angle-90;
    }
}
// this freely elongates sticks
void StickGraphicsItem::manipulate(QPointF *point)
{
    // set the new point
    data->myLine.setP2(*point);
    for(int i= 0; i<data->children.length();i++)
    {
        StickData* child = data->children[i];
        //effettua la traslazione dovuta alla manipolazione dell'estremo libero
        if(!(child->stepchild && this->isMaster()) && child->parent == data){
            QPointF pfBuf = child->myLine.p2();
            child->myLine = QLineF(*point,pfBuf);
            if(child->stickType == StickData::IMAGE || child->stickType == StickData::RECT
                    || child->stickType == StickData::TRAPEZOID || child->stickType == StickData::TAPER){
                child->imgAngle = -atan2(child->myLine.dx(),child->myLine.dy())*180/M_PI;
            }
            if(child->view)
                child->view->refresh(0);
        }

    }
    // se stiamo con un immagine ruotala anche
    if(data->stickType == StickData::IMAGE || data->stickType == StickData::RECT
            || data->stickType == StickData::TRAPEZOID || data->stickType == StickData::TAPER|| data->stickType == StickData::CIRCLE){
        data->imgAngle= -atan2(data->myLine.dx(),data->myLine.dy())*180/M_PI;
    }
    refresh(0);
}

bool StickGraphicsItem::isMaster() const
{
    if(this->myStickFigure != nullptr)
        return this->myStickFigure->masterStick == data;
    return false;
}
// è stato rilasciato il mouse termina la rotazione
void StickGraphicsItem::endRotation()
{
    // refresha con l'opzione di ridimensionare i box
    refresh(0);
    // anche per tutti i children
    for(int i= 0; i<data->children.length();i++)
    {
        if(data->children[i]->view)
            data->children[i]->view->refresh(0);
    }
}
QPointF StickGraphicsItem::getP2(QLineF* line)
{
    float x = line->p1().x()+line->length()*cos(line->angle());
    float y = line->p1().y()+line->length()*sin(line->angle());
    return QPointF(x,y);
}
void StickGraphicsItem::scale(float scaleFactor){
    QPointF oldEndPos = data->myLine.p2();
    float trueScale = scaleBuffer*scaleFactor;
    float DX = 0;
    float DY = 0;
    data->myLine.setLength(trueScale);
    DX = oldEndPos.x()-data->myLine.p2().x(); //traslazione dei nodi figli
    DY = oldEndPos.y()-data->myLine.p2().y();
    refresh(1);
    float previousFactor = data->Pen.widthF()/widthBuffer;
    data->Pen.setWidthF(widthBuffer*scaleFactor);
    if(data->stickType == StickData::RECT){
        data->imgWScale = (data->imgWScale/previousFactor)*scaleFactor;
    }
    if(data->stickType == StickData::TRAPEZOID || data->stickType == StickData::TAPER){
        data->imgWScale = (data->imgWScale/previousFactor)*scaleFactor;
        data->imgHScale = (data->imgHScale/previousFactor)*scaleFactor;
    }
    for(int i= 0; i<data->children.length();i++)
    {
        StickData* child = data->children[i];
        // se non è uno stepchild oppure lo è ma stai ruotando uno stepchild a sua volta
        if( (child->stepchild == false) || (data->stepchild == true && child->stepchild == true)) // dall'altra estremità del master non spostare gli stick quando il master è ruotato
        {
            //effettua la traslazione dovuta alla rotazione dell'estremo libero
            child->myLine.translate(-DX,-DY);
            if(child->view)
                child->view->refresh(1);
        }
    }
}
void StickGraphicsItem::addImage(QImage* imgToAdd, QString name){
    data->stickImgList.append(imgToAdd);
    data->stickImg = imgToAdd;
    QListWidgetItem * addedItem = new QListWidgetItem();
    QVariant newData(QVariant::fromValue(imgToAdd));
    addedItem->setData(Qt::UserRole,newData);
    addedItem->setData(Qt::DisplayRole,name);
    addedItem->setIcon(QIcon(QPixmap::fromImage(*imgToAdd)));
    data->imgNameList.append(name);
    imgListWidget->addItem(addedItem);
    populateImageListWidget();
}
void StickGraphicsItem::removeImgFromList(int idx){
    if(data->stickType == StickData::IMAGE){
        if(data->stickImgList.length()>1){
            QImage* toRemove = data->stickImgList[idx];
            data->stickImgList.removeAt(idx);
            data->imgNameList.removeAt(idx);
            QListWidgetItem * itemToRemove = imgListWidget->takeItem(idx);
            if(toRemove== data->stickImg)
                data->stickImg = data->stickImgList[0];
            delete itemToRemove;
            delete toRemove;

            populateImageListWidget();
        }
    }


}
void StickGraphicsItem::populateImageListWidget(){

    if(data->stickType == StickData::IMAGE){
        imgListWidget->clear();
        int j = 0;
        for(QImage* img:data->stickImgList)
        {
            QListWidgetItem * addedItem = new QListWidgetItem();
            QVariant newData(QVariant::fromValue(img));
            addedItem->setData(Qt::UserRole,newData);
            addedItem->setData(Qt::DisplayRole,data->imgNameList[j]);
            addedItem->setIcon(QIcon(QPixmap::fromImage(*img)));
            imgListWidget->addItem(addedItem);
            j++;
        }
    }
}
void sceneRemover(QGraphicsScene *sceneToClear){
    QList<QGraphicsItem*> thingsOnScene = sceneToClear->items( Qt::AscendingOrder);
    for(QGraphicsItem* i: thingsOnScene){
        if(i != myRect)
            sceneToClear->removeItem(i);
    }
}

QPolygonF getRegularPolygon(int n, QPointF center, float radius){
    QVector<QPointF> vertex;
    for(int i = 0;i<n;i++){
        float xoffset = cosf(M_PI/n*i)*radius;
        float yoffset = sinf(M_PI/n*i)*radius;
        vertex.append(QPointF(center+QPointF(xoffset,yoffset)));
    }
    return QPolygonF(vertex);
}
