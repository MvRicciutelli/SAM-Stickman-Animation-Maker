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

    /// @brief Field by field. A new field must be compared here too (see VectorShape::operator==).
    bool operator==(const VectorNode &other) const
    {
        return pos == other.pos && in_handle == other.in_handle && out_handle == other.out_handle
                && smooth == other.smooth;
    }
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
    static constexpr qint32 NO_MASK = -1; ///< the value of @c mask for a shape nothing masks

    QString name;                    ///< the layer's name in the vector panel
    bool visible = true;             ///< hidden shapes are neither painted nor hit by buckets/lasso
    bool closed = false;             ///< a closing segment runs from the last node back to the first
    QList<VectorNode> nodes;
    QPen pen;                        ///< stroke paint and width (see StickData::pen)
    QBrush brush;                    ///< fill paint; an open shape is filled as if closed (see StickData::brush)
    bool brush_follows_stick = true; ///< see StickData::brush_follows_stick
    bool texture_tiled = true;       ///< see StickData::texture_tiled
    BrushStroke stroke_brush;        ///< see StickData::stroke_brush
    double depth_offset = 0;         ///< own depth, as a distance from the owning stick's z; read only
                                     ///< while StickData::vector_depth_enabled. 0 = in the stick's own
                                     ///< plane, where the layer order decides (see StickData::VectorShapeDepth())
    qint32 mask = NO_MASK;           ///< index, in the same stick's list, of the shape masking this one, or
                                     ///< NO_MASK. Never this shape nor a loop (MaskChainReaches()); every removal
                                     ///< and reorder goes through RemoveVectorShape()/ReorderVectorShape() to
                                     ///< keep it naming the same shape. A hidden mask still masks.
    bool mask_inverted = false;      ///< false: transparent where the mask is (alpha x (1 - mask alpha));
                                     ///< true: shown only where the mask is (alpha x mask alpha)

    /**
     * @brief Field by field, every field.
     *
     * StickGraphicsItem::CachedMaskedShape() reuses a composed image while its stick's shapes
     * compare equal: a field added to this struct and not compared here would leave a stale
     * masked image on screen after it changes.
     */
    bool operator==(const VectorShape &other) const
    {
        return name == other.name && visible == other.visible && closed == other.closed
                && nodes == other.nodes && pen == other.pen && brush == other.brush
                && brush_follows_stick == other.brush_follows_stick
                && texture_tiled == other.texture_tiled && stroke_brush == other.stroke_brush
                && depth_offset == other.depth_offset && mask == other.mask
                && mask_inverted == other.mask_inverted;
    }
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
 * @brief One node mapped through @p xf: the anchor through the whole map, the handles (offsets)
 *        through its linear part. The Transform tool maps the selected nodes with it.
 */
VectorNode MapNode(const VectorNode &node, const QTransform &xf);

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

// The three mask helpers below are templates over the layer type: they serve a VECTOR stick's
// VectorShape list and a FREEHAND stick's FreehandLayer list (stickdata.h) alike. A layer type
// needs `mask`, `mask_inverted` and a static `NO_MASK`; a mask is an index into the same list.

/**
 * @brief Removes layer @p index, keeping every other layer's mask on the layer it named: a mask
 *        on the removed layer is cleared (inversion included), a mask above it shifts down.
 * @param shapes a VECTOR stick's shape list or a FREEHAND stick's layer list.
 * @param index a valid index into @p shapes.
 * @details A mask is an index into the same list, so the one edit that can leave it naming the
 * wrong layer, or none, is a change of the list's order or length: this and ReorderVectorShape()
 * are those edits.
 */
template <typename Layer>
void RemoveVectorShape(QList<Layer> &shapes, int index)
{
    shapes.removeAt(index);
    for(Layer &shape : shapes){
        if(shape.mask == index){
            shape.mask = Layer::NO_MASK;
            shape.mask_inverted = false;
        }
        else if(shape.mask > index)
            shape.mask--;
    }
}

/**
 * @brief Moves layer @p from to position @p to, like QList::move(), with the masks following the
 *        layers they name.
 * @param shapes a VECTOR stick's shape list or a FREEHAND stick's layer list.
 * @param from valid index of the layer to move.
 * @param to valid index it ends up at.
 * @details QList::move() shifts the layers between the two places by one towards @p from.
 */
template <typename Layer>
void ReorderVectorShape(QList<Layer> &shapes, int from, int to)
{
    shapes.move(from, to);
    for(Layer &shape : shapes){
        if(shape.mask == from)
            shape.mask = to;
        else if(from < to && shape.mask > from && shape.mask <= to)
            shape.mask--;
        else if(from > to && shape.mask >= to && shape.mask < from)
            shape.mask++;
    }
}

/**
 * @brief True when following the masks from layer @p from (itself included) reaches layer
 *        @p target, i.e. when giving @p target the mask @p from would close a loop.
 * @param shapes a VECTOR stick's shape list or a FREEHAND stick's layer list.
 * @param from the layer the walk starts at; out of range reaches nothing.
 * @param target the layer looked for.
 * @details The step bound only matters for a list that already holds a loop, which no edit makes
 * and the readers break on load.
 */
template <typename Layer>
bool MaskChainReaches(const QList<Layer> &shapes, int from, int target)
{
    for(int steps = 0; from >= 0 && from < shapes.size() && steps <= shapes.size(); steps++){
        if(from == target)
            return true;
        from = shapes[from].mask;
    }
    return false;
}

/**
 * @brief Whether layer @p index may take mask @p mask with inversion @p inverted: the one check
 *        behind MyView::SetVectorShapeMask() and MyView::SetFreehandLayerMask().
 * @param inverted forced to false when @p mask is NO_MASK (no mask, no inversion).
 * @return false for an index or mask out of range, the settings the layer already has, or a mask
 *         that would close a loop (a layer masked by itself through any chain).
 */
template <typename Layer>
bool MaskChangeAllowed(const QList<Layer> &shapes, int index, int mask, bool &inverted)
{
    if(index < 0 || index >= shapes.size() || mask < Layer::NO_MASK || mask >= shapes.size())
        return false;
    if(mask == Layer::NO_MASK)
        inverted = false;
    const Layer &shape = shapes.at(index);
    return !((shape.mask == mask && shape.mask_inverted == inverted)
             || (mask >= 0 && MaskChainReaches(shapes, mask, index)));
}

/**
 * @brief Load-time repair: a mask that does not name another layer of the list, or closes a loop
 *        (only a corrupt or truncated file can hold one), becomes NO_MASK, not inverted, so the
 *        painters' recursion over a chain always ends.
 */
template <typename Layer>
void BreakBadMasks(QList<Layer> &shapes)
{
    for(int i = 0; i < shapes.size(); i++){
        Layer &shape = shapes[i];
        if(shape.mask < 0 || shape.mask >= shapes.size() || MaskChainReaches(shapes, shape.mask, i)){
            shape.mask = Layer::NO_MASK;
            shape.mask_inverted = false;
        }
    }
}

/**
 * @brief Re-expresses the masks of copied layers inside the copy: a mask is kept only when its
 *        layer was copied too, as that layer's place in @p copied; otherwise NO_MASK, not inverted.
 * @param copied the copied layers, in the order of @p sources.
 * @param sources the index each copied layer had in its stick's list.
 */
template <typename Layer>
void KeepCopiedMasks(QList<Layer> &copied, const QList<int> &sources)
{
    for(Layer &shape : copied){
        const int place = sources.indexOf(shape.mask);
        shape.mask = place >= 0 ? place : Layer::NO_MASK; // its mask stayed behind
        shape.mask_inverted = shape.mask_inverted && place >= 0;
    }
}

/**
 * @brief Cuts @p image by @p mask, pixel by pixel on the same grid: DestinationOut scales the
 *        image's alpha by (1 - mask alpha), DestinationIn (@p inverted) by the mask alpha.
 *        Shared by a masked vector shape and a masked freehand layer.
 * @param image premultiplied image to cut in place.
 */
void CutByMask(QImage &image, const QImage &mask, bool inverted);

/**
 * @brief Index of the topmost visible shape of @p stick under @p scene_pt, or -1. "Topmost" is
 *        the drawing order: the layer order, or the shapes' own depths when the stick uses them.
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
