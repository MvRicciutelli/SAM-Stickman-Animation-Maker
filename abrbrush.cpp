#include "abrbrush.h"
#include <QFile>
#include <QDataStream>

/**
 * @details Photoshop .abr sampled-brush reader for the two on-disk families:
 *   - v1/v2: int16 version | int16 count | per brush {int16 type, int32 block, data}
 *   - v6   : int16 version(6) | int16 subversion | a chain of 8BIM sections; the
 *            'samp' section holds the sampled brushes back to back.
 * All fields are big-endian (Photoshop native). Both families end each sampled brush
 * with the same tail — int32 bounds(top,left,bottom,right), int16 depth, int8
 * compression (0=raw, 1=PSD-PackBits), bitmap — so ReadSampledBitmap() is shared.
 * Computed brushes and 16-bit depths are skipped (Phase 2 = sampled 8-bit).
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
