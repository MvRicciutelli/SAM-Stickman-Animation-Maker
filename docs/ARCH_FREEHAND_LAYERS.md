### FREEHAND layers

A FREEHAND stick carries a stack of raster **layers**, with parity to the VECTOR stick's
shape-layer panel (see `ARCH_VECTOR.md`): visibility, name, reorder, copy/paste, flips, masks and
per-layer depth. Layers are edited only in draw mode; the `advanced_draw_tab` in `Widget`'s
constructor shows the panel.

### Data

`FreehandLayer` (`stickdata.h`) is a plain value type: `image`, `origin` (`QPoint`: where the image's
top-left sits on the stick's pixel grid), `name`, `visible`, `depth_offset`,
`mask` (INDEX into the same list, `NO_MASK` = -1), `mask_inverted`, `operator==` (compares the origin).
`StickData::freehand_layers` is a `QList<FreehandLayer>`, index 0 bottom, last on top. Value
semantics, so `Clone()`, undo snapshots and the clipboard just copy (`QImage` is copy-on-write).
`CopyFrom()` is hand-written and must copy the list (`FreehandLayersRoundTrip` pins it).

**One stick, one pixel grid; each layer covers only its own rectangle of it** (`origin` + image
size). Masks are pixel-wise on the grid, offset by the two origins. An empty layer is a NULL image and
costs nothing. `StickData::freehand_grid` (`QSize`) is the grid = the composite's size (it drives
`CalcImgSize()`). It is set by `EnsureFreehandLayers()`, `MyView::EnterDrawMode()` (the D x D session
canvas), `CropFreehandLayers()` and the loader; deleting or emptying a layer never changes it.
`FreehandGrid()` returns it, or while unset the composite's size, else the layers' extent from (0,0).
`CopyFrom()` copies it (hand-written; it must). `ConvertStick()` clears it.

`stick_img` / `stick_img_list[0]` stays, but for FREEHAND it is a DERIVED composite: a grid-sized image
with each visible non-null layer drawn at its origin, masks applied, depth ignored, rebuilt by
`StickData::RebuildFreehandComposite()` (a lone visible unmasked layer covering the whole grid at
origin 0 is shared copy-on-write). **Never edit `stick_img` of a FREEHAND stick
directly; change a layer and rebuild.** Every older reader (`CalcImgSize()`, `OnionGreyImg()`, the
foreign exporters) keeps working unchanged.

**Crop rule.** `OpaqueBounds(const QImage&)` (free function, `stickdata.h`) reads alpha in place for
ARGB32 and ARGB32_Premultiplied (no conversion copy). `StickData::CropFreehandLayer(FreehandLayer&)`
(static) cuts one layer to its own opaque bounds and moves its origin; empty becomes null; a layer
with an opaque pixel on each border is taken as tight without a full scan.
`StickData::CroppedFreehand() const` returns `FreehandCrop {layers, grid, img_offset, img_w_scale,
img_h_scale}`: every layer cut to its own pixels, grid = their joint painted box, placement re-solved
so nothing moves on screen (`k = len * scale / max(W,H)` units per grid pixel; `img_offset += k * (new
grid centre - old grid centre)`; `scale = k * max(w,h) / len`). Nothing painted gives a 1x1 grid at the
old centre with all layers null. `CropFreehandLayers()` applies it and rebuilds the composite. **It is
THE crop rule**: used by `MyView::ExitDrawMode()` (which resets `my_line`/`img_angle`/`img_angle_offset`
to the session's, then calls it), the file writer and the library add. Do not write a second one.

`EnsureFreehandLayers()` turns `stick_img` into one layer "Layer 1" when the list is empty (on load
of a FREEHAND record, and in `EnterDrawMode()` on any stick). `ConvertStick()` clears
`freehand_layers`.

Per-layer depth reuses `StickData::vector_depth_enabled` (now "VECTOR and FREEHAND").
`FreehandLayerDepth(i)` is the twin of `VectorShapeDepth()`: `z + depth_offset`, read only while the
switch is on.

### Serialization

`StickData::CURRENT_VERSION` went 11 to 12. After the type block, `WriteFreehandLayers()` /
`ReadFreehandLayers()` write a FREEHAND trailer with its OWN `FREEHAND_LAYERS_VERSION`
(`FreehandLayersFormat`, `FREEHAND_LAYERS_BASE` = 1; gates compare against the names): `qint32`
count, per layer the image through the image table (`WriteRecordImage`, so a `.sam` v3 dedupes), name,
visible, `double` depth_offset, `qint32` mask, `bool` mask_inverted; then the per-stick depth switch
`bool`. Versions <= 11 load as one layer (`EnsureFreehandLayers()`). `BreakBadMasks()` repairs bad
masks on load (out of range, self, loop become `NO_MASK`). Newer is refused through
`RejectNewerVersion()` as everywhere.

`StickData::CURRENT_VERSION` went 12 to 13. A v13 FREEHAND record with layers is written CROPPED
(`CroppedFreehand()` on a copy; the live record, even mid-session, is untouched) and WITHOUT its
composite (image list count 0, index -1). The image block's size slot (it was read and ignored)
carries the grid. The reader sets `freehand_grid` from it (sizes over 32767 are ignored) and rebuilds
the composite from the layers (`if FREEHAND && stick_img_list.isEmpty()`, after
`EnsureFreehandLayers()`). v <= 12 records still store and load their composite. Older builds refuse
v13 files through `RejectNewerVersion()`.

`FREEHAND_LAYERS_VERSION` went 1 to 2 (`FREEHAND_LAYERS_ORIGIN`): each layer writes `qint32` x, y of
its origin after `mask_inverted`; format 1 reads origin 0.

Load-time repair `FitFreehandLayersToGrid()` (after `BreakBadMasks()`): a layer partly outside the grid
keeps the inside part (origin moved), one wholly outside becomes null; an unset grid is derived first.

### Shared code with vector

`vectorshape.h` holds helpers templated over the layer type (needs `mask`, `mask_inverted`, static
`NO_MASK`), used by `VectorShape` and `FreehandLayer` alike:

- `RemoveVectorShape()`, `ReorderVectorShape()`, `MaskChainReaches()`: **every removal or reorder of
  `freehand_layers` must go through them**, since a mask is an index.
- `MaskChangeAllowed()`: the check behind `SetVectorShapeMask()` and `SetFreehandLayerMask()` (range,
  unchanged, loop; no mask forces `inverted` false).
- `KeepCopiedMasks()`: copy-time remap (a mask survives only if its layer is copied too).
- `BreakBadMasks()`: the load-time repair.
- `CutByMask(QImage&, mask, inverted, QPoint offset = QPoint())`: `DestinationOut` / `DestinationIn`.
  `offset` = the mask's top-left in the image. Inverted: the part outside the mask's rectangle is
  cleared first (`DestinationIn` alone only touches the drawn area); a null mask inverted clears
  everything. Used by vector `MaskedShapeImage()` (no offset, same-size images),
  `StickData::FreehandLayerImage()` and `StickGraphicsItem::LiveFreehandLayer()`.

`myview.cpp` file statics `ScreenMirror(centre, horizontal)` and `DepthOffset(depth, z)` (clamp to
`MINDEPTH..MAXDEPTH`, snap |offset| < 0.005 to 0) serve both the vector and the freehand ops.

### Rendering

The image branch of `paint()`: a FREEHAND stick with layers calls `PaintFreehandLayers(painter,
layer_item)`, which draws the visible layers bottom-up, each into its sub-rectangle of the target
(origin and size scaled by target/grid); the active layer in a session draws the live canvas over the
whole target. It reads the draw state only on the GUI thread (`QThread::currentThread() ==
V->thread()`), since the `ImageExport` worker paints clones. The onion render
still draws the grey composite (`OnionGreyImg()`).

A masked layer is composed by `StickData::FreehandLayerImage(index)` and served per view by
`CachedFreehandLayer(i)` (`masked_layer_cache`), keyed by (index, `cacheKey()`, inversion, origin) along
the mask chain. An unmasked or empty layer drops its entry; entries whose index is past the list end
are dropped too (`DropCacheEntriesFrom()`, shared with the vector `masked_cache`). Freed in
`itemChange()` when the view leaves its scene.

Per-layer depth reuses `VectorShapeLayerItem`, which now serves both types: `SyncShapeLayers()`
collects FREEHAND offsets too, `LayerAtOffset(offset)` finds the helper, and
`VectorShapeLayerItem::paint()` dispatches to `PaintFreehandLayers()` for FREEHAND. The ownership
rules in `ARCH_VECTOR.md` (Per-shape depth) apply unchanged.

While a stick is `draw_target`, `paint()` shows the live canvas for the active layer
(`LiveFreehandLayer()`), not the stored image.

### Draw mode

**`MyView::draw_canvas` points at a MyView-OWNED working image `draw_layer_canvas` (the active layer
`draw_layer`), NOT into `freehand_layers`.** That `QList` is implicitly shared with undo snapshots and
render clones: a pointer into it would leak strokes into clones, or dangle after a detach.
- **Only the ACTIVE layer is a full D x D canvas during a session.** `SyncDrawCanvas()` writes the
  canvas back into the active layer with origin 0 (COW, free) at every draw-local undo push.
- `LoadDrawCanvas()` expands the active layer into `draw_layer_canvas` (D x D) and also stores it back
  into the record as the active layer (origin 0, shared); any OTHER layer still holding a whole canvas
  is cut back with `CropFreehandLayer()` first ("at most one full canvas"). Called from
  `SetDrawLayer()`, `EndLayerEdit()`, `RestoreDrawState()` and `EnterDrawMode()`.
- `SetDrawLayer(i)` switches the active layer (commits a pixel float first; not an undo step).

Draw-local undo: `draw_undo` entries are `DrawUndoState{layers, layer, depth_enabled}`, restored by
`RestoreDrawState()`, capped at `MAXDRAWUNDO`. `PushDrawUndo()` stores the active layer cut to its own
pixels (`CropFreehandLayer()` on the entry's copy), not the whole canvas; `draw_last_push_key` (canvas
`cacheKey()` at the last push) lets a step that did not touch the canvas (rename, visibility, mask,
depth) reuse the previous entry's cut. ponytail: an entry still costs the active layer's painted box per
stroke and each push scans the canvas once; stroke-rectangle patches would be cheaper but would trust
every canvas-writing path to report its rectangle. The session is still ONE global undo step.

`EnterDrawMode()` bakes each layer into only its footprint on the canvas (file static
`RedrawFreehandLayer(layer, to_canvas, canvas_size)`, also used by Flip and Paste); grid = D x D;
`LoadDrawCanvas()` then builds the composite (a lone layer shares the canvas). `ExitDrawMode()` has no
crop math of its own: it resets `my_line`/`img_angle`/`img_angle_offset` to the session's, then calls
`CropFreehandLayers()` (hidden layers included).

`MyView::FlushDrawSession()` syncs the canvas into its layer (skipped while a pixel float is lifted: the
layer then still holds those pixels and the canvas has a hole) and rebuilds the composite, WITHOUT ending
the session. It is called first by `MyMainWindow`'s Save, Save As / save-current (also on close) and
figure save (.stck/.stk/.nodes/.samlib), and by `AddStickFigureToLibrary()` (which also crops the
extracted FREEHAND copies with `CropFreehandLayers()`). `MyMainWindow::AutoSave()` returns while
`view->draw_mode` (`op_count` keeps counting). `FlushDrawSessionKeepsCompositeCurrent` pins it.

Layer ops (all need draw mode; the panel enters it first through the shared `enter_draw_mode` lambda,
also used by the tool buttons): `AddFreehandLayer()` (above the active one, "Layer N"; a NULL-image layer, no canvas allocation: it
becomes active, so `LoadDrawCanvas()` gives it the canvas),
`DeleteFreehandLayers()` (never the last), `MoveFreehandLayer()` (active follows),
`SetFreehandLayerVisible()`, `RenameFreehandLayer()`, `FlipFreehandLayers()` (on screen, about the
joint scene bbox of painted pixels, conjugated through `draw_disp`; pixels past the canvas edge are
lost), `CopyFreehandLayers()` / `PasteFreehandLayers()` (`freehand_clipboard` +
`freehand_clip_to_scene`, pasted where painted, on any freehand or frame). Copy commits a lifted float
first (`CommitPixelFloat()`, one draw-undo step) and stores each layer cut to its own pixels with its
origin; `freehand_clip_to_scene` maps SOURCE GRID pixels to scene (clipboard layer pixel p = grid pixel
p + origin). Paste redraws each layer through `translate(origin) * to_pixels`, allocating only its
footprint. Then `SetFreehandLayerMask()`,
`SetFreehandDepthEnabled()`, `SetFreehandLayerDepth()`, `ResetFreehandLayerDepths()`.

**Every layer op is `BeginLayerEdit()` -> mutate -> `EndLayerEdit()`.** The end reloads the canvas,
calls `PushDrawUndo()` (which emits `FreehandLayersChanged`) and `Refresh(0)`. Skipping the pair
leaves canvas and list out of sync.

ponytail limits: a masked ACTIVE layer recomposes on each repaint during a stroke; a layer masked BY
the active one shows the mask as of the last undo step until the stroke ends; there is no
`Frame::Refresh()` in draw mode, so a new depth item tying another stick's depth stacks last until the
next frame refresh.

### Panel

`LayerPanelOps<Layer>` and `BuildLayerSection<Layer>()` (anonymous namespace, `widget.cpp`) build the
layer section: the list (top first, row `r` = layer `count-1-r`), New (freehand only), Up / Down /
Delete / Copy / Paste / Flip H / Flip V, the Mask combo plus Invert, and the depth controls. The layer
thumbnail (`FreehandLayerIcon(const StickData*, int)`, `widget.cpp`) draws the layer at its origin on a
grid-shaped thumbnail. BOTH
`Widget::BuildVectorPanel()` and the draw panel use it, so a panel fix goes there once.

`refresh_vector_panel` and the new `refresh_freehand_panel` (`uiItems.h`, called from
`Animation::UpdateSliders()`, null in tests) rebuild the list. **The refresh connections
(`FreehandLayersChanged`, `DrawModeChanged` for freehand) are QUEUED**, for the same reason as the
vector panel: a rename or visibility edit fires from inside the list's own `itemChanged`.

### Tests

`tests/tst_characterization.cpp`: `FreehandLayersRoundTrip` (stream, `Clone()`, `CopyFrom()`, v<=11
load, the v13 cropped/no-composite write, a v12 record, the load-time repair),
`FreehandLayerOpsInDrawMode` (the ops and their undo/redo pixel equality, one full canvas per session),
`FreehandLayerMasksAndDepthRender` (mask cut, inversion, depth interleaving),
`FreehandLayerOriginsRenderInPlace`, `FreehandCropKeepsPixelsInPlace`,
`FlushDrawSessionKeepsCompositeCurrent`, `MaskCachesDropStaleEntries`. No counterproof was done for
any of them.

### Memory

- Steady state: each layer's own painted box plus one grid-sized composite; empty layers are free.
- Session: one full canvas for the active layer (about 19 MB at 1080p, 78 MB at 4K) plus the composite,
  with the other layers at their own size.
- Draw undo: the active layer's painted box per entry.

### Limits

- Layer thumbnails ignore masks.
- No per-layer opacity or blend modes.
- Inbetweens keep the base frame's layers (images are not blended).
- Flips lose pixels that land past the canvas edge.
- A save during draw mode keeps the session; the file is cropped by the writer.
- See the ponytail limits under Draw mode.
