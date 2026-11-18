### FREEHAND layers

A FREEHAND stick carries a stack of raster **layers**, with parity to the VECTOR stick's
shape-layer panel (see `ARCH_VECTOR.md`): visibility, name, reorder, copy/paste, flips, masks and
per-layer depth. Layers are edited only in draw mode; the `advanced_draw_tab` in `Widget`'s
constructor shows the panel.

### Data

`FreehandLayer` (`stickdata.h`) is a plain value type: `image`, `name`, `visible`, `depth_offset`,
`mask` (INDEX into the same list, `NO_MASK` = -1), `mask_inverted`, `operator==`.
`StickData::freehand_layers` is a `QList<FreehandLayer>`, index 0 bottom, last on top. Value
semantics, so `Clone()`, undo snapshots and the clipboard just copy (`QImage` is copy-on-write).
`CopyFrom()` is hand-written and must copy the list (`FreehandLayersRoundTrip` pins it).

**All layers of one stick share ONE pixel grid (same size)**, so masks are pixel-wise. Anything that
resizes one layer must resize all (`ExitDrawMode()` crops them together).

`stick_img` / `stick_img_list[0]` stays, but for FREEHAND it is a DERIVED composite: the visible
layers with masks applied, depth ignored, rebuilt by `StickData::RebuildFreehandComposite()` (a lone
visible unmasked layer is shared copy-on-write). **Never edit `stick_img` of a FREEHAND stick
directly; change a layer and rebuild.** Every older reader (`CalcImgSize()`, `OnionGreyImg()`, the
foreign exporters) keeps working unchanged.

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

### Shared code with vector

`vectorshape.h` holds helpers templated over the layer type (needs `mask`, `mask_inverted`, static
`NO_MASK`), used by `VectorShape` and `FreehandLayer` alike:

- `RemoveVectorShape()`, `ReorderVectorShape()`, `MaskChainReaches()`: **every removal or reorder of
  `freehand_layers` must go through them**, since a mask is an index.
- `MaskChangeAllowed()`: the check behind `SetVectorShapeMask()` and `SetFreehandLayerMask()` (range,
  unchanged, loop; no mask forces `inverted` false).
- `KeepCopiedMasks()`: copy-time remap (a mask survives only if its layer is copied too).
- `BreakBadMasks()`: the load-time repair.
- `CutByMask(QImage&, mask, inverted)` (plain): `DestinationOut` / `DestinationIn`. Used by vector
  `MaskedShapeImage()`, `StickData::FreehandLayerImage()` and `StickGraphicsItem::LiveFreehandLayer()`.

`myview.cpp` file statics `ScreenMirror(centre, horizontal)` and `DepthOffset(depth, z)` (clamp to
`MINDEPTH..MAXDEPTH`, snap |offset| < 0.005 to 0) serve both the vector and the freehand ops.

### Rendering

The image branch of `paint()`: a FREEHAND stick with layers calls `PaintFreehandLayers(painter,
layer_item)`, which draws the visible layers bottom-up in the image's target rect. The onion render
still draws the grey composite (`OnionGreyImg()`).

A masked layer is composed by `StickData::FreehandLayerImage(index)` and served per view by
`CachedFreehandLayer(i)` (`masked_layer_cache`), keyed by (index, `cacheKey()`, inversion) along the
mask chain. Freed in `itemChange()` when the view leaves its scene.

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
- `SyncDrawCanvas()` writes the canvas back into the active layer (COW, free) at every draw-local
  undo push.
- `LoadDrawCanvas()` reloads it after the list or the active layer changed.
- `SetDrawLayer(i)` switches the active layer (commits a pixel float first; not an undo step).

Draw-local undo: `draw_undo` entries are `DrawUndoState{layers, layer, depth_enabled}` (the whole
stack, COW), restored by `RestoreDrawState()`, capped at `MAXDRAWUNDO`. The session is still ONE
global undo step.

`EnterDrawMode()` bakes EACH layer into its own DxD canvas (same transform as before).
`ExitDrawMode()` crops ALL layers to the union of their `OpaqueBounds()` (hidden ones included), then
rebuilds the composite.

Layer ops (all need draw mode; the panel enters it first through the shared `enter_draw_mode` lambda,
also used by the tool buttons): `AddFreehandLayer()` (above the active one, "Layer N"),
`DeleteFreehandLayers()` (never the last), `MoveFreehandLayer()` (active follows),
`SetFreehandLayerVisible()`, `RenameFreehandLayer()`, `FlipFreehandLayers()` (on screen, about the
joint scene bbox of painted pixels, conjugated through `draw_disp`; pixels past the canvas edge are
lost), `CopyFreehandLayers()` / `PasteFreehandLayers()` (`freehand_clipboard` +
`freehand_clip_to_scene`, pasted where painted, on any freehand or frame), `SetFreehandLayerMask()`,
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
Delete / Copy / Paste / Flip H / Flip V, the Mask combo plus Invert, and the depth controls. BOTH
`Widget::BuildVectorPanel()` and the draw panel use it, so a panel fix goes there once.

`refresh_vector_panel` and the new `refresh_freehand_panel` (`uiItems.h`, called from
`Animation::UpdateSliders()`, null in tests) rebuild the list. **The refresh connections
(`FreehandLayersChanged`, `DrawModeChanged` for freehand) are QUEUED**, for the same reason as the
vector panel: a rename or visibility edit fires from inside the list's own `itemChanged`.

### Tests

`tests/tst_characterization.cpp`: `FreehandLayersRoundTrip` (stream, `Clone()`, `CopyFrom()`, v<=11
load), `FreehandLayerOpsInDrawMode` (the ops and their undo), `FreehandLayerMasksAndDepthRender`
(mask cut, inversion, depth interleaving). No counterproof was done for any of them.

### Limits

- Layer thumbnails ignore masks.
- No per-layer opacity or blend modes.
- Inbetweens keep the base frame's layers (images are not blended).
- Flips lose pixels that land past the canvas edge.
- See the ponytail limits under Draw mode.
