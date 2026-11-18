### Layer options: opacity, shadows, glow, outline, colorize, blur, pixelate

A **layer** is one `StickFigure` in one frame (a row of the layer list). Right-click the list
(`stickLayerView`) and pick "Layer options..." (`Widget::ShowLayerOptions()`) to open
`LayerOptionsDialog`: layer opacity, outer shadow, inner shadow (direction, distance, colour,
smoothness, opacity), glow inside/outside (colour incl. alpha, size, smoothness), outline (colour,
size, opacity), colorize (colour, opacity), blur (intensity) and pixelate (pixel size). Each effect is a checkable group box whose check
state is its on/off switch. The canvas previews every `Changed()` while the dialog is open;
**Cancel restores the previous options, OK is exactly one undo step.** The preview writes into the
live figure, so on OK the old options are put back first, then `StoreUndo()` snapshots that
pre-change state, then the new options are applied; nothing is pushed when nothing changed.

#### Data

`LayerOptions` (`layeroptions.h`) is pure data with no scene dependency, so it travels through undo
snapshots, copy/paste, frame duplication and the library for free. It lives in `StickFigureData` as
a **format v2 trailer after the sticks**; `StickFigureData::CURRENT_VERSION` is 2. A v1 record loads
with all options off and is raised to current at the end of `operator>>`. Defaults leave the layer
exactly as it renders without options (`IsActive()` false).

**`StickFigure::version` must default to `CURRENT_VERSION`.** `ExtractStickFigureData()` copies the
live figure's version, so a figure left at v1 would silently drop its options on save (the
`operator<<` gate skips the trailer).

Blur is `LayerBlur` (enabled, `intensity` = blur reach in scene units); pixelate is `LayerPixelate`
(enabled, `size` = block side in scene units). Both are appended to the same v2 trailer: `enabled`
as `qint32`, the value as `double`, blur then pixelate, after colorize. The format version was NOT
raised again because v2 had not been released yet.

Sizes are in **scene units**. `Reach()` is how far the enabled effects spill beyond the layer's own
pixels; it sizes the padding of the off-screen source. Blur intensity and pixel size are **added** to
the largest effect reach, not maxed against it: every other effect starts from the widened layer.

#### Inbetweens

`GenerateInbetweens()` calls `InterpolateLayerOptions()`: numbers (including blur `intensity` and pixelate
`size`) and colours blend; the **on/off switches (blur and pixelate included) and the glow type
(inside/outside) stay the base frame's**, like stick types. Blending a
switch has no meaning, so don't "fix" this.

#### Rendering

A figure whose options are `IsActive()` gets a `LayerGroupItem` (`stick.h`): it holds the figure's
stick views, and their `VectorShapeLayerItem` helpers, as **children**, and carries a `LayerEffect`
(`QGraphicsEffect`). `LayerEffect::draw()` renders the layer itself with `drawSource()` into a
device-pixel `QImage` and runs `ApplyLayerEffects()`, a pure `QImage` function (unit-testable without a
scene). The image covers only the part of the layer on the paint device plus the effects' reach,
`(Reach()+2) * zoom` px. Never use `QGraphicsEffect::sourcePixmap()`: it rendered the whole layer at
view zoom even off-screen (25 MP, 290 ms per change at x6 zoom) and padded by `Reach()` in device
pixels, which cut effects off when zoomed in. Order: first the layer's own pixels are blurred (all four premultiplied channels, two box
passes), then pixelated (each block replaced by its average premultiplied colour), and only then the
alpha mask that every other effect uses is derived, so shadows, glow and outline follow the
blurred/pixelated shape. Then: outer shadow, outside glow, outline, the layer itself, then, clipped
to the layer with `SourceAtop`, colorize, inside glow, inner shadow. The whole result is drawn at
`options.opacity`.

`LayerEffect::draw()` also hands `ApplyLayerEffects()` a transform mapping scene to the source
image's own pixels: `worldTransform × translate(-source offset) × scale(devicePixelRatio)`.
Pixelate's grid passes through the image of the scene origin, so blocks stay fixed on the canvas
while the layer moves. Limit: blocks are axis-aligned in the image, so with a rotated view they
don't rotate (`ponytail:` comment in `Pixelate()`).

Scale and direction come from the **world transform**: sizes are scene units times `sqrt|det|` of it
(times `devicePixelRatio`), and the shadow direction goes through its linear part. Zoom and view
rotation therefore don't change the effect as seen on the canvas.

**Depth rule** (user-confirmed): a layer with active options renders as ONE composite block at its
top stick's z (`LayerGroupItem::UpdateZ()`). Layers without options keep per-stick interleaving with
other layers. The consequence: sticks of another layer can no longer sit between two sticks of an
active layer.

The group has an empty `shape()` and `Qt::NoButton`, so picking reaches the stick views. Handles are
top-level items and are not affected by the effect.

#### Membership sync (rules that break silently)

- `StickFigure::SyncLayerGroup()` is the ONLY live code that creates, fills or destroys a group.
  Never `new LayerGroupItem` anywhere else for a live figure.
- It runs from `Frame::SyncLayerEffects()`, called at the end of `Frame::Refresh()` (Refresh removes
  and re-adds items, which detaches children from the group, so the sync must come after). The final
  `Refresh()` of `MoveToFrame()` covers a frame switch.
- Views that land top-level in the canvas scene by other paths (draw, merge, split, paste) are caught
  by `StickGraphicsItem::itemChange()`, which calls `MyView::QueueLayerSync()` (queued, coalesced).
- **Do NOT connect `QGraphicsScene::changed` for this.** A connection to it disables Qt's direct
  item-to-view update optimization for the whole app.
- `SyncShapeLayers()` parents shape helper layers to the view's parent, so they follow the view into
  the group.
- `~StickFigure()` deletes the group AFTER the views. A `QGraphicsItem` deletes its children, so the
  other order would destroy the views behind the stick loop's back, which then skips
  `RemoveImagesFromListWidget()` and `DetachView()` for them.
- Turning the options off moves every child back to top level (`setParentItem(nullptr)` keeps it in
  the scene) BEFORE the group is deleted, for the same reason. The loop is
  `while(!childItems().isEmpty()) childItems().constFirst()->setParentItem(nullptr);`, never a loop
  over a copied child list: un-parenting a view runs `SyncShapeLayers()`, which may delete sibling
  `VectorShapeLayerItem`s, so a copied list could hold freed pointers. `DisablingLayerOptionsKeepsShapeLayersAlive`
  pins it (no counterproof yet).
- `MoveToFrame()` removes the old frame's groups from the scene; they are not reachable through the
  figure's stick list.

#### Export

`ImageExport`'s constructor snapshots, per frame, for each active figure: the **indices into the
`total_sticks` clones** of its sticks, plus its options. `RenderFrame()` builds the same
`LayerGroupItem` in the render scene and calls `UpdateZ()` **before** `render()`: paint-time z is too
late in a one-pass render. It never reads the live animation (see the clone rule in CLAUDE.md).

Threading: export runs on a `QThread` and its render scene's effects are not `live`. The `QPixmap`
the effect draws with is fine where the platform reports `ThreadedPixmaps` (Windows, offscreen). The
fallback for other platforms is described in the `ponytail:` comment in the code.

#### Performance

- `LayerEffect::draw()` reuses its last `ApplyLayerEffects()` result while the source PIXELS, the
  effect transform and the options are unchanged. Many code paths call `update()` on a stick without
  changing how it looks, so the source is re-rendered every time; the fresh source is compared with the
  cached one pixel by pixel (`QImage::operator==`), plus the transform (within rounding,
  `SameTransform`), the options and the image size. Only a real difference recomposes.
  Scrolling shifts the world transform and the source offset alike, so the effect transform and the
  cache survive it. Zoom and real edits recompose. The cost is bounded by the viewport size, not the
  layer size.
- Partial exposure: on the canvas Qt culls children outside the exposed region, so
  `MyView::paintEvent()` records `exposed_region`. A live effect (`LayerEffect::live`, set only by
  `StickFigure::SyncLayerGroup()`; false in `ImageExport`'s render scene) that is only partly exposed
  would render a source with holes, so it reuses its cached image; with none to reuse (view scrolled
  over a layer larger than the viewport) it draws the layer plain once and requests a full repaint
  (`retried` guards against a loop).
- "Fast move": the checkbox `fastMoveCheckBox` (bottom of the Style panel in `ui/widget.ui`, wired in
  the `Widget` constructor to `MyView::fast_move_effects`; off by default, not persisted). While a
  mouse button is down and the mouse moves, `MyView::layer_effects_paused` is set and live effects
  paint the layer plain (`drawSource(painter)`). `MyView::ResumeLayerEffects()` at every mouse release
  clears it and updates each `layer_group`, so effects are composed once with the final state. Slider
  gestures are not covered.
- Diagnostic `layer_effects_pixels` (`layeroptions.h`): pixel count of the last composed image.
- Soft parts run at reduced resolution. `SoftFactor(reach)` = `qBound(1, int(reach / 2), 4)`, with
  `reach` the blur reach in px. From 4 px of blur, shadows and glows (`PaintSoft`: block-average
  `Downscale` of the mask, dilate/blur/tint at 1/f, drawn back with `SmoothPixmapTransform` over exactly
  f-px blocks) and the Blur effect (`BlurImage`: smooth-scaled down and back) are computed at 1/2 to
  1/4 size, so the default 4 px shadows and glows run at half size. Outline and anything under 4 px of
  blur stay full resolution.
- Threads: each enabled part (outer shadow, outer glow, outline, inner glow, inner shadow) is built on
  its own thread with `std::async` (`BuildPart`), then drawn in order (`PaintPart`). `BlurImage` blurs
  its 4 channels on 4 threads.
- Inside a part, the dilate and blur also run in row bands, one thread each. Bands = hardware threads /
  enabled parts, capped by `layer_effects_max_bands` (8, measured: more threads cost more to start than
  they save) and by a minimum band height of max(32, 2 × halo) rows. Each band carries `halo` extra rows
  (the summed radii of the four column box passes) on both sides, so the result is bit-identical to the
  one-piece run. The test `ApplyLayerEffectsShapesThePixels` pins that by setting
  `layer_effects_max_bands` to 1 and comparing.
- Why glow is the slow part: a glow is a shadow plus `Dilate()` (two more box passes and a ×8 clamp
  pass), and its resolution factor comes from its smoothness alone, so a hard-edged glow (smoothness ×
  zoom under 4 px) runs all four passes at full resolution. Measured on a 900×650 layer, -O2, 28
  hardware threads: glow size 5 / smoothness 2 went 7.3 ms -> 4.0 ms per recompose, size 10 /
  smoothness 1 7.1 -> 4.2 ms, default glow (5/4) 3.6 -> 2.3-2.5 ms. Default outer shadow alone is
  ~3.1 ms; the fixed cost without any part is ~1.6 ms.
- Two faster glow variants were tried and rejected because they change the look: one blur of size +
  smoothness with a gain folded into `Tint()` (thin strokes lose most of their glow), and running the
  glow on a coarser grid chosen from size + smoothness, with or without fractional box radii (the ×8
  clamp is nonlinear, so thin strokes average away; mean alpha error vs a full-resolution render rose
  from 8-16 to 23-30).
- `Downscale()` sums f pixels per cell instead of dividing every pixel index by f.
- The pixel routines are tuned: `BoxBlur`'s column pass slides per-column running sums row by row
  (cache-friendly, identical results) and divides by the window with an exact multiply-shift (radius
  clamped to 2000 cells). Hot loops use raw pointers, since `QVector::operator[]` costs a detach check
  per pixel in Debug builds. `Tint()` uses a 256-entry lookup table.
- Measured on a 1600x1000 layer with glow + shadow + outline + inner shadow, per recomposition: -O2
  128 ms -> 33 ms, -O0 (Debug) 884 ms -> 77 ms. Debug builds remain ~2x slower than Release.
- Measured after the source crop, lower soft threshold and threads, one recomposition of a ~0.58 MP
  layer with glow + outer shadow + outline + inner shadow at default settings: -O2 17.9 ms -> 5.7 ms,
  -O0 46 ms -> 13 ms.

#### Known limits

- Layer/frame thumbnails and onion skins ignore options.
- A drag inside a layer with effects, and zoom, recompose the layer per change (cost above), now
  bounded by the viewport size; "Fast move" avoids it during drags.
- Zoomed in, a stick wholly outside the viewport is culled, so its shadow/glow does not reach into the
  viewport edge. Scrolling over a layer larger than the viewport shows the newly exposed strip without
  effects for one repaint.
- Colorize also tints the selection highlight drawn by stick `paint()`.

#### Pinned by (`tests/tst_characterization.cpp`)

- `LayerOptionsRoundTripThroughStreamAndUndo`: v2 trailer, undo.
- `LayerOptionsInterpolateExceptSwitches`: inbetween rule.
- `ActiveLayerOptionsGroupTheFigureViews`: group membership and sync.
- `ApplyLayerEffectsShapesThePixels`: the pixel routine, including blur (solid middle, soft spread
  edge) and pixelate (block averages, grid shifts with the origin), and a wide 24 px shadow computed
  at reduced resolution landing where expected.
- `LayerEffectsRecomposeOnlyWhenTheLayerChanges`: idle, hover, viewport update and a no-op stick
  `update()` do not recompose; moving a stick does. It clears the `myview.cpp` global `is_pressed`
  first, because a press left by an earlier slot turns hover into a drag. It also pins the Fast move
  pause/resume (0 recomposes while paused, exactly 1 after resume) and that at x16 zoom the composed
  image (`layer_effects_pixels`) is bounded by the viewport plus reach and stays cached.
