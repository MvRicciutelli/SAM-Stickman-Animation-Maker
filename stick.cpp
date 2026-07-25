#include "stick.h"
#include <QGraphicsItem>
#include <QPainterPath>
#include <QVector>
#include <QListWidget>
#include <StickFigure.h>
bool StickGraphicsItem::onion_render = false;
bool StickGraphicsItem::offscreen_render = false;
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
/// @details Centerline with per-vertex quadratic corner smoothing: a vertex with
/// curvature c trades its sharp angle for a quadratic curve entering/leaving at
/// c·½·(shorter adjacent edge). Shared by the POLYGON (closed) render and usable for
/// open chains; curvature index i belongs to verts[i], missing entries read as 0.
static QPainterPath CurvedPath(const QPolygonF &verts, const QList<float> &curvatures, bool closed)
{
    QPainterPath path;
    int n = verts.size();
    if(n == 0) return path;
    if(n < 3){
        path.moveTo(verts[0]);
        if(n == 2) path.lineTo(verts[1]);
        return path;
    }
    auto corner = [&](int i, QPointF &a, QPointF &b) -> bool {
        QPointF prev = verts[(i-1+n)%n], next = verts[(i+1)%n];
        QLineF in(prev, verts[i]), out(verts[i], next);
        float c = qBound(0.0f, curvatures.value(i, 0.0f), 1.0f);
        float r = c * 0.5f * qMin(in.length(), out.length());
        if(c <= 0.001f || r < 0.5f){ a = verts[i]; b = verts[i]; return false; }
        a = in.pointAt(1.0 - r/in.length());
        b = out.pointAt(r/out.length());
        return true;
    };
    QPointF a, b;
    if(closed){
        bool rounded = corner(0, a, b);
        path.moveTo(rounded ? b : verts[0]); // parti DOPO l'eventuale smusso del vertice 0
        for(int i = 1; i < n; i++){
            rounded = corner(i, a, b);
            path.lineTo(a);
            if(rounded) path.quadTo(verts[i], b);
        }
        rounded = corner(0, a, b);
        path.lineTo(a);
        if(rounded) path.quadTo(verts[0], b);
        path.closeSubpath();
    }
    else{
        path.moveTo(verts[0]);
        for(int i = 1; i < n-1; i++){
            bool rounded = corner(i, a, b);
            path.lineTo(a);
            if(rounded) path.quadTo(verts[i], b);
        }
        path.lineTo(verts[n-1]);
    }
    return path;
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
    else if(data->stick_type == StickData::POLYGON){
        // i vertici sono in coordinate locali normalizzate: ScenePolygon() li mappa in
        // scena (inclusa l'anteprima del prossimo vertice durante il disegno)
        QPolygonF poly = ScenePolygon();
        if(poly.size() == 2){
            painter->drawLine(poly[0], poly[1]);
        }
        else if(poly.size() >= 3){
            // curvatura per vertice: gli spigoli con curvatura > 0 diventano curve
            // quadratiche (QPainterPath); l'indice va rimappato saltando l'anteprima
            QList<float> curvatures;
            for(int i = 0; i < poly.size(); i++){
                int data_idx = (poly_preview_active && i == poly_preview_idx) ? -1
                             : (poly_preview_active && i > poly_preview_idx) ? i-1 : i;
                curvatures.append(data->vertex_curvatures.value(data_idx, 0.0f));
            }
            painter->setBrush(data->brush);
            painter->drawPath(CurvedPath(poly, curvatures, true));
        }
        PaintVertexSquares(painter, poly);
    }
    else if(data->stick_type == StickData::MULTILINE){
        QPolygonF chain = ScenePolygon(); // P1 + interni + P2 (+ anteprima)
        if(chain.size() >= 2){
            // spezzata a larghezza variabile per vertice, con smussi di curvatura:
            // 1) densifica la linea centrale (campiona una quadratica agli angoli con
            //    curvatura > 0), portandosi dietro la larghezza per punto
            QList<QPointF> center;
            QList<float> widths;
            // l'anteprima del prossimo vertice è inserita nella chain ma non ha una
            // voce nelle liste larghezza/curvatura: rimappa l'indice (QList::value con
            // indice -1 restituisce il default, quindi l'anteprima eredita la penna)
            auto data_idx_of = [this](int i) -> int {
                if(poly_preview_active && i == poly_preview_idx) return -1;
                return (poly_preview_active && i > poly_preview_idx) ? i-1 : i;
            };
            auto width_at = [this, &data_idx_of](int idx) -> float {
                return data->vertex_widths.value(data_idx_of(idx), data->pen.widthF());
            };
            auto curvature_at = [this, &data_idx_of](int idx) -> float {
                return qBound(0.0f, data->vertex_curvatures.value(data_idx_of(idx), 0.0f), 1.0f);
            };
            center.append(chain[0]);
            widths.append(width_at(0));
            for(int i = 1; i < chain.size() - 1; i++){
                float c = curvature_at(i);
                QLineF in(chain[i-1], chain[i]);
                QLineF out(chain[i], chain[i+1]);
                float r = c * 0.5f * qMin(in.length(), out.length());
                if(c <= 0.001f || r < 0.5f){
                    center.append(chain[i]);
                    widths.append(width_at(i));
                    continue;
                }
                // smusso: entra a distanza r prima del vertice, esce r dopo; quadratica
                // con il vertice come punto di controllo
                QPointF a = in.pointAt(1.0 - r / in.length());
                QPointF b = out.pointAt(r / out.length());
                for(int s = 0; s <= 6; s++){
                    qreal u = s / 6.0;
                    QPointF q = (1-u)*(1-u)*a + 2*(1-u)*u*chain[i] + u*u*b;
                    center.append(q);
                    widths.append(width_at(i));
                }
            }
            center.append(chain[chain.size()-1]);
            widths.append(width_at(chain.size()-1));
            // 2) contorno: per ogni punto densificato sposta a destra/sinistra di metà
            //    larghezza lungo la normale della direzione media, poi riempi
            QPolygonF left, right;
            for(int k = 0; k < center.size(); k++){
                QPointF prev = center[qMax(0, k-1)];
                QPointF next = center[qMin(center.size()-1, k+1)];
                QLineF dir(prev, next);
                if(dir.length() < 1e-6) dir = QLineF(QPointF(0,0), QPointF(1,0));
                QPointF n(-dir.dy()/dir.length(), dir.dx()/dir.length());
                left  << center[k] + n * (widths[k]/2.0);
                right << center[k] - n * (widths[k]/2.0);
            }
            QColor stroke = painter->pen().color(); // già ingrigito durante l'onion render
            painter->setPen(Qt::NoPen);
            painter->setBrush(stroke);
            QPolygonF outline = left;
            for(int k = right.size()-1; k >= 0; k--)
                outline << right[k];
            painter->drawPolygon(outline);
            // estremità arrotondate come i cap RoundCap di uno stick normale
            if(data->pen.capStyle() == Qt::RoundCap){
                painter->drawEllipse(center.first(), widths.first()/2.0, widths.first()/2.0);
                painter->drawEllipse(center.last(),  widths.last()/2.0,  widths.last()/2.0);
            }
            painter->setPen(data->pen);
            painter->setBrush(Qt::NoBrush);
        }
        PaintVertexSquares(painter, chain);
    }
    else if(data->stick_type == StickData::LINE){
        painter->drawLine(data->my_line.x1(),data->my_line.y1(),data->my_line.x2(),data->my_line.y2());
    }

    // il marcatore della catena IK (linea tratteggiata + croce sul target) non è più
    // dipinto qui: era vincolato allo z di questo item (~0) e finiva sotto le maniglie
    // (z 100/101). Ora lo disegna MyView::drawForeground(), sopra tutto e solo nella view
    // interattiva (mai negli export) — vedi myview.cpp

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
/// @details Vertex preview squares shared by the POLYGON and MULTILINE paint branches:
/// live-UI only (never in onion skins, icons or exports, which render from clones
/// without selection), red, orange for the vertices Ctrl(+Shift)-selected on the
/// select tool; visible also on a non-current stick that holds part of the
/// multi-selection.
void StickGraphicsItem::PaintVertexSquares(QPainter *painter, const QPolygonF &verts)
{
    if((!selected && selected_vertices.isEmpty()) || onion_render)
        return;
    painter->setPen(Qt::NoPen);
    float s = handle_size*0.5;
    for(int i = 0; i < verts.size(); i++){
        bool is_preview = poly_preview_active && i == poly_preview_idx;
        int data_idx = (poly_preview_active && i > poly_preview_idx) ? i-1 : i;
        bool vertex_selected = !is_preview && selected_vertices.contains(data_idx);
        painter->setBrush(vertex_selected ? QColor(0xff,0xa5,0x00) : QColor(Qt::red));
        painter->drawRect(QRectF(verts[i].x()-s/2, verts[i].y()-s/2, s, s));
    }
    painter->setPen(data->pen);
    painter->setBrush(Qt::NoBrush);
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
    if(data->stick_type == StickData::POLYGON || data->stick_type == StickData::MULTILINE){
        // i vertici possono estendersi ben oltre il cerchio della linea base: unisci i
        // due estenti e lascia un margine per penna (o larghezza massima dei vertici)
        // e quadratini dei vertici
        QRectF line_rect(data->my_line.p1(), data->my_line.p2());
        QRectF r = line_rect.normalized() | ScenePolygon().boundingRect();
        float m = data->pen.widthF() + handle_size;
        for(float w : data->vertex_widths)
            m = qMax(m, w + handle_size);
        return r.adjusted(-m, -m, m, m);
    }
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
    // (il marcatore IK non si dipinge più in questo item, quindi il suo ingombro non va
    // più unito qui — vedi MyView::drawForeground())

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
    if((data->stick_type == StickData::IMAGE ||data->stick_type == StickData::CIRCLE|| data->stick_type == StickData::RECT || data->stick_type == StickData::TAPER || data->stick_type == StickData::TRAPEZOID || data->stick_type == StickData::POLYGON || data->stick_type == StickData::MULTILINE) && !traslationOnly){
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
    if(data->stick_type == StickData::IMAGE ||data->stick_type == StickData::CIRCLE|| data->stick_type == StickData::TRAPEZOID || data->stick_type == StickData::TAPER || data->stick_type == StickData::RECT || data->stick_type == StickData::POLYGON || data->stick_type == StickData::MULTILINE){
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
                    || child->stick_type == StickData::TRAPEZOID || child->stick_type == StickData::TAPER
                    || child->stick_type == StickData::POLYGON || child->stick_type == StickData::MULTILINE){
                child->img_angle = -atan2(child->my_line.dx(),child->my_line.dy())*180/M_PI;
            }
            if(child->view)
                child->view->Refresh(0);
        }

    }
    // se stiamo con un immagine ruotala anche
    if(data->stick_type == StickData::IMAGE || data->stick_type == StickData::RECT
            || data->stick_type == StickData::TRAPEZOID || data->stick_type == StickData::TAPER|| data->stick_type == StickData::CIRCLE
            || data->stick_type == StickData::POLYGON || data->stick_type == StickData::MULTILINE){
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
/// @details Shared predicate of the vertex-editing helpers: POLYGON and MULTILINE are
/// the two stick types whose extra vertices live in polygon_points.
static bool HasEditableVertices(const StickData *data)
{
    return data->stick_type == StickData::POLYGON || data->stick_type == StickData::MULTILINE;
}

/// @details The virtual vertex chain both tools edit: a POLYGON's vertices are all in
/// polygon_points; a MULTILINE brackets its interior vertices with my_line's own ends.
QPolygonF StickGraphicsItem::SceneVertices() const
{
    QPolygonF verts;
    if(!HasEditableVertices(data)) return verts;
    QTransform t = data->PolygonTransform();
    if(data->stick_type == StickData::MULTILINE)
        verts << data->my_line.p1();
    for(const QPointF &pt : data->polygon_points)
        verts << t.map(pt);
    if(data->stick_type == StickData::MULTILINE)
        verts << data->my_line.p2();
    return verts;
}

/// @details Re-anchors a MULTILINE's local frame on new endpoints: interior vertices
/// are captured in scene coordinates through the OLD transform, the line/angle are
/// replaced, then the vertices are re-normalized through the NEW transform — so an
/// endpoint edit never drags the rest of the path along.
void StickGraphicsItem::RebuildMultilineFrame(const QPointF &new_p1, const QPointF &new_p2)
{
    QTransform old_t = data->PolygonTransform();
    QList<QPointF> scene_points;
    for(const QPointF &pt : data->polygon_points)
        scene_points.append(old_t.map(pt));
    data->my_line = QLineF(new_p1, new_p2);
    data->img_angle = float(-atan2(data->my_line.dx(), data->my_line.dy())*180/M_PI);
    QTransform inverse = data->PolygonTransform().inverted();
    data->polygon_points.clear();
    for(const QPointF &scene_pt : scene_points)
        data->polygon_points.append(inverse.map(scene_pt));
}

/// @details Converts the click to normalized local coordinates through
/// StickData::PolygonTransform() (the same transform paint() draws with), then inserts.
/// For a MULTILINE the index is virtual and clamped to the interior range (endpoints
/// are my_line's, only AppendMultilineVertex()/EditPolygonPoint() move those), and the
/// parallel width/curvature lists get a matching entry (width interpolating the
/// neighbors). Insertion shifts every following index: the caller-side selection set
/// and preview index are the caller's responsibility.
bool StickGraphicsItem::AddPolygonPoint(const QPointF &scene_point, int insert_idx)
{
    if(!HasEditableVertices(data)) return false;
    if(data->stick_type == StickData::MULTILINE){
        // indice virtuale: gli interni occupano 1..count(); default in coda agli interni
        if(insert_idx < 1 || insert_idx > data->polygon_points.length() + 1)
            insert_idx = data->polygon_points.length() + 1;
        data->polygon_points.insert(insert_idx - 1,
                                    data->PolygonTransform().inverted().map(scene_point));
        float width_before = data->vertex_widths.value(insert_idx - 1, data->pen.widthF());
        float width_after  = data->vertex_widths.value(insert_idx, data->pen.widthF());
        data->vertex_widths.insert(qMin(insert_idx, (int)data->vertex_widths.length()),
                                   (width_before + width_after) / 2);
        data->vertex_curvatures.insert(qMin(insert_idx, (int)data->vertex_curvatures.length()), 0.0f);
    }
    else{
        if(insert_idx < 0 || insert_idx > data->polygon_points.length())
            insert_idx = data->polygon_points.length();
        data->polygon_points.insert(insert_idx, data->PolygonTransform().inverted().map(scene_point));
        // mantieni allineata la lista curvature (i vertici importati possono non averla)
        while(data->vertex_curvatures.length() < data->polygon_points.length() - 1)
            data->vertex_curvatures.append(0.0f);
        data->vertex_curvatures.insert(qMin(insert_idx, (int)data->vertex_curvatures.length()), 0.0f);
    }
    Refresh(1);
    return true;
}

/// @details MULTILINE drawing: each click extends the path — the old free end becomes
/// an interior vertex (keeping its width) and the click becomes the new my_line.p2().
bool StickGraphicsItem::AppendMultilineVertex(const QPointF &scene_point)
{
    if(data->stick_type != StickData::MULTILINE) return false;
    QTransform old_t = data->PolygonTransform();
    QList<QPointF> scene_points;
    for(const QPointF &pt : data->polygon_points)
        scene_points.append(old_t.map(pt));
    scene_points.append(data->my_line.p2()); // il vecchio estremo diventa interno
    data->my_line.setP2(scene_point);
    data->img_angle = float(-atan2(data->my_line.dx(), data->my_line.dy())*180/M_PI);
    QTransform inverse = data->PolygonTransform().inverted();
    data->polygon_points.clear();
    for(const QPointF &scene_pt : scene_points)
        data->polygon_points.append(inverse.map(scene_pt));
    // il nuovo estremo eredita larghezza dall'estremo precedente, curvatura zero
    data->vertex_widths.append(data->vertex_widths.value(data->vertex_widths.length() - 1,
                                                         data->pen.widthF()));
    data->vertex_curvatures.append(0.0f);
    Refresh(1);
    return true;
}

/// @details Nearest-vertex lookup in scene units, so the caller can compare the
/// returned distance against a zoom-aware pick radius without knowing the local frame.
/// Returns a VIRTUAL index (MULTILINE endpoints included).
int StickGraphicsItem::SelectPolygonPoint(const QPointF &scene_pos, float *min_dist_out)
{
    QPolygonF verts = SceneVertices();
    if(verts.isEmpty()) return -1;

    int chosen_idx = 0;
    float min_dist = QLineF(scene_pos, verts[0]).length();
    for(int idx = 1; idx < verts.length(); idx++){
        float current_dist = QLineF(scene_pos, verts[idx]).length();
        if(current_dist < min_dist){
            min_dist = current_dist;
            chosen_idx = idx;
        }
    }
    if(min_dist_out)
        *min_dist_out = min_dist;
    return chosen_idx;
}

/// @details Moves a vertex to a scene position (mapped back to normalized local
/// coords). A MULTILINE endpoint move rebuilds the local frame instead, keeping every
/// other vertex fixed in the scene.
bool StickGraphicsItem::EditPolygonPoint(int index, const QPointF &scene_pos)
{
    if(!HasEditableVertices(data)) return false;
    if(data->stick_type == StickData::MULTILINE){
        int last = data->polygon_points.length() + 1;
        if(index < 0 || index > last) return false;
        if(index == 0)
            RebuildMultilineFrame(scene_pos, data->my_line.p2());
        else if(index == last)
            RebuildMultilineFrame(data->my_line.p1(), scene_pos);
        else
            data->polygon_points[index - 1] = data->PolygonTransform().inverted().map(scene_pos);
    }
    else{
        if(index < 0 || index >= data->polygon_points.length()) return false;
        data->polygon_points[index] = data->PolygonTransform().inverted().map(scene_pos);
    }
    Refresh(1);
    return true;
}

/// @details Removes a vertex; any resulting count is legal here (MyView deletes the
/// whole stick when a MULTILINE drops below 2 vertices, per spec). Deleting a
/// MULTILINE endpoint promotes its neighbor to being the new P1/P2. Keeps the
/// selection set consistent by shifting the indices above the removed one.
bool StickGraphicsItem::DeletePolygonPoint(int index)
{
    if(!HasEditableVertices(data)) return false;
    if(data->stick_type == StickData::MULTILINE){
        int last = data->polygon_points.length() + 1;
        if(index < 0 || index > last) return false;
        QPolygonF verts = SceneVertices();
        if(verts.length() <= 2) return false; // sotto i 2 vertici ci pensa il chiamante (cancella lo stick)
        if(index == 0){
            data->polygon_points.removeFirst(); // il primo interno diventa il nuovo P1
            RebuildMultilineFrame(verts[1], data->my_line.p2());
        }
        else if(index == last){
            data->polygon_points.removeLast(); // l'ultimo interno diventa il nuovo P2
            RebuildMultilineFrame(data->my_line.p1(), verts[verts.length() - 2]);
        }
        else{
            data->polygon_points.removeAt(index - 1);
        }
        if(index < data->vertex_widths.length())
            data->vertex_widths.removeAt(index);
        if(index < data->vertex_curvatures.length())
            data->vertex_curvatures.removeAt(index);
    }
    else{
        if(index < 0 || index >= data->polygon_points.length()) return false;
        data->polygon_points.removeAt(index);
        if(index < data->vertex_curvatures.length())
            data->vertex_curvatures.removeAt(index);
    }
    QSet<int> shifted;
    for(int idx : selected_vertices){
        if(idx == index) continue;
        shifted.insert(idx > index ? idx - 1 : idx);
    }
    selected_vertices = shifted;
    Refresh(1);
    return true;
}

/// @details Distance from a scene point to a segment (perpendicular foot clamped to the
/// segment's ends) — QLineF has no point-distance helper of its own.
static float DistanceFromSegment(const QPointF &p, const QPointF &a, const QPointF &b)
{
    QPointF ab = b - a;
    float len2 = ab.x()*ab.x() + ab.y()*ab.y();
    if(len2 < 1e-9f)
        return QLineF(p, a).length();
    float u = ((p.x()-a.x())*ab.x() + (p.y()-a.y())*ab.y()) / len2;
    u = qBound(0.0f, u, 1.0f);
    return QLineF(p, a + u*ab).length();
}

/// @details Walks every edge of the vertex chain (POLYGON: closing edge included;
/// MULTILINE: open chain) and returns the VIRTUAL insertion index that puts a new
/// vertex on the nearest one: edge i→i+1 inserts at i+1. Below 2 vertices there is no
/// edge, so the new vertex simply appends.
int StickGraphicsItem::NearestEdgeInsertIndex(const QPointF &scene_pos, float *dist_out)
{
    if(!HasEditableVertices(data)) return -1;
    QPolygonF verts = SceneVertices();
    int n = verts.length();
    if(n < 2){
        if(dist_out) *dist_out = 0;
        return n;
    }
    bool closed = data->stick_type == StickData::POLYGON;
    int edge_count = closed ? n : n - 1;
    int best_idx = n;
    float best_dist = -1;
    for(int i = 0; i < edge_count; i++){
        float d = DistanceFromSegment(scene_pos, verts[i], verts[(i+1) % n]);
        if(best_dist < 0 || d < best_dist){
            best_dist = d;
            best_idx = i+1;
        }
    }
    if(dist_out)
        *dist_out = best_dist;
    return best_idx;
}

/// @details The preview vertex is pure view state: it lives on this item, never on
/// StickData, so undo snapshots and saves taken mid-gesture can't capture it. The
/// index is virtual; a MULTILINE drawing session passes one past the last vertex (the
/// next click extends the path).
void StickGraphicsItem::SetPolygonPreview(const QPointF &scene_pos, int insert_idx)
{
    if(!HasEditableVertices(data)) return;
    poly_preview_active = true;
    poly_preview_idx = qBound(0, insert_idx, (int)SceneVertices().length());
    poly_preview_point = data->PolygonTransform().inverted().map(scene_pos);
    Refresh(1);
}

void StickGraphicsItem::ClearPolygonPreview()
{
    if(!poly_preview_active) return;
    poly_preview_active = false;
    Refresh(1);
}

/// @details Scene-coordinate vertex chain used by paint() and ConstructBr(); includes
/// the preview vertex so the shape the user sees while placing a point is the shape
/// they get on click.
QPolygonF StickGraphicsItem::ScenePolygon() const
{
    QPolygonF poly = SceneVertices();
    if(poly_preview_active && HasEditableVertices(data))
        poly.insert(qBound(0, poly_preview_idx, (int)poly.length()),
                    data->PolygonTransform().map(poly_preview_point));
    return poly;
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
    // La MULTILINE non disegna con data->pen ma con lo spessore per-vertice vertex_widths:
    // senza scalarlo la spezzata cresce di forma ma resta spessa uguale (Group D bug 3).
    // Stessa normalizzazione incrementale di img_w_scale (il drag richiama Scale più volte).
    // Il POLYGON invece traccia il contorno con data->pen, già scalata sopra.
    if(data->stick_type == StickData::MULTILINE){
        for(float &w : data->vertex_widths)
            w = (w/previous_factor)*scale_factor;
    }
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
