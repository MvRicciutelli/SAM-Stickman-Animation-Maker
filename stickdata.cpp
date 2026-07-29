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
}

/**
 * @details Pure geometry/appearance blend, no scene or view involvement — it runs on the
 * pure-data records MyView::GenerateInbetweens() clones for the frames it creates, before
 * any of them is ever attached to the scene. img_angle is deliberately NOT blended from
 * the two stored values but recomputed from the interpolated segment with the codebase's
 * one convention (-atan2(dx, dy) in degrees, see StickFigure.cpp:50 and every drawing/
 * manipulation path): blending the stored angle would spin the long way round whenever the
 * two poses straddle the ±180° wrap, and would drift out of sync with the segment it is
 * supposed to track.
 */
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
    target.pen.setColor(MixColor(start.pen.color(), end.pen.color(), t));
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
