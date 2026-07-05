#include "stick.h"
#include <QGraphicsItem>
#include <QVector>
#include <QListWidget>
#include <StickFigure.h>
bool onionRender = false;
extern QGraphicsRectItem* myRect;
extern QPixmap * imageDrawBuffer ;
extern QListWidget* imgListWidget;
extern bool libFlag;
extern bool undoFlag;
extern bool copyFlag;
extern bool clearUndoFlag;
extern bool iconUpdateFlag;
extern bool renderFlag;
extern bool playBack;
extern int HANDLESIZE;
//lo stick si basa fondamentalmente su un QGraphicsLineObject, uno stick alloca una lineobject
stick::stick(QLineF *line)
{
    this->setFlag(QGraphicsItem::ItemIsSelectable,true);
    Pen = QPen();
    Brush = QBrush();
    Brush.setStyle(Qt::SolidPattern);
    highlight = true;
    setLine(line);
    stepchild = false;
    stickHandle[0] = new QGraphicsEllipseItem();
    stickHandle[1] = new QGraphicsEllipseItem();
    stickHandle[0]->setZValue(100);stickHandle[1]->setZValue(100);
    stickHandle[0]->setBrush(QBrush(Qt::red));
    stickHandle[1]->setBrush(QBrush(Qt::red));

}
stick::stick()
{
    this->setFlag(QGraphicsItem::ItemIsSelectable,true);
    stickHandle[0] = new QGraphicsEllipseItem();
    stickHandle[1] = new QGraphicsEllipseItem();
    stickHandle[0]->setZValue(100);stickHandle[1]->setZValue(100);
    stickHandle[0]->setBrush(QBrush(Qt::red));
    stickHandle[1]->setBrush(QBrush(Qt::red));
    //Pen = QPen();
}
stick::stick(stick* S)
{
    this->setFlag(QGraphicsItem::ItemIsSelectable,true);
    Pen = S->Pen;
    Brush = S->Brush;
    highlight = true;
    setLine(&S->myLine);
    stepchild = false;
    this->Z = S->Z;
    this->setZValue(S->Z);
    this->br = S->boundingRect();
    this->stickType = S->stickType;
    //this->master = S->master;//todo master replacement
    this->order = S->order;
    hardTop = S->hardTop;
    hardBottom = S->hardBottom;
    if(S->stickType == IMAGE|| S->stickType == RECT || S->stickType == TRAPEZOID|| S->stickType== CIRCLE || S->stickType== TAPER){
        if(S->stickType == IMAGE){
            for(QImage*i:S->stickImgList){
                stickImgList.append(new QImage(*i));
                if(i == S->stickImg)
                {
                    stickImg = stickImgList.last();
                }
            }
            imgNameList = S->imgNameList;
        }
        imgAngle = S->imgAngle;
        imgWScale = S->imgWScale;
        imgHScale = S->imgHScale;
        imgAngleOffset = S->imgAngleOffset;
        imgOffset = S->imgOffset;

    }
    stickHandle[0] = new QGraphicsEllipseItem();
    stickHandle[1] = new QGraphicsEllipseItem();
    stickHandle[0]->setZValue(100);stickHandle[1]->setZValue(100);
    stickHandle[0]->setBrush(QBrush(Qt::red));
    stickHandle[1]->setBrush(QBrush(Qt::red));

}
stick::~stick(){
    if(stickType == IMAGE){
        int j = 0;
        for(QImage*i:stickImgList)
        {
            delete i;
            // mancavano copyFlag/clearUndoFlag: senza di essi, cancellare uno stick scartato da un
            // buffer di copia o da clearUndo() (mai davvero mostrato) toccava comunque imgListWidget,
            // che riflette invece lo stick IMAGE correntemente selezionato sullo schermo
            if(!onionRender && !undoFlag && !libFlag && !copyFlag && !clearUndoFlag && !iconUpdateFlag){
                QListWidgetItem * item= imgListWidget->takeItem(j);
                delete  item;
            }
            j++;
        }
        if(!onionRender && !undoFlag && !libFlag && !copyFlag && !clearUndoFlag && !iconUpdateFlag)
        {
            stickImgList.clear();
        }
        stickImg = nullptr;

    }
    delete stickHandle[0] ;
    delete stickHandle[1] ;
}
void stick::paint(QPainter *painter, const QStyleOptionGraphicsItem *option, QWidget *widget)
{
    //Pen.setColor(Qt::black);
    QPointF p(myLine.p1().x(),myLine.p1().y());
    this->setZValue(Z);
    if(!onionRender)
        painter->setPen(Pen);
    else{
        QPen onionSkinPen(Pen);
        onionSkinPen.setColor(Qt::gray);
        painter->setPen(onionSkinPen);
    }
    /*
    if(stickType == StickType::CIRCLE){
        painter->translate(p);
        painter->rotate(imgAngle);
        painter->setBrush(Brush);
        painter->drawEllipse()
        painter->drawEllipse(QPointF
                             (0.5*(myLine.dx()),
                              0.5*(myLine.dy())), // Ellipse center = line center
                             myLine.length()*0.5, myLine.length()*0.5*imgHScale); //RX e RY


        painter->rotate(-imgAngle);
        painter->translate(-p.x(),-p.y());
    }
    */
    if(stickType == StickType::IMAGE){
        //QRectF imgRect = calcImgRect(myLine, stickImg->size());
        // ogni immagine la facciamo partire dal punto 0,0
        // la ruotiamo
        // la trasliamo al punto di p1
        // il punto p1 deve essere sempre il topleft corner
        QTransform t;
        QSizeF off;
        QPointF p;
        //vedi quale fra lunghezza o larghezza dell'immaggine èd la più alta per calcolare lóffset per centrare l'immagine su p1
        QImage scaled = calcImg();
        // in caso di onion rendering converti l'immagine in grayscale
        if(onionRender){
            scaled.convertToFormat(QImage::Format_Grayscale8);
        }
        p = QPointF(myLine.p1().x(),myLine.p1().y());
        QPointF p2 = QPointF(-scaled.width()/2,-scaled.height()/2);
        painter->translate(p);
        painter->rotate(imgAngle+imgAngleOffset);
        QPointF p3 = QPointF(imgOffset);
        painter->translate(p3);
        QPixmap pixmap;
        pixmap.convertFromImage(scaled);
        // se siamo in onionskin

        painter->drawPixmap(p2,
                            pixmap);
        painter->translate(-p3.x(),-p3.y());
        painter->rotate(-imgAngle-imgAngleOffset);
        painter->translate(-p.x(),-p.y());

    }
    else if(stickType == StickType::RECT || stickType == StickType::CIRCLE){



        painter->setBrush(Brush);
        painter->translate(p);
        painter->rotate(imgAngle);
         if(stickType == StickType::RECT){
             QPointF c(-imgWScale/2,0);
            painter->drawRect(c.x(),c.y(),imgWScale,myLine.length());
         }
         else{
             QPointF c(-imgWScale*myLine.length()/2,0);
             painter->drawEllipse(c.x(),c.y(),myLine.length()*imgWScale,myLine.length());
         }
        painter->rotate(-imgAngle);
        painter->translate(-p.x(),-p.y());

    }
    else if(stickType == StickType::TRAPEZOID || stickType == StickType::TAPER){
        QPointF points[] = { QPointF(-imgWScale/2,0),QPointF(imgWScale/2,0),QPointF(imgHScale/2,myLine.length()), QPointF(-imgHScale/2,myLine.length())};


        QVector <QPointF>pVect = {points[0],points[1],points[2],points[3]};
        QPolygonF poly(pVect);
        painter->setBrush(Brush);
        painter->translate(p);
        painter->rotate(imgAngle);

        if(stickType == TAPER){

            QPainterPath totalPath;
            totalPath.setFillRule(Qt::WindingFill);
            QRectF pie1(QPointF(-imgWScale/2,-imgWScale/2),QSize(imgWScale,imgWScale));
            if(!hardBottom){
                QPainterPath piePath;
                piePath.moveTo(points[0]);
                piePath.arcTo(pie1,0,180);
                piePath.closeSubpath();
                totalPath.addPath(piePath);
                //totalPath.addEllipse(pie1);
            }
            totalPath.addPolygon(poly);
            QRectF pie2(QPointF(-imgHScale/2,myLine.length()-imgHScale/2),QSize(imgHScale,imgHScale));
            if(!hardTop){
                QPainterPath piePath;
                piePath.moveTo(points[2]);
                piePath.arcTo(pie2,0,-180);
                piePath.closeSubpath();
                totalPath.addPath(piePath);
                //totalPath.addEllipse(pie2);

            }
            painter->drawPath(totalPath.simplified());
            //painter->drawPolygon(pVect);
            //painter->drawPie(pie2,0,-180*16);
            //painter->drawEllipse(pie2);
        }
        else{
            painter->drawPolygon(points,4);
        }
        painter->rotate(-imgAngle);
        painter->translate(-p.x(),-p.y());

    }
    else if(stickType == StickType::LINE){
        painter->drawLine(myLine.x1(),myLine.y1(),myLine.x2(),myLine.y2());
    }

    stickHandle[0]->setRect(myLine.p1().x()-HANDLESIZE/2,myLine.p1().y()-HANDLESIZE/2,HANDLESIZE,HANDLESIZE);
    stickHandle[1]->setRect(myLine.p2().x()-HANDLESIZE/2,myLine.p2().y()-HANDLESIZE/2,HANDLESIZE,HANDLESIZE);
    if(this->selected){
        QColor c(Qt::yellow);
        c.setAlphaF(0.75);
        painter->setPen(QPen(c,8));

        painter->drawEllipse(myLine.p2().x()-HANDLESIZE*0.4,myLine.p2().y()-HANDLESIZE*0.4,HANDLESIZE*0.8,HANDLESIZE*0.8);
        painter->drawEllipse(myLine.p1().x()-HANDLESIZE*0.4,myLine.p1().y()-HANDLESIZE*0.4,HANDLESIZE*0.8,HANDLESIZE*0.8);
        painter->drawLine(myLine.x1(),myLine.y1(),myLine.x2(),myLine.y2());

    }
    //Pen.setColor(Qt::red); //draw bounding box
    /*
    if(!onionRender && !renderFlag &&!playBack){
        if(this->master){ // il master ha entrambe le estremità disegnate
            if(this->highlight)
                painter->setPen(QPen(Qt::green,HANDLESIZE));
            else
                painter->setPen(QPen(Qt::darkGreen,HANDLESIZE));
            painter->drawEllipse(myLine.p1().x()-HANDLESIZE/2,myLine.p1().y()-HANDLESIZE/2,HANDLESIZE,HANDLESIZE);
        }
        if(this->parent != nullptr){
            if(this->highlight){
                painter->setPen(QPen(Qt::red,HANDLESIZE));
                if(this->stepchild && this->parent->master)
                    painter->setPen(QPen(Qt::green,HANDLESIZE));
            }
            else{
                painter->setPen(QPen(Qt::blue,HANDLESIZE));
                if(this->stepchild && this->parent->master)
                    painter->setPen(QPen(Qt::darkGreen,HANDLESIZE));
            }
            //if(this->parent->master == false)
                painter->drawEllipse(myLine.p1().x()-HANDLESIZE/2,myLine.p1().y()-HANDLESIZE/2,HANDLESIZE,HANDLESIZE);
        }
        if(this->highlight)
            painter->setPen(QPen(Qt::red,HANDLESIZE));
        else
            painter->setPen(QPen(Qt::blue,HANDLESIZE));
        painter->drawEllipse(myLine.p2().x()-HANDLESIZE/2,myLine.p2().y()-HANDLESIZE/2,HANDLESIZE,HANDLESIZE);

        //painter->drawRect(br);
    }*/
}
// bounding rectangle deve essere sempre "positivo"
QRectF stick::boundingRect() const
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

    /*
    //questa cerca adattivamente quale sia il punto più in alto a sx
    QRectF out =QRectF(QPointF                                                      // Upper left corner of rect =
                       (0.5*(l.p1().x()+l.p2().x())-abs(l.length())*0.5, //           line center X shifted by minus half line length
                        0.5*(l.p1().y()+l.p2().y())-abs(s.height()*f)*0.5), //
                       QPointF(0.5*(l.p1().x()+l.p2().x())+abs(l.length())*0.5, // bottom right corner =
                                           0.5*(l.p1().y()+l.p2().y())+abs(s.height()*f)*0.5));*/

    /*QRectF out =QRectF(0.5*(l.p1()+l.p2())-QPointF(),
                       QPointF(l.p1().x()+abs(l.dx()), // bottom right corner =
                                           l.p1().y()+(s.height()*f)));*/
    return out;

}

QImage stick::calcImg(){
    QSizeF offsetted;
    if(stickImg->width()>stickImg->height())
    {
        offsetted = QSizeF(myLine.length()*imgWScale, stickImg->height()*myLine.length()/stickImg->width()*imgHScale);
    }
    else
    {
        offsetted = QSizeF(myLine.length()*stickImg->width()*imgWScale/stickImg->height(), myLine.length()*imgHScale);
    }

    return  stickImg->scaled(offsetted.width(),offsetted.height());
}

/**
 * @brief stick::constructBr builds the boudning rect (without taking into account scaling factors)
 * @return
 */
QRectF stick::constructBr()
{

    if((stickType == IMAGE || stickType == CIRCLE||stickType ==RECT || stickType == TRAPEZOID || stickType == TAPER ) && myLine.length()>0)
    {
        float d = 0;

        if(stickType == IMAGE ){
            QImage scaled = calcImg();
            d = sqrt(pow(scaled.width(),2)+pow(scaled.height(),2));
        }
        else if(stickType !=CIRCLE){
            d = sqrt(pow(imgWScale,2)+pow(myLine.length()+2*imgHScale,2));
        }
        else{
            d = sqrt(pow(2*myLine.length()*imgWScale,2)+pow(myLine.length()*imgHScale,2));
        }
        return  QRectF(QPointF                                                      // Upper left corner of rect =
                       (myLine.p1().x()-d, //           line center X shifted by minus half line length
                        myLine.p1().y()-d), //          line center Y shifted by minus half line length (pushing top left)
                       QPointF(myLine.p1().x()+d, // bottom right corner =
                               myLine.p1().y()+d));
    }else
    {
        // il bounding rect sarà il bounding rect del CERCHIO di cui la linea dello stick è il RAGGIO! Questo per evitare che
        // l'immagine sia tagliata (in qualsiasi direzione)
        return  QRectF(QPointF                                                      // Upper left corner of rect =
                       (0.5*(myLine.p1().x()+myLine.p2().x())-myLine.length()*0.5, //  line center X shifted by minus half line length
                        0.5*(myLine.p1().y()+myLine.p2().y())-myLine.length()*0.5), // line center Y shifted by minus half line length (pushing top left)
                       QPointF(0.5*(myLine.p1().x()+myLine.p2().x())+myLine.length()*0.5, // bottom right corner =
                               0.5*(myLine.p1().y()+myLine.p2().y())+myLine.length()*0.5));     // line center shifted by adding half line length (pushing bottom right)
    }
}
//funzione per aggiornare il rettangolo di refresh del disegno della scena per l'item grafico
QRectF stick::updateBr(int mode)
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
    newBr.setX(newBr.x()-Pen.width()-HANDLESIZE);newBr.setY(newBr.y()-Pen.width()-HANDLESIZE); // TODO padding proporzionale alla dimensione della Penna
    newBr.setWidth(newBr.width()+Pen.width()+HANDLESIZE);
    newBr.setHeight(newBr.height()+Pen.width()+HANDLESIZE);
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
void stick::calcAngle()
{
    angleBuffer = myLine.angle();//(180/M_PI)*atan2(lineobject.line().dx(),lineobject.line().dy());
}

//aggiorna la linea dello stick
void stick::setLine(QLineF* line)
{
    //lineobject.setLine(*line);
    myLine = *line;
}
void stick::refresh(int mode)
{
    prepareGeometryChange();    // necessario chiamare questa funzione per non lasciare trails ed evitare il ghosting durante la manipolazione e il disegno!
    br = updateBr(mode);
    calcAngle();
    update();
}
//comando per cambiare la rotazione di uno stick
void stick::rotate(QPointF *point)
{

    //step 1 calcola l'angolo con alpha = atan2(dx/dy)
    int dx = static_cast<int>(myLine.p1().x()-point->x());
    int dy = static_cast<int>(-myLine.p1().y()+point->y());
    float DX, DY = 0;
    float angle         = atan2(dx,dy)*180/M_PI;
    bool traslationOnly = false;
    //agisci sull'oggetto linea
    QPointF pBuf        = getP2(&myLine);
    //setta il pivot dello stick
    //progettiamo lóffset degli stick figlio
    QPointF oldEndPos   = myLine.p2();
    // lo stick punta al cursore che lo trascina
    //calcola se stiamo lavorando con l'origine del master stick
    if(this->isMaster() && // todo master replacement
            QLineF(*point,myLine.p1()).length()<QLineF(*point,myLine.p2()).length()) // se la selezione è più prossima all'orgine che al secondo punto
    {
        oldEndPos   = myLine.p1();
        DX = oldEndPos.x()-point->x(); //opera una traslazione
        DY = oldEndPos.y()-point->y();
        myLine.translate(-DX,-DY);
        traslationOnly = true;
    }
    else
    {
        myLine.setAngle(-angle-90/*(angle+angleBuffer)*/);
    }
    refresh(1);
    for(int i= 0; i<children.length();i++)
    {
        if(traslationOnly){ // inc aso di sola traslazione dello stick non ricalcolare DX e DY
            children[i]->myLine.translate(-DX,-DY);
        }
        // se non è uno stepchild oppure lo è ma stai ruotando uno stepchild a sua volta
        else if( (children[i]->stepchild == false) || (this->stepchild == true && children[i]->stepchild == true)) // dall'altra estremità del master non spostare gli stick quando il master è ruotato
        {
            //effettua la traslazione dovuta alla rotazione dell'estremo libero
            DX = oldEndPos.x()-myLine.p2().x();
            DY = oldEndPos.y()-myLine.p2().y();
            children[i]->myLine.translate(-DX,-DY);
            qDebug("DX %f, DY %f", DX,DY);
            qDebug("X2= %f, Y2= %f", pBuf.x(),pBuf.y());

        }
        children[i]->refresh(1);
    }
    if((stickType == IMAGE ||stickType == CIRCLE|| stickType == RECT || stickType == TAPER || stickType == TRAPEZOID) && !traslationOnly){
        imgAngle = angle;
        //*stickImg = stickImg->transformed(QTransform().rotate(90-angle));
    }

}
// funzione usata per fare le rotazioni dei singoli stick in modalità stickfigure rotation
// angle è il delta di quanto lo vogliamo ruotare
void stick::rotate(float angle){
    // l'angolo viene settato, quindi in realtà andiamo ad aggiungere un delta
    angle = angleBuffer2+angle;
    qDebug(" anglebuf = %f \r\n angle = %f\r\n",angleBuffer2,angle);
    //angleBuffer = angle;
    QPointF oldEndPos = myLine.p2();
    myLine.setAngle(angle);
    float DX = 0;
    float DY = 0;
    DX = oldEndPos.x()-myLine.p2().x(); //traslazione dei nodi figli
    DY = oldEndPos.y()-myLine.p2().y();
    refresh(1);
    for(int i= 0; i<children.length();i++)
    {
        // se non è uno stepchild oppure lo è ma stai ruotando uno stepchild a sua volta
        if( (children[i]->stepchild == false) || (this->stepchild == true && children[i]->stepchild == true)) // dall'altra estremità del master non spostare gli stick quando il master è ruotato
        {
            //effettua la traslazione dovuta alla rotazione dell'estremo libero
            children[i]->myLine.translate(-DX,-DY);
        }
        children[i]->refresh(1);
    }
    if(stickType == IMAGE ||stickType == CIRCLE|| stickType == TRAPEZOID || stickType == TAPER || stickType == RECT){
        if(stickType == IMAGE)
            imgAngle = -angle-90;
        else
            imgAngle = -angle-90;
        //*stickImg = stickImg->transformed(QTransform().rotate(90-angle));
    }
}
// this freely elongates sticks
void stick::manipulate(QPointF *point)
{
    // set the new point
    myLine.setP2(*point);
    for(int i= 0; i<children.length();i++)
    {
        //effettua la traslazione dovuta alla manipolazione dell'estremo libero
        if(!(children[i]->stepchild && this->isMaster()) && children[i]->parent == this){ // todo master replacement
            QPointF pfBuf = children[i]->myLine.p2();
            children[i]->myLine = QLineF(*point,pfBuf);
            if(children[i]->stickType == stick::IMAGE || children[i]->stickType == RECT
                    || children[i]->stickType == TRAPEZOID || children[i]->stickType == TAPER){
                children[i]->imgAngle = -atan2(children[i]->myLine.dx(),children[i]->myLine.dy())*180/M_PI;
            }
            children[i]->refresh(0);
        }

    }
    // se stiamo con un immagine ruotala anche
    if(stickType == stick::IMAGE || stickType == RECT
            || stickType == TRAPEZOID || stickType == TAPER|| stickType == CIRCLE){
        imgAngle= -atan2(myLine.dx(),myLine.dy())*180/M_PI;
    }
    refresh(0);
}

bool stick::isMaster() const
{
    if(this->myStickFigure != nullptr)
        return this->myStickFigure->masterStick == this;
    return false;
}
// è stato rilasciato il mouse termina la rotazione
void stick::endRotation()
{
    // refresha con l'opzione di ridimensionare i box
    refresh(0);
    // anche per tutti i children
    for(int i= 0; i<children.length();i++)
    {
        children[i]->refresh(0);
    }
}
QPointF stick::getP2(QLineF* line)
{
    float x = line->p1().x()+line->length()*cos(line->angle());
    float y = line->p1().y()+line->length()*sin(line->angle());
    return QPointF(x,y);
}
void stick::scale(float scaleFactor){
    /*
    float trueScale = 1+scaleBuffer-scaleFactor;
    float oldLength = myLine.length();
    scaleBuffer = scaleFactor;
    */
    QPointF oldEndPos = myLine.p2();
    float oldLength = myLine.length();
    float trueScale = scaleBuffer*scaleFactor;
    float DX = 0;
    float DY = 0;
    myLine.setLength(trueScale);
    DX = oldEndPos.x()-myLine.p2().x(); //traslazione dei nodi figli
    DY = oldEndPos.y()-myLine.p2().y();
    refresh(1);
    float previousFactor = Pen.widthF()/widthBuffer;
    Pen.setWidthF(widthBuffer*scaleFactor);
    if(stickType == RECT){
        imgWScale = (imgWScale/previousFactor)*scaleFactor;
    }
    if(stickType == TRAPEZOID || stickType == TAPER){
        imgWScale = (imgWScale/previousFactor)*scaleFactor;
        imgHScale = (imgHScale/previousFactor)*scaleFactor;
    }
    for(int i= 0; i<children.length();i++)
    {
        // se non è uno stepchild oppure lo è ma stai ruotando uno stepchild a sua volta
        if( (children[i]->stepchild == false) || (this->stepchild == true && children[i]->stepchild == true)) // dall'altra estremità del master non spostare gli stick quando il master è ruotato
        {
            //effettua la traslazione dovuta alla rotazione dell'estremo libero
            children[i]->myLine.translate(-DX,-DY);
            children[i]->refresh(1);
        }
    }
    qDebug("scale = %f \r\n",trueScale);
}
void stick::addImage(QImage* imgToAdd, QString name){
    stickImgList.append(imgToAdd);
    stickImg = imgToAdd;
    QListWidgetItem * addedItem = new QListWidgetItem();
    QVariant newData(QVariant::fromValue(imgToAdd));
    addedItem->setData(Qt::UserRole,newData);
    addedItem->setData(Qt::DisplayRole,name);
    addedItem->setIcon(QIcon(QPixmap::fromImage(*imgToAdd)));
    imgNameList.append(name);
    imgListWidget->addItem(addedItem);
    populateImageListWidget();
}
void stick::removeImgFromList(int idx){
    if(stickType == stick::IMAGE){
        if(stickImgList.length()>1){
            QImage* toRemove = stickImgList[idx];
            stickImgList.removeAt(idx);
            imgNameList.removeAt(idx);
            QListWidgetItem * itemToRemove = imgListWidget->takeItem(idx);
            if(toRemove== stickImg)
                stickImg = stickImgList[0];
            delete itemToRemove;
            delete toRemove;

            populateImageListWidget();
        }
    }


}
void stick::populateImageListWidget(){

    if(stickType == stick::IMAGE){
        imgListWidget->clear();
        int j = 0;
        for(QImage* img:stickImgList)
        {
            QListWidgetItem * addedItem = new QListWidgetItem();
            QVariant newData(QVariant::fromValue(img));
            addedItem->setData(Qt::UserRole,newData);
            addedItem->setData(Qt::DisplayRole,imgNameList[j]);
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
