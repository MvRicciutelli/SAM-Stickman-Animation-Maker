#include "abrbrush.h"
#include <QFile>
#include <QDataStream>
#include <QPainter>
#include <QPainterPath>
#include <QRandomGenerator>
#include <math.h>

/**
 * @details Photoshop .abr sampled-brush reader for the two on-disk families:
 *   - v1/v2: int16 version | int16 count | per brush {int16 type, int32 block, data}
 *   - v6   : int16 version(6) | int16 subversion | a chain of 8BIM sections; the
 *            'samp' section holds the sampled brushes back to back.
 * All fields are big-endian (Photoshop native). Both families end each sampled brush
 * with the same tail — int32 bounds(top,left,bottom,right), int16 depth, int8
 * compression (0=raw, 1=PSD-PackBits), bitmap — so ReadSampledBitmap() is shared.
 * Computed brushes and 16-bit depths are skipped (Phase 2 = sampled 8-bit).
 *
 * The second half of this file is the .pat pattern-tile reader (LoadPat(), added with
 * the pattern/gradient/image fills). It lives here rather than in a file of its own
 * because .pat is the same big-endian PSD container and ends each colour plane with
 * that very same bitmap tail: DecodeRle() and ReadUcs2Name() are reused verbatim, so
 * the whole format costs one function and no new plumbing.
 */

namespace {

/// UCS-2 (UTF-16BE) name used by v2: int32 code-unit count, then that many shorts.
QString ReadUcs2Name(QDataStream &s)
{
    qint32 chars = 0;
    s >> chars;
    if(chars <= 0 || chars > 4096)
        return QString();
    QString out;
    out.reserve(chars);
    for(qint32 i = 0; i < chars; i++){
        quint16 c;
        s >> c;
        if(c)
            out.append(QChar(c));
    }
    return out;
}

/// PSD PackBits decode of a full sampled bitmap into `dst`. The per-row compressed
/// byte-count table (one int16 per row) precedes the data and is discarded — we
/// decode straight through until `expected` output bytes are produced.
bool DecodeRle(QDataStream &s, int height, int expected, QByteArray &dst)
{
    for(int i = 0; i < height; i++){
        quint16 row_count;
        s >> row_count;
    }
    dst.resize(expected);
    int n = 0;
    while(n < expected){
        qint8 hdr;
        s >> hdr;
        if(s.status() != QDataStream::Ok)
            return false;
        if(hdr >= 0){                       // literal run of hdr+1 bytes
            int count = int(hdr) + 1;
            for(int j = 0; j < count && n < expected; j++){
                quint8 v;
                s >> v;
                dst[n++] = char(v);
            }
        } else if(hdr != -128){             // repeat next byte (1 - hdr) times
            int count = 1 - int(hdr);
            quint8 v;
            s >> v;
            for(int j = 0; j < count && n < expected; j++)
                dst[n++] = char(v);
        }
        // hdr == -128: no-op (PackBits)
    }
    return s.status() == QDataStream::Ok;
}

/// Grayscale sample buffer -> ARGB32 stamp with alpha = coverage. Photoshop sampled
/// bitmaps store the value AS the coverage (bright = paint, dark = no paint), verified
/// against real v6 smoke packs (a mostly-empty brush must read as mostly-transparent).
QImage MaskToStamp(const QByteArray &mask, int width, int height)
{
    QImage img(width, height, QImage::Format_ARGB32);
    for(int y = 0; y < height; y++){
        QRgb *row = reinterpret_cast<QRgb*>(img.scanLine(y));
        for(int x = 0; x < width; x++){
            quint8 v = quint8(mask[y * width + x]);
            row[x] = qRgba(0, 0, 0, v);
        }
    }
    return img;
}

/// Stream is positioned at the bitmap; read raw or RLE 8-bit data into a stamp.
/// Returns a null QImage on unsupported depth / bad dimensions / short read.
QImage ReadSampledBitmap(QDataStream &s, int width, int height, int depth, int compression)
{
    if(!(width > 0 && height > 0 && width <= 8192 && height <= 8192 && depth == 8))
        return QImage();
    int expected = width * height;
    QByteArray mask;
    bool ok = false;
    if(compression == 0){
        mask.resize(expected);
        ok = (s.readRawData(mask.data(), expected) == expected);
    } else {
        ok = DecodeRle(s, height, expected, mask);
    }
    return ok ? MaskToStamp(mask, width, height) : QImage();
}

/// v1/v2: flat list of typed brush blocks. block_size skips computed brushes and
/// any sampled brush we can't decode.
void LoadV12(QDataStream &s, QFile &f, qint16 version, QList<AbrBrush> &out)
{
    qint16 count = 0;
    s >> count;
    if(count <= 0 || count > 4096)
        return;

    for(int i = 0; i < count; i++){
        qint16 brush_type = 0;
        qint32 block_size = 0;
        s >> brush_type >> block_size;
        if(s.status() != QDataStream::Ok || block_size <= 0)
            break;
        qint64 next_brush = f.pos() + block_size;

        if(brush_type == 2){                // sampled
            qint32 misc;
            qint16 spacing;
            s >> misc >> spacing;
            QString name;
            if(version == 2)
                name = ReadUcs2Name(s);
            qint8 antialias;
            s >> antialias;
            qint16 short_bounds[4];
            for(int k = 0; k < 4; k++) s >> short_bounds[k];
            qint32 bounds[4];
            for(int k = 0; k < 4; k++) s >> bounds[k];
            qint16 depth;
            qint8 compression;
            s >> depth >> compression;

            QImage stamp = ReadSampledBitmap(s, bounds[3] - bounds[1], bounds[2] - bounds[0],
                                             depth, compression);
            if(!stamp.isNull()){
                AbrBrush b;
                b.name  = name.isEmpty() ? QString("Brush %1").arg(i + 1) : name;
                b.stamp = stamp;
                out.append(b);
            }
        }
        f.seek(next_brush);
        if(f.atEnd())
            break;
    }
}

/// v6: 8BIM section chain; the 'samp' section holds concatenated sampled brushes.
/// A fixed descriptor is skipped before each brush's bounds (Photoshop writes it at a
/// constant size for sampled brushes).
/// ponytail: fixed 47/301 descriptor skip is the pragmatic approach shared by abrViewer
/// et al.; upgrade path is a real PS Descriptor (Objc) parser if a pack ever misaligns.
void LoadV6(QDataStream &s, QFile &f, QList<AbrBrush> &out)
{
    qint16 subversion = 0;
    s >> subversion;
    const int skip = (subversion == 1) ? 47 : 301;

    while(true){
        char sig[4];
        if(s.readRawData(sig, 4) != 4 || qstrncmp(sig, "8BIM", 4) != 0)
            break;
        char key[4];
        if(s.readRawData(key, 4) != 4)
            break;
        qint32 section_len = 0;
        s >> section_len;
        if(s.status() != QDataStream::Ok || section_len < 0)
            break;
        qint64 section_end = f.pos() + section_len;

        if(qstrncmp(key, "samp", 4) == 0){
            int idx = 0;
            while(f.pos() + 4 <= section_end){
                qint32 brush_size = 0;
                s >> brush_size;
                if(s.status() != QDataStream::Ok || brush_size <= 0)
                    break;
                qint64 data_start = f.pos();
                qint64 padded = brush_size + ((brush_size % 4) ? 4 - (brush_size % 4) : 0);
                qint64 next_brush = data_start + padded;

                // name: leading pascal ASCII string (a UUID) — cosmetic only, bounds are
                // read from the fixed offset regardless
                quint8 nlen = 0;
                s >> nlen;
                QByteArray nm(nlen, '\0');
                if(nlen)
                    s.readRawData(nm.data(), nlen);

                if(data_start + skip + 18 <= next_brush){
                    f.seek(data_start + skip);
                    qint32 top, left, bottom, right;
                    qint16 depth;
                    qint8 compression;
                    s >> top >> left >> bottom >> right >> depth >> compression;
                    QImage stamp = ReadSampledBitmap(s, right - left, bottom - top,
                                                     depth, compression);
                    if(!stamp.isNull()){
                        AbrBrush b;
                        b.name  = nm.isEmpty() ? QString("Brush %1").arg(idx + 1)
                                               : QString::fromLatin1(nm);
                        b.stamp = stamp;
                        out.append(b);
                    }
                }
                idx++;
                if(next_brush <= data_start)
                    break;
                f.seek(next_brush);
            }
        }
        f.seek(section_end);
        if(f.atEnd())
            break;
    }
}

} // namespace

QList<AbrBrush> LoadAbr(const QString &path)
{
    QList<AbrBrush> out;
    QFile f(path);
    if(!f.open(QIODevice::ReadOnly))
        return out;

    QDataStream s(&f);
    s.setByteOrder(QDataStream::BigEndian);

    qint16 version = 0;
    s >> version;
    if(version == 1 || version == 2)
        LoadV12(s, f, version, out);
    else if(version == 6)
        LoadV6(s, f, out);
    return out;
}

// ================================================================ .pat patterns ====

namespace {

/**
 * @brief One decoded 8-bit colour plane of a .pat tile.
 *
 * A .pat tile is stored the way a PSD layer is: not interleaved, but one full-size
 * greyscale plane per channel (R, then G, then B, then an optional alpha). PlanesToTile()
 * interleaves them back into a QImage once all of them are in.
 */
struct PatPlane {
    int width = 0;       ///< plane width in pixels; every plane of one tile must agree
    int height = 0;      ///< plane height in pixels; likewise
    QByteArray bytes;    ///< width*height 8-bit samples, row-major; empty if unwritten
};

/**
 * @brief Reads one Virtual Memory Array — a single colour plane — from a .pat record.
 * @param s    stream positioned at the array's `is_written` flag.
 * @param f    the same file behind @p s, used to realign on the array's recorded length.
 * @param out  receives the decoded plane; left untouched (empty `bytes`) for an array
 *             that is unwritten or in a flavour we don't decode.
 * @return false only on a genuine short read (a truncated file), which aborts the whole
 *         record — an unsupported plane is not an error, it just yields nothing.
 *
 * Layout, all big-endian: int32 is_written | int32 length | int32 depth |
 * int32 bounds(top,left,bottom,right) | int16 pixel_depth | int8 compression | bitmap.
 * `length` spans the 23 header bytes as well as the bitmap, which is what lets this
 * function skip a plane it cannot read and still leave the stream exactly on the next
 * one.
 */
bool ReadPatPlane(QDataStream &s, QFile &f, PatPlane *out)
{
    qint32 written = 0;
    s >> written;
    if(s.status() != QDataStream::Ok)
        return false;
    if(!written)
        return true;                       // slot present but empty: normal, not an error

    qint32 length = 0;
    s >> length;
    if(s.status() != QDataStream::Ok || length < 23)
        return false;
    const qint64 plane_end = f.pos() + length;

    qint32 depth = 0, top = 0, left = 0, bottom = 0, right = 0;
    qint16 pixel_depth = 0;
    qint8 compression = 0;
    s >> depth >> top >> left >> bottom >> right >> pixel_depth >> compression;
    const int width = right - left, height = bottom - top;
    // 8-bit only, same scope as the .abr side; the bounds guard also catches a garbage
    // record that got this far
    if(s.status() == QDataStream::Ok && pixel_depth == 8
            && width > 0 && height > 0 && width <= 8192 && height <= 8192){
        const int expected = width * height;
        QByteArray bytes;
        bool decoded = false;
        if(compression == 0){
            bytes.resize(expected);
            decoded = (s.readRawData(bytes.data(), expected) == expected);
        } else if(compression == 1){
            decoded = DecodeRle(s, height, expected, bytes);
        }
        if(decoded){
            out->width = width;
            out->height = height;
            out->bytes = bytes;
        }
    }
    const bool stream_ok = (s.status() == QDataStream::Ok);
    f.seek(plane_end);                     // realign whatever the plane turned out to be
    return stream_ok;
}

/**
 * @brief How many colour planes one image mode's pixels are built from.
 * @param image_mode the record's mode field (Photoshop's ColorMode numbering).
 * @return 1, 3 or 4; zero for a mode this reader doesn't map.
 *
 * The plane COUNT alone cannot identify a mode — CMYK's four planes are not an RGB tile
 * with an alpha channel — so both the reader and PlanesToTile() key off the mode and use
 * this as their single source of truth for it.
 */
int PlanesNeededFor(int image_mode)
{
    switch(image_mode){
    case 1: return 1;   // Grayscale
    case 3: return 3;   // RGB
    case 4: return 4;   // CMYK
    default: return 0;  // Bitmap(0), Indexed(2), Multichannel(7), Duotone(8), Lab(9)
    }
}

/**
 * @brief Interleaves the decoded planes of one tile into an ARGB32 image.
 * @param planes the written planes, in file (channel) order; extra ones are ignored.
 * @param image_mode the record's mode field, which decides how they combine.
 * @return the tile, or a null QImage when the mode is out of scope, a plane is missing, or
 *         the planes' geometry disagrees.
 *
 * Grayscale and RGB may carry one extra plane, which is alpha. CMYK may not — its fourth
 * plane is the black ink.
 */
QImage PlanesToTile(const QList<PatPlane> &planes, int image_mode)
{
    const int needed = PlanesNeededFor(image_mode);
    if(needed == 0 || planes.count() < needed)
        return QImage();
    const bool has_alpha = (image_mode != 4 && planes.count() > needed);
    const int used = needed + (has_alpha ? 1 : 0);

    const int width = planes[0].width, height = planes[0].height;
    for(int p = 0; p < used; p++)
        if(planes[p].width != width || planes[p].height != height
                || planes[p].bytes.size() < width * height)
            return QImage();

    QImage img(width, height, QImage::Format_ARGB32);
    if(img.isNull())
        return QImage(); // allocazione rifiutata (tile enorme): scanLine() sarebbe un deref nullo

    // puntatori ai piani: l'accesso per pixel è il ciclo caldo di tutta l'importazione
    const uchar *plane[4] = { nullptr, nullptr, nullptr, nullptr };
    for(int p = 0; p < needed; p++)
        plane[p] = reinterpret_cast<const uchar*>(planes[p].bytes.constData());
    const uchar *alpha = has_alpha
            ? reinterpret_cast<const uchar*>(planes[needed].bytes.constData()) : nullptr;

    for(int y = 0; y < height; y++){
        QRgb *row = reinterpret_cast<QRgb*>(img.scanLine(y));
        for(int x = 0; x < width; x++){
            const int i = y * width + x;
            const quint8 a = alpha ? alpha[i] : 255;
            if(image_mode == 1)
                row[x] = qRgba(plane[0][i], plane[0][i], plane[0][i], a);
            else if(image_mode == 3)
                row[x] = qRgba(plane[0][i], plane[1][i], plane[2][i], a);
            else {
                // CMYK: Photoshop lo memorizza INVERTITO (0 = inchiostro pieno), da cui il
                // 255- su ogni canale prima della conversione sottrattiva
                const int c = 255 - plane[0][i], m = 255 - plane[1][i];
                const int yl = 255 - plane[2][i], k = 255 - plane[3][i];
                row[x] = qRgba(255 - qMin(255, c + k), 255 - qMin(255, m + k),
                               255 - qMin(255, yl + k), 255);
            }
        }
    }
    return img;
}

/**
 * @brief Reads one whole pattern record and appends its tile to @p out if usable.
 * @param s     stream positioned at the start of the record.
 * @param f     the same file behind @p s, for the length-based realignments.
 * @param index zero-based position in the file, used only to name a nameless tile.
 * @param out   receives the tile; left alone for a record in an unsupported flavour.
 * @return false when there is nothing more to read (end of file, or a desync this
 *         function cannot recover from), which ends LoadPat()'s loop early.
 *
 * Layout: int32 version(1) | int32 image_mode | int16 point_h | int16 point_w |
 * Unicode name | pascal-string id | [256*3 palette + 4 bytes, indexed mode only] |
 * Virtual Memory Array List {int32 version(3), int32 length, int32 bounds[4],
 * int32 channels, then channels+2 arrays}. The two trailing arrays are Photoshop's
 * alpha/mask slots, normally unwritten — reading them is how the stream stays aligned.
 *
 * @note `channels` is a CAPACITY, not a count: real Photoshop files write 24 (its maximum)
 * regardless of how many planes follow, so the arrays are read until the VMA list's own end
 * instead of counting to `channels + 2`. Verified against a shipped .pat whose RGB/CMYK
 * records all declare 24 and then write 3 or 4 planes plus 22 empty slots.
 *
 * Only the modes PlanesNeededFor() maps have their planes read at all; anything else skips
 * straight to the realignment at the end. Reading stops one plane past what the mode needs
 * (that one is the optional alpha), which also caps how much memory a single record can ask
 * for no matter what its lengths claim.
 *
 * @note Two writer dialects exist: with and without a byte-length prefix in front of
 * each record (the PSD 'Ptrn' resource block carries one, plain .pat files usually do
 * not). Since a record always opens with version == 1, a leading int32 that isn't 1 is
 * taken as that prefix — which both supports the dialect and gives an exact
 * resynchronization point if the record turns out to be undecodable.
 */
bool ReadPatRecord(QDataStream &s, QFile &f, int index, QList<PatPattern> &out)
{
    qint32 probe = 0;
    s >> probe;
    if(s.status() != QDataStream::Ok)
        return false;
    qint64 record_end = -1;
    if(probe != 1){
        if(probe <= 0)
            return false;
        record_end = f.pos() + probe;
        s >> probe;
    }
    if(probe != 1)
        return false;                      // not a pattern record: give up on the file

    qint32 image_mode = 0;
    qint16 point_height = 0, point_width = 0;
    s >> image_mode >> point_height >> point_width;
    const QString name = ReadUcs2Name(s);
    quint8 id_length = 0;
    s >> id_length;
    if(id_length)
        f.seek(f.pos() + id_length);       // the pattern's UUID: never shown, always skipped
    if(image_mode == 2)                    // Indexed: 256-entry palette + 4 reserved bytes
        f.seek(f.pos() + 768 + 4);
    if(s.status() != QDataStream::Ok)
        return false;

    qint32 vma_version = 0, vma_length = 0;
    s >> vma_version >> vma_length;
    if(s.status() != QDataStream::Ok || vma_version != 3 || vma_length <= 20)
        return false;
    const qint64 vma_end = f.pos() + vma_length;

    qint32 top = 0, left = 0, bottom = 0, right = 0, channels_capacity = 0;
    s >> top >> left >> bottom >> right >> channels_capacity;
    Q_UNUSED(channels_capacity) // capacità, non conteggio: vedi la nota sopra
    // si legge fino alla fine della VMA list, un piano oltre quelli che il modo richiede
    // (quello è l'eventuale alpha): il limite su vma_end tiene la lettura dentro il record
    // anche con lunghezze di piano malevole
    const int needed = PlanesNeededFor(image_mode);
    QList<PatPlane> planes;
    bool readable = (s.status() == QDataStream::Ok && needed > 0);
    while(readable && f.pos() < vma_end && planes.count() < needed + 1){
        PatPlane plane;
        readable = ReadPatPlane(s, f, &plane);
        if(readable && !plane.bytes.isEmpty())
            planes.append(plane);
    }

    const QImage tile = readable ? PlanesToTile(planes, image_mode) : QImage();
    if(!tile.isNull()){
        PatPattern pattern;
        pattern.name = name.isEmpty() ? QString("Pattern %1").arg(index + 1) : name;
        pattern.tile = tile;
        out.append(pattern);
    }
    // realign on the record's own end marker so one tile in an unsupported mode costs
    // that tile and not the rest of the library
    f.seek(record_end > 0 ? record_end : vma_end);
    s.resetStatus();
    return !f.atEnd();
}

} // namespace

QList<PatPattern> LoadPat(const QString &path)
{
    QList<PatPattern> out;
    QFile f(path);
    if(!f.open(QIODevice::ReadOnly))
        return out;

    QDataStream s(&f);
    s.setByteOrder(QDataStream::BigEndian); // Photoshop native, same as .abr

    // header: la firma '8BPT', poi int16 version e int32 count. La version è a 16 bit
    // esattamente come nell'.abr, non a 32.
    char signature[4];
    if(s.readRawData(signature, 4) != 4 || qstrncmp(signature, "8BPT", 4) != 0)
        return out;
    qint16 version = 0;
    qint32 count = 0;
    s >> version >> count;
    if(version != 1 || count <= 0 || count > 4096)
        return out;

    for(int i = 0; i < count; i++)
        if(!ReadPatRecord(s, f, i, out))
            break;
    return out;
}

/// A jitter percentage as a random REDUCTION factor in (1-pct/100 .. 1]. One helper for the two
/// jitters that shrink (size, opacity), so "40%" cannot come to mean two different things.
static double JitterDown(int percent)
{
    if(percent <= 0)
        return 1.0;
    return 1.0 - qBound(0, percent, 100)/100.0 * QRandomGenerator::global()->generateDouble();
}

/// The stamp scaled to a diameter and tinted: the EXPENSIVE half of a dab, and the half that does
/// not vary along a stroke. Split out so StampAlongPath() can do it once instead of per dab — a
/// .abr tip is routinely several hundred pixels square, and smooth-scaling one of those a hundred
/// times per repaint is what made a brush border unusable on the canvas.
static QImage TintedStamp(const QImage &stamp, const QColor &color, int diameter)
{
    QImage scaled = stamp.scaled(diameter, diameter, Qt::IgnoreAspectRatio,
                                 Qt::SmoothTransformation);
    QImage dab(scaled.size(), QImage::Format_ARGB32_Premultiplied);
    dab.fill(Qt::transparent);
    QPainter dp(&dab);
    dp.drawImage(0, 0, scaled);
    dp.setCompositionMode(QPainter::CompositionMode_SourceIn); // tiene solo l'alpha del timbro
    QColor opaque = color;
    opaque.setAlpha(255); // l'opacità è BrushStroke::opacity: tenerla anche qui la applicherebbe
    dp.fillRect(dab.rect(), opaque);   // due volte, e un bordo al 50% uscirebbe al 25%
    dp.end();
    return dab;
}

/// The cheap half: rotating an ALREADY scaled dab. A no-op at a multiple of a full turn.
static QImage Rotated(const QImage &dab, double angle)
{
    if(fmod(angle, 360.0) == 0.0)
        return dab;
    return dab.transformed(QTransform().rotate(angle), Qt::SmoothTransformation);
}

/// The tip angle plus this dab's share of the rotation jitter.
static double JitteredAngle(const BrushStroke &brush)
{
    if(brush.angle_jitter <= 0)
        return brush.angle;
    return brush.angle + (QRandomGenerator::global()->generateDouble()*2.0 - 1.0)
                         * qBound(0, int(brush.angle_jitter), 100) * 1.8; // 100% = ±180°
}

/// @details Extracted verbatim from MyView::StampBrush(), which is now one of its two callers —
/// the other being a stick's brush BORDER. Tinting through CompositionMode_SourceIn is what lets
/// a grayscale .abr tip be painted in any colour.
QImage BrushDab(const BrushStroke &brush, double size_factor, double opacity_factor,
                qreal *out_opacity)
{
    if(out_opacity != nullptr)
        *out_opacity = qBound(0.0, brush.opacity * opacity_factor
                                   * JitterDown(brush.opacity_jitter), 1.0);
    if(brush.stamp.isNull())
        return QImage();
    const int s = qMax(1, int(brush.size * size_factor * JitterDown(brush.size_jitter) + 0.5));
    return Rotated(TintedStamp(brush.stamp, brush.color, s), JitteredAngle(brush));
}

/// @details The border counterpart of MyView::PaintStrokeSegment(): that one walks a live drag
/// segment by segment and has to carry the leftover pitch between mouse events, this one has the
/// whole line up front and can just march along it.
///
/// Written for SPEED, because unlike a freehand drag — which stamps a handful of dabs per mouse
/// event, once — this runs inside StickGraphicsItem::paint(), i.e. on every single repaint of the
/// canvas. Two things were making that unusable and both are gone:
/// - the tip was scaled and tinted PER DAB. It is now built once per stroke (TintedStamp()) and,
///   when neither jitter varies it, rotated once too — so the common case does no per-dab image
///   work at all, just a drawImage of the same QImage. With jitter, the per-dab rescale starts
///   from the already-small tinted dab instead of from the full-size tip.
/// - the walk used QPainterPath::pointAtPercent()/percentAtLength(), each of which re-flattens
///   the whole path. The path is flattened ONCE now (toSubpathPolygons()) and walked segment by
///   segment, carrying the leftover pitch across segments and subpaths so the rhythm is measured
///   along the whole outline and a corner neither restarts it nor re-stamps on itself.
///
/// ponytail: the tinted dab is rebuilt per paint() call, not cached across them. That is one
/// scale per stick per repaint, which is fast enough; if a crowded frame ever drags, the cache
/// belongs on StickGraphicsItem keyed by (tip, colour, diameter).
void StampAlongPath(QPainter *painter, const QPainterPath &path, const BrushStroke &brush)
{
    if(brush.stamp.isNull() || brush.size <= 0 || path.isEmpty())
        return;
    const double pitch = qMax(1.0, brush.size * qMax(1, int(brush.spacing)) / 100.0);
    const bool varies = brush.size_jitter > 0 || brush.angle_jitter > 0;
    const QImage base = TintedStamp(brush.stamp, brush.color, qMax(1, int(brush.size + 0.5)));
    const QImage fixed = varies ? QImage() : Rotated(base, brush.angle);

    painter->save();
    if(brush.opacity_jitter <= 0)
        painter->setOpacity(qBound(0.0, brush.opacity, 1.0)); // costante: fuori dal ciclo
    // pitch e non 0: la corsa "già percorsa" parte piena, così il primo dab cade sull'inizio
    double carry = pitch;
    for(const QPolygonF &poly : path.toSubpathPolygons()){
        for(int i = 1; i < poly.size(); i++){
            const QPointF from = poly[i-1], to = poly[i];
            const double len = QLineF(from, to).length();
            if(len < 1e-9)
                continue;
            for(double d = pitch - carry; d <= len; d += pitch){
                QImage dab = fixed;
                if(varies){
                    dab = base;
                    if(brush.size_jitter > 0){
                        // riscala il dab GIÀ tinto e già piccolo: una frazione del costo di
                        // ripartire dalla punta a piena risoluzione
                        const int s = qMax(1, int(base.width()*JitterDown(brush.size_jitter) + 0.5));
                        dab = dab.scaled(s, s, Qt::IgnoreAspectRatio, Qt::SmoothTransformation);
                    }
                    dab = Rotated(dab, JitteredAngle(brush));
                }
                if(brush.opacity_jitter > 0)
                    painter->setOpacity(qBound(0.0, brush.opacity
                                               * JitterDown(brush.opacity_jitter), 1.0));
                const QPointF at = from + (to - from) * (d / len);
                // un dab ruotato è più grande di size: centralo sulle sue dimensioni effettive
                painter->drawImage(QPointF(at.x() - dab.width()/2.0,
                                           at.y() - dab.height()/2.0), dab);
            }
            carry = fmod(carry + len, pitch);
        }
    }
    painter->restore();
}
