#include "stickdata.h"
#include <QFontDatabase>
#include <QtMath>

/// @details Filled by operator>> when a TEXT stick names a font this system doesn't have;
/// emptied by MyView::ReportMissingTextFonts() after it has shown the prompt.
QStringList missing_text_fonts;

/**
 * @details All defaults live as in-class initializers in stickdata.h so they are visible
 * next to each field's documentation; the constructor has nothing left to do.
 */
StickData::StickData()
{
}

/**
 * @details Image ownership rule: a StickData owns the QImage copies in stick_img_list
 * (stick_img only points into that list). Freeing them here — unconditionally — replaces
 * the flag-guarded cleanup the legacy stick destructor needed, because a pure data
 * object has no widget or scene to protect.
 */
StickData::~StickData()
{
    qDeleteAll(stick_img_list);
    stick_img_list.clear();
}

/**
 * @details This is the in-memory replacement for the historical clone idiom
 * (serialize to a QBuffer, deserialize into the target). It copies exactly the field
 * set the stream operators handle, so both paths stay interchangeable while call sites
 * migrate. Images are duplicated so the copy never shares pixel buffers with the
 * source; stick_img is remapped onto the copied list entry.
 */
void StickData::CopyFrom(const StickData &source)
{
    version = source.version;
    stick_type = source.stick_type;
    is_linker = source.is_linker;
    z = source.z;
    stepchild = source.stepchild;
    my_line = source.my_line;
    parent_idx = source.parent_idx;
    pen = source.pen;
    brush = source.brush;
    brush_follows_stick = source.brush_follows_stick;
    texture_tiled = source.texture_tiled;
    stroke_brush = source.stroke_brush;
    order = source.order;
    img_angle = source.img_angle;
    img_w_scale = source.img_w_scale;
    img_h_scale = source.img_h_scale;
    img_angle_offset = source.img_angle_offset;
    img_offset = source.img_offset;
    hard_top = source.hard_top;
    hard_bottom = source.hard_bottom;
    polygon_points = source.polygon_points;
    vertex_widths = source.vertex_widths;
    vertex_curvatures = source.vertex_curvatures;
    text = source.text;
    text_font = source.text_font;
    text_align = source.text_align;
    text_line_spacing = source.text_line_spacing;
    text_v_stretch = source.text_v_stretch;
    text_word_wrap = source.text_word_wrap;
    ik_role = source.ik_role;
    ik_start_idx = source.ik_start_idx;
    ik_target_idx = source.ik_target_idx;
    // ik_start/ik_target NON copiati: puntatori non posseduti, come parent/children —
    // difesa in profondità: azzerali esplicitamente nel caso il target di CopyFrom non
    // sia appena costruito, così non trapelano i puntatori di un uso precedente
    ik_start = nullptr;
    ik_target = nullptr;

    // difesa in profondità come in operator>>: oggi il target è sempre appena costruito,
    // ma un riuso futuro non deve far trapelare le immagini precedenti
    qDeleteAll(stick_img_list);
    stick_img_list.clear();
    img_name_list = source.img_name_list;
    stick_img = nullptr;
    for (QImage *image : source.stick_img_list) {
        QImage *copy = new QImage(*image);
        stick_img_list.append(copy);
        if (image == source.stick_img)
            stick_img = copy;
    }
}

/**
 * @details Fills in the three derived numbers of the stored BrushStroke. `pen.brush().color()`
 * rather than `pen.color()` because the two disagree for a pen whose paint is not solid, and the
 * brush's is the one an opacity slider has been writing into (see ReadPaintOpacity()). The width
 * floor keeps a hairline pen (width 0, "one device pixel") from asking for a zero-size dab.
 */
BrushStroke StickData::StrokeBrush() const
{
    BrushStroke brush = stroke_brush;
    brush.color = pen.brush().color();
    brush.size = qMax(1.0, pen.widthF());
    brush.opacity = ReadPaintOpacity(pen.brush());
    return brush;
}

/**
 * @details Single source of truth for the polygon's local frame: paint(), the vertex
 * hit-test/edit helpers (stick.cpp) and any future exporter must all go through this,
 * or an edited vertex would land somewhere else than where it is drawn. The zero-length
 * guard keeps the transform invertible while the base line is still being drawn.
 */
QTransform StickData::PolygonTransform() const
{
    qreal len = my_line.length();
    if (len < 1e-6)
        len = 1e-6; // linea degenere: mantieni la trasformazione invertibile
    QTransform t;
    t.translate(my_line.p1().x(), my_line.p1().y());
    t.rotate(img_angle + img_angle_offset);
    t.translate(img_offset.x(), img_offset.y());
    t.scale(len, len);
    return t;
}

/**
 * @details Convenience wrapper over CopyFrom() for the call sites (Phase 2/3) that
 * want a fresh heap object instead of filling an existing one.
 */
StickData *StickData::Clone() const
{
    StickData *copy = new StickData();
    copy->CopyFrom(*this);
    return copy;
}

namespace {
/// @brief Linear blend of two scalars.
inline qreal Mix(qreal from, qreal to, float t) { return from + (to - from) * t; }

/// @brief Linear blend of two points, component by component.
inline QPointF MixPoint(const QPointF &from, const QPointF &to, float t)
{
    return QPointF(Mix(from.x(), to.x(), t), Mix(from.y(), to.y(), t));
}

/// @brief Linear blend of two colours, RGBA channel by channel (alpha included, so a
/// fade in/out between two keyframes interpolates as well).
inline QColor MixColor(const QColor &from, const QColor &to, float t)
{
    return QColor(qRound(Mix(from.red(), to.red(), t)),
                  qRound(Mix(from.green(), to.green(), t)),
                  qRound(Mix(from.blue(), to.blue(), t)),
                  qRound(Mix(from.alpha(), to.alpha(), t)));
}

/// @brief Element-wise blend of a per-vertex float list (vertex_widths /
/// vertex_curvatures). No-op unless all three lists agree on the vertex count: two poses
/// with a different number of vertices have no pairwise correspondence to blend, so the
/// target keeps the list its Clone() came with.
inline void MixFloatList(QList<float> &target, const QList<float> &start, const QList<float> &end, float t)
{
    if (start.count() != end.count() || target.count() != start.count())
        return;
    for (int i = 0; i < target.count(); i++)
        target[i] = float(Mix(start[i], end[i], t));
}

/// @brief True when a fill/stroke is a plain colour, i.e. the only case where blending its
/// colour is meaningful. A pattern, an image tile or a gradient (see Widget::StyleKind)
/// carries its appearance in the brush's texture/stops, which have no linear midpoint —
/// and `QPen::setColor()` would REPLACE such a pen brush with a solid colour, silently
/// flattening a gradient stroke in every generated frame.
inline bool IsSolidFill(const QBrush &brush)
{
    return brush.style() == Qt::SolidPattern || brush.style() == Qt::NoBrush;
}
}

/**
 * @details Pure geometry/appearance blend, no scene or view involvement — it runs on the
 * pure-data records MyView::GenerateInbetweens() clones for the frames it creates, before
 * any of them is ever attached to the scene. img_angle is deliberately NOT blended from
 * the two stored values but recomputed from the interpolated segment with the codebase's
 * one convention (-atan2(dx, dy) in degrees, see StickFigure.cpp:50 and every drawing/
 * manipulation path): blending the stored angle would spin the long way round whenever the
 * two poses straddle the ±180° wrap, and would drift out of sync with the segment it is
 * supposed to track. Nothing here allocates or frees: images, gradient stops and pattern
 * tiles are left exactly as the caller's Clone() produced them (see IsSolidFill()).
 */
/// @details Built to match ComposeTextureXf()'s call order exactly: the scale is the length of
/// the matrix's first basis vector, the angle its direction, the offset its translation. Lossless
/// for every brush this app produces (all similarity transforms) — a sheared transform, which
/// nothing here creates, would read back as its nearest rotation.
TextureXf ReadTextureXf(const QBrush &brush)
{
    const QTransform t = brush.transform();
    TextureXf xf;
    xf.scale = std::hypot(t.m11(), t.m12());
    if(xf.scale <= 0.0)
        xf.scale = 1.0; // matrice degenere (o brush senza transform): riparti dall'identità
    xf.angle = qRadiansToDegrees(qAtan2(t.m12(), t.m11()));
    xf.offset = QPointF(t.dx(), t.dy());
    return xf;
}

/// @details The inverse of ReadTextureXf(). Order matters and is the QTransform/QPainter one:
/// the calls read outermost-first, so a point is scaled, then rotated, then translated.
void WriteTextureXf(QBrush *brush, const TextureXf &xf)
{
    // il floor tiene la matrice INVERTIBILE, non solo diversa da zero: il determinante è
    // scale^2, e Qt considera nullo (QTransform::isInvertible) tutto ciò che sta sotto 1e-12
    const qreal scale = qMax(xf.scale, 1e-3);
    QTransform t;
    t.translate(xf.offset.x(), xf.offset.y());
    t.rotate(xf.angle);
    t.scale(scale, scale);
    brush->setTransform(t);
}

/// @details QLineF::angle() reports in [0, 360), so negating it for QTransform's clockwise
/// convention turns a +33 degree gradient into -327 — the same direction, but a value the options
/// dialog would display and a round-trip comparison would fail on. ReadTextureXf() has no such
/// problem: qAtan2() already answers in (-180, 180], which is what this brings the other path to.
static qreal NormalizeDegrees(qreal degrees)
{
    while(degrees > 180.0)   degrees -= 360.0;
    while(degrees <= -180.0) degrees += 360.0;
    return degrees;
}

/// @details Angles come out in QTransform's clockwise convention (hence the negated
/// QLineF::angle(), which counts anticlockwise) so that a caller can treat a gradient's
/// placement and a texture's identically.
TextureXf ReadGradientXf(const QBrush &brush)
{
    TextureXf xf;
    const QGradient *gradient = brush.gradient();
    if(gradient == nullptr)
        return xf;
    if(gradient->type() == QGradient::LinearGradient){
        const QLinearGradient *linear = static_cast<const QLinearGradient*>(gradient);
        const QLineF axis(linear->start(), linear->finalStop());
        xf.offset = axis.center() - QPointF(0.5, 0.5);
        xf.angle = -axis.angle();
        xf.scale = axis.length();
    }
    else if(gradient->type() == QGradient::RadialGradient){
        const QRadialGradient *radial = static_cast<const QRadialGradient*>(gradient);
        xf.offset = radial->center() - QPointF(0.5, 0.5);
        xf.scale = 2.0 * radial->centerRadius(); // diametro: confrontabile con l'asse lineare
        xf.angle = -QLineF(radial->center(), radial->focalPoint()).angle();
    }
    if(xf.scale <= 0.0)
        xf.scale = 1.0; // geometria degenere: riparti da un box pieno
    xf.angle = NormalizeDegrees(xf.angle);
    return xf;
}

/// @details The gradient is rebuilt rather than mutated because QBrush only ever hands out a
/// const QGradient*. Everything that is not geometry is copied across explicitly — losing the
/// coordinate mode in particular would silently un-anchor the fill (see
/// StickGraphicsItem::AlignBrush()).
void WriteGradientXf(QBrush *brush, const TextureXf &xf)
{
    const QGradient *gradient = brush->gradient();
    if(gradient == nullptr)
        return;
    const QPointF centre = QPointF(0.5, 0.5) + xf.offset;
    const qreal length = qMax(xf.scale, 1e-3); // una lunghezza nulla non ha una direzione
    const QPointF half = QLineF::fromPolar(length / 2.0, -xf.angle).p2();
    const QTransform keep_transform = brush->transform();

    if(gradient->type() == QGradient::LinearGradient){
        QLinearGradient rebuilt(centre - half, centre + half);
        rebuilt.setStops(gradient->stops());
        rebuilt.setSpread(gradient->spread());
        rebuilt.setCoordinateMode(gradient->coordinateMode());
        *brush = QBrush(rebuilt);
    }
    else if(gradient->type() == QGradient::RadialGradient){
        const QRadialGradient *radial = static_cast<const QRadialGradient*>(gradient);
        // il fuoco viene ri-puntato, non ri-avvicinato: mantiene la sua distanza dal centro
        const qreal focal_distance = QLineF(radial->center(), radial->focalPoint()).length();
        const QPointF focus = centre + QLineF::fromPolar(focal_distance, -xf.angle).p2();
        QRadialGradient rebuilt(centre, length / 2.0, focus, radial->focalRadius());
        rebuilt.setStops(gradient->stops());
        rebuilt.setSpread(gradient->spread());
        rebuilt.setCoordinateMode(gradient->coordinateMode());
        *brush = QBrush(rebuilt);
    }
    else{
        return; // conico o altro: nessuna geometria che queste tre cifre descrivano
    }
    brush->setTransform(keep_transform);
}

/// @details See the header for why a gradient reads its opacity somewhere else than the other two
/// kinds of paint. The peak stop and not the average: it is the value the slider has to show, and
/// only the peak can be brought back up to 1.0 without inventing alpha the user never asked for.
qreal ReadPaintOpacity(const QBrush &brush)
{
    const QGradient *gradient = brush.gradient();
    if(gradient == nullptr)
        return qBound(0.0, brush.color().alphaF(), 1.0);
    qreal peak = 0.0;
    for(const QGradientStop &stop : gradient->stops())
        peak = qMax(peak, stop.second.alphaF());
    return qBound(0.0, peak, 1.0);
}

/// @details The gradient is rebuilt for the same reason WriteGradientXf() rebuilds it (QBrush hands
/// out only a const QGradient*), and everything that is not a stop — geometry, spread, coordinate
/// mode, the brush transform — comes across untouched. A gradient whose every stop is fully
/// transparent has no ratios left to preserve, so there the value is written flat.
void WritePaintOpacity(QBrush *brush, qreal opacity)
{
    opacity = qBound(0.0, opacity, 1.0);
    const QGradient *gradient = brush->gradient();
    if(gradient == nullptr){
        QColor tint = brush->color();
        tint.setAlphaF(opacity);
        brush->setColor(tint);
        return;
    }
    const qreal peak = ReadPaintOpacity(*brush);
    QGradientStops stops = gradient->stops();
    for(QGradientStop &stop : stops){
        QColor tint = stop.second;
        tint.setAlphaF(peak > 0.0 ? qBound(0.0, tint.alphaF() * opacity / peak, 1.0) : opacity);
        stop.second = tint;
    }
    QGradient rebuilt = *gradient;   // copia di valore: tipo, geometria, spread e modo restano
    rebuilt.setStops(stops);
    const QTransform keep_transform = brush->transform();
    *brush = QBrush(rebuilt);
    brush->setTransform(keep_transform);
}

void InterpolateStickData(StickData &target, const StickData &start, const StickData &end, float t)
{
    target.my_line = QLineF(MixPoint(start.my_line.p1(), end.my_line.p1(), t),
                            MixPoint(start.my_line.p2(), end.my_line.p2(), t));
    target.img_angle = float(-qAtan2(target.my_line.dx(), target.my_line.dy()) * 180.0 / M_PI);
    target.z = Mix(start.z, end.z, t);
    target.img_offset = MixPoint(start.img_offset, end.img_offset, t);
    target.img_w_scale = float(Mix(start.img_w_scale, end.img_w_scale, t));
    target.img_h_scale = float(Mix(start.img_h_scale, end.img_h_scale, t));
    target.img_angle_offset = qRound(Mix(start.img_angle_offset, end.img_angle_offset, t));
    target.pen.setWidthF(Mix(start.pen.widthF(), end.pen.widthF(), t));
    // la larghezza si interpola sempre, il COLORE solo fra due tinte piatte: vedi IsSolidFill()
    if (IsSolidFill(start.pen.brush()) && IsSolidFill(end.pen.brush()))
        target.pen.setColor(MixColor(start.pen.color(), end.pen.color(), t));
    if (IsSolidFill(start.brush) && IsSolidFill(end.brush))
        target.brush.setColor(MixColor(start.brush.color(), end.brush.color(), t));
    // vertici di POLYGON/MULTILINE: normalizzati nel frame locale dello stick
    // (StickData::PolygonTransform), quindi interpolabili direttamente
    if (start.polygon_points.count() == end.polygon_points.count()
            && target.polygon_points.count() == start.polygon_points.count()) {
        for (int i = 0; i < target.polygon_points.count(); i++)
            target.polygon_points[i] = MixPoint(start.polygon_points[i], end.polygon_points[i], t);
    }
    MixFloatList(target.vertex_widths, start.vertex_widths, end.vertex_widths, t);
    MixFloatList(target.vertex_curvatures, start.vertex_curvatures, end.vertex_curvatures, t);
}

/**
 * @details Moved verbatim from StickFigure.cpp; this operator was already pure-data (no
 * flag guards) and is the single write path for undo snapshots, copy/paste, the library
 * and .mAnim/.stck files. The field order IS the on-disk format — never reorder it.
 * Image-family types append their extra block; IMAGE additionally streams every image
 * variant plus the index of the displayed one.
 */
QDataStream &operator<<(QDataStream &stream, const StickData &stick_data)
{
    stream << (double)stick_data.z; // explicit double: qreal itself is float on some Qt builds
    stream << stick_data.version;
    stream << stick_data.stick_type;
    stream << stick_data.is_linker;
    stream << stick_data.stepchild;
    stream << stick_data.my_line;
    stream << stick_data.parent_idx;
    stream << stick_data.pen;
    stream << stick_data.brush;
    stream << stick_data.order;
    if (stick_data.version >= 4) {
        // ancoraggio della texture/gradiente: come l'IK sotto, ortogonale allo stick_type
        // e quindi fuori dal blocco type-gated, e version-gated perché un file salvato
        // prima che l'opzione esistesse deve leggere il default (true)
        stream << stick_data.brush_follows_stick;
    }
    if (stick_data.version >= 5) {
        stream << stick_data.texture_tiled;
    }
    if (stick_data.version >= 6) {
        // il pennello del contorno: solo la metà MEMORIZZATA di BrushStroke — colore, diametro
        // e opacità sono quelli della penna, che è già qui sopra, e StrokeBrush() li rimette
        stream << stick_data.stroke_brush.stamp;
        stream << stick_data.stroke_brush.spacing;
        stream << stick_data.stroke_brush.angle;
        stream << stick_data.stroke_brush.size_jitter;
        stream << stick_data.stroke_brush.angle_jitter;
        stream << stick_data.stroke_brush.opacity_jitter;
    }
    if (stick_data.version >= 2) {
        // ruolo IK: ortogonale allo stick_type, quindi fuori dal blocco type-gated sotto;
        // version-gated così un file salvato prima che l'IK esistesse (version 1) resta
        // leggibile — ik_start_idx/ik_target_idx sono scratch di (de)serializzazione,
        // ricalcolati da StickFigureData::operator<< appena prima di questa chiamata,
        // esattamente come parent_idx
        stream << stick_data.ik_role;
        stream << stick_data.ik_start_idx;
        stream << stick_data.ik_target_idx;
    }
    if ((stick_data.stick_type == StickData::CIRCLE) || stick_data.IsImageType() || (stick_data.stick_type == StickData::RECT) || (stick_data.stick_type == StickData::TRAPEZOID) || (stick_data.stick_type == StickData::TAPER) || (stick_data.stick_type == StickData::POLYGON) || (stick_data.stick_type == StickData::MULTILINE) || (stick_data.stick_type == StickData::TEXT)) {
        if (stick_data.IsImageType()) {
            // stick_img può essere nullo solo per un record ricostruito da un file corrotto:
            // meglio serializzare una QSize vuota che dereferenziare un puntatore nullo
            QSize imgSize = stick_data.stick_img ? stick_data.stick_img->size() : QSize();
            stream << imgSize;
        }
        stream << stick_data.img_angle;
        stream << stick_data.img_angle_offset; // mancava: si azzerava a ogni save/load/clone (vedi paint(): img_angle+img_angle_offset)
        stream << stick_data.img_h_scale;
        stream << stick_data.img_w_scale;
        stream << stick_data.img_offset;
        stream << stick_data.hard_top;
        stream << stick_data.hard_bottom;

        if (stick_data.IsImageType()) {
            stream << (qint32)stick_data.stick_img_list.length();
            stream << (qint32)stick_data.stick_img_list.indexOf(stick_data.stick_img);
            int j = 0;
            for (QImage *i : stick_data.stick_img_list) {
                stream << *i;
                stream << stick_data.img_name_list[j];
                j++;
            }
        }
        if (stick_data.stick_type == StickData::POLYGON || stick_data.stick_type == StickData::MULTILINE) {
            stream << (qint32)stick_data.polygon_points.length();
            for (const QPointF &pt : stick_data.polygon_points)
                stream << pt;
            // curvatura per vertice (entrambi i tipi)
            stream << (qint32)stick_data.vertex_curvatures.length();
            for (float c : stick_data.vertex_curvatures)
                stream << c;
        }
        if (stick_data.stick_type == StickData::MULTILINE) {
            // larghezza per vertice virtuale (interni + 2 estremi), solo MULTILINE
            stream << (qint32)stick_data.vertex_widths.length();
            for (float w : stick_data.vertex_widths)
                stream << w;
        }
        if (stick_data.stick_type == StickData::TEXT) {
            // QFont ha i suoi operatori di stream: famiglia, corpo, grassetto, corsivo,
            // spaziatura fra le lettere e stretch orizzontale viaggiano in un colpo solo
            stream << stick_data.text;
            stream << stick_data.text_font;
            stream << stick_data.text_align;
            stream << stick_data.text_line_spacing;
            stream << stick_data.text_word_wrap;
            if (stick_data.version >= 3)
                stream << stick_data.text_v_stretch; // lo stretch verticale non è nel QFont
        }
    }

    return stream;
}

/**
 * @details Mirror of operator<< (moved verbatim from StickFigure.cpp). Fills a
 * freshly-constructed record; IMAGE sticks get their image list rebuilt with newly
 * allocated copies, so the record owns everything it points to. No scene attach, no
 * widget update happens here anymore — that is the whole point of the Phase 1 split.
 */
QDataStream &operator>>(QDataStream &stream, StickData &stick_data)
{
    double z_value = 0;
    stream >> z_value; // matches the explicit double written by operator<<, regardless of qreal's local width
    stick_data.z = z_value;
    stream >> stick_data.version;
    stream >> stick_data.stick_type;
    stream >> stick_data.is_linker;
    stream >> stick_data.stepchild;
    stream >> stick_data.my_line;
    stream >> stick_data.parent_idx;
    stream >> stick_data.pen;
    stream >> stick_data.brush;
    stream >> stick_data.order;
    if (stick_data.version >= 4) {
        stream >> stick_data.brush_follows_stick;
    }
    if (stick_data.version >= 5) {
        stream >> stick_data.texture_tiled;
    }
    if (stick_data.version >= 6) {
        stream >> stick_data.stroke_brush.stamp;
        stream >> stick_data.stroke_brush.spacing;
        stream >> stick_data.stroke_brush.angle;
        stream >> stick_data.stroke_brush.size_jitter;
        stream >> stick_data.stroke_brush.angle_jitter;
        stream >> stick_data.stroke_brush.opacity_jitter;
    }
    if (stick_data.version >= 2) {
        stream >> stick_data.ik_role;
        stream >> stick_data.ik_start_idx;
        stream >> stick_data.ik_target_idx;
        // ik_start/ik_target vengono risolti dagli indici più tardi, a livello di
        // StickFigureData::operator>> (stessa fase in cui parent_idx diventa parent)
    }
    if (stick_data.IsImageType() || (stick_data.stick_type == StickData::CIRCLE) || (stick_data.stick_type == StickData::RECT) || (stick_data.stick_type == StickData::TRAPEZOID) || (stick_data.stick_type == StickData::TAPER) || (stick_data.stick_type == StickData::POLYGON) || (stick_data.stick_type == StickData::MULTILINE) || (stick_data.stick_type == StickData::TEXT)) {
        QSize mysize;
        if (stick_data.IsImageType()) {
            stream >> mysize;
        }
        stream >> stick_data.img_angle;
        stream >> stick_data.img_angle_offset;
        stream >> stick_data.img_h_scale;
        stream >> stick_data.img_w_scale;
        stream >> stick_data.img_offset;
        stream >> stick_data.hard_top;
        stream >> stick_data.hard_bottom;
        if (stick_data.IsImageType()) {
            qint32 numOfImgs;
            qint32 stickImgIdx;
            stream >> numOfImgs;
            stream >> stickImgIdx;
            for (int i = 0; i < numOfImgs; i++) {
                QImage *img = new QImage(mysize, QImage::Format_ARGB32);
                stream >> *img;
                QString name;
                stick_data.stick_img_list.append(img);
                stream >> name;
                stick_data.img_name_list.append(name);
                if (stream.status() != QDataStream::Ok)
                    break; // file troncato: non continuare ad allocare immagini da dati inesistenti
            }
            // l'indice viene dal file: su un file corrotto può essere -1 o fuori range, e
            // indicizzare la lista senza controllo scrive/legge fuori dai limiti
            if (stickImgIdx >= 0 && stickImgIdx < stick_data.stick_img_list.count())
                stick_data.stick_img = stick_data.stick_img_list[stickImgIdx];
            else if (!stick_data.stick_img_list.isEmpty())
                stick_data.stick_img = stick_data.stick_img_list[0];
        }
        if (stick_data.stick_type == StickData::POLYGON || stick_data.stick_type == StickData::MULTILINE) {
            qint32 num_points = 0;
            stream >> num_points;
            for (qint32 i = 0; i < num_points; i++) {
                QPointF pt;
                stream >> pt;
                if (stream.status() != QDataStream::Ok)
                    break; // file troncato/corrotto: non inventare vertici da dati inesistenti
                stick_data.polygon_points.append(pt);
            }
            qint32 num_curvatures = 0;
            stream >> num_curvatures;
            for (qint32 i = 0; i < num_curvatures; i++) {
                float c = 0;
                stream >> c;
                if (stream.status() != QDataStream::Ok)
                    break;
                stick_data.vertex_curvatures.append(c);
            }
        }
        if (stick_data.stick_type == StickData::MULTILINE) {
            qint32 num_widths = 0;
            stream >> num_widths;
            for (qint32 i = 0; i < num_widths; i++) {
                float w = 0;
                stream >> w;
                if (stream.status() != QDataStream::Ok)
                    break;
                stick_data.vertex_widths.append(w);
            }
        }
        if (stick_data.stick_type == StickData::TEXT) {
            stream >> stick_data.text;
            stream >> stick_data.text_font;
            stream >> stick_data.text_align;
            stream >> stick_data.text_line_spacing;
            stream >> stick_data.text_word_wrap;
            if (stick_data.version >= 3)
                stream >> stick_data.text_v_stretch;
            // font non installato su questa macchina: ripiega su Arial e registra la
            // famiglia mancante. Il prompt lo mostra chi ha avviato il caricamento
            // (MyView::ReportMissingTextFonts()): qui siamo nel layer dati, senza UI
            const QString family = stick_data.text_font.family();
            static const QStringList installed = QFontDatabase::families();
            // installed vuota = font DB non utilizzabile (piattaforma headless senza font):
            // "non lo so" non è "manca", altrimenti riscriveremmo ad Arial ogni text stick
            if (!installed.isEmpty() && !family.isEmpty()
                    && !installed.contains(family, Qt::CaseInsensitive)) {
                if (!missing_text_fonts.contains(family))
                    missing_text_fonts.append(family);
                stick_data.text_font.setFamily("Arial");
            }
        }
    }
    return stream;
}
