#include "vectorshape.h"
#include "stickdata.h"
#include <QPainterPathStroker>

/// @details A segment is straight only when BOTH inner handles are zero: one handle alone still
/// bends it (the other control point then sits on its anchor), which is how a corner node next to
/// a smooth one looks in every vector editor.
QPainterPath VectorPath(const VectorShape &shape, const QTransform &to_scene)
{
    QPainterPath path;
    const int n = shape.nodes.size();
    if(n == 0)
        return path;
    auto segment = [&](const VectorNode &a, const VectorNode &b){
        const QPointF end = to_scene.map(b.pos);
        if(a.out_handle.isNull() && b.in_handle.isNull())
            path.lineTo(end);
        else
            path.cubicTo(to_scene.map(a.pos + a.out_handle), to_scene.map(b.pos + b.in_handle), end);
    };
    path.moveTo(to_scene.map(shape.nodes[0].pos));
    for(int i = 1; i < n; i++)
        segment(shape.nodes[i - 1], shape.nodes[i]);
    if(shape.closed && n >= 2){
        segment(shape.nodes[n - 1], shape.nodes[0]);
        path.closeSubpath();
    }
    return path;
}

/// @details A handle is an offset, so it maps through the LINEAR part of @p xf: map both of its
/// ends and subtract, which drops the translation whatever the matrix is.
VectorShape MapShape(const VectorShape &shape, const QTransform &xf)
{
    VectorShape mapped = shape;
    for(VectorNode &node : mapped.nodes){
        const QPointF pos = xf.map(node.pos);
        node.in_handle = xf.map(node.pos + node.in_handle) - pos;
        node.out_handle = xf.map(node.pos + node.out_handle) - pos;
        node.pos = pos;
    }
    return mapped;
}

bool VectorShapesCompatible(const QList<VectorShape> &a, const QList<VectorShape> &b)
{
    if(a.size() != b.size())
        return false;
    for(int i = 0; i < a.size(); i++)
        if(a[i].nodes.size() != b[i].nodes.size() || a[i].closed != b[i].closed)
            return false;
    return true;
}

/// @details Top first, i.e. from the end of the list, which is the order they are painted in
/// reverse. The outline test widens the path by the pick radius so a hairline stays clickable.
int ShapeAt(const StickData &stick, const QPointF &scene_pt, qreal radius)
{
    const QTransform to_scene = stick.PolygonTransform();
    for(int i = stick.vector_shapes.size() - 1; i >= 0; i--){
        const VectorShape &shape = stick.vector_shapes[i];
        if(!shape.visible || shape.nodes.size() < 2)
            continue;
        const QPainterPath path = VectorPath(shape, to_scene);
        if(path.contains(scene_pt))
            return i;
        QPainterPathStroker stroker;
        stroker.setWidth(2 * qMax(radius, shape.pen.widthF() / 2));
        if(stroker.createStroke(path).contains(scene_pt))
            return i;
    }
    return -1;
}

/// @details `pen.brush().color()` rather than `pen.color()`: the two disagree for a pen whose
/// paint is not solid, and the brush's is the one an opacity slider writes into. The width floor
/// keeps a hairline pen from asking for a zero-size dab.
BrushStroke StrokeBrushFor(const BrushStroke &stored, const QPen &pen)
{
    BrushStroke brush = stored;
    brush.color = pen.brush().color();
    brush.size = qMax(1.0, pen.widthF());
    brush.opacity = ReadPaintOpacity(pen.brush());
    return brush;
}
