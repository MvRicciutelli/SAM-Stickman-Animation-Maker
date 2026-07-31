#include "stkformat.h"
#include "stickfiguredata.h"
#include <QFile>
#include <QFileInfo>
#include <QHash>
#include <QObject>
#include <QtEndian>
#include <QtMath>
#include <algorithm>
#include <cstring>

/**
 * @details Implements STK_IMPORT_EXPORT_PLAN.md. Layout knowledge is concentrated in
 * the Parse/Write helpers; both .stk variants funnel into one intermediate segment
 * list (joint tree with absolute angles) and one BuildFigure() that produces the
 * StickFigureData, so the mapping rules exist in exactly one place.
 */
namespace {

// ---------------------------------------------------------------- byte cursor ----

/**
 * @brief Bounds-checked reader over a byte buffer, little- or big-endian.
 *
 * Every primitive read checks the remaining size and latches a fail flag instead of
 * reading out of bounds; callers check Failed() at section boundaries (same hardening
 * philosophy as the QDataStream status checks in the *Data operator>>).
 */
class ByteCursor
{
public:
    ByteCursor(const QByteArray &buffer, bool big_endian)
        : data(buffer), big(big_endian) {}

    quint8 U8()   { return Fixed<quint8>(); }
    quint16 U16() { return Fixed<quint16>(); }
    qint16 I16()  { return Fixed<qint16>(); }
    qint32 I32()  { return Fixed<qint32>(); }
    float F32()   { quint32 u = Fixed<quint32>(); float f;  std::memcpy(&f, &u, 4); return f; }
    double F64()  { quint64 u = Fixed<quint64>(); double d; std::memcpy(&d, &u, 8); return d; }
    void Skip(qint64 count)
    {
        if (fail || count < 0 || pos + count > data.size()) { fail = true; return; }
        pos += count;
    }
    bool Failed() const { return fail; }
    bool AtEnd() const { return pos >= data.size(); }
    qint64 Remaining() const { return data.size() - pos; }

private:
    template<typename T> T Fixed()
    {
        if (fail || pos + qint64(sizeof(T)) > data.size()) { fail = true; return T(); }
        T value;
        std::memcpy(&value, data.constData() + pos, sizeof(T));
        pos += sizeof(T);
        return big ? qFromBigEndian(value) : qFromLittleEndian(value);
    }

    const QByteArray &data;
    qint64 pos = 0;
    bool big = false;
    bool fail = false;
};

/**
 * @brief Little-endian append-only writer (both .stk output variants are LE).
 */
class ByteWriter
{
public:
    void U8(quint8 v)   { out.append(char(v)); }
    void U16(quint16 v) { Fixed<quint16>(v); }
    void I16(qint16 v)  { Fixed<qint16>(v); }
    void I32(qint32 v)  { Fixed<qint32>(v); }
    void F32(float v)   { quint32 u; std::memcpy(&u, &v, 4); Fixed<quint32>(u); }
    void F64(double v)  { quint64 u; std::memcpy(&u, &v, 8); Fixed<quint64>(u); }
    QByteArray out;

private:
    template<typename T> void Fixed(T v)
    {
        T le = qToLittleEndian(v);
        out.append(reinterpret_cast<const char *>(&le), sizeof(T));
    }
};

// --------------------------------------------------------------- zlib helpers ----

/**
 * @brief Inflates a raw zlib stream using qUncompress (which wants a 4-byte
 *        big-endian expected-size prefix; Qt grows the buffer itself if the hint is
 *        low, so one generous hint suffices).
 */
QByteArray DecompressZlib(const QByteArray &zlib_stream)
{
    if (zlib_stream.size() < 2)
        return QByteArray();
    quint32 hint = quint32(qMin<qint64>(qint64(zlib_stream.size()) * 20 + 1024, 64 * 1024 * 1024));
    QByteArray framed;
    framed.append(char((hint >> 24) & 0xFF));
    framed.append(char((hint >> 16) & 0xFF));
    framed.append(char((hint >> 8) & 0xFF));
    framed.append(char(hint & 0xFF));
    framed.append(zlib_stream);
    return qUncompress(framed);
}

/**
 * @brief Deflates to a raw zlib stream (qCompress output minus its 4-byte size prefix).
 */
QByteArray CompressZlib(const QByteArray &plain)
{
    return qCompress(plain, 9).mid(4);
}

// ------------------------------------------------------ intermediate segments ----

/**
 * @brief One STK segment in the format-neutral intermediate both .stk variants parse
 *        into: a joint-tree edge with absolute direction (plan §4.0).
 */
struct StkSegment
{
    int parent_joint = 0;      ///< 0 = origin, k = end of segment k (1-based)
    double length = 0;
    double angle = 0;          ///< radians, absolute, +x axis, y-down
    double thickness = 0;
    int type = 0;              ///< modern type byte (v2 maps its 0/1 onto the same values)
    bool is_static = false;
    QColor color = QColor(0, 0, 0, 255);
    int draw_position = 0;     ///< rank in the figure's draw order (defaults to file order)
};

/**
 * @brief One polygon fill in the format-neutral intermediate (STK section B and the
 *        .nodes trailer both resolve their joint/node references into this).
 */
struct StkPolyfill
{
    QColor color = QColor(0, 0, 0, 255);
    QList<int> joints;         ///< STK joint indices (0 = origin, k = end of segment k)
    int draw_position = 0;     ///< rank in the figure-wide draw order
};

/**
 * @brief Appends `stick` to the children list of every ancestor, replicating the
 *        flattened all-descendants invariant (see StickFigure::EndDrawing()).
 */
void AppendToAncestors(StickData *stick)
{
    for (StickData *ancestor = stick->parent; ancestor; ancestor = ancestor->parent)
        ancestor->children.append(stick);
}

/**
 * @brief Builds a POLYGON StickData from absolute vertex positions.
 * @param verts polygon outline, absolute figure coordinates, in order.
 * @param anchor joint the stick attaches to (becomes my_line.p1(), so the stick is
 *        exactly welded to its parent's free end).
 * @param fill polygon fill color; the outline is Qt::NoPen (foreign fills have none).
 * @param z figure-wide depth.
 * @return heap-allocated stick; the caller wires parent/stepchild/children.
 *
 * The foreign fills deform with every joint they reference; a POLYGON stick attaches
 * to ONE joint and moves rigidly with it — the shape is exact at import time, the
 * animation behavior is the (warned) approximation.
 */
StickData *MakePolygonStick(const QVector<QPointF> &verts, const QPointF &anchor,
                            const QColor &fill, double z)
{
    // base line: from the anchor to the first vertex not sitting on it, so the local
    // frame has a usable length; fully degenerate outlines get a unit base line
    QPointF far_vertex = anchor + QPointF(1, 0);
    for (const QPointF &v : verts) {
        if (QLineF(anchor, v).length() > 0.001) {
            far_vertex = v;
            break;
        }
    }
    StickData *stick = new StickData();
    stick->stick_type = StickData::POLYGON;
    stick->my_line = QLineF(anchor, far_vertex);
    stick->img_angle = float(-qAtan2(stick->my_line.dx(), stick->my_line.dy()) * 180.0 / M_PI);
    QPen pen(fill);
    pen.setStyle(Qt::NoPen);
    stick->pen = pen;
    stick->brush = QBrush(fill);
    stick->z = z;
    QTransform inverse = stick->PolygonTransform().inverted();
    for (const QPointF &v : verts)
        stick->polygon_points.append(inverse.map(v));
    return stick;
}

/**
 * @brief Recenters every stick line so the figure's bounding box lands on
 *        `target_center` (foreign formats are origin-relative, plan §4).
 */
void RecenterFigure(StickFigureData *figure, const QPointF &target_center)
{
    if (figure->stick_list.isEmpty())
        return;
    QRectF bounds;
    for (StickData *stick : figure->stick_list) {
        QRectF line_box = QRectF(stick->my_line.p1(), stick->my_line.p2()).normalized();
        bounds = bounds.isNull() ? line_box : bounds.united(line_box);
    }
    QPointF delta = target_center - bounds.center();
    for (StickData *stick : figure->stick_list)
        stick->my_line.translate(delta);
}

/**
 * @brief Sets stick_type/pen/brush/img fields from a modern-STK type byte and color
 *        (plan §4 mapping table). Returns false for unknown type values (caller
 *        counts them for the warning; the stick is already set up as a plain line).
 */
bool ApplySegmentStyle(StickData *stick, int type, const QColor &color, double thickness)
{
    QPen pen(color);
    pen.setWidthF(thickness);
    if (thickness == 0.0)
        pen.setStyle(Qt::NoPen); // invisible handle-carrier segments (e.g. Chuck's root)
    bool known = true;
    switch (type) {
    case 0:
        stick->stick_type = StickData::LINE;
        pen.setCapStyle(Qt::RoundCap);
        break;
    case 6:
        stick->stick_type = StickData::LINE;
        pen.setCapStyle(Qt::SquareCap);
        break;
    case 1:
    case 3:
    case 4:
        stick->stick_type = StickData::CIRCLE;
        stick->img_w_scale = 1;
        stick->img_h_scale = 1;
        stick->brush = (type == 1) ? QBrush(Qt::white)
                     : (type == 3) ? QBrush(color)
                                   : QBrush(Qt::NoBrush);
        break;
    default:
        stick->stick_type = StickData::LINE;
        pen.setCapStyle(Qt::RoundCap);
        known = false;
        break;
    }
    stick->pen = pen;
    if (stick->stick_type == StickData::CIRCLE) {
        QLineF line = stick->my_line;
        stick->img_angle = float(-qAtan2(line.dx(), line.dy()) * 180.0 / M_PI); // StickFigure.cpp:148 convention
    }
    return known;
}

/**
 * @brief Turns the intermediate segment list into a StickFigureData: forward
 *        kinematics for the joints, one StickData per segment, tree pointers with
 *        the stepchild rule for origin-attached segments (plan §4).
 */
StickFigureData *BuildFigure(const QList<StkSegment> &segments,
                             const QList<StkPolyfill> &polyfills, const QString &name,
                             const QPointF &target_center, QStringList *warnings)
{
    QVector<QPointF> joints(segments.count() + 1);
    joints[0] = QPointF(0, 0);
    for (int i = 0; i < segments.count(); i++) {
        const StkSegment &seg = segments[i];
        joints[i + 1] = joints[seg.parent_joint]
                + QPointF(seg.length * qCos(seg.angle), seg.length * qSin(seg.angle));
    }

    StickFigureData *figure = new StickFigureData();
    figure->name = name;
    int unknown_types = 0;
    int static_count = 0;
    for (int i = 0; i < segments.count(); i++) {
        const StkSegment &seg = segments[i];
        StickData *stick = new StickData();
        stick->my_line = QLineF(joints[seg.parent_joint], joints[i + 1]);
        if (!ApplySegmentStyle(stick, seg.type, seg.color, seg.thickness))
            unknown_types++;
        stick->z = seg.draw_position / 100.0;
        if (i == 0) {
            stick->parent = nullptr;
            figure->master_stick = stick;
        } else if (seg.parent_joint == 0) {
            // attached at the origin = at the master's P1: the stepchild rule
            stick->parent = figure->master_stick;
            stick->stepchild = true;
        } else {
            stick->parent = figure->stick_list[seg.parent_joint - 1];
            stick->stepchild = stick->parent->stepchild; // children of a stepchild are stepchildren
        }
        AppendToAncestors(stick);
        figure->stick_list.append(stick);
        if (seg.is_static)
            static_count++;
    }
    figure->current_stick = figure->master_stick;

    // polyfills → POLYGON sticks anchored at their first joint (ParseModernStk has
    // already bounds-checked every joint index against the segment count)
    int poly_imported = 0;
    for (const StkPolyfill &poly : polyfills) {
        if (poly.joints.isEmpty())
            continue;
        QVector<QPointF> verts;
        for (int joint : poly.joints)
            verts.append(joints[joint]);
        int anchor_joint = poly.joints.first();
        StickData *stick = MakePolygonStick(verts, joints[anchor_joint], poly.color,
                                            poly.draw_position / 100.0);
        if (anchor_joint == 0) {
            stick->parent = figure->master_stick; // origin-attached: the stepchild rule
            stick->stepchild = true;
        } else {
            stick->parent = figure->stick_list[anchor_joint - 1];
            stick->stepchild = stick->parent->stepchild;
        }
        AppendToAncestors(stick);
        figure->stick_list.append(stick);
        poly_imported++;
    }
    if (poly_imported > 0)
        warnings->append(QObject::tr("%1 polygon fill(s) imported as polygon sticks (each attached to one joint; it moves rigidly with it instead of deforming).")
                         .arg(poly_imported));

    if (static_count > 0)
        warnings->append(QObject::tr("%1 static segment(s) imported as normal sticks (they will have handles).")
                         .arg(static_count));
    if (unknown_types > 0)
        warnings->append(QObject::tr("%1 segment(s) of unknown type imported as plain sticks.")
                         .arg(unknown_types));
    RecenterFigure(figure, target_center);
    return figure;
}

/**
 * @brief Shared numeric plausibility check for foreign floats (plan §4 defensive rules).
 */
bool SaneSegment(double length, double angle, double thickness)
{
    return qIsFinite(length) && qIsFinite(angle) && qIsFinite(thickness)
            && qAbs(length) <= 1e6 && qAbs(thickness) <= 1e6 && qAbs(angle) <= 1e3
            && length >= 0 && thickness >= 0;
}

// ------------------------------------------------------- modern .stk parsing ----

/**
 * @brief Parses the decompressed payload of a modern (Pivot 3/4/5) .stk file
 *        (plan §2.2-§2.6).
 */
bool ParseModernStk(const QByteArray &payload, QList<StkSegment> *segments,
                    QList<StkPolyfill> *polyfills, QStringList *warnings, QString *error)
{
    ByteCursor cur(payload, false);
    quint8 flags = cur.U8();
    // unknown-layout guard: bits 5/7 must be set (feature absent), bit 6 cleared
    if ((flags & 0x20) == 0 || (flags & 0x80) == 0 || (flags & 0x40) != 0) {
        *error = QObject::tr("Unsupported STK variant (feature flags 0x%1).").arg(flags, 2, 16, QChar('0'));
        return false;
    }
    bool has_type    = !(flags & 0x01);
    bool has_static  = !(flags & 0x02);
    bool has_color   = !(flags & 0x04);
    bool has_opacity = !(flags & 0x08);
    bool has_poly    = !(flags & 0x10);

    qint32 count = cur.I32();
    if (cur.Failed() || count < 1 || count > 65535) {
        *error = QObject::tr("Invalid STK segment count.");
        return false;
    }
    for (int i = 0; i < count; i++) {
        StkSegment seg;
        if (i > 0) {
            seg.parent_joint = cur.U16();
            if (seg.parent_joint > i) { // may only reference earlier segments (or 0 = origin)
                *error = QObject::tr("Corrupt STK file: segment %1 references a later segment.").arg(i + 1);
                return false;
            }
        }
        seg.length = cur.F32();
        seg.angle = cur.F64();
        seg.thickness = cur.F32();
        if (has_type)
            seg.type = cur.U8();
        if (has_static)
            seg.is_static = cur.U8() != 0;
        if (has_color) {
            int r = cur.U8(), g = cur.U8(), b = cur.U8();
            seg.color = QColor(r, g, b);
        }
        if (has_opacity) {
            // 0 = opaque; non-zero = transparency amount (plan §2.3 hypothesis)
            int transparency = cur.U8();
            seg.color.setAlpha(transparency == 0 ? 255 : 255 - transparency);
        }
        seg.draw_position = i;
        if (cur.Failed() || !SaneSegment(seg.length, seg.angle, seg.thickness)) {
            *error = QObject::tr("Corrupt or truncated STK file (segment %1).").arg(i + 1);
            return false;
        }
        segments->append(seg);
    }

    // section A: unknown semantics, fixed entry size — skip (plan §2.4)
    quint16 count_a = cur.U16();
    cur.Skip(qint64(count_a) * 10);

    // section B: polyfills → POLYGON sticks (plan §2.5; BuildFigure does the mapping)
    quint16 poly_count = cur.U16();
    for (int p = 0; p < poly_count && !cur.Failed(); p++) {
        StkPolyfill poly;
        quint16 vert_count = cur.U16();
        int r = cur.U8(), g = cur.U8(), b = cur.U8();
        int transparency = cur.U8(); // 0 = opaque, like the segment opacity byte (§2.3)
        poly.color = QColor(r, g, b, transparency == 0 ? 255 : 255 - transparency);
        for (int v = 0; v < vert_count && !cur.Failed(); v++) {
            int joint = cur.U16();
            if (joint > count) {
                *error = QObject::tr("Corrupt STK file: polygon fill %1 references a missing joint.").arg(p + 1);
                return false;
            }
            poly.joints.append(joint);
        }
        poly.draw_position = count + p; // default: after every segment (overridden by section C)
        polyfills->append(poly);
    }
    if (cur.Failed()) {
        *error = QObject::tr("Corrupt or truncated STK file (polyfill section).");
        return false;
    }

    // section C: draw order permutation, present only with the polyfill feature (plan §2.6)
    if (has_poly) {
        for (int k = 0; k < count + int(poly_count); k++) {
            int id = cur.U16();
            if (id < count)
                (*segments)[id].draw_position = k; // entry k = drawable drawn k-th
            else if (id - count < polyfills->count())
                (*polyfills)[id - count].draw_position = k;
        }
        if (cur.Failed()) {
            *error = QObject::tr("Corrupt or truncated STK file (draw-order section).");
            return false;
        }
    }
    if (!cur.AtEnd())
        warnings->append(QObject::tr("%1 unrecognized trailing byte(s) ignored.").arg(cur.Remaining()));
    return true;
}

// ----------------------------------------------------------- v2 .stk parsing ----

/**
 * @brief Parses a legacy Pivot ≤2.2.5 file: u8 version, u8 count, fixed 24-byte
 *        records (plan §2.8). `raw` is the whole uncompressed file.
 */
bool ParseV2Stk(const QByteArray &raw, QList<StkSegment> *segments, QString *error)
{
    ByteCursor cur(raw, false);
    quint8 version = cur.U8();
    quint8 count = cur.U8();
    if (version != 1 || count < 1 || raw.size() != 2 + 24 * count) {
        *error = QObject::tr("Not a supported STK file (supported: Pivot 2 and Pivot 3/4/5 stickfigures).");
        return false;
    }
    for (int i = 0; i < count; i++) {
        StkSegment seg;
        seg.parent_joint = cur.U8();
        quint16 id = cur.U16();
        quint8 unknown_a = cur.U8();
        seg.length = cur.F32();
        seg.angle = cur.F64();
        seg.thickness = cur.F32();
        quint8 type = cur.U8();
        seg.is_static = cur.U8() != 0;
        quint16 unknown_b = cur.U16();
        if (cur.Failed() || id != i + 1 || seg.parent_joint > i
                || !SaneSegment(seg.length, seg.angle, seg.thickness)) {
            *error = QObject::tr("Corrupt Pivot 2 STK file (segment %1).").arg(i + 1);
            return false;
        }
        Q_UNUSED(unknown_a);
        Q_UNUSED(unknown_b);
        seg.type = (type == 1) ? 1 : 0; // v2 has only stick/circle; map onto the modern values
        seg.draw_position = i;
        segments->append(seg);
    }
    return true;
}

// ------------------------------------------------------------- .stk writing ----

/**
 * @brief One stick resolved to STK terms during export (shared by both variants).
 */
struct ExportSegment
{
    int parent_joint = 0;
    double length = 0;
    double angle = 0;
    double thickness = 0;
    int type = 0;
    QColor color = QColor(0, 0, 0, 255);
};

/**
 * @brief One POLYGON stick resolved to an STK section-B polyfill: the vertices ride as
 *        extra invisible zero-thickness segments (the standard Pivot idiom for solid
 *        shapes) and the fill references their free-end joints.
 */
struct ExportPolyfill
{
    QColor color = QColor(0, 0, 0, 255);
    QList<int> joint_ids;     ///< 1-based segment ids of the vertex carriers, outline order
    int after_file_id = 0;    ///< 0-based file id of the polygon's skeleton segment (section C slot)
};

/**
 * @brief True when a brush is already a single colour, i.e. needs no flattening on export.
 * @param brush the fill (or a pen's paint) to classify.
 * @return true for Qt::SolidPattern and Qt::NoBrush, false for a gradient or a texture.
 *
 * Used only to count how many sticks the export had to simplify, so the user gets one
 * warning instead of silence.
 */
bool IsFlatPaint(const QBrush &brush)
{
    return brush.style() == Qt::NoBrush || brush.style() == Qt::SolidPattern;
}

/**
 * @brief Reduces any brush to the one colour the foreign formats can carry.
 * @param brush the fill (or a pen's paint) to flatten.
 * @return a representative colour: a gradient's FIRST stop, a texture's average pixel, or
 *         the brush's own colour for a solid or a hatch pattern.
 *
 * @details Neither .stk (Pivot) nor .nodes has any notion of a fill beyond a flat colour,
 * so the pattern/gradient/image fills have to be dropped on the way out. They are dropped
 * to something recognizable rather than to QBrush::color(), which for a gradient or a
 * texture brush is an unset default (black) and would turn every gradient-filled figure
 * into a black one. The native formats (.mAnim, .stck, .marlib) keep the real brush — see
 * StickData::brush.
 *
 * ponytail: first stop / box-filter average. Both are approximations of "what colour is
 * this, roughly", which is all the target formats can express; a perceptual average or a
 * mid-stop sample would be more code for a lossy field either way.
 */
QColor FlattenFill(const QBrush &brush)
{
    switch (brush.style()) {
    case Qt::LinearGradientPattern:
    case Qt::RadialGradientPattern:
    case Qt::ConicalGradientPattern: {
        // il brush arriva anche da file: non dare per scontato che il gradiente ci sia
        const QGradient *gradient = brush.gradient();
        if (!gradient)
            return brush.color();
        const QGradientStops stops = gradient->stops();
        return stops.isEmpty() ? brush.color() : stops.first().second;
    }
    case Qt::TexturePattern: {
        const QImage tile = brush.textureImage();
        if (tile.isNull())
            return brush.color();
        // scalare a 1x1 con lo smooth scale di Qt (filtro a media) dà il colore medio
        return QColor::fromRgba(tile.scaled(1, 1, Qt::IgnoreAspectRatio,
                                            Qt::SmoothTransformation).pixel(0, 0));
    }
    default:
        return brush.color(); // tinta unita, retini (Dense*/Cross/...), NoBrush
    }
}

/**
 * @brief Orders the figure's sticks parent-before-child, preferring lower z among the
 *        ready ones (plan §5: preserves draw order without a section C), and resolves
 *        each to STK fields via the §5.1 mapping table.
 */
bool ResolveExportSegments(const StickFigureData &figure, QList<ExportSegment> *segments,
                           QList<ExportPolyfill> *polyfills, QStringList *warnings,
                           QString *error)
{
    const QList<StickData *> &sticks = figure.stick_list;
    StickData *master = figure.master_stick ? figure.master_stick : sticks.first();

    // topological emission order: master first (STK segment 1 IS the origin segment),
    // then repeatedly the lowest-z stick whose parent has been emitted
    QList<StickData *> order;
    QSet<StickData *> ordered;
    QList<StickData *> pending = sticks;
    pending.removeAll(master);
    order.append(master);
    ordered.insert(master);
    while (!pending.isEmpty()) {
        int best = -1;
        for (int i = 0; i < pending.count(); i++) {
            StickData *candidate = pending[i];
            if (candidate->parent && !ordered.contains(candidate->parent))
                continue;
            if (best < 0 || candidate->z < pending[best]->z)
                best = i;
        }
        if (best < 0) { // parent outside the figure: broken tree
            *error = QObject::tr("The figure's stick tree is inconsistent; cannot export.");
            return false;
        }
        StickData *next = pending.takeAt(best);
        ordered.insert(next);
        order.append(next);
    }

    // joint_of[stick] = 1-based id of the segment whose free end carries the stick's
    // children. One segment per stick — except a MULTILINE, which becomes a welded
    // CHAIN of segments (one per path edge, per-segment thickness from the vertex
    // widths); its children hang off the chain's last joint. Filled during emission:
    // the topological order guarantees the parent's entry exists before it is read.
    QHash<StickData *, int> joint_of;

    int approximated = 0;
    int multilines = 0;
    int flattened_paints = 0; // sticks whose pattern/gradient/image paint had to be flattened
    bool curvature_warned = false;
    bool geometry_warned = false;
    bool ellipse_warned = false;
    for (StickData *stick : order) {
        ExportSegment seg;
        QLineF line = stick->my_line;
        seg.length = line.length();
        seg.angle = qAtan2(line.dy(), line.dx());

        // attach joint: master's own stepchildren hang off the origin; everyone else
        // off their parent's free end
        if (stick == master)
            seg.parent_joint = 0;
        else if (!stick->parent || (stick->stepchild && stick->parent == master))
            seg.parent_joint = 0;
        else
            seg.parent_joint = joint_of.value(stick->parent);

        // detached-geometry guard: STK re-welds the joint implicitly, just warn once
        if (stick != master && !geometry_warned) {
            QPointF expected = (seg.parent_joint == 0) ? master->my_line.p1()
                                                       : stick->parent->my_line.p2();
            if (QLineF(expected, line.p1()).length() > 0.5) {
                warnings->append(QObject::tr("Some sticks were not exactly attached to their parent; STK re-welds them."));
                geometry_warned = true;
            }
        }

        // il formato porta un solo colore per segmento: pattern, gradienti e immagini
        // escono appiattiti (e contati, per l'avviso in coda)
        if (!IsFlatPaint(stick->pen.brush()) || !IsFlatPaint(stick->brush))
            flattened_paints++;
        QColor pen_color = FlattenFill(stick->pen.brush());
        QColor brush_color = (stick->brush.style() != Qt::NoBrush) ? FlattenFill(stick->brush)
                                                                  : pen_color;
        double pen_width = (stick->pen.style() == Qt::NoPen) ? 0.0 : stick->pen.widthF();

        // MULTILINE: la catena sostituisce il singolo segmento dello stick
        if (stick->stick_type == StickData::MULTILINE) {
            multilines++;
            QList<QPointF> verts;
            verts << stick->my_line.p1();
            QTransform t = stick->PolygonTransform();
            for (const QPointF &pt : stick->polygon_points)
                verts << t.map(pt);
            verts << stick->my_line.p2();
            for (float c : stick->vertex_curvatures) {
                if (c > 0.001f && !curvature_warned) {
                    warnings->append(QObject::tr("Curved multi-line corners were exported as sharp angles."));
                    curvature_warned = true;
                }
            }
            int previous_joint = seg.parent_joint;
            for (int v = 0; v + 1 < verts.count(); v++) {
                ExportSegment link;
                QLineF edge(verts[v], verts[v + 1]);
                link.parent_joint = previous_joint;
                link.length = edge.length();
                link.angle = qAtan2(edge.dy(), edge.dx());
                link.type = 0;
                link.color = pen_color;
                double width_a = stick->vertex_widths.value(v, pen_width);
                double width_b = stick->vertex_widths.value(v + 1, pen_width);
                link.thickness = (width_a + width_b) / 2.0;
                segments->append(link);
                previous_joint = segments->count(); // il giunto appena creato
            }
            joint_of.insert(stick, previous_joint); // i figli si agganciano in coda alla catena
            continue;
        }

        switch (stick->stick_type) {
        case StickData::LINE:
            seg.type = (stick->pen.capStyle() == Qt::RoundCap) ? 0 : 6;
            seg.thickness = pen_width;
            seg.color = pen_color;
            break;
        case StickData::CIRCLE:
            // brush_color, non brush.color(): un gradiente/tile bianco deve pesare come
            // bianco (tipo 1) e non come il nero di default di QBrush::color()
            seg.type = (stick->brush.style() == Qt::NoBrush) ? 4
                     : (brush_color == QColor(Qt::white)) ? 1 : 3;
            seg.thickness = pen_width;
            seg.color = (seg.type == 3) ? brush_color : pen_color;
            if (!qFuzzyCompare(stick->img_w_scale, 1.0f) && !ellipse_warned) {
                warnings->append(QObject::tr("Squashed circles (ellipses) were exported as plain circles."));
                ellipse_warned = true;
            }
            break;
        case StickData::RECT:
            seg.type = 6;
            seg.thickness = stick->img_w_scale;
            seg.color = brush_color;
            approximated++;
            break;
        case StickData::TAPER:
            seg.type = 0;
            seg.thickness = stick->img_w_scale;
            seg.color = brush_color;
            if (!qFuzzyCompare(stick->img_w_scale, stick->img_h_scale))
                approximated++;
            break;
        case StickData::TRAPEZOID:
            seg.type = 6;
            seg.thickness = (stick->img_w_scale + stick->img_h_scale) / 2.0;
            seg.color = brush_color;
            approximated++;
            break;
        case StickData::POLYGON:
            // lo scheletro esporta invisibile; i vertici viaggiano nella polyfill (sotto)
            seg.type = 0;
            seg.thickness = 0;
            seg.color = pen_color;
            break;
        case StickData::IMAGE:
        case StickData::TEXT: // il formato non ha testo: esce come stick normale (spec)
        default:
            seg.type = 0;
            seg.thickness = pen_width;
            seg.color = pen_color;
            approximated++;
            break;
        }
        segments->append(seg);
        joint_of.insert(stick, segments->count());
    }
    if (multilines > 0)
        warnings->append(QObject::tr("%1 multi-line stick(s) exported as chains of welded segments.")
                         .arg(multilines));
    if (flattened_paints > 0)
        warnings->append(QObject::tr("%1 stick(s): pattern/gradient/image fill or stroke exported as a flat color.")
                         .arg(flattened_paints));

    // POLYGON sticks → one invisible carrier segment per vertex + a section-B polyfill
    // referencing their joints; the Pivot 2 caller passes no polyfill list (the format
    // has no fills) and gets the count as a warning instead
    int poly_count = 0;
    for (StickData *stick : order) {
        if (stick->stick_type != StickData::POLYGON || stick->polygon_points.isEmpty())
            continue;
        poly_count++;
        if (!polyfills)
            continue;
        ExportPolyfill poly;
        poly.color = FlattenFill((stick->brush.style() != Qt::NoBrush) ? stick->brush
                                                                      : stick->pen.brush());
        poly.after_file_id = joint_of.value(stick) - 1;
        QTransform t = stick->PolygonTransform();
        QPointF base = stick->my_line.p2(); // il joint dello scheletro porta i carrier
        for (const QPointF &pt : stick->polygon_points) {
            QLineF to_vertex(base, t.map(pt));
            ExportSegment carrier;
            carrier.parent_joint = joint_of.value(stick);
            carrier.length = to_vertex.length();
            carrier.angle = qAtan2(to_vertex.dy(), to_vertex.dx());
            carrier.thickness = 0; // invisibile: esiste solo per creare il joint
            carrier.type = 0;
            segments->append(carrier);
            poly.joint_ids.append(segments->count()); // 1-based: l'id del carrier appena scritto
        }
        polyfills->append(poly);
    }
    if (poly_count > 0) {
        if (polyfills)
            warnings->append(QObject::tr("%1 polygon stick(s) exported as a polygon fill plus invisible vertex segments.")
                             .arg(poly_count));
        else
            warnings->append(QObject::tr("Polygon fills are not supported by the Pivot 2 format; %1 polygon stick(s) exported as invisible segments.")
                             .arg(poly_count));
    }

    if (approximated > 0)
        warnings->append(QObject::tr("%1 stick(s) (images/rectangles/tapers) exported as plain Pivot segments.")
                         .arg(approximated));
    return true;
}

/**
 * @brief Serializes the resolved segments as a modern .stk payload with minimized
 *        feature flags, exactly the byte patterns Pivot itself writes (plan §5.1).
 */
QByteArray WriteModernPayload(const QList<ExportSegment> &segments,
                              const QList<ExportPolyfill> &polyfills)
{
    bool any_type = false, any_color = false, any_alpha = false;
    for (const ExportSegment &seg : segments) {
        any_type |= seg.type != 0;
        any_color |= seg.color.red() || seg.color.green() || seg.color.blue();
        any_alpha |= seg.color.alpha() != 255;
    }
    quint8 flags = 0xBF;
    if (any_type)  flags &= ~0x01;
    if (any_color) flags &= ~0x04;
    if (any_alpha) flags &= ~0x08;
    if (!polyfills.isEmpty()) flags &= ~0x10; // polyfill feature ⇒ section C required

    ByteWriter writer;
    writer.U8(flags);
    writer.I32(segments.count());
    for (int i = 0; i < segments.count(); i++) {
        const ExportSegment &seg = segments[i];
        if (i > 0)
            writer.U16(quint16(seg.parent_joint));
        writer.F32(float(seg.length));
        writer.F64(seg.angle);
        writer.F32(float(seg.thickness));
        if (any_type)
            writer.U8(quint8(seg.type));
        if (any_color) {
            writer.U8(quint8(seg.color.red()));
            writer.U8(quint8(seg.color.green()));
            writer.U8(quint8(seg.color.blue()));
        }
        if (any_alpha)
            writer.U8(quint8(255 - seg.color.alpha()));
    }
    writer.U16(0); // section A: always empty on export (plan §2.4)
    writer.U16(quint16(polyfills.count()));
    for (const ExportPolyfill &poly : polyfills) {
        writer.U16(quint16(poly.joint_ids.count()));
        writer.U8(quint8(poly.color.red()));
        writer.U8(quint8(poly.color.green()));
        writer.U8(quint8(poly.color.blue()));
        writer.U8(quint8(255 - poly.color.alpha())); // 0 = opaque (§2.3/§2.5)
        for (int joint : poly.joint_ids)
            writer.U16(quint16(joint));
    }
    if (!polyfills.isEmpty()) {
        // section C: segments keep file order, each fill slots in right after its own
        // skeleton segment so it draws under everything drawn later (matches the local
        // render, where the skeleton itself is invisible)
        QList<int> sequence;
        for (int id = 0; id < segments.count(); id++) {
            sequence.append(id);
            for (int p = 0; p < polyfills.count(); p++) {
                if (polyfills[p].after_file_id == id)
                    sequence.append(segments.count() + p);
            }
        }
        for (int id : sequence)
            writer.U16(quint16(id));
    }
    return writer.out;
}

/**
 * @brief Serializes the resolved segments in the legacy Pivot 2 layout (plan §5.2).
 */
QByteArray WriteV2Payload(const QList<ExportSegment> &segments, QStringList *warnings)
{
    bool any_color = false, any_shape = false, any_invisible = false;
    ByteWriter writer;
    writer.U8(1);
    writer.U8(quint8(segments.count()));
    for (int i = 0; i < segments.count(); i++) {
        const ExportSegment &seg = segments[i];
        any_color |= seg.color.red() || seg.color.green() || seg.color.blue()
                || seg.color.alpha() != 255;
        any_shape |= (seg.type != 0 && seg.type != 1 && seg.type != 3 && seg.type != 4);
        any_invisible |= seg.thickness == 0.0;
        bool is_circle = (seg.type == 1 || seg.type == 3 || seg.type == 4);
        writer.U8(quint8(seg.parent_joint));
        writer.U16(quint16(i + 1));
        writer.U8(0);
        writer.F32(float(seg.length));
        double angle = std::fmod(seg.angle, 2 * M_PI);
        if (angle < 0)
            angle += 2 * M_PI;
        writer.F64(angle);
        writer.F32(float(seg.thickness));
        writer.U8(is_circle ? 1 : 0);
        writer.U8(0); // static: no local concept yet (plan §9.1)
        writer.U16(0);
    }
    if (any_color)
        warnings->append(QObject::tr("Colors are not supported by the Pivot 2 format and were discarded."));
    if (any_shape)
        warnings->append(QObject::tr("Square end caps are not supported by the Pivot 2 format."));
    if (any_invisible)
        warnings->append(QObject::tr("Invisible (zero-thickness) sticks may not be supported by Pivot 2 / Stick Nodes."));
    return writer.out;
}

// ----------------------------------------------------------- .nodes parsing ----

/**
 * @brief The subset of a Stick Nodes node this importer maps; everything else in the
 *        record is consumed at its exact size and dropped (plan §2.9 field table).
 */
struct NodeFields
{
    int type = 0;
    qint32 draw_index = 0;
    bool is_static = false;
    bool is_stretchy = false;
    bool use_color = false;
    bool use_outline = false;
    bool hollow = false;
    bool gradient = false;
    float local_x = 0, local_y = 0;
    qint32 thickness = 0;
    qint32 curve = 0;
    bool angle_locked = false;
    float trap_start = 0, trap_end = 0;
    bool trap_explicit = false;
    bool trap_rounded = false;
    float trap_ratio = 1.0f;
    qint16 poly_vertices = 0;
    QColor color;
    QColor outline_color;
    qint32 child_count = 0;
    QList<bool> connector_flags;
};

QColor ReadColorAbgr(ByteCursor &cur)
{
    int a = cur.U8(), b = cur.U8(), g = cur.U8(), r = cur.U8();
    return QColor(r, g, b, a);
}

/**
 * @brief Reads one node record honoring every version/build gate (plan §2.9,
 *        transcribed from sticknodes-rs read.rs).
 */
NodeFields ReadNode(ByteCursor &cur, qint32 ver, qint32 build, bool is_connector)
{
    NodeFields n;
    if (is_connector) { // only reachable when ver>=403 && build>38
        cur.F32(); cur.F32(); cur.F32();
        if (build >= 44) cur.F32();
        cur.F32(); cur.I32(); cur.U8();
        if (build >= 65) cur.F32();
    }
    n.type = qint8(cur.U8());
    n.draw_index = cur.I32();
    n.is_static = cur.U8() != 0;
    n.is_stretchy = cur.U8() != 0;
    if (ver >= 403 && build >= 48) cur.U8();            // is_floaty
    if (ver >= 248) cur.U8();                           // is_smart_stretch
    if (ver >= 252) cur.U8();                           // do_not_apply_smart_stretch
    if (ver >= 403 && build >= 50) cur.U8();            // smart_stretch_reset_impulse
    n.use_color = cur.U8() != 0;
    if (ver >= 256) n.use_outline = cur.U8() != 0;
    if (ver >= 403 && build >= 21) n.hollow = cur.U8() != 0;
    if (ver >= 176) { n.gradient = cur.U8() != 0; cur.U8(); }
    if (ver >= 403 && build >= 20) cur.I16();           // gradient_mode
    cur.U8();                                           // use_segment_scale
    n.local_x = cur.F32();
    n.local_y = cur.F32();
    cur.F32();                                          // scale
    cur.F32();                                          // default_length
    cur.F32();                                          // length (redundant with local_x/y)
    cur.I32();                                          // default_thickness
    n.thickness = cur.I32();
    if (ver >= 320) n.curve = cur.I32();
    if (ver >= 403 && build >= 20) cur.U8();            // curve_circulization
    if (ver >= 403 && build >= 21) cur.I16();           // curve polyfill precision
    if (ver >= 256) { cur.U8(); cur.I16(); }            // half_arc, right_triangle_direction
    if (ver >= 300) cur.U8();                           // triangle_upside_down
    if (ver >= 403 && build >= 36) {
        n.trap_explicit = true;
        if (build < 64) { n.trap_start = float(cur.I32()); n.trap_end = float(cur.I32()); }
        else            { n.trap_start = cur.F32();        n.trap_end = cur.F32(); }
    }
    if (ver >= 403 && build == 36) { cur.I32(); cur.I32(); cur.U8(); cur.U8(); }
    if (ver >= 256 && build != 36) n.trap_ratio = cur.F32();
    if (ver >= 403 && build >= 36)
        n.trap_rounded = (cur.U8() != 0) | (cur.U8() != 0);
    if (ver >= 256) n.poly_vertices = cur.I16();        // num_polygon_vertices
    if (ver >= 248) cur.F32();                          // default_local_angle
    cur.F32();                                          // local_angle (redundant)
    if (ver >= 248) cur.F32();                          // default_angle
    n.color = ReadColorAbgr(cur);
    if (ver >= 176) cur.Skip(4);                        // gradient_color
    if (ver >= 256) n.outline_color = ReadColorAbgr(cur);
    if (ver >= 403 && build >= 39) n.angle_locked = cur.U8() != 0;
    if (ver >= 403 && build >= 39 && build <= 50) cur.F32();
    if (ver >= 403 && build >= 51) cur.U8();            // angle_lock_is_main_node
    if (ver >= 403 && build >= 51 && build <= 56) { cur.F32(); cur.F32(); }
    if (ver >= 403 && build >= 57) cur.F32();           // angle_lock_offset
    if (ver >= 403 && build >= 63) cur.F32();           // angle_lock_relative_start
    if (ver >= 403 && build >= 67) cur.F32();           // angle_lock_stickfigure_start
    if (ver >= 403 && build >= 63) cur.U8();            // angle_lock_relative_multiplier (i8)
    if (ver >= 403 && build >= 39) {
        if (build <= 40) cur.I16(); else cur.U8();      // is_drag_locked
    }
    if (ver >= 403 && build >= 41 && build <= 45) cur.I16();
    if (ver >= 403 && build >= 46) cur.F32();           // drag_lock_angle
    if (ver >= 403 && build >= 41) cur.F32();           // smart_stretch_multiplier
    if (ver >= 403 && build >= 41 && build <= 45) cur.U8();
    n.child_count = cur.I32();
    if (ver >= 403 && build > 38) {
        for (qint32 i = 0; i < n.child_count && !cur.Failed(); i++)
            n.connector_flags.append(cur.I32() != 0);
    }
    return n;
}

/**
 * @brief Maps one Stick Nodes node onto a StickData's type/pen/brush fields
 *        (plan §4b type table). `effective` is the color after the
 *        use_segment_color/figure-color resolution, already scaled thickness.
 */
void ApplyNodeStyle(StickData *stick, const NodeFields &node, const QColor &effective,
                    double thickness, int *approximations)
{
    QPen pen(effective);
    pen.setWidthF(thickness);
    if (thickness == 0.0)
        pen.setStyle(Qt::NoPen);
    stick->pen = pen;
    QLineF line = stick->my_line;
    switch (node.type) {
    case 0: // rounded segment
        stick->stick_type = StickData::LINE;
        stick->pen.setCapStyle(Qt::RoundCap);
        return;
    case 1: // flat-capped segment
        stick->stick_type = StickData::LINE;
        stick->pen.setCapStyle(Qt::SquareCap);
        return;
    case 2: // circle (outline)
    case 4: // filled circle
    case 5: // ellipse (lateral scale unknown pre-403: approximate as circle)
        stick->stick_type = StickData::CIRCLE;
        stick->img_w_scale = 1;
        stick->img_h_scale = 1;
        stick->img_angle = float(-qAtan2(line.dx(), line.dy()) * 180.0 / M_PI);
        if (node.type == 4 && !node.hollow)
            stick->brush = QBrush(effective);
        else
            stick->brush = QBrush(Qt::NoBrush);
        if (node.use_outline && node.outline_color.isValid())
            stick->pen.setColor(node.outline_color);
        if (node.type == 5)
            (*approximations)++;
        return;
    case 3: // triangle -> trapezoid with a zero-width tip
    case 6: // trapezoid
        stick->stick_type = (node.type == 6 && node.trap_rounded) ? StickData::TAPER
                                                                  : StickData::TRAPEZOID;
        stick->img_angle = float(-qAtan2(line.dx(), line.dy()) * 180.0 / M_PI);
        if (node.type == 3) {
            stick->img_w_scale = float(thickness);
            stick->img_h_scale = 0;
        } else if (node.trap_explicit) {
            stick->img_w_scale = node.trap_start;
            stick->img_h_scale = node.trap_end;
        } else {
            stick->img_w_scale = float(thickness);
            stick->img_h_scale = float(thickness) * node.trap_ratio;
        }
        stick->brush = QBrush(effective);
        (*approximations)++;
        return;
    case 7: // polygon node: a regular N-gon; assumed inscribed in the circle whose
            // diameter is the segment (the circle-node convention), first vertex at P2
        if (node.poly_vertices >= 3) {
            stick->stick_type = StickData::POLYGON;
            stick->img_angle = float(-qAtan2(line.dx(), line.dy()) * 180.0 / M_PI);
            stick->brush = QBrush(effective);
            // normalized local frame: +y along the stick, so the center sits at (0, 0.5)
            for (int k = 0; k < node.poly_vertices; k++) {
                double phi = 2.0 * M_PI * k / node.poly_vertices;
                stick->polygon_points.append(QPointF(0.5 * qSin(phi), 0.5 + 0.5 * qCos(phi)));
            }
            (*approximations)++; // orientation/size convention is a documented assumption
            return;
        }
        Q_FALLTHROUGH();
    default:
        stick->stick_type = StickData::LINE;
        stick->pen.setCapStyle(Qt::RoundCap);
        (*approximations)++;
        return;
    }
}

// ----------------------------------------------------------- .nodes writing ----

// the one dialect ExportNodes() emits: the newest layout the importer understands
const qint32 kNodesExportVersion = 403;
const qint32 kNodesExportBuild = 72;

void WriteColorAbgr(ByteWriter &writer, const QColor &color)
{
    writer.U8(quint8(color.alpha()));
    writer.U8(quint8(color.blue()));
    writer.U8(quint8(color.green()));
    writer.U8(quint8(color.red()));
}

/**
 * @brief One stick resolved to .nodes terms before serialization.
 */
struct NodeExport
{
    StickData *stick = nullptr;
    int type = 0;                 ///< .nodes node_type
    qint32 draw_index = 0;
    double global_angle_deg = 0;  ///< direction of the segment, degrees, y-down
    double local_angle_deg = 0;   ///< global minus the parent node's global
    float local_x = 0, local_y = 0;
    double length = 0;
    qint32 thickness = 0;
    float trap_start = 0, trap_end = 0, trap_ratio = 1.0f;
    bool trap_rounded_start = false, trap_rounded_end = false;
    QColor color;                 ///< main node color
    QColor outline_color;         ///< circle outline
    bool hollow = false;
    QList<int> children;          ///< indices into the NodeExport list
};

double NormalizeDegrees(double degrees)
{
    while (degrees > 180.0)
        degrees -= 360.0;
    while (degrees <= -180.0)
        degrees += 360.0;
    return degrees;
}

/**
 * @brief Serializes one v403/b72 node record — every field of that build, in the
 *        exact stream order of §2.9, with neutral values for the features
 *        MarionettaPlus has no data for (no statics/stretch/curves/locks/gradients;
 *        multipliers written as their NEUTRAL value, not 0, so a reader that applies
 *        them anyway cannot zero the figure out).
 */
void WriteNodeRecord(ByteWriter &w, const NodeExport &node)
{
    w.U8(quint8(qint8(node.type)));
    w.I32(node.draw_index);
    w.U8(0);                                  // is_static
    w.U8(0);                                  // is_stretchy
    w.U8(0);                                  // is_floaty            (b>=48)
    w.U8(0);                                  // is_smart_stretch     (v>=248)
    w.U8(0);                                  // do_not_apply_smart_stretch (v>=252)
    w.U8(0);                                  // smart_stretch_reset_impulse (b>=50)
    w.U8(1);                                  // use_segment_color: color always explicit
    w.U8(node.stick && node.stick->stick_type == StickData::CIRCLE ? 1 : 0); // use_circle_outline (v>=256)
    w.U8(node.hollow ? 1 : 0);                // circle_is_hollow     (b>=21)
    w.U8(0); w.U8(0);                         // use_gradient, reverse_gradient (v>=176)
    w.I16(0);                                 // gradient_mode        (b>=20)
    w.U8(0);                                  // use_segment_scale
    w.F32(node.local_x);
    w.F32(node.local_y);
    w.F32(1.0f);                              // scale (unused: use_segment_scale = 0)
    w.F32(float(node.length));                // default_length
    w.F32(float(node.length));                // length
    w.I32(node.thickness);                    // default_thickness
    w.I32(node.thickness);                    // thickness
    w.I32(0);                                 // segment_curve_radius (v>=320)
    w.U8(0);                                  // curve_circulization  (b>=20)
    w.I16(0);                                 // curve polyfill precision (b>=21)
    w.U8(0);                                  // half_arc             (v>=256)
    w.I16(0);                                 // right_triangle_direction
    w.U8(0);                                  // triangle_upside_down (v>=300)
    w.F32(node.trap_start);                   // trapezoid start width (b>=64: f32)
    w.F32(node.trap_end);
    w.F32(node.trap_ratio);                   // trapezoid_top_thickness_ratio (v>=256)
    w.U8(node.trap_rounded_start ? 1 : 0);    // trapezoid_is_rounded_start (b>=36)
    w.U8(node.trap_rounded_end ? 1 : 0);
    w.I16(0);                                 // num_polygon_vertices (v>=256)
    w.F32(float(node.local_angle_deg));       // default_local_angle  (v>=248)
    w.F32(float(node.local_angle_deg));       // local_angle
    w.F32(float(node.global_angle_deg));      // default_angle        (v>=248)
    WriteColorAbgr(w, node.color);
    WriteColorAbgr(w, node.color);            // gradient_color: same, gradients off (v>=176)
    WriteColorAbgr(w, node.outline_color);    // circle_outline_color (v>=256)
    w.U8(0);                                  // is_angle_locked      (b>=39)
    w.U8(0);                                  // angle_lock_is_main_node (b>=51)
    w.F32(0.0f);                              // angle_lock_offset    (b>=57)
    w.F32(0.0f);                              // angle_lock_relative_start (b>=63)
    w.F32(0.0f);                              // angle_lock_stickfigure_start (b>=67)
    w.U8(1);                                  // angle_lock_relative_multiplier (b>=63): neutral 1
    w.U8(0);                                  // is_drag_locked       (b>=39)
    w.F32(0.0f);                              // drag_lock_angle      (b>=46)
    w.F32(1.0f);                              // smart_stretch_multiplier (b>=41): neutral 1
    w.I32(node.children.count());
    for (int i = 0; i < node.children.count(); i++)
        w.I32(0);                             // connector flags, one per child (b>38)
}

} // namespace

// =================================================================== imports ====

StickFigureData *StkFormat::ImportStk(const QString &path, const QPointF &target_center,
                                      QStringList *warnings, QString *error)
{
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly)) {
        *error = QObject::tr("Cannot open %1.").arg(path);
        return nullptr;
    }
    QByteArray raw = file.readAll();
    file.close();
    if (raw.size() < 2) {
        *error = QObject::tr("File is empty or truncated.");
        return nullptr;
    }

    QList<StkSegment> segments;
    QList<StkPolyfill> polyfills; // stays empty for v2 (the format has no fills)
    if (quint8(raw.at(0)) == 0x7A) { // modern: 'z' marker + zlib payload
        QByteArray payload = DecompressZlib(raw.mid(1));
        if (payload.isEmpty()) {
            *error = QObject::tr("Not a supported STK file (corrupt compressed data).");
            return nullptr;
        }
        if (!ParseModernStk(payload, &segments, &polyfills, warnings, error))
            return nullptr;
    } else {
        if (!ParseV2Stk(raw, &segments, error))
            return nullptr;
    }
    return BuildFigure(segments, polyfills, QFileInfo(path).completeBaseName(), target_center, warnings);
}

StickFigureData *StkFormat::ImportNodes(const QString &path, const QPointF &target_center,
                                        QStringList *warnings, QString *error)
{
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly)) {
        *error = QObject::tr("Cannot open %1.").arg(path);
        return nullptr;
    }
    QByteArray raw = file.readAll();
    file.close();
    if (raw.size() < 12) {
        *error = QObject::tr("File is empty or truncated.");
        return nullptr;
    }

    // container detection (plan §2.9): plausible big-endian version => raw BE file;
    // otherwise the whole file is a zlib stream read little-endian
    qint32 version = qFromBigEndian<qint32>(reinterpret_cast<const uchar *>(raw.constData()));
    QByteArray payload = raw;
    bool big_endian = true;
    if (version < 160 || version > 403) {
        payload = DecompressZlib(raw);
        big_endian = false;
        if (payload.size() < 12) {
            *error = QObject::tr("Not a supported .nodes file.");
            return nullptr;
        }
    }

    ByteCursor cur(payload, big_endian);
    version = cur.I32();
    qint32 build = 0;
    if (version >= 403)
        build = cur.I32();
    if (version < 160 || version > 403 || (version == 403 && build > 72)) {
        *error = QObject::tr("This .nodes file was made with a newer Stick Nodes version (v%1 build %2) than supported.")
                 .arg(version).arg(build);
        return nullptr;
    }
    float figure_scale = cur.F32();
    QColor figure_color = ReadColorAbgr(cur);
    if (cur.Failed() || !qIsFinite(figure_scale) || figure_scale <= 0 || figure_scale > 1000) {
        *error = QObject::tr("Corrupt .nodes file (header).");
        return nullptr;
    }

    // iterative depth-first walk (explicit stack: a corrupt child_count must not be
    // able to overflow the C++ stack)
    struct PendingChildren {
        int parent_slot;          // index into positions/sticks; 0 = root
        qint32 remaining;
        QList<bool> connector_flags;
        int consumed = 0;
    };
    const int node_budget = 65535;
    StickFigureData *figure = new StickFigureData();
    figure->name = QFileInfo(path).completeBaseName();
    QVector<QPointF> positions;   // slot 0 = root joint
    QVector<StickData *> sticks;  // parallel to positions; nullptr for the root
    QHash<qint32, int> slot_of_draw_index; // the polyfill trailer references nodes by draw index
    int static_count = 0, stretchy_count = 0, curved_count = 0, gradient_count = 0;
    int locked_count = 0, connector_count = 0, approximations = 0;

    NodeFields root = ReadNode(cur, version, build, false);
    if (cur.Failed() || root.child_count < 0 || root.child_count > node_budget) {
        *error = QObject::tr("Corrupt .nodes file (root node).");
        delete figure;
        return nullptr;
    }
    positions.append(QPointF(0, 0));
    sticks.append(nullptr);
    slot_of_draw_index.insert(root.draw_index, 0);
    QList<PendingChildren> stack;
    stack.append({0, root.child_count, root.connector_flags});

    while (!stack.isEmpty()) {
        PendingChildren &top = stack.last();
        if (top.remaining == 0) {
            stack.removeLast();
            continue;
        }
        bool is_connector = top.connector_flags.value(top.consumed, false);
        int parent_slot = top.parent_slot;
        top.remaining--;
        top.consumed++;
        if (is_connector)
            connector_count++;

        NodeFields node = ReadNode(cur, version, build, is_connector);
        if (cur.Failed() || node.child_count < 0 || positions.count() + node.child_count > node_budget
                || !qIsFinite(node.local_x) || !qIsFinite(node.local_y)
                || qAbs(node.local_x) > 1e6 || qAbs(node.local_y) > 1e6) {
            *error = QObject::tr("Corrupt or truncated .nodes file (node %1).").arg(positions.count());
            delete figure;
            return nullptr;
        }

        QPointF pos = positions[parent_slot]
                + QPointF(node.local_x * figure_scale, node.local_y * figure_scale);
        StickData *stick = new StickData();
        stick->my_line = QLineF(positions[parent_slot], pos);
        QColor effective = node.use_color ? node.color : figure_color;
        ApplyNodeStyle(stick, node, effective, double(node.thickness) * figure_scale,
                       &approximations);
        stick->z = node.draw_index / 100.0;
        StickData *parent_stick = sticks[parent_slot];
        if (!parent_stick && figure->master_stick == nullptr) {
            stick->parent = nullptr; // the root's first child is the master
            figure->master_stick = stick;
        } else if (!parent_stick) {
            stick->parent = figure->master_stick; // other root children: origin-attached
            stick->stepchild = true;
        } else {
            stick->parent = parent_stick;
            stick->stepchild = parent_stick->stepchild;
        }
        AppendToAncestors(stick);
        figure->stick_list.append(stick);
        positions.append(pos);
        sticks.append(stick);
        slot_of_draw_index.insert(node.draw_index, positions.count() - 1);
        if (node.is_static) static_count++;
        if (node.is_stretchy) stretchy_count++;
        if (node.curve != 0) curved_count++;
        if (node.gradient) gradient_count++;
        if (node.angle_locked) locked_count++;

        stack.append({int(positions.count()) - 1, node.child_count, node.connector_flags});
    }

    if (figure->stick_list.isEmpty()) {
        *error = QObject::tr("The .nodes file contains no segments.");
        delete figure;
        return nullptr;
    }
    figure->current_stick = figure->master_stick;

    // trailing sections: polyfills (v>=230) → POLYGON sticks; connector endpoints have
    // no local equivalent. Still parsed TOLERANTLY (plan §2.9: whether compressed files
    // carry these sections is only partially verified): any inconsistency stops the
    // section with a warning and keeps the figure imported so far.
    if (version >= 230 && !cur.AtEnd()) {
        qint32 poly_count = cur.I32();
        int poly_imported = 0, poly_skipped = 0;
        if (!cur.Failed() && poly_count >= 0 && poly_count <= 100000) {
            for (qint32 p = 0; p < poly_count && !cur.Failed(); p++) {
                qint32 anchor_draw = cur.I32();
                QColor color = ReadColorAbgr(cur);
                bool use_polyfill_color = cur.U8() != 0;
                qint32 vert_count = cur.I32();
                if (vert_count < 0 || vert_count > node_budget)
                    break; // inconsistent record: stop the section, keep the figure
                QList<int> vert_slots;
                bool resolved = true;
                for (qint32 v = 0; v < vert_count && !cur.Failed(); v++) {
                    int slot = slot_of_draw_index.value(cur.I32(), -1);
                    if (slot < 0)
                        resolved = false; // reference to a node we never saw
                    else
                        vert_slots.append(slot);
                }
                int anchor_slot = slot_of_draw_index.value(anchor_draw, -1);
                if (cur.Failed() || !resolved || anchor_slot < 0 || vert_slots.isEmpty()) {
                    poly_skipped++;
                    continue;
                }
                QVector<QPointF> verts;
                for (int slot : vert_slots)
                    verts.append(positions[slot]);
                // draw z: just behind the anchor node, so the fill sits under its lines
                StickData *anchor_stick = sticks[anchor_slot];
                double z = (anchor_stick ? anchor_stick->z : 0.0) - 0.005;
                StickData *stick = MakePolygonStick(verts, positions[anchor_slot],
                                                    use_polyfill_color ? color : figure_color, z);
                if (anchor_slot == 0) {
                    stick->parent = figure->master_stick;
                    stick->stepchild = true;
                } else {
                    stick->parent = anchor_stick;
                    stick->stepchild = anchor_stick->stepchild;
                }
                AppendToAncestors(stick);
                figure->stick_list.append(stick);
                poly_imported++;
            }
            if (poly_imported > 0)
                warnings->append(QObject::tr("%1 polygon fill(s) imported as polygon sticks (each attached to one joint; it moves rigidly with it instead of deforming).")
                                 .arg(poly_imported));
            if (poly_skipped > 0)
                warnings->append(QObject::tr("%1 polygon fill(s) skipped (unresolvable node references).").arg(poly_skipped));
        }
    }
    if (!cur.AtEnd() && !cur.Failed())
        warnings->append(QObject::tr("Trailing data (connectors or newer sections) ignored."));

    if (static_count > 0)
        warnings->append(QObject::tr("%1 static segment(s) imported as normal sticks (they will have handles).").arg(static_count));
    if (stretchy_count > 0)
        warnings->append(QObject::tr("%1 stretchy segment(s) imported as rigid sticks.").arg(stretchy_count));
    if (curved_count > 0)
        warnings->append(QObject::tr("%1 curved segment(s) straightened.").arg(curved_count));
    if (gradient_count > 0)
        warnings->append(QObject::tr("%1 gradient(s) flattened to a single color.").arg(gradient_count));
    if (locked_count > 0)
        warnings->append(QObject::tr("%1 angle/drag lock(s) ignored.").arg(locked_count));
    if (connector_count > 0)
        warnings->append(QObject::tr("%1 connector(s) imported as normal sticks.").arg(connector_count));
    if (approximations > 0)
        warnings->append(QObject::tr("%1 shape segment(s) (triangles/trapezoids/ellipses/polygons) approximated.").arg(approximations));

    RecenterFigure(figure, target_center);
    return figure;
}

// ==================================================================== export ====

bool StkFormat::ExportStk(const QString &path, const StickFigureData &figure,
                          StkVariant variant, QStringList *warnings, QString *error)
{
    if (figure.stick_list.isEmpty()) {
        *error = QObject::tr("The figure has no sticks.");
        return false;
    }
    if (figure.stick_list.count() > 65535
            || (variant == PIVOT2 && figure.stick_list.count() > 255)) {
        *error = (variant == PIVOT2)
                ? QObject::tr("The Pivot 2 format supports at most 255 sticks; use the Pivot 5 format instead.")
                : QObject::tr("Too many sticks for the STK format (max 65535).");
        return false;
    }

    QList<ExportSegment> segments;
    QList<ExportPolyfill> polyfills;
    // PIVOT2 has no fills: no polyfill list ⇒ ResolveExportSegments degrades + warns
    if (!ResolveExportSegments(figure, &segments,
                               variant == PIVOT5 ? &polyfills : nullptr, warnings, error))
        return false;
    // i carrier dei vertici si aggiungono DOPO la guardia sul conteggio degli stick
    if (segments.count() > 65535 || (variant == PIVOT2 && segments.count() > 255)) {
        *error = QObject::tr("Too many segments for the STK format once polygon vertices are counted.");
        return false;
    }

    QByteArray bytes;
    if (variant == PIVOT5) {
        bytes.append(char(0x7A));
        bytes.append(CompressZlib(WriteModernPayload(segments, polyfills)));
    } else {
        bytes = WriteV2Payload(segments, warnings);
    }

    QFile file(path);
    if (!file.open(QIODevice::WriteOnly)) {
        *error = QObject::tr("Cannot write %1.").arg(path);
        return false;
    }
    if (file.write(bytes) != bytes.size()) {
        *error = QObject::tr("Write failed on %1.").arg(path);
        file.close();
        return false;
    }
    file.close();
    return true;
}

bool StkFormat::ExportNodes(const QString &path, const StickFigureData &figure,
                            QStringList *warnings, QString *error)
{
    const QList<StickData *> &sticks = figure.stick_list;
    if (sticks.isEmpty()) {
        *error = QObject::tr("The figure has no sticks.");
        return false;
    }
    if (sticks.count() > 65535) {
        *error = QObject::tr("Too many sticks for the .nodes exporter (max 65535).");
        return false;
    }
    StickData *master = figure.master_stick ? figure.master_stick : sticks.first();

    // draw_order_index: the figure-wide draw sequence, from z (list position breaks ties);
    // the root joint takes index 0, sticks get 1..n
    QList<int> by_z;
    for (int i = 0; i < sticks.count(); i++)
        by_z.append(i);
    std::sort(by_z.begin(), by_z.end(), [&sticks](int a, int b) {
        if (sticks[a]->z != sticks[b]->z)
            return sticks[a]->z < sticks[b]->z;
        return a < b;
    });
    QVector<qint32> draw_rank(sticks.count());
    for (int pos = 0; pos < by_z.count(); pos++)
        draw_rank[by_z[pos]] = pos + 1;

    // node 0 = the root joint (a pure origin, sitting at the master's P1)
    QList<NodeExport> nodes;
    NodeExport root;
    root.type = -1;
    root.color = QColor(0, 0, 0, 255);
    root.outline_color = root.color;
    nodes.append(root);
    QList<int> parent_node; // parent-node index of each emitted node, root included
    parent_node.append(0);

    // pre-pass: allocate node slots — one per stick, except a MULTILINE which becomes
    // a CHAIN of nodes (one per path edge); children attach to the chain's LAST node.
    // The allocation must exist up front because stick_list order doesn't guarantee
    // parent-before-child.
    QHash<StickData *, int> first_node_of, last_node_of;
    {
        int next_slot = 1;
        for (StickData *stick : sticks) {
            int expansion = (stick->stick_type == StickData::MULTILINE
                             && !stick->polygon_points.isEmpty())
                    ? stick->polygon_points.count() + 1 : 1;
            first_node_of.insert(stick, next_slot);
            last_node_of.insert(stick, next_slot + expansion - 1);
            next_slot += expansion;
        }
    }
    qint32 next_draw = sticks.count() + 1; // draw indices oltre il root (0) e gli stick (1..n)

    QPointF origin = master->my_line.p1();
    int images = 0, ellipses = 0, subpixel = 0, multilines = 0;
    int flattened_paints = 0; // come in ResolveExportSegments(): .nodes ha un solo colore per nodo
    bool orphan_warned = false, curvature_warned = false;

    for (int i = 0; i < sticks.count(); i++) {
        StickData *stick = sticks[i];
        NodeExport node;
        node.stick = stick;
        node.draw_index = draw_rank[i];

        // parent node: the master and its origin-attached stepchildren hang off the
        // root joint; everyone else off their parent stick's (last) node
        int parent = 0;
        if (stick != master) {
            if (!stick->parent) {
                if (!orphan_warned) {
                    warnings->append(QObject::tr("A stick without a parent was attached to the figure origin."));
                    orphan_warned = true;
                }
            } else if (!(stick->stepchild && stick->parent == master)) {
                parent = last_node_of.value(stick->parent, 0);
            }
        }

        // geometry: node position = the stick's free end, expressed relative to the
        // parent node's position (both pose representations derive from this offset)
        QPointF parent_pos = (parent == 0) ? origin : stick->parent->my_line.p2();
        QPointF offset = stick->my_line.p2() - parent_pos;
        node.local_x = float(offset.x());
        node.local_y = float(offset.y());
        node.length = QLineF(QPointF(0, 0), offset).length();
        node.global_angle_deg = (node.length > 0)
                ? NormalizeDegrees(qAtan2(offset.y(), offset.x()) * 180.0 / M_PI) : 0.0;

        if (!IsFlatPaint(stick->pen.brush()) || !IsFlatPaint(stick->brush))
            flattened_paints++;
        QColor pen_color = FlattenFill(stick->pen.brush());
        QColor brush_color = (stick->brush.style() != Qt::NoBrush) ? FlattenFill(stick->brush)
                                                                  : pen_color;
        double pen_width = (stick->pen.style() == Qt::NoPen) ? 0.0 : stick->pen.widthF();
        auto rounded_thickness = [&subpixel](double width) -> qint32 {
            if (width <= 0.0)
                return 0;
            qint32 rounded = qRound(width);
            if (rounded == 0) { // .nodes thickness is an int: keep hairlines visible
                subpixel++;
                rounded = 1;
            }
            return rounded;
        };
        node.color = pen_color;
        node.outline_color = pen_color;
        node.thickness = rounded_thickness(pen_width);
        node.trap_start = float(node.thickness);
        node.trap_end = float(node.thickness);

        // MULTILINE: la catena di nodi sostituisce il singolo nodo dello stick
        if (stick->stick_type == StickData::MULTILINE && !stick->polygon_points.isEmpty()) {
            multilines++;
            QList<QPointF> verts;
            verts << stick->my_line.p1();
            QTransform t = stick->PolygonTransform();
            for (const QPointF &pt : stick->polygon_points)
                verts << t.map(pt);
            verts << stick->my_line.p2();
            for (float c : stick->vertex_curvatures) {
                if (c > 0.001f && !curvature_warned) {
                    warnings->append(QObject::tr("Curved multi-line corners were exported as sharp angles."));
                    curvature_warned = true;
                }
            }
            int previous = parent;
            for (int v = 1; v < verts.count(); v++) {
                NodeExport link;
                link.type = 0;
                link.draw_index = (v == 1) ? draw_rank[i] : next_draw++;
                QPointF from = (v == 1) ? parent_pos : verts[v - 1];
                QPointF off = verts[v] - from;
                link.local_x = float(off.x());
                link.local_y = float(off.y());
                link.length = QLineF(QPointF(0, 0), off).length();
                link.global_angle_deg = (link.length > 0)
                        ? NormalizeDegrees(qAtan2(off.y(), off.x()) * 180.0 / M_PI) : 0.0;
                double width_a = stick->vertex_widths.value(v - 1, pen_width);
                double width_b = stick->vertex_widths.value(v, pen_width);
                link.thickness = rounded_thickness((width_a + width_b) / 2.0);
                link.trap_start = float(link.thickness);
                link.trap_end = float(link.thickness);
                link.color = pen_color;
                link.outline_color = pen_color;
                nodes.append(link);
                parent_node.append(previous);
                previous = nodes.count() - 1;
            }
            continue;
        }

        switch (stick->stick_type) {
        case StickData::LINE:
            node.type = (stick->pen.capStyle() == Qt::RoundCap) ? 0 : 1;
            break;
        case StickData::CIRCLE:
            node.type = (stick->brush.style() == Qt::NoBrush) ? 2 : 4;
            if (node.type == 4)
                node.color = brush_color; // fill color; the outline keeps the pen color
            if (!qFuzzyCompare(stick->img_w_scale, 1.0f))
                ellipses++;
            break;
        case StickData::RECT:
        case StickData::TRAPEZOID:
        case StickData::TAPER:
            node.type = 6;
            node.color = brush_color;
            node.trap_start = stick->img_w_scale;
            node.trap_end = (stick->stick_type == StickData::RECT) ? stick->img_w_scale
                                                                   : stick->img_h_scale;
            node.trap_ratio = (node.trap_start > 0) ? node.trap_end / node.trap_start : 1.0f;
            node.thickness = rounded_thickness(node.trap_start);
            if (stick->stick_type == StickData::TAPER) {
                node.trap_rounded_start = !stick->hard_bottom;
                node.trap_rounded_end = !stick->hard_top;
            }
            break;
        case StickData::POLYGON:
            // scheletro invisibile: il riempimento viaggia nella sezione polyfill in coda
            node.type = 0;
            node.thickness = 0;
            break;
        case StickData::MULTILINE: // senza vertici interni è un semplice segmento
            node.type = 0;
            node.thickness = rounded_thickness((stick->vertex_widths.value(0, pen_width)
                                                + stick->vertex_widths.value(1, pen_width)) / 2.0);
            break;
        case StickData::TEXT: // il formato non ha testo: esce come stick normale (spec)
            node.type = 0;
            break;
        case StickData::IMAGE:
        default:
            node.type = 0;
            images++;
            break;
        }
        nodes.append(node);
        parent_node.append(parent);
    }

    // POLYGON sticks → un nodo carrier invisibile per vertice (figli del nodo dello
    // scheletro) + un record polyfill nel trailer che li referenzia per draw index
    struct NodesPolyfill {
        qint32 anchor_draw = 0;
        QColor color;
        QList<qint32> attached;
    };
    QList<NodesPolyfill> polyfills;
    int polygons = 0;
    for (int i = 0; i < sticks.count(); i++) {
        StickData *stick = sticks[i];
        if (stick->stick_type != StickData::POLYGON || stick->polygon_points.isEmpty())
            continue;
        polygons++;
        NodesPolyfill poly;
        poly.anchor_draw = draw_rank[i];
        poly.color = FlattenFill((stick->brush.style() != Qt::NoBrush) ? stick->brush
                                                                      : stick->pen.brush());
        QTransform t = stick->PolygonTransform();
        QPointF base = stick->my_line.p2(); // posizione del nodo dello scheletro
        for (const QPointF &pt : stick->polygon_points) {
            QPointF offset = t.map(pt) - base;
            NodeExport carrier;
            carrier.type = 0;
            carrier.draw_index = next_draw++;
            carrier.local_x = float(offset.x());
            carrier.local_y = float(offset.y());
            carrier.length = QLineF(QPointF(0, 0), offset).length();
            carrier.global_angle_deg = (carrier.length > 0)
                    ? NormalizeDegrees(qAtan2(offset.y(), offset.x()) * 180.0 / M_PI) : 0.0;
            carrier.thickness = 0; // invisibile: esiste solo per portare il vertice
            carrier.color = QColor(0, 0, 0, 0);
            carrier.outline_color = carrier.color;
            poly.attached.append(carrier.draw_index);
            nodes.append(carrier);
            parent_node.append(last_node_of.value(stick));
        }
        polyfills.append(poly);
    }
    if (nodes.count() > 65535) {
        *error = QObject::tr("Too many nodes for the .nodes format once polygon/multi-line vertices are counted.");
        return false;
    }

    // local angles: each node's direction relative to its parent node's direction —
    // this chain is what Stick Nodes itself treats as the authoritative pose
    for (int i = 1; i < nodes.count(); i++)
        nodes[i].local_angle_deg = NormalizeDegrees(nodes[i].global_angle_deg
                                                    - nodes[parent_node[i]].global_angle_deg);

    // tree: children in stick_list order under their resolved parent node
    for (int i = 1; i < nodes.count(); i++)
        nodes[parent_node[i]].children.append(i);
    // a .nodes reader takes the root's FIRST child as the figure's main branch —
    // keep that role on the master even if it isn't first in stick_list
    int master_node = first_node_of.value(master);
    int master_pos = nodes[0].children.indexOf(master_node);
    if (master_pos > 0)
        nodes[0].children.move(master_pos, 0);

    ByteWriter writer;
    writer.I32(kNodesExportVersion);
    writer.I32(kNodesExportBuild);
    writer.F32(1.0f);                    // figure scale
    WriteColorAbgr(writer, QColor(0, 0, 0, 255));

    // depth-first pre-order, exactly the shape the reader recurses over; iterative so
    // a pathological parent chain cannot overflow the C++ stack
    int emitted = 0;
    WriteNodeRecord(writer, nodes[0]);
    emitted++;
    QList<QPair<int, int>> stack; // node index, next-child cursor
    stack.append(qMakePair(0, 0));
    while (!stack.isEmpty()) {
        QPair<int, int> &top = stack.last();
        if (top.second >= nodes[top.first].children.count()) {
            stack.removeLast();
            continue;
        }
        int child = nodes[top.first].children[top.second++];
        WriteNodeRecord(writer, nodes[child]);
        emitted++;
        stack.append(qMakePair(child, 0));
    }
    if (emitted != nodes.count()) { // a parent-pointer cycle would leave nodes unreachable
        *error = QObject::tr("The figure's stick tree is inconsistent; cannot export.");
        return false;
    }
    // polyfill trailer (v>=230): plan §2.9 record layout; anchor and vertices are
    // referenced by draw index
    writer.I32(polyfills.count());
    for (const NodesPolyfill &poly : polyfills) {
        writer.I32(poly.anchor_draw);
        WriteColorAbgr(writer, poly.color);
        writer.U8(1); // use_polyfill_color: il colore è sempre esplicito
        writer.I32(poly.attached.count());
        for (qint32 draw : poly.attached)
            writer.I32(draw);
    }
    writer.I32(0); // connector endpoints (b>=38)

    if (polygons > 0)
        warnings->append(QObject::tr("%1 polygon stick(s) exported as a polygon fill plus invisible vertex nodes.")
                         .arg(polygons));
    if (multilines > 0)
        warnings->append(QObject::tr("%1 multi-line stick(s) exported as chains of welded segments.")
                         .arg(multilines));
    if (images > 0)
        warnings->append(QObject::tr("%1 image stick(s) exported as plain segments.").arg(images));
    if (ellipses > 0)
        warnings->append(QObject::tr("Squashed circles (ellipses) were exported as plain circles."));
    if (subpixel > 0)
        warnings->append(QObject::tr("%1 sub-pixel line width(s) rounded up to 1 (.nodes thickness is integer).").arg(subpixel));
    if (flattened_paints > 0)
        warnings->append(QObject::tr("%1 stick(s): pattern/gradient/image fill or stroke exported as a flat color.")
                         .arg(flattened_paints));

    // the .nodes container is the bare zlib stream, no marker byte
    QByteArray bytes = CompressZlib(writer.out);
    QFile file(path);
    if (!file.open(QIODevice::WriteOnly)) {
        *error = QObject::tr("Cannot write %1.").arg(path);
        return false;
    }
    if (file.write(bytes) != bytes.size()) {
        *error = QObject::tr("Write failed on %1.").arg(path);
        file.close();
        return false;
    }
    file.close();
    return true;
}
