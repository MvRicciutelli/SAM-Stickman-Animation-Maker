#ifndef VECTORSHAPE_H
#define VECTORSHAPE_H
#include <abrbrush.h>   // BrushStroke: a shape's border brush, like a stick's
#include <QBrush>
#include <QList>
#include <QPainterPath>
#include <QPen>
#include <QPointF>
#include <QString>
#include <QTransform>

class StickData;

/**
 * @brief One anchor of a VectorShape: a point plus its two bezier handles.
 *
 * Every coordinate is in the owning stick's normalized local frame (StickData::PolygonTransform()),
 * like a POLYGON's vertices, so the shape moves, turns and scales with the stick for free. The
 * handles are OFFSETS from @c pos, so moving a node carries its handles along. A zero handle means
 * "no curve on that side": a segment whose two inner handles are both zero is a straight line.
 */
struct VectorNode
{
    QPointF pos;                     ///< the anchor, normalized stick-local
    QPointF in_handle;               ///< control point of the segment arriving here, as an offset from pos
    QPointF out_handle;              ///< control point of the segment leaving here, as an offset from pos
    bool smooth = false;             ///< dragging one handle keeps the other collinear (Alt+drag breaks it)
};

/**
 * @brief One shape layer of a VECTOR stick: an open or closed bezier path with its own paint.
 *
 * Pure value type: copying it copies everything (textures are copy-on-write QImages), so a
 * stick's list of shapes, an undo snapshot and the shape clipboard need no ownership rules. The
 * paint fields have exactly the names and meaning of StickData's own, so the whole fill/stroke
 * machinery of StickGraphicsItem (gradients, textures, opacity, tiling, brush border) applies to
 * a shape unchanged — see StickGraphicsItem::paint_shape.
 */
struct VectorShape
{
    QString name;                    ///< the layer's name in the vector panel
    bool visible = true;             ///< hidden shapes are neither painted nor hit by buckets/lasso
    bool closed = false;             ///< a closing segment runs from the last node back to the first
    QList<VectorNode> nodes;
    QPen pen;                        ///< stroke paint and width (see StickData::pen)
    QBrush brush;                    ///< fill paint; an open shape is filled as if closed (see StickData::brush)
    bool brush_follows_stick = true; ///< see StickData::brush_follows_stick
    bool texture_tiled = true;       ///< see StickData::texture_tiled
    BrushStroke stroke_brush;        ///< see StickData::stroke_brush
};

/**
 * @brief The shape as a path, every point mapped through @p to_scene.
 * @param shape the shape to trace.
 * @param to_scene normalized stick-local -> scene, i.e. the stick's PolygonTransform(); an affine
 *        map takes a bezier onto the bezier of the mapped control points, so mapping them is exact.
 * @return cubic segments where a segment has a handle, straight ones otherwise, plus the closing
 *         segment of a closed shape. Empty for a shape with no node.
 */
QPainterPath VectorPath(const VectorShape &shape, const QTransform &to_scene);

/**
 * @brief The shape with its nodes and handles mapped through @p xf (handles stay offsets).
 *
 * Copy/paste uses it to lift a shape into scene space and drop it into another stick's frame.
 */
VectorShape MapShape(const VectorShape &shape, const QTransform &xf);

/**
 * @brief True when two shape lists can be interpolated: same number of shapes and, shape by
 *        shape, the same node count and the same open/closed state.
 */
bool VectorShapesCompatible(const QList<VectorShape> &a, const QList<VectorShape> &b);

/**
 * @brief Index of the topmost visible shape of @p stick under @p scene_pt, or -1.
 * @param radius pick tolerance in scene units around the outline (the stroke counts at least
 *        half its pen width).
 *
 * A shape is hit inside its filled area (an open shape counts as closed) or on its outline, for
 * both buckets: the bucket names what is painted, not where one has to aim.
 */
int ShapeAt(const StickData &stick, const QPointF &scene_pt, qreal radius);

/**
 * @brief The border brush ready to stamp with: the stored tip and dynamics plus the three numbers
 *        that are the pen's (colour, dab diameter, opacity). Shared by StickData::StrokeBrush()
 *        and the shapes of a VECTOR stick.
 */
BrushStroke StrokeBrushFor(const BrushStroke &stored, const QPen &pen);

#endif // VECTORSHAPE_H
