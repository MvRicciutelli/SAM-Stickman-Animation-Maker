#include "kritabundle.h"
#include "myview.h"   // MakeRoundBrush(): an auto brush becomes the same round tip the built-ins use

#include <QBuffer>
#include <QDataStream>
#include <QFileInfo>
#include <QHash>
#include <QImageReader>
#include <QXmlStreamReader>
#include <QtCore/private/qzipreader_p.h>
#include <QtMath>
#include <cmath>
#include <cstring>

namespace {

/// The draw-mode size spin box stops at 500 px, so more resolution than this is never painted.
const int MAX_STAMP = 512;

double Attr(const QXmlStreamAttributes &attributes, const char *name, double fallback)
{
    bool ok = false;
    const double value = attributes.value(QLatin1String(name)).toString().toDouble(&ok);
    return ok ? value : fallback;
}

/// @details Krita reads an image tip as "dark paints, alpha keeps": a black-on-white PNG and a
/// black-on-transparent one are the same brush, so coverage is alpha x darkness.
QImage ImageToStamp(const QImage &image)
{
    if(image.isNull())
        return QImage();
    const QImage argb = image.convertToFormat(QImage::Format_ARGB32);
    QImage stamp(argb.size(), QImage::Format_Alpha8);
    for(int y = 0; y < argb.height(); y++){
        const QRgb *src = reinterpret_cast<const QRgb*>(argb.constScanLine(y));
        uchar *dst = stamp.scanLine(y);
        for(int x = 0; x < argb.width(); x++)
            dst[x] = uchar(qAlpha(src[x]) * (255 - qGray(src[x])) / 255);
    }
    return stamp;
}

/**
 * @details One GIMP brush (.gbr v1/v2, big-endian): header size, version, width, height, bytes
 * per pixel, then (v2) "GIMP" + spacing and the name, all inside header size; the pixels follow.
 * A 1-byte brush IS a coverage mask (255 = full paint); a 4-byte one is a colour brush and goes
 * through ImageToStamp() like a PNG tip.
 */
QImage ReadGbr(const QByteArray &data, qint64 offset)
{
    QDataStream s(data);
    s.setByteOrder(QDataStream::BigEndian);
    if(s.skipRawData(int(offset)) != offset)
        return QImage();
    quint32 header_size = 0, version = 0, width = 0, height = 0, bytes = 0;
    s >> header_size >> version >> width >> height >> bytes;
    if(s.status() != QDataStream::Ok || header_size < 20 || width == 0 || height == 0
            || width > 8192 || height > 8192 || (bytes != 1 && bytes != 4))
        return QImage();
    const qint64 pixels = offset + header_size;
    if(pixels + qint64(width) * height * bytes > data.size())
        return QImage();
    const char *src = data.constData() + pixels;
    if(bytes == 1){
        QImage stamp(int(width), int(height), QImage::Format_Alpha8);
        for(int y = 0; y < int(height); y++)
            std::memcpy(stamp.scanLine(y), src + qint64(y) * width, width);
        return stamp;
    }
    QImage rgba(int(width), int(height), QImage::Format_RGBA8888);
    for(int y = 0; y < int(height); y++)
        std::memcpy(rgba.scanLine(y), src + qint64(y) * width * 4, width * 4);
    return ImageToStamp(rgba);
}

/// @details A .gih (GIMP image hose) is two text lines — name, then cell count and selection
/// rules — followed by its cells as plain .gbr brushes. One stamp per brush here, so the first
/// cell stands for the hose.
QImage ReadGih(const QByteArray &data)
{
    const int first = data.indexOf('\n');
    const int second = first < 0 ? -1 : data.indexOf('\n', first + 1);
    return second < 0 ? QImage() : ReadGbr(data, second + 1);
}

/// @details GIMP pattern (.pat, NOT Photoshop's 8BPT one): header size, version, width, height,
/// bytes per pixel (1 grey, 2 grey+alpha, 3 RGB, 4 RGBA), "GPAT", the name, then the pixels.
QImage ReadGimpPat(const QByteArray &data)
{
    QDataStream s(data);
    s.setByteOrder(QDataStream::BigEndian);
    quint32 header_size = 0, version = 0, width = 0, height = 0, bytes = 0;
    char magic[4] = {};
    s >> header_size >> version >> width >> height >> bytes;
    s.readRawData(magic, 4);
    if(s.status() != QDataStream::Ok || std::memcmp(magic, "GPAT", 4) != 0 || header_size < 24
            || width == 0 || height == 0 || width > 8192 || height > 8192 || bytes < 1 || bytes > 4
            || header_size + qint64(width) * height * bytes > data.size())
        return QImage();
    const uchar *p = reinterpret_cast<const uchar*>(data.constData()) + header_size;
    QImage tile(int(width), int(height), QImage::Format_ARGB32);
    for(int y = 0; y < int(height); y++){
        QRgb *row = reinterpret_cast<QRgb*>(tile.scanLine(y));
        for(int x = 0; x < int(width); x++, p += bytes){
            switch(bytes){
            case 1:  row[x] = qRgb(p[0], p[0], p[0]); break;
            case 2:  row[x] = qRgba(p[0], p[0], p[0], p[1]); break;
            case 3:  row[x] = qRgb(p[0], p[1], p[2]); break;
            default: row[x] = qRgba(p[0], p[1], p[2], p[3]); break;
            }
        }
    }
    return tile;
}

/// ponytail: hard-edged; Krita fades a rectangle along both axes, which MakeRoundBrush() has no
/// counterpart for. A soft rectangle tip is a generator of its own if anyone misses it.
QImage RectStamp(double ratio)
{
    QImage stamp(128, 128, QImage::Format_Alpha8);
    stamp.fill(0);
    const int rows = qBound(1, qRound(128 * ratio), 128);
    for(int y = (128 - rows) / 2; y < (128 - rows) / 2 + rows; y++)
        std::memset(stamp.scanLine(y), 255, 128);
    return stamp;
}

struct PresetXml {
    QString name;
    QString paintop;
    QHash<QString, QString> params;   ///< <param name="..."> -> its text (CDATA included)
};

PresetXml ParsePreset(const QString &xml)
{
    PresetXml out;
    QXmlStreamReader r(xml);
    while(!r.atEnd()){
        if(r.readNext() != QXmlStreamReader::StartElement)
            continue;
        if(r.name() == QLatin1String("Preset")){
            out.name = r.attributes().value(QLatin1String("name")).toString();
            out.paintop = r.attributes().value(QLatin1String("paintopid")).toString();
        } else if(r.name() == QLatin1String("param")){
            const QString key = r.attributes().value(QLatin1String("name")).toString();
            out.params.insert(key, r.readElementText());
        }
    }
    return out;
}

/// @details A sensor counts as jitter only when it is RANDOM ("fuzzy" per dab, "fuzzystroke" per
/// stroke) and its option is switched on. Pressure, tilt and drawing-angle sensors follow the
/// stylus or the stroke, which a fixed per-dab jitter would only misrepresent.
bool Fuzzy(const QHash<QString, QString> &params, const QString &option)
{
    return params.value("Pressure" + option) == QLatin1String("true")
            && params.value(option + "Sensor").contains(QLatin1String("id=\"fuzzy"));
}

bool ConvertPreset(const PresetXml &preset, const QHash<QString, QByteArray> &tips, KritaBrush *out)
{
    const QHash<QString, QString> &p = preset.params;
    if(preset.paintop != QLatin1String("paintbrush")
            || p.value("CompositeOp") == QLatin1String("erase")
            || p.value("EraserMode") == QLatin1String("true"))
        return false;   // una gomma convertita dipingerebbe col colore: draw mode ha la sua gomma

    QXmlStreamAttributes brush, mask;
    QXmlStreamReader definition(p.value("brush_definition"));
    while(!definition.atEnd()){
        if(definition.readNext() != QXmlStreamReader::StartElement)
            continue;
        if(definition.name() == QLatin1String("Brush"))
            brush = definition.attributes();
        else if(definition.name() == QLatin1String("MaskGenerator"))
            mask = definition.attributes();
    }

    BrushPreset &params = out->params;
    params.name = preset.name;
    const QString type = brush.value(QLatin1String("type")).toString();
    double diameter = 0;
    if(type == QLatin1String("auto_brush")){
        // fade 1 = bordo netto, 0 = tutto sfumato: la stessa scala della durezza
        const double ratio = qBound(0.01, Attr(mask, "ratio", 1.0), 1.0);
        params.hardness = qRound(qBound(0.0, Attr(mask, "hfade", 1.0), 1.0) * 100);
        params.roundness = qBound(1, qRound(ratio * 100), 100);
        diameter = Attr(mask, "diameter", 20);
        out->stamp = mask.value(QLatin1String("type")) == QLatin1String("rect")
                ? RectStamp(ratio) : MyView::MakeRoundBrush(128, params.hardness, params.roundness);
    } else if(type == QLatin1String("png_brush") || type == QLatin1String("gbr_brush")){
        const QString file = QFileInfo(brush.value(QLatin1String("filename")).toString()).fileName();
        const QByteArray data = tips.value(file);
        if(data.isEmpty())
            return false;   // una punta di Krita stesso, che il bundle non si porta dietro
        if(file.endsWith(QLatin1String(".gih"), Qt::CaseInsensitive))
            out->stamp = ReadGih(data);
        else if(file.endsWith(QLatin1String(".gbr"), Qt::CaseInsensitive))
            out->stamp = ReadGbr(data, 0);
        else
            out->stamp = ImageToStamp(QImage::fromData(data));
        if(out->stamp.isNull())
            return false;
        diameter = qMax(out->stamp.width(), out->stamp.height()) * Attr(brush, "scale", 1.0);
        if(qMax(out->stamp.width(), out->stamp.height()) > MAX_STAMP)
            out->stamp = out->stamp.scaled(MAX_STAMP, MAX_STAMP, Qt::KeepAspectRatio,
                                           Qt::SmoothTransformation)
                                   .convertToFormat(QImage::Format_Alpha8);
    } else {
        return false;   // abr_brush, text_brush, svg_brush...
    }

    params.size = qBound(1, qRound(diameter), 500);
    params.spacing = qBound(1, qRound(Attr(brush, "spacing", 0.1) * 100), 1000);
    params.angle = qRound(std::remainder(qRadiansToDegrees(Attr(brush, "angle", 0.0)), 360.0));
    // un solo numero per dab qui: in Krita l'opacità limita il tratto e il flow il singolo dab
    params.opacity = qBound(1, qRound(p.value("OpacityValue", "1").toDouble()
                                      * p.value("FlowValue", "1").toDouble() * 100), 100);
    if(Fuzzy(p, "Rotation"))
        params.angle_jitter = qBound(0, qRound(p.value("RotationValue", "1").toDouble() * 100), 100);
    // ponytail: Krita scales a random size/opacity sensor through a curve; a flat 50% reduction is
    // the honest middle of it. Reading the curve is the upgrade if a converted brush looks off.
    if(Fuzzy(p, "Size"))
        params.size_jitter = 50;
    if(Fuzzy(p, "Opacity"))
        params.opacity_jitter = 50;
    return true;
}

} // namespace

/// @details Tips are read up front (a few MB at most) so each preset can look its tip up by file
/// name; presets are converted in name order, which is the order Krita lists them in.
KritaBundleBrushes LoadKritaBundleBrushes(const QString &path)
{
    KritaBundleBrushes out;
    QZipReader zip(path);
    if(!zip.isReadable() || zip.status() != QZipReader::NoError)
        return out;
    QHash<QString, QByteArray> tips;
    QStringList presets;
    for(const QZipReader::FileInfo &entry : zip.fileInfoList()){
        if(!entry.isFile)
            continue;
        if(entry.filePath.startsWith(QLatin1String("brushes/")))
            tips.insert(QFileInfo(entry.filePath).fileName(), zip.fileData(entry.filePath));
        else if(entry.filePath.startsWith(QLatin1String("paintoppresets/"))
                && entry.filePath.endsWith(QLatin1String(".kpp"), Qt::CaseInsensitive))
            presets << entry.filePath;
    }
    presets.sort(Qt::CaseInsensitive);
    for(const QString &entry : presets){
        // un .kpp è un PNG: l'XML del preset sta nel suo chunk di testo "preset"
        QBuffer buffer;
        buffer.setData(zip.fileData(entry));
        buffer.open(QIODevice::ReadOnly);
        QImageReader reader(&buffer, "png");
        PresetXml preset = ParsePreset(reader.text(QStringLiteral("preset")));
        if(preset.name.isEmpty())
            preset.name = QFileInfo(entry).completeBaseName();
        KritaBrush brush;
        if(ConvertPreset(preset, tips, &brush))
            out.brushes.append(brush);
        else
            out.skipped++;
    }
    return out;
}

QList<PatPattern> LoadKritaBundlePatterns(const QString &path)
{
    QList<PatPattern> out;
    QZipReader zip(path);
    if(!zip.isReadable() || zip.status() != QZipReader::NoError)
        return out;
    QStringList entries;
    for(const QZipReader::FileInfo &entry : zip.fileInfoList())
        if(entry.isFile && entry.filePath.startsWith(QLatin1String("patterns/")))
            entries << entry.filePath;
    entries.sort(Qt::CaseInsensitive);
    for(const QString &entry : entries){
        const QByteArray data = zip.fileData(entry);
        const QImage tile = entry.endsWith(QLatin1String(".pat"), Qt::CaseInsensitive)
                ? ReadGimpPat(data) : QImage::fromData(data);
        if(!tile.isNull())
            out.append({ QFileInfo(entry).completeBaseName(), tile.convertToFormat(QImage::Format_ARGB32) });
    }
    return out;
}
