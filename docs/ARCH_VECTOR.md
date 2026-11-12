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
`stroke_brush`, `depth_offset`) are plain value types in `vectorshape.h`. The paint fields have
`StickData`'s names and meaning on purpose: that is what lets the whole
fill/stroke machinery be reused per shape (see Rendering). **Handles are OFFSETS
from `pos`**, not points, so moving an anchor needs no handle bookkeeping and a
transform maps them through the linear part only (`MapShape()`). List index is
drawing order within one depth, last on top (see Per-shape depth).

Coordinates are normalized stick-local through `StickData::PolygonTransform()`,
exactly like POLYGON, so shapes follow, rotate and scale with the stick and its
Stick Transform. Value semantics mean there are **no ownership rules**: delete,
`Clone()`, undo snapshots and the clipboard all just copy, and textures are
copy-on-write. The one trap is that `CopyFrom()` is hand-written and must copy
`vector_shapes`; `VectorStickRoundTrip` fails without that line (counterproof
done).

Free functions in `vectorshape.cpp`: `VectorPath()` (a `cubicTo` where a segment
has a handle, a `lineTo` when both inner handles are zero, plus the closing
segment), `MapNode()`, `MapShape()`, `VectorShapesCompatible()`, `ShapeAt()` (topmost visible
shape hit by area or by outline, the outline widened by the pick radius) and
`StrokeBrushFor()` (shared with `StickData::StrokeBrush()`).

### Serialization

`StickData::CURRENT_VERSION` went 10 to 11. VECTOR sits in the type-gated `img_*`
block (`PolygonTransform()` reads `img_offset`/`img_angle_offset`) and then
`WriteVectorShapes()` follows with its OWN `qint32` `VECTOR_SHAPES_VERSION` (2;
newer is refused through `RejectNewerVersion()`), a `qint32` count, and per shape
name/visible/closed, the nodes, then pen, brush and stroke-brush tip. Format 2
appends a `double` `depth_offset` at the END of each shape and the
`vector_depth_enabled` bool AFTER the last shape, so the head of the block
(version, count, first shape) is unchanged. Format 1 still loads (offsets 0,
switch off). `WriteVectorShapes()`/`ReadVectorShapes()` take the `StickData`,
not just the list, for that bool. Those go
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

### Per-shape depth

**`StickGraphicsItem::SyncShapeLayers()` is the ONLY code that creates,
destroys or adds a `VectorShapeLayerItem` to a scene, and every paint path for
shapes must go through `PaintVectorShapes()`.** Never assume one scene item per
stick.

`VectorShape::depth_offset` (default 0) is the shape's depth as an OFFSET from
the owning stick's `z`; 0 is the stick's own plane. `StickData::
vector_depth_enabled` (default false) is the per-stick switch. Off: every shape
is drawn at the stick's z in layer order, and the offsets are ignored but kept.
On: a shape is drawn at `StickData::VectorShapeDepth(i)` = `z + depth_offset`, a
frame-wide depth, so it can show in front of or behind OTHER sticks; shapes
sharing a depth keep their layer order. It is an offset on purpose: whatever
moves the stick's z (the Depth tab buttons, `StickFigure::UpdateZ()` on a layer
reorder, inbetweens) carries the shapes along with no extra code, and "reset"
is all offsets = 0. `CopyFrom()` copies the switch; `ShapeAt()` walks the shapes
in drawing order (layer order stable-sorted by `VectorShapeDepth()`).

A `QGraphicsItem` has one z, so a shape at another depth cannot be painted by
its stick's item. `VectorShapeLayerItem` (`stick.h`/`stick.cpp`) is a top-level
helper at `z + depth_offset`, one per DISTINCT non-zero offset in use. It paints
through its owner, `StickGraphicsItem::PaintVectorShapes(painter, layer)`
(`layer == nullptr` is the stick's own item painting the offset-0 shapes), holds
no data, takes no mouse input (`setAcceptedMouseButtons(Qt::NoButton)`) and
reports the owner's bounding rect. `StickGraphicsItem::shape_layers` owns them.

`SyncShapeLayers()` runs from `itemChange(ItemSceneHasChanged)` and from
`Refresh()`, so the layers follow their view into every scene
(`Frame::Refresh()`, `MoveToFrame()`, `AttachView()` and the throwaway render
scenes of icons, onion skins and `ImageExport`) with no call site knowing they
exist. Ownership: `~StickGraphicsItem()` nulls each layer's `owner` then deletes
it; `~VectorShapeLayerItem()` removes itself from `owner->shape_layers` when the
owner is still set (a scene being cleared may destroy the layer first).
`SceneRemover()` skips items that already left the scene. `ShapeLayer(shape)`
matches a shape to its layer by the STORED offset (exact equality on the same
stored number, never on a computed depth); a shape whose layer does not exist
yet is painted by the stick's own item rather than not at all.

`SyncShapeLayers()` also calls `setZValue(data->z)` on the stick's own view.
One-shot render scenes (icons, onion skins, exports) used to apply z only inside
the first `paint()`, too late for that render, so they stacked by insertion
order; they now stack by depth like the live canvas. A shape layer's depth only
means something against the other sticks' z, hence this. The edit overlay
`PaintVectorOverlay()` is painted by `TopShapeLayer()` (the layer with the
greatest positive offset) when there is one, else by the stick's own item, so a
raised shape cannot cover its own anchors.

Interaction: `MyView::SetVectorDepthEnabled(bool)`,
`SetVectorShapeDepth(const QList<int>&, double)` and `ResetVectorShapeDepths()`
are one undo step each (the reset takes none when every offset is already 0).
Each ends with `VectorEdited()` then `Frame::Refresh()`, like the stick Depth
buttons: that re-adds the frame's views in `total_sticks` order, so a new layer
tying with another stick's depth stacks right after its own stick, not last in
the scene. `SetVectorShapeDepth()` takes an ABSOLUTE depth, clamps it to
`MINDEPTH..MAXDEPTH` and stores `depth - z`; an offset under 0.005 in magnitude
snaps to 0 (the spin box has two decimals and z is often float-rounded, such as
1.0099999), so typing the stick's own depth puts the shape back in layer order.
`VectorShapeDepthInterleavesWithOtherSticks` pins render order against another
stick (raised, lowered, shared depth, switch off), one-shot scenes stacking by
z, stream and `Clone()` round trip, `ShapeAt()` order, and on the live canvas
the undo steps and the helper items' presence in the scene (also after an
undo). No counterproof was done for it.

### Interaction (`myview.cpp`, end of file)

`MyView::vector_tool` (`VectorTool`: `VT_NONE`, `VT_PEN`, `VT_LASSO`, `VT_FILL`,
`VT_STROKE`, `VT_TRANSFORM`, `VT_FILL_STROKE` (both buckets in one click); `VT_PEN` is "vector draw mode") is modal like draw mode:
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

`VectorPenStartOnShape()` checks the FIRST click of a pen shape (nothing being drawn)
before any undo step. On an end anchor of a visible open shape the pen continues that
shape: a click on the first anchor reverses the node list and swaps each node's in/out
handles (one undo step; the picture is unchanged) so the pen always appends; a click on
the last anchor takes no undo step. A click on any other anchor (a middle one, or one of
a closed shape) starts a new shape as before. Otherwise, on the outline of a SELECTED
shape (the current layer `CurrentVectorShape()`, or any shape with a selected anchor) a
new anchor is inserted there as one undo step and becomes the only selected anchor: the
segment is split with de Casteljau so the path is unchanged, and a straight segment
(both inner handles null) stays straight. ponytail: the outline is hit by sampling each
segment 32 times. No drag follows either case. `VectorPenContinuesAndSplitsShapes` pins it.

The mode is entered by `DrawCmd()`'s 2nd click for `DRAWVECTOR` and by the
panel's pen button. It is left by Esc, Enter, a double click, `SetTool()`
(`ChangeTool`), a REAL frame change, and `ExitDrawMode()`, which calls
`ExitVectorMode()`, so every existing "end the drawing session" call site ends
vector mode too. `MoveToFrame()` exits only when the frame differs: undo/redo
goes through `MoveToFrame(current)` and must keep the mode.

Editing: Ctrl+press on a selected vector stick (selection tools NOTOOL/CLAMP/
DISPLACE, or any vector tool) picks handles of selected anchors first, then the
nearest anchor; Shift adds it (a selected one stays selected), Alt takes a selected one
out, leaving the rest, and neither drops the selection on a miss. A drag moves the selected anchors (handles follow,
being offsets). A handle drag keeps a smooth anchor's opposite handle collinear
at its own length, Alt breaks it to a corner. `StoreUndo()` is deferred to the
drag threshold, so a click leaves no empty step. A consumed press clears
`is_pressed` so no tool drag starts, and a double click outside pen mode is
treated as the press it replaces (re-grabbing an anchor quickly is a double
click to Qt). The lasso (`VT_LASSO`, drawn in `drawForeground()`) ends in
`LassoSelectVectorNodes()`. Delete runs `DeleteSelectedVectorNodes()` first (one
undo step, shapes left with < 2 anchors go), then the old vertex/stick deletion.

The panel's "Sharp / Curved" button runs `ToggleVectorNodeCurves()` on the selected
anchors as one undo step. An anchor is curved when it has a handle; a selection with
any sharp anchor becomes all curved, otherwise all sharp. Curved handles are the
Catmull-Rom tangent `(next - prev) / 6`, symmetric, `smooth = true`, computed in
stick-local space (affine-invariant); an end of an open shape gets no handle on its
missing side. Sharp clears both handles and `smooth`. Pinned by the same test.

**The Transform tool** (`VT_TRANSFORM`) has no selection of its own: it works on the
anchor selection, so a "selected shape" is a shape with every anchor selected, and
whatever the lasso or a Ctrl+click picked is what it transforms. `VectorTransformPress()`
gives a press to the first of: a box handle; an anchor or shown curve handle
(`VectorEditPress()`'s rules); a shape (`ShapeAt()`: all its anchors, Shift adds or
removes it; a shape that already has selected anchors keeps the selection so the drag
moves it, and a release without a drag then takes the whole shape, `vec_click_shape`);
empty canvas, a lasso. A drag on the selection is the ordinary `VG_MOVE`. The box
(`VectorSelectionBox()`: the anchors plus the exact extent of every curve between two
selected neighbours) is drawn in `drawForeground()` with `DrawTransformHandles()` and
`BoxHandles()`, shared with the figure box; its squares come BEFORE the anchors (a corner
often sits on one), and Ctrl+click still picks that anchor. `VG_SCALE` fixes the opposite
handle (a side handle scales its own axis, past it mirrors, Shift on a corner keeps the
proportions); `VG_ROTATE` turns about the box centre (Shift: 15° steps).
`VectorGestureTransform()` builds the scene map, `ApplyVectorTransform()` conjugates it
through `PolygonTransform()` and maps every selected node from `vec_shapes0`, the
shapes latched at press, through `MapNode()` (handles by the linear part): the whole drag
is re-applied each move, so nothing drifts. The undo snapshot waits for the drag threshold
as everywhere here. Since the box lives in the foreground, `VectorEdited()`, the presses,
the moves, the release and `undo_stack` `indexChanged` repaint the viewport while the tool
is active. `VectorTransformToolSelectsMovesScalesAndTurns` pins it.

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

Below the layer buttons are the depth controls: the check box "Own depth per
shape" (off by default; it enables the rest), a slider and spin box with the
range and resolution of the stick Depth tab's `depthSlider`/`depthSpinbox`
(0..99.99, two decimals, slider x100) that only hold a value, "Set depth"
(applies it to the SELECTED shapes) and "Reset depths" (all offsets to 0; the
switch stays on). `refresh_vector_panel` echoes the switch, enables the
controls, shows the current shape's depth in the spin box and puts "Depth N" in
each row's tooltip.

`MyView::vector_clipboard` is a `QList<VectorShape>` in SCENE space (`MapShape`
through the source stick's `PolygonTransform()`). Paste maps through the target's
inverse, appends on top, leaves the buffer (repeatable, survives frame changes)
and keeps widths absolute. `depth_offset` is copied with the shape (value copy);
a shape drawn with the pen starts at offset 0. There are no Ctrl+C/V shortcuts,
the app has none.

`GenerateInbetweens()` refuses (no change, no undo step, an explanatory message)
unless `VectorShapesCompatible()`: same shape count, and per shape the same anchor
count and open/closed. `InterpolateStickData` blends anchors, both handles, pen
width, `depth_offset` and solid colours only, and guards on compatibility too. Disabling the check
fails `VectorBucketCopyPasteAndInbetween` (counterproof done).

`ConvertStick()` clears `vector_shapes` and the anchor selection; VECTOR is not a
conversion target. The foreign `.stk`/`.nodes` exporters write a vector stick as a
plain segment (default branch).
