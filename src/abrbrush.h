#ifndef ABRBRUSH_H
#define ABRBRUSH_H

#include <QColor>
#include <QImage>
#include <QList>
#include <QString>

class QPainter;
class QPainterPath;

/**
 * @file abrbrush.h
 * @brief Readers for the two Photoshop asset containers the app imports: brush tips
 *        (.abr, used by draw mode) and pattern tiles (.pat, used by pattern fills).
 *
 * Both live in one header/source pair on purpose: the two formats are big-endian PSD
 * containers that share the same per-plane bitmap tail (bounds, depth, compression byte,
 * raw-or-PackBits data), so the RLE decoder and the UTF-16BE name reader are written
 * once in abrbrush.cpp and used by both. Neither reader ever throws or reports a parse
 * error: an unreadable or unsupported file yields an empty list, which the two callers
 * (the brush grid and PatternLibrary, both in the UI layer) present as "nothing to show".
 *
 * @see PatternLibrary for the .pat consumer, MyView::StampBrush() for the .abr one.
 */

/**
 * @brief One brush tip parsed from a Photoshop .abr file.
 *
 * @details `stamp` is an ARGB32 image whose ALPHA channel is the brush coverage
 * (RGB is left black; DrawMode tints the dab with the active brush color, exactly
 * like the built-in round stamp). This matches what MyView::StampBrush() expects
 * of draw_brush_stamp, so an AbrBrush stamp is a drop-in replacement for it.
 */
struct AbrBrush {
    QString name;
    QImage  stamp;
};

/**
 * @brief Parse a Photoshop .abr v1/v2 file, returning every *sampled* brush tip.
 * @param path Filesystem path to the .abr file.
 * @return List of sampled brushes. Empty if the file is missing, unreadable, not
 *         version 1/2, or contains only computed brushes. Computed brushes are
 *         skipped and version 6+ (the 8BIM-based CS format) is not handled — Phase 2
 *         is sampled-v2 scope by design.
 */
QList<AbrBrush> LoadAbr(const QString &path);

/**
 * @brief One pattern tile parsed from a Photoshop .pat file.
 *
 * @details `tile` is an ARGB32 image meant to be handed straight to
 * `QBrush(QPixmap::fromImage(tile))`, i.e. a Qt::TexturePattern brush — which is what
 * makes a pattern fill need no new StickData field and no new serialization code (Qt's
 * own QBrush stream operator writes the texture for us). Unlike AbrBrush::stamp, the
 * alpha channel here is real transparency, not brush coverage: a .pat tile is opaque
 * unless the file carried an alpha plane.
 */
struct PatPattern {
    QString name;   ///< the pattern's own Unicode name, or "Pattern N" if the file has none
    QImage  tile;   ///< ARGB32 tile, ready for QBrush; never null for a returned entry
};

/**
 * @brief Parse a Photoshop .pat file, returning every tile it can decode.
 * @param path Filesystem path to the .pat file. Must carry the '8BPT' signature.
 * @return List of tiles in file order. Empty if the file is missing, unreadable, not a
 *         version-1 '8BPT' container, or contains only tiles in an unsupported flavour.
 *
 * Supported: 8-bit Grayscale, RGB and CMYK tiles, raw or PSD-PackBits compressed — the
 * same self-imposed scope as LoadAbr() plus CMYK, which real-world pattern packs use far
 * more often than the format's age suggests. Bitmap, Indexed, Duotone, Multichannel, Lab
 * and 16-bit tiles are skipped individually rather than failing the whole file, so a mixed
 * library still imports whatever it can.
 */
QList<PatPattern> LoadPat(const QString &path);

/**
 * @brief Everything one brush tip needs to be laid down: the tip itself, what it is painted
 *        with, and how much each dab is allowed to differ from the last.
 *
 * The one description of a brush shared by the two places that stamp with one — draw mode's
 * freehand canvas (MyView) and a stick's "Brush" BORDER (StickData::stroke_brush, painted by
 * StickGraphicsItem) — so the two cannot drift about what "25% spacing" or "40% size jitter"
 * mean. Both stamp through BrushDab().
 *
 * @note @c color, @c size and @c opacity are DERIVED for a stick border and are deliberately
 * not serialized with the rest (see StickData::stroke_brush): a border's colour is the pen's
 * colour, its dab diameter is the pen's width and its opacity is the pen's opacity, so the
 * thickness/opacity/colour controls that already exist keep driving it and there is one number
 * per thing rather than two that can disagree. Draw mode fills all three itself.
 */
struct BrushStroke {
    QImage stamp;               ///< tip mask, ALPHA = coverage (RGB ignored); null = no brush
    QColor color = Qt::black;   ///< tint applied to every dab; its ALPHA is ignored — see @c opacity
    double size = 20;           ///< dab diameter in the painter's logical units
    double opacity = 1.0;       ///< 0..1, before any jitter, and the ONLY place opacity lives
    qint32 spacing = 25;        ///< dab pitch as a % of @c size; over 100% the dabs separate
    qint32 angle = 0;           ///< tip rotation in degrees
    qint32 size_jitter = 0;     ///< % of random per-dab size REDUCTION (0 = every dab full size)
    qint32 angle_jitter = 0;    ///< % of random per-dab rotation (100 = anywhere in the circle)
    qint32 opacity_jitter = 0;  ///< % of random per-dab opacity REDUCTION
};

/**
 * @brief Builds one tinted, jittered dab image.
 * @param brush the brush; only stamp/color/size/angle and the three jitters are read.
 * @param size_factor extra multiplier on the diameter (pen pressure in draw mode, 1.0 elsewhere).
 * @param opacity_factor extra multiplier on the opacity (same).
 * @param out_opacity receives the opacity 0..1 this dab must be painted at — jitter included, so
 *        the caller cannot forget it. May be null.
 * @return an ARGB image already tinted; a rotated dab is LARGER than @p size, so centre it on the
 *         returned image's own dimensions, never on the requested diameter.
 *
 * Rotation is applied to the scaled stamp rather than baked into the mask, which is what makes
 * the angle and the angle jitter work on an imported .abr tip as well as on a generated round one.
 */
QImage BrushDab(const BrushStroke &brush, double size_factor = 1.0, double opacity_factor = 1.0,
                qreal *out_opacity = nullptr);

/**
 * @brief Stamps a brush along a path, at the brush's own spacing.
 * @param painter target, in whatever coordinates @p path is expressed in.
 * @param path the line to stamp along; every subpath is walked, closing edges included.
 * @param brush the brush. A null stamp or a non-positive size paints nothing.
 *
 * The pitch is measured along the WHOLE path rather than per subpath segment, so a corner does
 * not restart the rhythm and re-stamp on top of itself. Painter opacity and transform are saved
 * and restored.
 *
 * ponytail: QPainterPath::pointAtPercent() re-flattens the path per call, so this is O(dabs x
 * path complexity) — fine for a stick-sized outline, and the route if a huge POLYGON ever drags
 * is to flatten once with toSubpathPolygons() and walk the segments by hand.
 */
void StampAlongPath(QPainter *painter, const QPainterPath &path, const BrushStroke &brush);

#endif // ABRBRUSH_H
