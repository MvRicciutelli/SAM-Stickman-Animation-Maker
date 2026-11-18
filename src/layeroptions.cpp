#include "layeroptions.h"
#include "stickdata.h" // Mix(), MixColor()
#include <QPainter>
#include <QtMath>
#include <future>
#include <thread>

bool LayerShadow::operator==(const LayerShadow &o) const
{
    return enabled == o.enabled && angle == o.angle && distance == o.distance && color == o.color
            && smoothness == o.smoothness && opacity == o.opacity;
}

bool LayerGlow::operator==(const LayerGlow &o) const
{
    return enabled == o.enabled && inside == o.inside && color == o.color && size == o.size
            && smoothness == o.smoothness;
}

bool LayerOutline::operator==(const LayerOutline &o) const
{
    return enabled == o.enabled && color == o.color && size == o.size && opacity == o.opacity;
}

bool LayerColorize::operator==(const LayerColorize &o) const
{
    return enabled == o.enabled && color == o.color && opacity == o.opacity;
}

bool LayerBlur::operator==(const LayerBlur &o) const
{
    return enabled == o.enabled && intensity == o.intensity;
}

bool LayerPixelate::operator==(const LayerPixelate &o) const
{
    return enabled == o.enabled && size == o.size;
}

bool LayerOptions::operator==(const LayerOptions &o) const
{
    return opacity == o.opacity && outer_shadow == o.outer_shadow && inner_shadow == o.inner_shadow
            && glow == o.glow && outline == o.outline && colorize == o.colorize
            && blur == o.blur && pixelate == o.pixelate;
}

/// @details The test that decides whether a figure gets a LayerGroupItem at all: a layer with
/// every option at default keeps today's per-stick depth interleaving.
bool LayerOptions::IsActive() const
{
    return opacity < 1 || outer_shadow.enabled || inner_shadow.enabled || glow.enabled
            || outline.enabled || colorize.enabled || blur.enabled || pixelate.enabled;
}

/// @details Inner effects count too: the inverted mask they blur must have "outside" pixels
/// around the silhouette to read from, or the image border would bleed into the edge. Blur and
/// pixelate widen the layer itself, and every other effect starts from that wider layer, so
/// their reach adds to the rest instead of competing with it.
double LayerOptions::Reach() const
{
    double reach = 0;
    double grown = 0; // how far blur and pixelate push the layer's own pixels out
    if(blur.enabled)
        grown += blur.intensity;
    if(pixelate.enabled)
        grown += pixelate.size;
    if(outer_shadow.enabled)
        reach = qMax(reach, outer_shadow.distance + outer_shadow.smoothness);
    if(inner_shadow.enabled)
        reach = qMax(reach, inner_shadow.distance + inner_shadow.smoothness);
    if(glow.enabled)
        reach = qMax(reach, glow.size + glow.smoothness);
    if(outline.enabled)
        reach = qMax(reach, outline.size);
    return reach + grown;
}

static void WriteShadow(QDataStream &stream, const LayerShadow &s)
{
    stream << qint32(s.enabled) << double(s.angle) << double(s.distance) << s.color
           << double(s.smoothness) << double(s.opacity);
}

static void ReadShadow(QDataStream &stream, LayerShadow &s)
{
    qint32 enabled = 0;
    stream >> enabled >> s.angle >> s.distance >> s.color >> s.smoothness >> s.opacity;
    s.enabled = enabled != 0;
}

/// @details Fixed field order; StickFigureData::operator<< writes it only for records of
/// version 2 or later.
QDataStream &operator<<(QDataStream &stream, const LayerOptions &o)
{
    stream << double(o.opacity);
    WriteShadow(stream, o.outer_shadow);
    WriteShadow(stream, o.inner_shadow);
    stream << qint32(o.glow.enabled) << qint32(o.glow.inside) << o.glow.color
           << double(o.glow.size) << double(o.glow.smoothness);
    stream << qint32(o.outline.enabled) << o.outline.color << double(o.outline.size)
           << double(o.outline.opacity);
    stream << qint32(o.colorize.enabled) << o.colorize.color << double(o.colorize.opacity);
    stream << qint32(o.blur.enabled) << double(o.blur.intensity);
    stream << qint32(o.pixelate.enabled) << double(o.pixelate.size);
    return stream;
}

QDataStream &operator>>(QDataStream &stream, LayerOptions &o)
{
    qint32 glow_enabled = 0, glow_inside = 0, outline_enabled = 0, colorize_enabled = 0;
    qint32 blur_enabled = 0, pixelate_enabled = 0;
    stream >> o.opacity;
    ReadShadow(stream, o.outer_shadow);
    ReadShadow(stream, o.inner_shadow);
    stream >> glow_enabled >> glow_inside >> o.glow.color >> o.glow.size >> o.glow.smoothness;
    stream >> outline_enabled >> o.outline.color >> o.outline.size >> o.outline.opacity;
    stream >> colorize_enabled >> o.colorize.color >> o.colorize.opacity;
    stream >> blur_enabled >> o.blur.intensity;
    stream >> pixelate_enabled >> o.pixelate.size;
    o.blur.enabled = blur_enabled != 0;
    o.pixelate.enabled = pixelate_enabled != 0;
    o.glow.enabled = glow_enabled != 0;
    o.glow.inside = glow_inside != 0;
    o.outline.enabled = outline_enabled != 0;
    o.colorize.enabled = colorize_enabled != 0;
    return stream;
}

static void MixShadow(LayerShadow &target, const LayerShadow &a, const LayerShadow &b, float t)
{
    target.angle = Mix(a.angle, b.angle, t);
    target.distance = Mix(a.distance, b.distance, t);
    target.color = MixColor(a.color, b.color, t);
    target.smoothness = Mix(a.smoothness, b.smoothness, t);
    target.opacity = Mix(a.opacity, b.opacity, t);
}

/// @details Only numbers and colours move; `enabled` and the glow type stay as the base frame's
/// clone brought them (user spec: on/off settings are not interpolated).
void InterpolateLayerOptions(LayerOptions &target, const LayerOptions &start,
                             const LayerOptions &end, float t)
{
    target.opacity = Mix(start.opacity, end.opacity, t);
    MixShadow(target.outer_shadow, start.outer_shadow, end.outer_shadow, t);
    MixShadow(target.inner_shadow, start.inner_shadow, end.inner_shadow, t);
    target.glow.color = MixColor(start.glow.color, end.glow.color, t);
    target.glow.size = Mix(start.glow.size, end.glow.size, t);
    target.glow.smoothness = Mix(start.glow.smoothness, end.glow.smoothness, t);
    target.outline.color = MixColor(start.outline.color, end.outline.color, t);
    target.outline.size = Mix(start.outline.size, end.outline.size, t);
    target.outline.opacity = Mix(start.outline.opacity, end.outline.opacity, t);
    target.colorize.color = MixColor(start.colorize.color, end.colorize.color, t);
    target.colorize.opacity = Mix(start.colorize.opacity, end.colorize.opacity, t);
    target.blur.intensity = Mix(start.blur.intensity, end.blur.intensity, t);
    target.pixelate.size = Mix(start.pixelate.size, end.pixelate.size, t);
}

/// @details Running sums, so a pass costs the same at any radius. Moved here from myview.cpp
/// (pixel selection Smooth) so the layer effects share it. The column pass slides one running
/// sum per column down the rows, so it reads memory in order instead of striding a whole row per
/// step. Raw pointers, not QVector::operator[]: the non-const operator checks for a detach on
/// every pixel, which an unoptimized (Debug) build pays as a function call. The division by the
/// window is an exact multiply-shift: with m = ceil(2^32 / window), (sum * m) >> 32 equals
/// sum / window whenever sum * window < 2^32, true for 0..255 inputs up to a 4103-cell window,
/// hence the radius clamp (2000 cells, far beyond any effect).
void BoxBlur(QVector<int> &grid, int w, int h, int r)
{
    r = qMin(r, 2000);
    const quint64 window = quint64(2*r + 1);
    const quint64 m = ((quint64(1) << 32) + window - 1) / window;
    int *g = grid.data();
    QVector<int> line_buffer(w);
    int *line = line_buffer.data();
    for(int y = 0; y < h; y++){                     // rows
        int *row = g + y*w;
        int sum = 0;
        for(int x = 0; x <= r && x < w; x++)
            sum += row[x];
        for(int x = 0; x < w; x++){
            line[x] = int((quint64(sum) * m) >> 32);
            const int in = x + r + 1, out = x - r;
            if(in < w)
                sum += row[in];
            if(out >= 0)
                sum -= row[out];
        }
        for(int x = 0; x < w; x++)
            row[x] = line[x];
    }
    QVector<int> sums_buffer(w, 0);                 // columns
    int *sums = sums_buffer.data();
    for(int y = 0; y <= r && y < h; y++){
        const int *row = g + y*w;
        for(int x = 0; x < w; x++)
            sums[x] += row[x];
    }
    QVector<int> blurred(w * h);
    int *b = blurred.data();
    for(int y = 0; y < h; y++){
        int *dest = b + y*w;
        for(int x = 0; x < w; x++)
            dest[x] = int((quint64(sums[x]) * m) >> 32);
        const int in = y + r + 1, out = y - r;
        if(in < h){
            const int *row = g + in*w;
            for(int x = 0; x < w; x++)
                sums[x] += row[x];
        }
        if(out >= 0){
            const int *row = g + out*w;
            for(int x = 0; x < w; x++)
                sums[x] -= row[x];
        }
    }
    grid.swap(blurred);
}

namespace {
/// @brief The layer's alpha channel as a 0..255 grid (premultiplied ARGB input).
QVector<int> AlphaGrid(const QImage &img)
{
    const int w = img.width(), h = img.height();
    QVector<int> grid(w * h);
    int *g = grid.data();
    for(int y = 0; y < h; y++){
        const QRgb *row = reinterpret_cast<const QRgb*>(img.constScanLine(y));
        int *dest = g + y*w;
        for(int x = 0; x < w; x++)
            dest[x] = qAlpha(row[x]);
    }
    return grid;
}

/// @brief 255 - grid: the area outside the layer, which the inner effects grow inward.
QVector<int> Inverted(QVector<int> grid)
{
    int *g = grid.data();
    for(int i = 0, n = grid.size(); i < n; i++)
        g[i] = 255 - g[i];
    return grid;
}

/// @brief Two box passes of half the reach approximate a gaussian reaching `reach` pixels.
void Blur(QVector<int> &grid, int w, int h, double reach)
{
    const int r = qRound(reach / 2);
    if(r < 1)
        return;
    BoxBlur(grid, w, h, r);
    BoxBlur(grid, w, h, r);
}

/// @brief Grows the covered area by about `reach` pixels: blur, then push every touched pixel
/// back toward opaque (×8 keeps a pixel of antialiasing on the new edge).
void Dilate(QVector<int> &grid, int w, int h, double reach)
{
    if(reach < 0.5)
        return;
    Blur(grid, w, h, qMax(2.0, reach));
    int *g = grid.data();
    for(int i = 0, n = grid.size(); i < n; i++)
        g[i] = qMin(255, g[i] * 8);
}

/// @brief A premultiplied image of `color`, its alpha scaled by the grid and `opacity`. The grid
/// only holds 0..255, so the 256 possible pixels are computed once and looked up.
QImage Tint(const QVector<int> &grid, int w, int h, const QColor &color, double opacity, qreal dpr)
{
    QImage img(w, h, QImage::Format_ARGB32_Premultiplied);
    img.setDevicePixelRatio(dpr);
    const double k = qBound(0.0, color.alphaF() * opacity, 1.0) / 255.0;
    QRgb lut[256];
    for(int v = 0; v < 256; v++){
        const double a = v * k; // 0..1
        lut[v] = qRgba(qRound(color.red() * a), qRound(color.green() * a),
                       qRound(color.blue() * a), qRound(255 * a));
    }
    for(int y = 0; y < h; y++){
        QRgb *row = reinterpret_cast<QRgb*>(img.scanLine(y));
        const int *src = grid.constData() + y*w;
        for(int x = 0; x < w; x++)
            row[x] = lut[src[x]];
    }
    return img;
}

/// @brief How much a soft effect may be shrunk before blurring: 1 (full resolution) under 4 px
/// of blur, up to 4; the shrunk grid always keeps at least 2 px of blur, so it has no detail a
/// smooth upscale would show. The work drops with the square of the factor, which is what keeps
/// a drag inside the layer fluid (the default 4 px shadows and glows run at half size).
int SoftFactor(double blur_reach)
{
    return qBound(1, int(blur_reach / 2), 4);
}

/// @brief Blurs all four premultiplied channels alike (linear, so the result stays premultiplied).
/// A wide blur runs on a smooth-scaled copy at 1/SoftFactor() size and is scaled back over the
/// image (both scalings map the whole image, so nothing shifts).
void BlurImage(QImage &img, double reach)
{
    const int f = SoftFactor(reach);
    if(f > 1){
        const QSize full = img.size();
        QImage small = img.scaled((full.width() + f - 1) / f, (full.height() + f - 1) / f,
                                  Qt::IgnoreAspectRatio, Qt::SmoothTransformation);
        small.setDevicePixelRatio(1);
        BlurImage(small, reach / f);
        const qreal dpr = img.devicePixelRatio();
        img = small.scaled(full, Qt::IgnoreAspectRatio, Qt::SmoothTransformation)
                .convertToFormat(QImage::Format_ARGB32_Premultiplied);
        img.setDevicePixelRatio(dpr);
        return;
    }
    const int r = qRound(reach / 2);
    if(r < 1)
        return;
    const int w = img.width(), h = img.height();
    QVector<int> channel[4];
    for(QVector<int> &c : channel)
        c.resize(w * h);
    int *red = channel[0].data(), *green = channel[1].data(), *blue = channel[2].data(), *alpha = channel[3].data();
    for(int y = 0; y < h; y++){
        const QRgb *row = reinterpret_cast<const QRgb*>(img.constScanLine(y));
        for(int x = 0, i = y*w; x < w; x++, i++){
            red[i] = qRed(row[x]);
            green[i] = qGreen(row[x]);
            blue[i] = qBlue(row[x]);
            alpha[i] = qAlpha(row[x]);
        }
    }
    // one thread per channel: they share nothing
    auto blur_channel = [&channel, w, h, r](int i){
        BoxBlur(channel[i], w, h, r);
        BoxBlur(channel[i], w, h, r);
    };
    std::future<void> jobs[3];
    for(int i = 0; i < 3; i++)
        jobs[i] = std::async(std::launch::async, blur_channel, i);
    blur_channel(3);
    for(std::future<void> &job : jobs)
        job.get();
    red =channel[0].data(); green = channel[1].data(); blue = channel[2].data(); alpha = channel[3].data();
    for(int y = 0; y < h; y++){
        QRgb *row = reinterpret_cast<QRgb*>(img.scanLine(y));
        for(int x = 0, i = y*w; x < w; x++, i++)
            row[x] = qRgba(red[i], green[i], blue[i], alpha[i]);
    }
}

/// @brief Replaces every block-by-block square with its average (premultiplied) colour. The grid
/// lines pass through world's image of the scene origin, so blocks stay put on the canvas while
/// the layer moves. ponytail: blocks are axis-aligned in the image, so a rotated view rotates
/// nothing; anchor the grid in scene space if that ever matters.
void Pixelate(QImage &img, double block, const QTransform &world)
{
    const int b = qRound(block);
    if(b < 2)
        return;
    const int w = img.width(), h = img.height();
    const QPointF origin = world.map(QPointF(0, 0));
    const int ox = ((qRound(origin.x()) % b) + b) % b;
    const int oy = ((qRound(origin.y()) % b) + b) % b;
    for(int by = oy - b; by < h; by += b){
        const int y0 = qMax(by, 0), y1 = qMin(by + b, h);
        for(int bx = ox - b; bx < w; bx += b){
            const int x0 = qMax(bx, 0), x1 = qMin(bx + b, w);
            if(x0 >= x1 || y0 >= y1)
                continue;
            qint64 sum[4] = { 0, 0, 0, 0 };
            for(int y = y0; y < y1; y++){
                const QRgb *row = reinterpret_cast<const QRgb*>(img.constScanLine(y));
                for(int x = x0; x < x1; x++){
                    sum[0] += qRed(row[x]);
                    sum[1] += qGreen(row[x]);
                    sum[2] += qBlue(row[x]);
                    sum[3] += qAlpha(row[x]);
                }
            }
            const qint64 n = qint64(x1 - x0) * (y1 - y0);
            const QRgb average = qRgba(int(sum[0] / n), int(sum[1] / n), int(sum[2] / n), int(sum[3] / n));
            for(int y = y0; y < y1; y++){
                QRgb *row = reinterpret_cast<QRgb*>(img.scanLine(y));
                for(int x = x0; x < x1; x++)
                    row[x] = average;
            }
        }
    }
}

/// @brief Averages f×f blocks into a grid of ceil(w/f) × ceil(h/f) cells; cells reaching past the
/// image count the missing pixels as 0, so block (i, j) covers exactly pixels [i*f, (i+1)*f).
QVector<int> Downscale(const QVector<int> &grid, int w, int h, int f, int *small_w, int *small_h)
{
    const int sw = (w + f - 1) / f, sh = (h + f - 1) / f;
    QVector<int> small(sw * sh, 0);
    int *d = small.data();
    const int *g = grid.constData();
    for(int y = 0; y < h; y++){
        int *dest = d + (y / f) * sw;
        const int *src = g + y*w;
        for(int x0 = 0; x0 < w; x0 += f){ // one cell per f pixels: no division per pixel
            const int x1 = qMin(x0 + f, w);
            int sum = 0;
            for(int x = x0; x < x1; x++)
                sum += src[x];
            *dest++ += sum;
        }
    }
    const int n = f * f;
    for(int i = 0; i < sw * sh; i++)
        d[i] /= n;
    *small_w = sw;
    *small_h = sh;
    return small;
}

/// @brief One effect part ready to be drawn: its tinted image and where it goes (logical pixels).
/// An empty target size means "at target.topLeft(), unscaled" (a full-resolution part).
struct EffectPart
{
    QImage image;
    QRectF target;
};

/// @brief Builds one part of an effect: @p coverage (inverted first when @p invert) grown by
/// @p grow pixels, blurred by @p soften pixels and tinted, to be drawn with its top-left at @p at
/// (image pixels). The work runs at 1/@p f resolution and is drawn back up with smooth scaling.
/// Reads only its arguments, so the parts of one layer are built on separate threads. The
/// dilate and blur also run split into up to @p bands row bands, one thread each: a glow does
/// four box passes, and with a hard edge (blur under 4 px) at full resolution, so one thread
/// per part left it the slowest part by far. Each band carries `halo` extra rows of real data
/// on both sides, as many as the four column passes read, so the rows it keeps equal the
/// one-piece result exactly (the grid's own border reads zeros either way).
EffectPart BuildPart(const QVector<int> &coverage, bool invert, int w, int h, int f, double grow,
                     double soften, const QColor &color, double opacity, QPointF at, qreal dpr,
                     int bands)
{
    int sw = w, sh = h;
    QVector<int> g = invert ? Inverted(coverage) : coverage;
    if(f > 1)
        g = Downscale(g, w, h, f, &sw, &sh);
    const int dilate_r = grow / f < 0.5 ? 0 : qRound(qMax(2.0, grow / f) / 2); // as in Dilate()
    const int halo = 2 * (dilate_r + qMax(0, qRound(soften / f / 2)));         // and Blur()
    bands = qBound(1, bands, sh / qMax(32, 2 * halo)); // thin bands would be mostly halo
    if(bands == 1){
        Dilate(g, sw, sh, grow / f);
        Blur(g, sw, sh, soften / f);
    }
    else{
        QVector<int> result(sw * sh);
        const int band = (sh + bands - 1) / bands;
        auto run = [&g, &result, sw, sh, band, halo, f, grow, soften](int y0){
            const int y1 = qMin(y0 + band, sh);
            const int a = qMax(0, y0 - halo), b = qMin(sh, y1 + halo);
            QVector<int> part(g.constData() + a * sw, g.constData() + b * sw);
            Dilate(part, sw, b - a, grow / f);
            Blur(part, sw, b - a, soften / f);
            std::copy(part.constData() + (y0 - a) * sw, part.constData() + (y1 - a) * sw,
                      result.data() + y0 * sw);
        };
        std::vector<std::future<void>> jobs;
        for(int y0 = band; y0 < sh; y0 += band)
            jobs.push_back(std::async(std::launch::async, run, y0));
        run(0);
        for(std::future<void> &job : jobs)
            job.get();
        g.swap(result);
    }
    if(f == 1)
        return { Tint(g, sw, sh, color, opacity, dpr), QRectF(at / dpr, QSizeF()) };
    return { Tint(g, sw, sh, color, opacity, 1), QRectF(at / dpr, QSizeF(sw * f / dpr, sh * f / dpr)) };
}

/// @brief Waits for a part started by ApplyLayerEffects() and draws it; nothing for a part that
/// was never started (its effect is off).
void PaintPart(QPainter &painter, std::future<EffectPart> &pending)
{
    if(!pending.valid())
        return;
    const EffectPart part = pending.get();
    if(part.target.isEmpty())
        painter.drawImage(part.target.topLeft(), part.image);
    else
        painter.drawImage(part.target, part.image);
}

/// @brief The shadow's offset in image pixels: scene-unit vector through the linear part of world.
QPointF ShadowOffset(const LayerShadow &s, const QTransform &world)
{
    const double rad = qDegreesToRadians(s.angle);
    const QPointF scene_offset(qCos(rad) * s.distance, qSin(rad) * s.distance);
    return world.map(scene_offset) - world.map(QPointF(0, 0));
}
}

/// @details Every size is multiplied by the world scale (sqrt|det|), so an effect measures the
/// same on the canvas at any zoom. The drawing offsets are divided by the image's
/// devicePixelRatio because QPainter positions in logical pixels; the grids are physical.
QAtomicInt layer_effects_runs = 0;
QAtomicInt layer_effects_pixels = 0;
int layer_effects_max_bands = 8; // measured: more threads cost more to start than they save

QImage ApplyLayerEffects(const QImage &source, const LayerOptions &o, const QTransform &world)
{
    layer_effects_runs.fetchAndAddRelaxed(1);
    layer_effects_pixels.storeRelaxed(source.width() * source.height());
    QImage layer = source.convertToFormat(QImage::Format_ARGB32_Premultiplied);
    const int w = layer.width(), h = layer.height();
    if(w == 0 || h == 0)
        return layer;
    const qreal dpr = layer.devicePixelRatio();
    double s = qSqrt(qAbs(world.determinant()));
    if(s <= 0)
        s = 1;
    // the layer's own pixels first: every effect below then follows the blurred/pixelated shape
    if(o.blur.enabled)
        BlurImage(layer, o.blur.intensity * s);
    if(o.pixelate.enabled)
        Pixelate(layer, o.pixelate.size * s, world);
    const QVector<int> mask = AlphaGrid(layer);

    // every enabled part is built on its own thread (they only read the mask), then drawn in
    // order below. ponytail: a thread per part (and band) per recompose; a pool if start-up shows
    const int parts = int(o.outer_shadow.enabled) + int(o.glow.enabled) + int(o.outline.enabled)
            + int(o.inner_shadow.enabled);
    const int bands = qBound(1, int(std::thread::hardware_concurrency()) / qMax(1, parts),
                             layer_effects_max_bands);
    auto start = [&mask, w, h, dpr, bands](bool enabled, bool invert, int f, double grow, double soften,
                                    QColor color, double opacity, QPointF at){
        if(!enabled)
            return std::future<EffectPart>();
        return std::async(std::launch::async, [=, &mask]{
            return BuildPart(mask, invert, w, h, f, grow, soften, color, opacity, at, dpr, bands);
        });
    };
    const double glow_soft = o.glow.smoothness * s;
    std::future<EffectPart> outer_shadow = start(o.outer_shadow.enabled, false,
            SoftFactor(o.outer_shadow.smoothness * s), 0, o.outer_shadow.smoothness * s,
            o.outer_shadow.color, o.outer_shadow.opacity, ShadowOffset(o.outer_shadow, world));
    std::future<EffectPart> outer_glow = start(o.glow.enabled && !o.glow.inside, false,
            SoftFactor(glow_soft), o.glow.size * s, glow_soft, o.glow.color, 1, QPointF(0, 0));
    // the outline keeps full resolution: its edge is hard
    std::future<EffectPart> outline = start(o.outline.enabled, false, 1, o.outline.size * s, 0,
            o.outline.color, o.outline.opacity, QPointF(0, 0));
    std::future<EffectPart> inner_glow = start(o.glow.enabled && o.glow.inside, true,
            SoftFactor(glow_soft), o.glow.size * s, glow_soft, o.glow.color, 1, QPointF(0, 0));
    std::future<EffectPart> inner_shadow = start(o.inner_shadow.enabled, true,
            SoftFactor(o.inner_shadow.smoothness * s), 0, o.inner_shadow.smoothness * s,
            o.inner_shadow.color, o.inner_shadow.opacity, ShadowOffset(o.inner_shadow, world));

    QImage out(w, h, QImage::Format_ARGB32_Premultiplied);
    out.setDevicePixelRatio(dpr);
    out.fill(Qt::transparent);
    QPainter p(&out);
    p.setRenderHint(QPainter::SmoothPixmapTransform); // the reduced-resolution soft parts
    PaintPart(p, outer_shadow);
    PaintPart(p, outer_glow);
    PaintPart(p, outline);
    // the inner effects: painted over the layer and clipped to it (SourceAtop keeps the
    // layer's own alpha, so nothing lands outside the silhouette)
    if(o.colorize.enabled || inner_glow.valid() || inner_shadow.valid()){
        QPainter q(&layer);
        q.setCompositionMode(QPainter::CompositionMode_SourceAtop);
        q.setRenderHint(QPainter::SmoothPixmapTransform);
        if(o.colorize.enabled){
            QColor c = o.colorize.color;
            c.setAlphaF(qBound(0.0, c.alphaF() * o.colorize.opacity, 1.0));
            q.fillRect(QRectF(0, 0, w / dpr, h / dpr), c);
        }
        PaintPart(q, inner_glow);
        PaintPart(q, inner_shadow);
    }
    p.drawImage(0, 0, layer);
    p.end();
    if(o.opacity >= 1)
        return out;
    QImage faded(w, h, QImage::Format_ARGB32_Premultiplied);
    faded.setDevicePixelRatio(dpr);
    faded.fill(Qt::transparent);
    QPainter f(&faded);
    f.setOpacity(qBound(0.0, o.opacity, 1.0));
    f.drawImage(0, 0, out);
    f.end();
    return faded;
}
