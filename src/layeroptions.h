#ifndef LAYEROPTIONS_H
#define LAYEROPTIONS_H
#include <QColor>
#include <QDataStream>
#include <QImage>
#include <QTransform>
#include <QVector>
#include <QAtomicInt>

/**
 * @file layeroptions.h
 * @brief Per-layer (per-stickfigure) effects: opacity, outer/inner shadow, glow, outline and
 *        colorize, plus the pure-image routine that paints them.
 *
 * Pure data with no scene dependency, so StickFigureData can carry it through undo snapshots,
 * copy/paste, the library and files. The scene side (LayerGroupItem/LayerEffect, stick.h)
 * only feeds ApplyLayerEffects() the layer rendered off-screen. Sizes are in scene units.
 * See docs/ARCH_LAYER_OPTIONS.md.
 */

/**
 * @brief A shadow cast by the whole layer, outside it (drop shadow) or inside its edge.
 */
struct LayerShadow
{
    bool enabled = false;        ///< on/off switch (never interpolated)
    double angle = 45;           ///< direction the shadow falls toward, degrees, 0 = right, clockwise
    double distance = 5;         ///< shadow offset, scene units
    QColor color = Qt::black;    ///< shadow colour
    double smoothness = 4;       ///< blur reach, scene units (0 = hard edge)
    double opacity = 0.75;       ///< 0..1
    /// @brief Field-wise equality. @param o other value. @return true when every field matches.
    bool operator==(const LayerShadow &o) const;
};

/**
 * @brief A glow around the layer's silhouette, outside it or inside its edge.
 */
struct LayerGlow
{
    bool enabled = false;                  ///< on/off switch (never interpolated)
    bool inside = false;                   ///< type: false = outside, true = inside (never interpolated)
    QColor color = QColor(255, 255, 160);  ///< glow colour; its alpha is the glow's opacity
    double size = 5;                       ///< how far the solid part spreads, scene units
    double smoothness = 4;                 ///< blur reach beyond `size`, scene units
    /// @brief Field-wise equality. @param o other value. @return true when every field matches.
    bool operator==(const LayerGlow &o) const;
};

/**
 * @brief A solid stroke all around the layer's silhouette, outside it.
 */
struct LayerOutline
{
    bool enabled = false;      ///< on/off switch (never interpolated)
    QColor color = Qt::black;  ///< stroke colour
    double size = 2;           ///< stroke width, scene units
    double opacity = 1;        ///< 0..1
    /// @brief Field-wise equality. @param o other value. @return true when every field matches.
    bool operator==(const LayerOutline &o) const;
};

/**
 * @brief A solid colour laid over everything the layer paints.
 */
struct LayerColorize
{
    bool enabled = false;      ///< on/off switch (never interpolated)
    QColor color = Qt::red;    ///< overlay colour
    double opacity = 1;        ///< 0..1; 1 replaces the layer's colours entirely
    /// @brief Field-wise equality. @param o other value. @return true when every field matches.
    bool operator==(const LayerColorize &o) const;
};

/**
 * @brief Blurs everything the layer paints.
 */
struct LayerBlur
{
    bool enabled = false;      ///< on/off switch (never interpolated)
    double intensity = 4;      ///< blur reach, scene units
    /// @brief Field-wise equality. @param o other value. @return true when every field matches.
    bool operator==(const LayerBlur &o) const;
};

/**
 * @brief Turns the layer into square blocks of its average colour.
 */
struct LayerPixelate
{
    bool enabled = false;      ///< on/off switch (never interpolated)
    double size = 8;           ///< block side, scene units; the grid is anchored to the scene origin
    /// @brief Field-wise equality. @param o other value. @return true when every field matches.
    bool operator==(const LayerPixelate &o) const;
};

/**
 * @brief Every option of one layer. The defaults leave the layer exactly as it renders without
 *        options (IsActive() false).
 */
struct LayerOptions
{
    double opacity = 1;          ///< whole-layer opacity, 0..1, effects included
    LayerShadow outer_shadow;    ///< drop shadow behind the layer
    LayerShadow inner_shadow;    ///< shadow inside the layer's edge
    LayerGlow glow;              ///< inside or outside glow
    LayerOutline outline;        ///< stroke around the layer
    LayerColorize colorize;      ///< solid colour overlay
    LayerBlur blur;              ///< blur of the layer's pixels (before the other effects)
    LayerPixelate pixelate;      ///< pixelation of the layer's pixels (after blur, before the other effects)

    /**
     * @brief Whether the layer needs compositing at all.
     * @return true when the opacity is below 1 or any effect is switched on.
     */
    bool IsActive() const;

    /**
     * @brief How far the enabled effects reach beyond the layer's own pixels.
     * @return padding in scene units (0 when nothing reaches outside).
     */
    double Reach() const;

    /// @brief Field-wise equality. @param o other value. @return true when every field matches.
    bool operator==(const LayerOptions &o) const;
    /// @brief Negation of operator==. @param o other value. @return true when any field differs.
    bool operator!=(const LayerOptions &o) const { return !(*this == o); }
};

/**
 * @brief Writes the options (StickFigureData format v2 trailer).
 * @param stream destination. @param options value to write. @return the stream.
 */
QDataStream &operator<<(QDataStream &stream, const LayerOptions &options);

/**
 * @brief Reads the options written by operator<<.
 * @param stream source. @param options value to fill. @return the stream.
 */
QDataStream &operator>>(QDataStream &stream, LayerOptions &options);

/**
 * @brief Blends two keyframes' options for an inbetween.
 * @param target options to rewrite; its switches and glow type are kept (they come from the
 *        base frame, like stick types).
 * @param start options at t = 0.
 * @param end options at t = 1.
 * @param t blend factor, 0..1.
 */
void InterpolateLayerOptions(LayerOptions &target, const LayerOptions &start,
                             const LayerOptions &end, float t);

/**
 * @brief Paints the effects around and over a layer rendered off-screen.
 * @param layer the layer's pixels, padded by at least Reach() on every side.
 * @param options the effects to apply.
 * @param world scene-to-image-pixel transform of `layer`: its scale converts scene-unit sizes
 *        to pixels, its rotation turns the shadow direction and its translation anchors the
 *        pixelate grid to the scene origin, so effects keep their canvas size, direction and
 *        grid at any zoom, view rotation or layer position.
 * @return premultiplied ARGB image of the same size: the layer blurred then pixelated, and
 *         from that outer shadow, outside glow, outline, the layer with colorize/inside
 *         glow/inner shadow on top, all at `options.opacity`.
 */
QImage ApplyLayerEffects(const QImage &layer, const LayerOptions &options, const QTransform &world);

/**
 * @brief How many times ApplyLayerEffects() has run since start-up. Diagnostic only: the tests
 *        read it to pin that LayerEffect recomposes only when the layer changes.
 */
extern QAtomicInt layer_effects_runs;

/**
 * @brief Pixel count of the last image ApplyLayerEffects() composed. Diagnostic only: the tests
 *        read it to pin that a zoomed-in layer is composed for the viewport, not in full.
 */
extern QAtomicInt layer_effects_pixels;

/**
 * @brief One box-blur pass along x then y over a w×h grid of 0..255 values.
 * @param grid values, row-major, rewritten in place.
 * @param w grid width. @param h grid height.
 * @param r box radius in cells; pixels outside the grid count as 0.
 */
void BoxBlur(QVector<int> &grid, int w, int h, int r);

#endif // LAYEROPTIONS_H
