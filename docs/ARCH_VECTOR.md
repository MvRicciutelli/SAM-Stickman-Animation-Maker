### The VECTOR stick

`StickData::VECTOR` (tool `DRAWVECTOR`, both 30 — the tool-id == stick-type
convention; 26-29 were taken) is a stick that carries a stack of Bezier
**shapes**, each with its own fill and stroke, like layers in a vector editor.
The toolbar button is `drawVectorBtn` (`ui/widget.ui`, icons
`resources/addvector_0000{1,2,3}.png`, generated in the style of the other
draw-tool icons). Drawing is two clicks (origin, free end) like every type; the
2-click preview is the skeleton, which `paint()` draws only while no shape is
visible. Everything after that is shapes, edited in a modal "vector draw mode".

### Data: shapes are values

`StickData::vector_shapes` is a `QList<VectorShape>`; `VectorNode` (`pos`,
`in_handle`, `out_handle`, `smooth`) and `VectorShape` (`name`, `visible`,
`closed`, `nodes`, `pen`, `brush`, `brush_follows_stick`, `texture_tiled`,
`stroke_brush`) are plain value types in `vectorshape.h`. The paint fields have
`StickData`'s names and meaning on purpose: that is what lets the whole
fill/stroke machinery be reused per shape (see Rendering). **Handles are OFFSETS
from `pos`**, not points, so moving an anchor needs no handle bookkeeping and a
transform maps them through the linear part only (`MapShape()`). List index is
drawing order, last on top.

Coordinates are normalized stick-local through `StickData::PolygonTransform()`,
exactly like POLYGON, so shapes follow, rotate and scale with the stick and its
Stick Transform. Value semantics mean there are **no ownership rules**: delete,
`Clone()`, undo snapshots and the clipboard all just copy, and textures are
copy-on-write. The one trap is that `CopyFrom()` is hand-written and must copy
`vector_shapes`; `VectorStickRoundTrip` fails without that line (counterproof
done).

Free functions in `vectorshape.cpp`: `VectorPath()` (a `cubicTo` where a segment
has a handle, a `lineTo` when both inner handles are zero, plus the closing
segment), `MapShape()`, `VectorShapesCompatible()`, `ShapeAt()` (topmost visible
shape hit by area or by outline, the outline widened by the pick radius) and
`StrokeBrushFor()` (shared with `StickData::StrokeBrush()`).

### Serialization

`StickData::CURRENT_VERSION` went 10 to 11. VECTOR sits in the type-gated `img_*`
block (`PolygonTransform()` reads `img_offset`/`img_angle_offset`) and then
`WriteVectorShapes()` follows with its OWN `qint32` `VECTOR_SHAPES_VERSION` (1;
newer is refused through `RejectNewerVersion()`), a `qint32` count, and per shape
name/visible/closed, the nodes, then pen, brush and stroke-brush tip. Those go
through the SAME file-static image-table helpers as the stick record
(`WriteRecordPen`/`Brush`/`Image`), so a `.sam` v3 dedupes shape textures;
`WriteStrokeBrush`/`ReadStrokeBrush` were factored out of the stick record for
this. `VectorStickRoundTrip` pins it.

### Rendering: the `paint_shape` seam

The VECTOR branch of `paint()` draws in SCENE coordinates (it is in
`PainterIsLocalFor()`'s scene list). Rather than duplicate the fill/stroke code,
`StickGraphicsItem::paint_shape` (a `const VectorShape*`) is set while one shape
is painted, and the accessors `PaintFollowsStick()`, `PaintTiled()` and
`PaintStrokeBrush()` answer for the shape or for the stick. `AlignBrush()`,
`TextureToScene()`, `SingleInstancePath()`, `FillAndStroke()` and
`StrokeWithBrush()` read through them. **A helper that reads
`data->brush_follows_stick`, `texture_tiled` or `stroke_brush` directly silently
paints every shape with the stick's flags.** `VectorShapesRenderPerShapePaintInLayerOrder`
is the pin: making `PaintTiled()` return the stick's flag fails it (counterproof
done).

Onion skins grey the shape pen. `ConstructBr()` unions anchors, handle tips and
the drawing preview. The edit overlay `PaintVectorOverlay()` (anchor squares,
handles of selected anchors and of the shape being drawn, a dashed
next-segment preview) is live UI only and never reaches a render. Rotate,
Manipulate, `PreviewDrawing` and `SolveIkChain` `img_angle` lists, and `Scale()`
(shape pen widths), include VECTOR. A master change re-anchors anchors AND
handle tips (`CaptureVertexScenePositions()`/`ReanchorVertexScenePositions()`),
pinned by `ChangeMasterKeepsVectorShape`.

### Interaction (`myview.cpp`, end of file)

`MyView::vector_tool` (`VectorTool`: `VT_NONE`, `VT_PEN`, `VT_LASSO`, `VT_FILL`,
`VT_STROKE`; `VT_PEN` is "vector draw mode") is modal like draw mode:
`VectorMousePress/Move/Release/KeyPress` run before the tool switch. **No
`StickData*` is stored**: `VectorStick()` re-derives the current stick on every
use (the `PolySessionStick()` rule), and gesture state is indices
(`vec_drawing_shape`, `vec_edit_shape/node/side`) re-checked against the stick.
Every gesture must name its shape and anchor: `VectorEditPress` once left
`vec_edit_shape` unset for an anchor press, a stale index from a previous stick
made the move gesture abort.

Pen: a click is a corner anchor, click+drag a smooth anchor with symmetric
handles, a click within the pick radius of the first anchor (shape has >= 2)
closes, a right click ends an open shape, and the next click starts a new top
shape with the current paint (`my_pen`/`my_brush`/...). One `StoreUndo()` per
anchor, the placing drag takes none. `EndVectorShape()` drops a one-anchor shape.

The mode is entered by `DrawCmd()`'s 2nd click for `DRAWVECTOR` and by the
panel's pen button. It is left by Esc, Enter, a double click, `SetTool()`
(`ChangeTool`), a REAL frame change, and `ExitDrawMode()`, which calls
`ExitVectorMode()`, so every existing "end the drawing session" call site ends
vector mode too. `MoveToFrame()` exits only when the frame differs: undo/redo
goes through `MoveToFrame(current)` and must keep the mode.

Editing: Ctrl+press on a selected vector stick (selection tools NOTOOL/CLAMP/
DISPLACE, or any vector tool) picks handles of selected anchors first, then the
nearest anchor; Shift toggles. A drag moves the selected anchors (handles follow,
being offsets). A handle drag keeps a smooth anchor's opposite handle collinear
at its own length, Alt breaks it to a corner. `StoreUndo()` is deferred to the
drag threshold, so a click leaves no empty step. A consumed press clears
`is_pressed` so no tool drag starts, and a double click outside pen mode is
treated as the press it replaces (re-grabbing an anchor quickly is a double
click to Qt). The lasso (`VT_LASSO`, drawn in `drawForeground()`) ends in
`LassoSelectVectorNodes()`. Delete runs `DeleteSelectedVectorNodes()` first (one
undo step, shapes left with < 2 anchors go), then the old vertex/stick deletion.

The selection (`selected_vector_nodes`, a `QSet<QPair<shape,node>>`) lives on the
view and is cleared by `Animation::SwitchVectorStick()` whenever the current stick
changes (all three `UpdateSelection` overloads), by `DeselectAllVertices()` (tool
switches, plain clicks) and on a real frame change. `DrawVectorViaRealMouseEvents`
and `VectorNodeEditingLassoAndDelete` pin the gestures.

### Current shape and the Style column

`Animation::vector_shape_row` is the current shape (selection state, clamped by
`CurrentVectorShape()`); `SwitchVectorStick()` points it at the new stick's top
shape. `StickData::Paint(shape_index)` returns a `PaintRef` of pointers (pen,
brush, follows, tiled): the current shape's for a VECTOR stick, the stick's
otherwise. `UpdateStyleControls()` echo, `Widget::SetPaintOpacity` live edit and
the Texture-follows/Tile check boxes all go through it. `SetGraphics()` (Apply)
goes through `ApplyStyleToStick()`: the stick fields always, plus the current
shape (single Apply) or every shape ("all"). The buckets use `BucketVectorShape()`
(via `ShapeAt()`) and `ApplyStyleToShape()` (fill: brush/follows/tiled; stroke:
`CopyPenPaint()` + width + border brush), one undo step each.
`VectorStyleTargetsTheCurrentShape` pins it. The texture arrow is gated off for
VECTOR (`TextureArrowPoints`, `UpdateStyleControls`): v1 scope.

### Panel, clipboard, inbetweens

`Widget::BuildVectorPanel()` builds the code-built `advanced_vector_tab` ("Vector
Shapes", inserted by `Animation::UpdateTab`): tool buttons in a NON-exclusive
`QButtonGroup` (so "no tool" is possible, synced by `VectorToolChanged`), and a
layer `QListWidget` listing the TOP shape first (row `r` = shape `count-1-r`)
with a check box for visibility, an editable name, a 32px thumbnail
(`VectorShapeIcon`, plain `QPainter`) and Up/Down/Delete/Copy/Paste.
`refresh_vector_panel` (`uiItems.h`, null in tests) is rebuilt on
`VectorShapesChanged` and on `undo_stack` `indexChanged`. **Both connections are
QUEUED**: a rename or visibility edit fires from inside the list's own
`itemChanged`, and rebuilding there deletes the item being reported.
`PixelIcon`/`DashedIcon`/`ToolButtonStyle` were lifted out of the draw panel to
file-level statics and are shared.

`MyView::vector_clipboard` is a `QList<VectorShape>` in SCENE space (`MapShape`
through the source stick's `PolygonTransform()`). Paste maps through the target's
inverse, appends on top, leaves the buffer (repeatable, survives frame changes)
and keeps widths absolute. There are no Ctrl+C/V shortcuts, the app has none.

`GenerateInbetweens()` refuses (no change, no undo step, an explanatory message)
unless `VectorShapesCompatible()`: same shape count, and per shape the same anchor
count and open/closed. `InterpolateStickData` blends anchors, both handles, pen
width and solid colours only, and guards on compatibility too. Disabling the check
fails `VectorBucketCopyPasteAndInbetween` (counterproof done).

`ConvertStick()` clears `vector_shapes` and the anchor selection; VECTOR is not a
conversion target. The foreign `.stk`/`.nodes` exporters write a vector stick as a
plain segment (default branch).
