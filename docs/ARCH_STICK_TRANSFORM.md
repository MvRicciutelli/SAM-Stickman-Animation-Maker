### The Stick Transform

Every stick type has a "Stick Transform" tab with offset, width/height scale, rotation, and
flip horizontal/vertical, each in two sets selected by a Local | Global bar:
- **Local** (`StickData::local_xf`) moves this stick's shape only.
- **Global** (`global_xf`) moves the shapes of this stick and every descendant.

Both sets are a `StickXf` and live in the stick's own frame (origin P1, +y along the stick).
`StickXf::Matrix()` composes a set as flip, then scale, then offset, then rotation.
- Rotation and scale pivot on P1, in both sets.
- A local flip mirrors the shape in place: about the image's centre for an image type, and
  about the stick's midpoint `(0, len/2)` otherwise (`ShapeCentre(len)`). The pivot depends
  only on the spine and the internal placement, never on vertices, so a vertex edit cannot move
  it under the others.
- A global flip pivots on P1 like the rest of the global set. It mirrors the whole branch at the
  joint.

Record versions: v9 had the local offset/angle/scale; v10 adds the local flips and `global_xf`.

**How a stick is drawn:** `ShapeFrame() = LocalTransform(len) · global_xf · spine ·
InheritedTransform()`. `InheritedTransform()` multiplies the ancestors' `GlobalInScene()`
(spine⁻¹ · global · spine), innermost first, so nested globals behave like nested groups. An
ancestor counts by the rule the rotate tool uses for what follows a stick: a stepchild follows
only a stepchild ancestor. Every type draws through `ShapeFrame()`:
- RECT/CIRCLE/TAPER/TRAPEZOID, through `StickLocalFrame()`, which is `ShapeFrame()`. So the
  brush anchoring, `TextureToScene()` and a brush border follow the transforms too.
- IMAGE/FREEHAND, by `ShapeFrame()` plus their internal `img_angle_offset`/`img_offset`.
- TEXT, through `TextFrame()`.
- POLYGON and MULTILINE interiors, through `PolygonTransform()`, which is built on `ShapeFrame()`.
- LINE and the MULTILINE endpoints, which are drawn in scene coordinates, through
  `SceneStickTransform()` (scene → scene) and `DrawnLine()`.

`StickFrameAngle()` reads the angle off the frame matrix, so it picks up every rotation.

**A LINE's frame is read off its line, not off `img_angle`.** Nothing keeps a LINE's
`img_angle` up to date: `PreviewDrawing()` skips LINE, and a LINE record doesn't even stream it.

**Render clones must keep the tree.** Icons (`StickFigure::UpdateIcon()`, `Frame::UpdateIcon()`),
onion skins and `ImageExport` used to clone sticks one by one, which drops `parent`, and an
ancestor's global transform then vanished from every thumbnail and export. They all go through
`CloneForRender()`, which re-links each clone's parent to the clone of its parent.

**The tab** reuses the image panel's five slider rows (`ui->groupBox_5`), re-parented into
`advanced_transform_tab` in `Widget`'s ctor. The rows sit under a `QTabBar` with no pages
(`transform_mode_bar`), so the `.ui` auto-connected `on_img*_valueChanged()` slots stay one per
control; `Widget::EditedXf()` picks the set they write. The flips are two check boxes, each toggle
one undo step (`SetTransformFlip()`). `refresh_transform_panel` (a `std::function` hook in
`uiItems.h`, like `refresh_text_panel`) echoes the shown set under `QSignalBlocker` from
`UpdateSliders()`. It also runs on a Local/Global switch. `Animation::UpdateTab()` inserts the tab
after the type's own tabs. The image panel keeps only the image variants list and its
swap/add/remove buttons. POLYGON has no "Polygon Editing" tab and TEXT no "Text Placement": every
control in them was the transform.

**The `img_*` fields are internal placement, not the user's.** Before v9, IMAGE, POLYGON and
TEXT kept their user offset/rotation in `img_offset`/`img_angle_offset`, and IMAGE its scale in
`img_*_scale`. `operator>>` moves them into `local_xf` for a record older than v9, which gives the
same drawn frame. TEXT's offset is turned by 90°, because its frame is. FREEHAND is not migrated:
its `img_*` fields are draw mode's canvas placement.

**Draw mode:**
- `EnterDrawMode()` bakes the local transform into the canvas and resets it.
- It cannot bake the global ones: they move the children too, and the ancestors' belong to the
  ancestors. So the canvas is painted through them. `draw_branch` (own global · inherited) is part
  of `draw_disp`, which every scene↔canvas mapping already uses. The viewport is pulled back
  through it to size and centre the canvas. `ExitDrawMode()` takes it back out before placing the
  crop.
- The canvas's pixel count is the viewport's, and `draw_px_scale` stick units per pixel stretch it
  over the covered region. Sized in stick units, an ancestor's 0.25 scale made it 16x the pixels
  (measured: 38 MB instead of 2 per canvas, times up to `MAXDRAWUNDO` undo entries).
- The tab is disabled during a session.

**A MULTILINE is edited where it is drawn.** `SceneVertices()` returns the drawn endpoints.
`StickData::SetDrawnLine()` is the exact inverse of `DrawnLine()`. It undoes the inherited part
first (it doesn't depend on this spine), then this stick's own local·global, whose linear part
doesn't depend on the length: that gives the direction and length, and then the ends, flips
included. `SetMultilineVertices()`, endpoint drags (`EditPolygonPoint()` passes the other end as
drawn) and the change-master re-anchoring (`Capture`/`ReanchorVertexScenePositions()` carry the
drawn ends) all go through it. It is exact bit for bit with no transform; under one it is exact to
about 1e-5 px, since `img_angle` is a `float`.

**Performance, measured** with a throwaway benchmark:
- No transforms cost nothing: each matrix of a default `StickXf` is `TxNone`, and the ancestor
  walk is one `IsIdentity()` per ancestor.
- With a global on every stick of a 20-deep chain: 50 renders 6 → 8 ms. At a pathological depth
  of 100 (the walk is O(depth) per call): 0.9 ms per render, 0.5 ms per full refresh. No cache was
  added for that.
- A brush border scaled up 9x by nested globals: 2 → 4 ms per 20 renders. Off-screen dabs are
  cheap, so there is no culling.
- The draw-mode canvas (above) was the one real problem.
- The sliders refresh only what moved: the stick for a local edit, the stick and its descendants
  for a global one (`RefreshTransformed()`). They used to refresh the whole figure, a bounding rect
  and a text layout per stick, per tick.

**The DISPLACE tool** (`#define DISPLACE 29`, the button next to Weld) edits the same numbers on
the canvas. It selects like the arrow tool: it shares `NOTOOL`'s branch of `mousePressEvent()` and
`mouseMoveEvent()`, with click, Shift/Alt, rubber band and Ctrl vertices. It shows no transformation
box and never runs the skeleton-rotating drag. Instead it draws two handles on the current stick
(`DisplaceHandles()`): a square on the drawn centre (`DrawnCentre()`, which moves the shape) and a knob
on a stem along the drawn stick (which turns the shape about that centre). A press on a stick selects
it and then drags its shape, so one gesture picks and moves. What the tool does:
- **It edits the set the tab shows.** `MyView::displace_global` follows the Local | Global bar,
  so the tab always shows what the tool edits, and Global moves the descendants too.
- **The math is in `StickData`.** `MoveDrawnShape()` pulls the scene drag back through what comes
  after the set (`AfterSet()`). `TurnDrawnShape()` solves for the offset that keeps the centre still.
  Both are exact under any surrounding flips, scales and inherited globals. A mirrored frame turns
  the set the other way, so the shape turns the way the cursor did on screen.
- **No drift.** `DragDisplace()` restores the set latched at press and re-applies the whole drag
  each move, rather than the step since the last event.
- **No empty undo steps.** The `StoreUndo()` waits for the first move past Qt's drag distance, so a
  click that only selects pushes nothing.

**"Reset transform"** in the tab (`Widget::ResetTransform()`) puts the shown set back to rest as one
undo step, or does nothing when it already is.

`DisplaceMovesAndTurnsTheDrawnShapeExactly` (checked to fail without the mirror sign) and
`DisplaceToolMovesAndTurnsViaRealMouseEvents` pin the tool.

`StickTransformMovesEveryTypeAndMigratesOldPlacement`,
`GlobalTransformMovesTheBranchAndReachesRenderClones`, `StickTransformFlipsAndVersion10`,
`MultilineEditsWhereItIsDrawnUnderATransform` (with an ancestor's global and flips),
`ChangeMasterKeepsTransformedMultilineShape` and `DrawModeUnderAnInheritedGlobalTransform` (real
clicks land under the cursor, canvas pixels capped) pin it. The re-anchoring and `draw_branch`
were each checked to fail when reverted.

### Converting a stick's type

`MyView::ConvertStick()` turns any stick type into any other, in place, as one
`StoreUndo()`. It keeps `my_line`, the IK role/chain, the tree links, `pen` (colour,
thickness, border brush) and `brush`, and is **destructive by spec**: it first drops
everything only the old type reads (POLYGON/MULTILINE vertices, IMAGE/FREEHAND images —
IMAGE's `img_list_widget` rows too — TEXT's `text`, and the internal `img_offset`/
`img_angle_offset`/`img_*_scale` placement), then seeds the new type the way `DrawCmd()`
seeds a fresh one (RECT/TAPER/TRAPEZOID widths from their sliders, MULTILINE's two endpoint
widths from the pen). Two seeds are this function's own: a POLYGON starts as a **diamond
through both ends** (an empty one is invisible), and a FREEHAND enters draw mode on a blank
canvas. IMAGE and TEXT need user input, which the caller (the Widget lambda) asks for
BEFORE calling, so cancelling leaves neither a half-converted stick nor an empty undo step.
The Stick Transform survives a conversion, since every type reads it (a FREEHAND's draw-mode
entry then bakes it into the canvas).

The UI is a single "Convert to" row (combo + button; `convert_type_combo` in `uiItems.h`,
its parent is the row). A widget has one parent, so `Animation::UpdateTab()` moves that row
to the top of the first advanced panel of whatever type it is showing, and preselects that
type.

### Copying and pasting a stick branch

The Transform tab's "Copy sticks"/"Paste sticks"/"Paste sticks at origin" buttons
(`copyStickBtn`/`pasteStickBtn`/`pasteStickAtOriginBtn`) act on
the ONE selected stick (`MyView::SingleSelectedStick()`: the current stick, but not during a
multi-selection or a 2-click draw). There is no multi-stick selection, so a copy is that stick plus
everything in its `children` list, which already holds every descendant. **A master copies its own
chain only**: the stepchildren hanging from its P1 and their chains stay behind (a chain is on the
side of its stick hanging from the master, the test `Weld()` uses). Both methods return a
refusal string, and Widget shows it as a warning, the `GenerateInbetweens()` shape.
- **Copy** (`MyView::CopyStickBranch()`) pushes no undo step. It replaces `stick_copy_buffer`, a
  `StickFigureData` built by `ExtractStickBranch()` whose `master_stick` is the copied stick. The
  buffer is freed by the next copy and by `~MyView()`, and undo never touches it.
- **Paste** (`MyView::PasteStickBranch(at_origin)`) is one `StoreUndo()`. `GraftStickBranch()`
  clones the buffer (so it can be pasted again), gives every pasted stick the `stepchild` flag of
  the side it lands on, rebuilds the flattened `children`, remaps the IK links and sanitizes the
  chains the copy cut. Where it lands:
  - "Paste sticks": the root's P1 on the selected stick's P2, hanging from that stick, on its side.
  - "Paste sticks at origin": the root's P1 on the selected stick's P1, hanging from that stick's
    parent, beside it and on its side. So at a stepchild hanging from the master it starts from
    the master's origin too. A master has no parent: the paste hangs from the master itself, at
    its origin, as a stepchild chain.
  - With no stick selected and an EMPTY current layer, both buttons paste the branch where it was
    copied, and its root becomes the layer's master, so no pasted stick is a stepchild. That is
    `GraftStickBranch(figure, nullptr, ...)`. It is refused mid-draw: a figure's first stick is
    already its master while being drawn, though `stick_list` is still empty.

`CopyPasteStickBranch` (checked to fail with the master's stepchild exclusion removed),
`PasteStickBranchAtOrigin` (checked to fail with the master's stepchild rule removed) and
`PasteStickBranchIntoEmptyLayer` (checked to fail without the new master) pin it.

### View navigation: zoom view, rotate view, pan view, reset

`ZOOMVIEW` (the `zoomInBtn`, now a drag tool: right/up zooms in, left/down out, 200 px = x2,
Shift snaps the absolute level to 10% steps; the zoom-out button is gone) and
`ROTATEVIEW`/`PANVIEW` change only the `QGraphicsView`
transform and scroll position. No `StoreUndo()`, no figure moves, and nothing a render reads:
icons, onion skins and `ImageExport` all build their own scene, and the preview's resolution
comes from `sqrt(|det|)`, which ignores rotation. Do not confuse them with `ROTATESCENE`/`MOVESCENE`,
which move every figure in the frame. Rotation and zoom pivot on the **canvas centre**, which
stays at the screen spot it had at the press (`TurnAndZoomView()`). QGraphicsView itself only
anchors on the viewport centre, so the canvas is scrolled back after each step. The target is
that latched spot, never the previous step's position. The first cut re-centred through
`centerOn()` every move, and since it truncates, the canvas crept ~½ px per event: "it scales
and moves at the same time". Shift snaps the **absolute** angle (`ViewAngle()`, read back from
the transform) to 15° steps. Pan scrolls the
bars by the exact pixel delta, as `ScrollHandDrag` does. Re-mapping a centre through the
transform on every move drifted.

**Why `UnlockViewNavigation()` exists:** whenever the scene rect fits the viewport, QGraphicsView
centres it itself. That swallowed a pan entirely and made a rotation pivot on the canvas instead
of the screen. Each drag press therefore sets the **view's** own `sceneRect` (never the scene's)
to the scene rect plus a few viewport diagonals around the current centre. The zoom calls it
on EVERY move, not just the press, since zooming out shrinks that room back to fitting.
`ViewZoom()` is `sqrt(|det|)` (rotation-proof), and the tool keeps the global `zoom_lvl` in step
for the middle-drag zoom. `ResetView()`
(the reset-zoom button) clears it with `setSceneRect(QRectF())` together with the zoom and the
rotation. `ViewRotateAndPanLeaveTheDocumentUntouched` pins all of it, and was checked to fail
with the unlock removed from either press.

