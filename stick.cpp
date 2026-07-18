#include "stick.h"
#include <QGraphicsItem>
#include <QVector>
#include <QListWidget>
#include <StickFigure.h>
bool StickGraphicsItem::onion_render = false;
extern QGraphicsRectItem* my_rect;
extern QListWidget* img_list_widget;
extern int handle_size;
// nota flag sweep: qui c'erano anche `extern QPixmap * image_draw_buffer` (mai usato in
// questo file, e per giunta dichiarato QPixmap* mentre la definizione reale è QImage* —
// una violazione ODR pronta a esplodere), `extern bool render_flag` e `extern bool
// play_back` (mai letti in questo file): rimossi.
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
    stick_handle[0] = new QGraphicsEllipseItem();
    stick_handle[1] = new QGraphicsEllipseItem();
    stick_handle[0]->setZValue(100);stick_handle[1]->setZValue(100);
    stick_handle[0]->setBrush(QBrush(Qt::red));
    stick_handle[1]->setBrush(QBrush(Qt::red));
    br = UpdateBr(0);
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
    delete stick_handle[0] ;
    delete stick_handle[1] ;
}
/**
 * @details Called right before a LIVE stick's view is detached (DeleteStick, Weld,
 * CancelDrawing, FramePresenter::DetachView()): empties the rows this stick's images
 * occupy in the image list widget, which mirrors the on-screen selected IMAGE stick.
 * Scratch deletions (undo/copy/library buffers never have a view at all) and render
 * clones (onion skins, icons, preview/export — see below) simply never call it.
 */
void StickGraphicsItem::RemoveImagesFromListWidget(){
    if(data->stick_type != StickData::IMAGE)
        return;
    // takeItem(0) una volta per immagine: le righe scalano ad ogni rimozione, quindi
    // prendere gli indici 0,1,2,... saltava una riga sì e una no (con N>=2 immagini metà
    // delle voci restava orfana nel widget); delete su un takeItem oltre l'ultima riga
    // è un delete nullptr, innocuo
    for(int j = 0; j < data->stick_img_list.count(); j++)
    {
        delete img_list_widget->takeItem(0);
    }
}
void StickGraphicsItem::paint(QPainter *painter, const QStyleOptionGraphicsItem *option, QWidget *widget)
{
    Q_UNUSED(option);
    Q_UNUSED(widget);
    QPointF p(data->my_line.p1().x(),data->my_line.p1().y());
    this->setZValue(data->z);
    if(!onion_render)
        painter->setPen(data->pen);
    else{
        QPen onionSkinPen(data->pen);
        onionSkinPen.setColor(Qt::gray);
        painter->setPen(onionSkinPen);
    }
    if(data->stick_type == StickData::IMAGE && data->stick_img){ // stick_img nullo solo da file corrotto: non c'è nulla da dipingere
        // ogni immagine la facciamo partire dal punto 0,0
        // la ruotiamo
        // la trasliamo al punto di p1
        // il punto p1 deve essere sempre il topleft corner
        QImage scaled = CalcImg();
        // in caso di onion rendering converti l'immagine in grayscale
        if(onion_render){
            // convertToFormat è const e RESTITUISCE l'immagine convertita: scartarne il
            // risultato lasciava gli onion skin delle immagini a colori invece che in grigio
            scaled = scaled.convertToFormat(QImage::Format_Grayscale8);
        }
        p = QPointF(data->my_line.p1().x(),data->my_line.p1().y());
        QPointF p2 = QPointF(-scaled.width()/2,-scaled.height()/2);
        painter->translate(p);
        painter->rotate(data->img_angle+data->img_angle_offset);
        QPointF p3 = QPointF(data->img_offset);
        painter->translate(p3);
        QPixmap pixmap;
        pixmap.convertFromImage(scaled);

        painter->drawPixmap(p2,
                            pixmap);
        painter->translate(-p3.x(),-p3.y());
        painter->rotate(-data->img_angle-data->img_angle_offset);
        painter->translate(-p.x(),-p.y());

    }
    else if(data->stick_type == StickData::RECT || data->stick_type == StickData::CIRCLE){

        painter->setBrush(data->brush);
        painter->translate(p);
        painter->rotate(data->img_angle);
         if(data->stick_type == StickData::RECT){
             QPointF c(-data->img_w_scale/2,0);
            painter->drawRect(c.x(),c.y(),data->img_w_scale,data->my_line.length());
         }
         else{
             QPointF c(-data->img_w_scale*data->my_line.length()/2,0);
             painter->drawEllipse(c.x(),c.y(),data->my_line.length()*data->img_w_scale,data->my_line.length());
         }
        painter->rotate(-data->img_angle);
        painter->translate(-p.x(),-p.y());

    }
    else if(data->stick_type == StickData::TRAPEZOID || data->stick_type == StickData::TAPER){
        QPointF points[] = { QPointF(-data->img_w_scale/2,0),QPointF(data->img_w_scale/2,0),QPointF(data->img_h_scale/2,data->my_line.length()), QPointF(-data->img_h_scale/2,data->my_line.length())};

        QVector <QPointF>pVect = {points[0],points[1],points[2],points[3]};
        QPolygonF poly(pVect);
        painter->setBrush(data->brush);
        painter->translate(p);
        painter->rotate(data->img_angle);

        if(data->stick_type == StickData::TAPER){

            QPainterPath totalPath;
            totalPath.setFillRule(Qt::WindingFill);
            QRectF pie1(QPointF(-data->img_w_scale/2,-data->img_w_scale/2),QSize(data->img_w_scale,data->img_w_scale));
            if(!data->hard_bottom){
                QPainterPath piePath;
                piePath.moveTo(points[0]);
                piePath.arcTo(pie1,0,180);
                piePath.closeSubpath();
                totalPath.addPath(piePath);
            }
            totalPath.addPolygon(poly);
            QRectF pie2(QPointF(-data->img_h_scale/2,data->my_line.length()-data->img_h_scale/2),QSize(data->img_h_scale,data->img_h_scale));
            if(!data->hard_top){
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
        painter->rotate(-data->img_angle);
        painter->translate(-p.x(),-p.y());

    }
    else if(data->stick_type == StickData::LINE){
        painter->drawLine(data->my_line.x1(),data->my_line.y1(),data->my_line.x2(),data->my_line.y2());
    }

    stick_handle[0]->setRect(data->my_line.p1().x()-handle_size/2,data->my_line.p1().y()-handle_size/2,handle_size,handle_size);
    stick_handle[1]->setRect(data->my_line.p2().x()-handle_size/2,data->my_line.p2().y()-handle_size/2,handle_size,handle_size);
    if(this->selected){
        QColor c(Qt::yellow);
        c.setAlphaF(0.75);
        painter->setPen(QPen(c,8));

        painter->drawEllipse(data->my_line.p2().x()-handle_size*0.4,data->my_line.p2().y()-handle_size*0.4,handle_size*0.8,handle_size*0.8);
        painter->drawEllipse(data->my_line.p1().x()-handle_size*0.4,data->my_line.p1().y()-handle_size*0.4,handle_size*0.8,handle_size*0.8);
        painter->drawLine(data->my_line.x1(),data->my_line.y1(),data->my_line.x2(),data->my_line.y2());

    }
}
// bounding rectangle deve essere sempre "positivo"
QRectF StickGraphicsItem::boundingRect() const
{
    return this->br;
}

QRectF CalcImgRect(QLineF l,QSizeF s){
    float f = l.length()/s.width();
    QRectF out = QRectF(QPointF//                                Upper left corner of rect =
                   (l.p1().x()-abs(l.length())*0.5, //           line center X shifted by minus half line length
                    l.p1().y()-abs(s.height()*f)*0.5), //        line center Y shifted by minus half line length (pushing top left)
                   QPointF(l.p1().x()+abs(l.length())*0.5, //    bottom right corner =
                        l.p1().y()+abs(s.height()*f)*0.5));//    line center shifted by adding half line length (pushing bottom right)
    return out;

}

QImage StickGraphicsItem::CalcImg(){
    QSizeF offsetted;
    if(!data->stick_img) // possibile solo per un record IMAGE ricostruito da un file corrotto
        return QImage();
    if(data->stick_img->width()>data->stick_img->height())
    {
        offsetted = QSizeF(data->my_line.length()*data->img_w_scale, data->stick_img->height()*data->my_line.length()/data->stick_img->width()*data->img_h_scale);
    }
    else
    {
        offsetted = QSizeF(data->my_line.length()*data->stick_img->width()*data->img_w_scale/data->stick_img->height(), data->my_line.length()*data->img_h_scale);
    }

    return  data->stick_img->scaled(offsetted.width(),offsetted.height());
}

/**
 * @brief StickGraphicsItem::ConstructBr builds the boudning rect (without taking into account scaling factors)
 * @return
 */
QRectF StickGraphicsItem::ConstructBr()
{

    if((data->stick_type == StickData::IMAGE || data->stick_type == StickData::CIRCLE||data->stick_type ==StickData::RECT || data->stick_type == StickData::TRAPEZOID || data->stick_type == StickData::TAPER ) && data->my_line.length()>0)
    {
        float d = 0;

        if(data->stick_type == StickData::IMAGE ){
            QImage scaled = CalcImg();
            d = sqrt(pow(scaled.width(),2)+pow(scaled.height(),2));
        }
        else if(data->stick_type !=StickData::CIRCLE){
            d = sqrt(pow(data->img_w_scale,2)+pow(data->my_line.length()+2*data->img_h_scale,2));
        }
        else{
            d = sqrt(pow(2*data->my_line.length()*data->img_w_scale,2)+pow(data->my_line.length()*data->img_h_scale,2));
        }
        return  QRectF(QPointF                                                      // Upper left corner of rect =
                       (data->my_line.p1().x()-d, //           line center X shifted by minus half line length
                        data->my_line.p1().y()-d), //          line center Y shifted by minus half line length (pushing top left)
                       QPointF(data->my_line.p1().x()+d, // bottom right corner =
                               data->my_line.p1().y()+d));
    }else
    {
        // il bounding rect sarà il bounding rect del CERCHIO di cui la linea dello stick è il RAGGIO! Questo per evitare che
        // l'immagine sia tagliata (in qualsiasi direzione)
        return  QRectF(QPointF                                                      // Upper left corner of rect =
                       (0.5*(data->my_line.p1().x()+data->my_line.p2().x())-data->my_line.length()*0.5, //  line center X shifted by minus half line length
                        0.5*(data->my_line.p1().y()+data->my_line.p2().y())-data->my_line.length()*0.5), // line center Y shifted by minus half line length (pushing top left)
                       QPointF(0.5*(data->my_line.p1().x()+data->my_line.p2().x())+data->my_line.length()*0.5, // bottom right corner =
                               0.5*(data->my_line.p1().y()+data->my_line.p2().y())+data->my_line.length()*0.5));     // line center shifted by adding half line length (pushing bottom right)
    }
}
//funzione per aggiornare il rettangolo di Refresh del disegno della scena per l'item grafico
QRectF StickGraphicsItem::UpdateBr(int mode)
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
    QRectF newBr = ConstructBr();

    // 1 Assicurati che a seconda della direzione della linea, il rettangolo abbia come P1 il punto più in alto a sx
    //      e come P2 il punto più in basso a destra
    newBr.setX(newBr.x()-data->pen.width()-handle_size);newBr.setY(newBr.y()-data->pen.width()-handle_size); // TODO padding proporzionale alla dimensione della Penna
    newBr.setWidth(newBr.width()+data->pen.width()+handle_size);
    newBr.setHeight(newBr.height()+data->pen.width()+handle_size);
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
void StickGraphicsItem::CalcAngle()
{
    angle_buffer = data->my_line.angle();
}

//aggiorna la linea dello stick
void StickGraphicsItem::SetLine(QLineF* line)
{
    data->my_line = *line;
}
void StickGraphicsItem::Refresh(int mode)
{
    prepareGeometryChange();    // necessario chiamare questa funzione per non lasciare trails ed evitare il ghosting durante la manipolazione e il disegno!
    br = UpdateBr(mode);
    CalcAngle();
    update();
}
//comando per cambiare la rotazione di uno stick
void StickGraphicsItem::Rotate(QPointF *point)
{

    //step 1 calcola l'angolo con alpha = atan2(dx/dy)
    int dx = static_cast<int>(data->my_line.p1().x()-point->x());
    int dy = static_cast<int>(-data->my_line.p1().y()+point->y());
    float DX, DY = 0;
    float angle         = atan2(dx,dy)*180/M_PI;
    bool traslationOnly = false;
    //setta il pivot dello stick
    //progettiamo lóffset degli stick figlio
    QPointF oldEndPos   = data->my_line.p2();
    // lo stick punta al cursore che lo trascina
    //calcola se stiamo lavorando con l'origine del master stick
    if(this->IsMaster() &&
            QLineF(*point,data->my_line.p1()).length()<QLineF(*point,data->my_line.p2()).length()) // se la selezione è più prossima all'orgine che al secondo punto
    {
        oldEndPos   = data->my_line.p1();
        DX = oldEndPos.x()-point->x(); //opera una traslazione
        DY = oldEndPos.y()-point->y();
        data->my_line.translate(-DX,-DY);
        traslationOnly = true;
    }
    else
    {
        data->my_line.setAngle(-angle-90);
    }
    Refresh(1);
    for(int i= 0; i<data->children.length();i++)
    {
        StickData* child = data->children[i];
        if(traslationOnly){ // inc aso di sola traslazione dello stick non ricalcolare DX e DY
            child->my_line.translate(-DX,-DY);
        }
        // se non è uno stepchild oppure lo è ma stai ruotando uno stepchild a sua volta
        else if( (child->stepchild == false) || (data->stepchild == true && child->stepchild == true)) // dall'altra estremità del master non spostare gli stick quando il master è ruotato
        {
            //effettua la traslazione dovuta alla rotazione dell'estremo libero
            DX = oldEndPos.x()-data->my_line.p2().x();
            DY = oldEndPos.y()-data->my_line.p2().y();
            child->my_line.translate(-DX,-DY);
        }
        if(child->view)
            child->view->Refresh(1);
    }
    if((data->stick_type == StickData::IMAGE ||data->stick_type == StickData::CIRCLE|| data->stick_type == StickData::RECT || data->stick_type == StickData::TAPER || data->stick_type == StickData::TRAPEZOID) && !traslationOnly){
        data->img_angle = angle;
    }

}
// funzione usata per fare le rotazioni dei singoli stick in modalità stickfigure rotation
// angle è il delta di quanto lo vogliamo ruotare
void StickGraphicsItem::Rotate(float angle){
    // l'angolo viene settato, quindi in realtà andiamo ad aggiungere un delta
    angle = angle_buffer2+angle;
    QPointF oldEndPos = data->my_line.p2();
    data->my_line.setAngle(angle);
    float DX = 0;
    float DY = 0;
    DX = oldEndPos.x()-data->my_line.p2().x(); //traslazione dei nodi figli
    DY = oldEndPos.y()-data->my_line.p2().y();
    Refresh(1);
    for(int i= 0; i<data->children.length();i++)
    {
        StickData* child = data->children[i];
        // se non è uno stepchild oppure lo è ma stai ruotando uno stepchild a sua volta
        if( (child->stepchild == false) || (data->stepchild == true && child->stepchild == true)) // dall'altra estremità del master non spostare gli stick quando il master è ruotato
        {
            //effettua la traslazione dovuta alla rotazione dell'estremo libero
            child->my_line.translate(-DX,-DY);
        }
        if(child->view)
            child->view->Refresh(1);
    }
    if(data->stick_type == StickData::IMAGE ||data->stick_type == StickData::CIRCLE|| data->stick_type == StickData::TRAPEZOID || data->stick_type == StickData::TAPER || data->stick_type == StickData::RECT){
        data->img_angle = -angle-90;
    }
}
// this freely elongates sticks
void StickGraphicsItem::Manipulate(QPointF *point)
{
    // set the new point
    data->my_line.setP2(*point);
    for(int i= 0; i<data->children.length();i++)
    {
        StickData* child = data->children[i];
        //effettua la traslazione dovuta alla manipolazione dell'estremo libero
        if(!(child->stepchild && this->IsMaster()) && child->parent == data){
            QPointF pfBuf = child->my_line.p2();
            child->my_line = QLineF(*point,pfBuf);
            if(child->stick_type == StickData::IMAGE || child->stick_type == StickData::RECT
                    || child->stick_type == StickData::TRAPEZOID || child->stick_type == StickData::TAPER){
                child->img_angle = -atan2(child->my_line.dx(),child->my_line.dy())*180/M_PI;
            }
            if(child->view)
                child->view->Refresh(0);
        }

    }
    // se stiamo con un immagine ruotala anche
    if(data->stick_type == StickData::IMAGE || data->stick_type == StickData::RECT
            || data->stick_type == StickData::TRAPEZOID || data->stick_type == StickData::TAPER|| data->stick_type == StickData::CIRCLE){
        data->img_angle= -atan2(data->my_line.dx(),data->my_line.dy())*180/M_PI;
    }
    Refresh(0);
}

bool StickGraphicsItem::IsMaster() const
{
    if(this->my_stick_figure != nullptr)
        return this->my_stick_figure->master_stick == data;
    return false;
}
// è stato rilasciato il mouse termina la rotazione
void StickGraphicsItem::EndRotation()
{
    // refresha con l'opzione di ridimensionare i box
    Refresh(0);
    // anche per tutti i children
    for(int i= 0; i<data->children.length();i++)
    {
        if(data->children[i]->view)
            data->children[i]->view->Refresh(0);
    }
}
void StickGraphicsItem::Scale(float scale_factor){
    QPointF oldEndPos = data->my_line.p2();
    float true_scale = scale_buffer*scale_factor;
    float DX = 0;
    float DY = 0;
    data->my_line.setLength(true_scale);
    DX = oldEndPos.x()-data->my_line.p2().x(); //traslazione dei nodi figli
    DY = oldEndPos.y()-data->my_line.p2().y();
    Refresh(1);
    float previous_factor = data->pen.widthF()/width_buffer;
    data->pen.setWidthF(width_buffer*scale_factor);
    if(data->stick_type == StickData::RECT){
        data->img_w_scale = (data->img_w_scale/previous_factor)*scale_factor;
    }
    if(data->stick_type == StickData::TRAPEZOID || data->stick_type == StickData::TAPER){
        data->img_w_scale = (data->img_w_scale/previous_factor)*scale_factor;
        data->img_h_scale = (data->img_h_scale/previous_factor)*scale_factor;
    }
    for(int i= 0; i<data->children.length();i++)
    {
        StickData* child = data->children[i];
        // se non è uno stepchild oppure lo è ma stai ruotando uno stepchild a sua volta
        if( (child->stepchild == false) || (data->stepchild == true && child->stepchild == true)) // dall'altra estremità del master non spostare gli stick quando il master è ruotato
        {
            //effettua la traslazione dovuta alla rotazione dell'estremo libero
            child->my_line.translate(-DX,-DY);
            if(child->view)
                child->view->Refresh(1);
        }
    }
}
void StickGraphicsItem::AddImage(QImage* img_to_add, QString name){
    data->stick_img_list.append(img_to_add);
    data->stick_img = img_to_add;
    QListWidgetItem * addedItem = new QListWidgetItem();
    QVariant newData(QVariant::fromValue(img_to_add));
    addedItem->setData(Qt::UserRole,newData);
    addedItem->setData(Qt::DisplayRole,name);
    addedItem->setIcon(QIcon(QPixmap::fromImage(*img_to_add)));
    data->img_name_list.append(name);
    img_list_widget->addItem(addedItem);
    PopulateImageListWidget();
}
void StickGraphicsItem::RemoveImgFromList(int idx){
    if(idx < 0 || idx >= data->stick_img_list.length()) // nessuna riga selezionata nel widget: currentRow() torna -1
        return;
    if(data->stick_type == StickData::IMAGE){
        if(data->stick_img_list.length()>1){
            QImage* to_remove = data->stick_img_list[idx];
            data->stick_img_list.removeAt(idx);
            data->img_name_list.removeAt(idx);
            QListWidgetItem * itemToRemove = img_list_widget->takeItem(idx);
            if(to_remove== data->stick_img)
                data->stick_img = data->stick_img_list[0];
            delete itemToRemove;
            delete to_remove;

            PopulateImageListWidget();
        }
    }


}
void StickGraphicsItem::PopulateImageListWidget(){

    if(data->stick_type == StickData::IMAGE){
        img_list_widget->clear();
        int j = 0;
        for(QImage* img:data->stick_img_list)
        {
            QListWidgetItem * addedItem = new QListWidgetItem();
            QVariant newData(QVariant::fromValue(img));
            addedItem->setData(Qt::UserRole,newData);
            addedItem->setData(Qt::DisplayRole,data->img_name_list[j]);
            addedItem->setIcon(QIcon(QPixmap::fromImage(*img)));
            img_list_widget->addItem(addedItem);
            j++;
        }
    }
}
void SceneRemover(QGraphicsScene *scene_to_clear){
    QList<QGraphicsItem*> thingsOnScene = scene_to_clear->items( Qt::AscendingOrder);
    for(QGraphicsItem* i: thingsOnScene){
        if(i != my_rect)
            scene_to_clear->removeItem(i);
    }
}

// getRegularPolygon() è stato eliminato nel safety sweep: mai chiamato da nessuno,
// e per giunta copriva solo mezzo giro (M_PI/n invece di 2*M_PI/n)
