### Fill and stroke: pattern, gradient, image

`StickData::pen`/`StickData::brush` are a whole `QPen`/`QBrush`, and that is the
entire feature: a **pattern** and an **image** are both
`Qt::TexturePattern` (differing only in where the tile came from), the two
**gradients** are `Qt::LinearGradientPattern`/`Qt::RadialGradientPattern`, and
`QDataStream`'s own `QPen`/`QBrush` operators — already called by
`StickData::operator<<`/`>>` — write the tile, the stops, the spread, the
coordinate mode and the brush transform. **No new `StickData` field, no
`version` bump, no new serialization code**, and `painter->setBrush(data->brush)`
in `paint()` already renders all of it. Nothing pins the stream version
anywhere in the project, so it defaults to the current Qt and the gradient
extras are covered (`GradientAndTextureFillsRoundTrip` pins this).

The UI is one `QComboBox` per group at the top of the Style column —
`fill_type_combo` in "Fill" (`ui->verticalLayout_16`) and `stroke_type_combo`
in "Border" (`ui->verticalLayout_15`), both built in code by
`Widget::BuildStyleCombos()` and listed in `uiItems.h` because
`Animation::UpdateSliders()` also greys them out (fill has nothing to act on
for `LINE`/`MULTILINE`/`TEXT`, neither does for `IMAGE`/`FREEHAND`). The
entries are `Widget::StyleKind`; `Widget::ChooseStyle()` opens the matching
chooser and `ApplyStyleBrush()` adopts the result, re-syncing the combo and
the 20x20 swatch button (now a painted **icon**, since a stylesheet
`background-color` cannot show a gradient or a tile). Picking a style only
sets the *current* `my_brush`/`my_pen`, exactly like the historical colour
buttons — the Apply buttons still push it onto the selection.

The Style column itself is a two-tab widget built by `Widget::BuildStyleTabs()`
(right after `BuildStyleCombos()`, whose check boxes it groups): **"Paint"**
holds the Fill and Border groups plus a **"Display options"** group with the
four texture check boxes, **"Depth"** holds the depth/Z-order group — which
used to sit UNDER Fill and Border and made the column taller than a short
screen, so the rest of it fell off the window. Everything there is
*re-parented*, not rebuilt: same widgets, same signals, same per-stick
enablement from `Animation::UpdateSliders()`/`UpdateStyleControls()`. The tab
labels are short because the column is 150 px wide — anything longer and the
tab bar switches to scroll arrows.

The bottom row (tools column | canvas | Style column | layers column) is one
`QSplitter` built by `Widget::BuildResizableSidebars()`, with each side column inside
a `QScrollArea`. It lifts the .ui's fixed column widths and the `viewBox`'s 800x500
minimum, which kept the window from going below about 1600x1000 together with
`bottomframe`'s old 600 px minimum (gone now, see below). `groupBox_9` (it held tools +
canvas) is deleted there.

**When the window narrows, the side columns give up their spare width before the canvas
shrinks; when it widens, they get it back before the canvas grows** (`BottomRowSplitter`,
widget.cpp). A column's spare width is what it has beyond what its content needs (its
`minimumSizeHint()` plus a scrollbar). It grows back up to its starting width, or the width the
user last dragged it to. The canvas still has the only stretch factor, and that alone did the
opposite: every change of width went to the canvas until its minimum, so the canvas got
crushed while the tools column kept a hundred unneeded pixels. No stretch factor can express
the rule either, because a stretch factor applies to growing as well. Measured by dragging
the window edge in 10 px steps, 1920 -> 1500 -> 1920: no drift.

**The tools column widens until all its tabs show, while the canvas can spare it**
(`BottomRowSplitter::FitToolTabs()`). The width comes from the canvas, but only if the canvas
viewport still fits 130% of the frame's on-screen width afterwards. Height is not checked, since
the column takes only width. The column never gets wider than the tab bar's `sizeHint()`. The fit
runs on a resize and on a `LayoutRequest` to `toolsTabWidget`, which a tab insert or remove posts,
so selecting another stick type triggers it. While a mouse button is held it waits, because the
press that selects a stick is the one that changes the tabs, and widening then would slide the
canvas under the drag it starts. It only grows: a shorter tab set doesn't shrink the column back,
or the canvas would shift on every selection. Once the user drags that column's handle, it stops
for good. Measured at 1900 px: the trapezoid tab set took the column from 201 to 370 px. At
1350 px the canvas is under 130% and the column returns to scroll arrows.

The .ui's icon buttons paint their 50 px PNGs through the stylesheet **`image:`** property,
never `background-image:`. A background image is never scaled, so a button the layout
shrank below 50 px showed a cropped corner of its icon. `image:` scales down to fit, keeping
the aspect ratio, and renders identically at full size (measured with a throwaway probe).

**Every icon button may shrink to half size when it doesn't fit.** That is set in widget.ui
alone, with no code:
- Each button gets a 25x25 minimum, a maximum of at least its full size, and a stylesheet
  `width`/`height` at full size. That stylesheet size is the *preferred* size, not a limit.
  Without it the preferred size came from the empty `QIcon` (48x12), so a row with no spare
  room sat at 25 px even on a big screen.
- The rows' and boxes' fixed minimums (55, 60, 65, 85, 130, 150, 160, 250, and 500 on
  `toolsTabWidget`) are gone. Where a maximum sat below the minimum (min 55 / max 50, and
  `topFrame` min 160 / max 140), the minimum used to win, so the maximum took its value.
- So did the vertical size policies that forbid shrinking below the preferred size
  (*Minimum*, *MinimumExpanding*): the Fill colour picker's row (`frame_22`), the Depth
  group, and every top-bar container.
- The lists (layers, library, image list) have a layout stretch of 1. A stretched item counts
  at its minimum when space is short (`QLayoutStruct::smartSizeHint`), so lists give way
  before buttons.
- The onion-skin icon in the top bar is a `QLabel`, and `image:` scales on a label too
  (measured). It shrinks like the buttons; fixed at 50 it held its whole row up.

**The top bar and the canvas row share a shortage of height the default Qt way**: evenly, as
soon as the canvas row can't have its preferred height (about 600 px, the canvas view's own).
So the top bar starts shrinking at roughly a 900 px window and is at its floor by about 800.
- A canvas-first rule was tried, with a stretch factor on the canvas row plus a minimum on it.
  The top bar then stayed full size down to a 650 px window. And since Qt never shrinks a widget
  below an explicit minimum, that minimum cut the window's bottom off below about 560 px.
- Now nothing in the column has an explicit minimum except `drawtools` (below), so the bottom
  is only cut off below about 370 px. `Widget`'s own .ui minimum (150) hides the real one from
  the window.

The frame-tools part is compact. The global stylesheet gives every `QFrame` a 6 px
`margin-top`, which nested five deep, so `frame_3` overrides it to 0 for itself and everything
inside it. The rows' and containers' bottom margins (15, 15, 10) are 0, with 4 px between the
two rows. Full size is 110 px instead of 160. Before, those margins didn't even fit the 160 px:
the buttons' fixed 50 px minimum made them spill into them.

`drawtools`:
- It has a 10 px top layout margin, the convention of the other titled groups. Its old fixed
  85 px height had been what kept its title clear of the buttons.
- Its 57 px minimum is where its buttons are 39 px, which is as small as the frame buttons
  get. Their floor comes from the frame strip (`frameListWidget`), whose minimum height is
  83 px. Without it, the draw tools went down to 25 px next to 39 px frame tools.

**The tools column's width came from its hidden pages.** A tab widget is as wide as its widest
page. In the Edit and Libraries pages that was the lists, whose preferred width is Qt's
default for a list (256 px), not anything about their contents. With their horizontal size
policy *Ignored* they no longer count, and the column starts at 251 px instead of 327.

The Style column's Paint/Depth tabs are built in code (`BuildStyleTabs()`), so their two fixes
are code too:
- Paint scrolls in its own `QScrollArea`, or as the taller page it would make Depth tall
  enough that Depth's buttons never shrink.
- The tab widget is inserted with stretch 1. A `QScrollArea`'s preferred height is capped at
  24 lines of text (about 312 px), and the .ui's tail spacer split the spare height with it.

**A `QTabWidget` is as tall as its tallest page, hidden or not**, and the column's scroll
area only scrolls what reports itself taller than its viewport. The image panel (the
"advanced" page) used to hold the tools column at 614 px on every tab, so the drawing tab's
buttons never needed to shrink. Two .ui changes fixed it, and the column is now sized by the
drawing and library tabs (389 px):
- The image panel's contents sit in their own `QScrollArea` (`advancedScrollArea`). It reports
  a small minimum and scrolls inside the tab. Setting the page's size policy to *Ignored*
  instead would have hidden its real height from the column's scroll area as well, so the
  page would be cut off instead of scrolling.
- The rect and taper panels (`advancedtab.ui`, `advancedtaperwidget.ui`) had copied the image
  panel's fixed 400 px box around one or two sliders. That minimum is gone (116 and 230 px
  now).

The old 500 px minimum on `toolsTabWidget` sat below the image panel's 614 and clipped it.
The draw-mode panel (499 px, built in code) is still the tallest page while a FREEHAND stick
is selected.

What .ui options cannot do: Qt shares a shortage evenly at each layout level, so the Style
column's nested groups end up with buttons of different sizes (25 to 50 px side by side at
1920x1040). A code version (`IconColumnFit`, in git history) shrank a whole column's buttons
alike.

`ApplyStyleBrush()` takes the `StyleKind` as an argument (defaulting to -1 =
work it out from the brush) for one reason: a pattern and an image are BOTH
`Qt::TexturePattern`, so re-syncing the combo from the brush alone could only
ever read back "Pattern", and the combo jumping to "Pattern" the instant you
picked an image looked as though the pick had gone somewhere else. `STYLE_IMAGE`
is the only kind ever passed explicitly. The distinction is UI-only and is not
persisted — a texture loaded from a file reads as "Pattern" again, which only
decides which chooser the pick button re-opens.

`MyView::SetGraphics()`'s `ATTRIBUTE_BRUSHCOLOR` therefore assigns the WHOLE
brush and `ATTRIBUTE_PENCOLOR` the whole pen paint (via `CopyPenPaint()`,
which also carries the `Qt::NoPen` state and leaves width/cap/join alone),
where both used to assign only a `QColor` — `QBrush::setColor()` is a
documented no-op on a gradient or texture brush. Visible consequence:
applying a fill to a stick that had none now gives it one; "no fill" is the
combo's `None` entry, not something Apply preserves.

Three choosers, all reachable **twice**: from the type combo (on picking a
type) and from that group's original colour button
(`selectBrushColorBtn`/`chooseColorBtn`), which now re-opens the options of
the CURRENT type instead of always the colour picker — so the one button that
used to be "pick the colour" is still the one place to go back and edit
whatever the fill/stroke happens to be.

`PatternLibrary` (`patternlibrary.h/cpp`) is a floating non-modal `Qt::Tool`
grid over the `myPatterns` folder, shaped after the `.abr` brush grid — so
picking is **asynchronous** (it stays open and re-emits `patternChosen` on
every click AND on every tile-size change, which is why
`pattern_target_is_fill` latches the target). Being a palette rather than a
modal step, it needs an explicit commit: a single click only *previews* into
the Style column, while **double-clicking a thumbnail or "Apply to stick"**
emits `patternApplied` and Widget routes that through the ordinary
`SetGraphics()` path — same undo, same code, as the Apply buttons.
`GradientDialog`
(`gradientdialog.h/cpp`) edits stops/spread/angle/offset/radius/focus and
always builds a `QGradient::ObjectMode` gradient — coordinates in the painted
shape's 0..1 box, which is what makes a gradient follow a stick's
move/rotate/scale/length change for free. Image is a `QFileDialog` (picking WHICH
image comes before deciding how it sits) followed by `TextureOptionsDialog`.

**One fill and one stroke at a time, and a new pick replaces the old one whole.**
`ApplyStyleBrush()` assigns the brush and `SetGraphics()` assigns it onto the
stick, so choosing a gradient over a pattern drops the tile, its placement and its
tiling with it — nothing of the previous paint survives. The one path where that
was not true is why `ChooseStyle()` takes a `replace` flag: a pattern and an image
are both `Qt::TexturePattern`, so "Image" on a stick that already had EITHER used
to skip the file dialog and open the placement options on the old tile. The type
combos pass `replace = true` (picking a type IS an assignment); the two swatch
buttons keep passing false, which is what makes them the way back into the current
paint's options — the gradient's stops, the pattern library, another image file.

**A raster tile is never used at 1:1.** Pattern packs are authored for print
canvases (300–500 px square is typical) and an image file is worse, while a
stick is a few dozen pixels across — painted 1:1 such a tile shows one flat
crop of itself and the fill reads as a plain colour, i.e. exactly like the
feature is broken. `TextureBrush()` (`widget.cpp`) therefore scales it to
`Widget::texture_tile_size` px on its longer side through the brush
**transform**, not by resampling the QImage: the transform is part of what
`QDataStream` writes for a `QBrush`, so the size persists with no new field
and the tile keeps full resolution for a scaled-up figure or a large export.
`PatternLibrary`'s "Tile size" spin box writes that member and the Image
branch reads it, so both texture sources share one setting.

**Anchoring: `StickData::brush_follows_stick`** (one flag for the pen AND the
brush, record `version >= 4`, default true, toggled by the "Texture follows
stick" check box in the Style column's "Display options" group) decides whether a
texture/gradient turns and moves with the stick or stays put in the scene. It
exists because the stick types **disagree about which coordinate frame they
fill in**: `RECT`/`CIRCLE`/`TRAPEZOID`/`TAPER`/`TEXT`/`IMAGE` paint inside the
stick's own translated+rotated frame (a texture follows for free), while
`LINE`/`POLYGON`/`MULTILINE` draw straight in scene coordinates (it does not)
— which is exactly why a gradient-stroked LINE looked pinned to the scene
while a gradient-filled RECT rotated correctly.

`StickGraphicsItem::AlignBrush()` reconciles both by pre-multiplying the brush
transform with the stick's local frame, or its inverse, in whichever direction
that type needs. **How much of the frame goes in depends on the brush style,
and getting this wrong is not subtle:**
- a **texture** lives in logical coordinates and needs the WHOLE frame,
  translation included;
- a **`QGradient::ObjectMode` gradient that must FOLLOW the stick** is already
  anchored to the shape's bounding box, so it needs the **rotation only**, taken
  **about the centre of that box** — i.e.
  `translate(0.5,0.5) · rotate · translate(-0.5,-0.5)` (`ObjectGradientTurn()`).
  Composing the translation as well slides it off the shape and the fill
  collapses to one flat colour; rotating about the box's ORIGIN instead of its
  centre is the same mistake in miniature, and it is the easy one to make,
  because **the transform of an ObjectMode gradient acts in the box's own 0..1
  space, whose origin is its top-left CORNER**. A corner-pivoted rotation
  therefore swings the gradient axis partly outside the shape (and at -90°
  entirely outside, rendering flat black). Both measured, not theorised — with
  a throwaway probe that fills a square under a series of brush transforms and
  reads the luminance at its edge midpoints. That corner pivot was the
  "gradient follows the stick but turns the wrong way" bug.
- a **`QGradient::ObjectMode` gradient that must NOT follow the stick** is not a
  transform problem at all, in either direction. **ObjectMode *means* "relative
  to the painted shape's bounding box"**, so the box travels with the stick and
  the gradient goes with it no matter what is composed onto the brush. Undoing
  the rotation therefore un-anchors the gradient's DIRECTION and leaves its
  POSITION riding along — which is exactly how the second gradient bug looked
  ("still moves and rotates along the stick" with the check box off). Such a
  gradient is instead **rebuilt** as a `LogicalMode` one pinned to
  `scene()->sceneRect()` (`SceneAnchoredGradient()`): its 0..1 numbers are left
  untouched and a `unit → scene rect` transform maps them, so it is the same
  gradient — non-uniform radial stretch included — merely anchored to the canvas.
  This is the one case where `AlignBrush()` returns a *different brush* rather
  than a re-transformed one. The scene rect is the right anchor because every
  render path in the project already sets it to the canvas (icons, onion skins,
  `ImageExport`), so a figure's thumbnail shows the same slice of gradient the
  canvas does. `UnanchoredGradientStaysPutWhenTheStickMoves` renders a shape at
  two angles, samples a scene point inside both footprints, and requires the
  colour to be identical — plus the mirror-image counterproof with the flag on,
  so it cannot be satisfied by a gradient that has degenerated to a flat colour.

`AlignBrush()` is called once for the pen at the top of `paint()` and at each
`setBrush()`, so MULTILINE (which fills with `painter->pen().brush()`) is
covered by the pen call. Solid colours and `Qt::NoBrush` pass through
untouched — no geometry to anchor — which is also why
`Animation::UpdateSliders()` disables the check box for them.
`BrushAnchoringCompensatesPerTypeAndStyle` pins all four combinations, and
pins the pivot as a property (the box centre is a fixed point of the
transform) rather than as a hardcoded matrix.

`StickGraphicsItem::TextureToScene()` is the other half of the same knowledge:
where a texture's tile actually lands, in scene coordinates. It falls out of
`AlignBrush()`'s four cases collapsing to two — `brush.transform() · localFrame`
when the texture follows the stick, `brush.transform()` alone when it doesn't
(because `AlignBrush()` has already cancelled the frame out) — and it is what
both the non-tiling clip and the arrow handle below are built on.

### Texture placement: `TextureXf`, the options dialog and the arrow handle

A texture's scale, angle and offset live in `QBrush::transform()` and nowhere
else, which is why they persist for free. What a 3x3 matrix cannot be is
*edited*, so `TextureXf` + `ReadTextureXf()`/`WriteTextureXf()`
(`stickdata.h/cpp`) are that matrix as three numbers, and the conversion is
lossless in both directions for every brush this app builds (all similarity
transforms). `WriteTextureXf()` floors the scale on the **determinant's**
terms, not merely above zero: the determinant is `scale²`, and Qt calls
anything under 1e-12 singular, so a `1e-6` floor still produced a matrix
`QTransform::isInvertible()` rejected.

Two editors act on those three numbers, deliberately:
- **`TextureOptionsDialog`** (`textureoptionsdialog.h/cpp`), the "Image" entry's
  chooser — an image button, Scale/Angle/Offset X/Offset Y and the **Tile**
  check box, with a live checkerboard-backed preview painted with the very
  brush it returns. `Seed()` re-reads the brush on every opening, so unlike
  `GradientDialog` it never shows stale values. The Tile box is disabled (and
  forced on) when seeding for a STROKE.
- **The on-canvas arrow handle** (`MyView`), for aiming rather than for exact
  values: tail = offset, direction = angle, length = scale, and the length is
  literally the painted size of one tile so the arrow reads as "one tile is
  this big". Up to two are shown — orange for the fill, purple for the stroke —
  toggled by one check box each in the "Display options" group — **"Fill
  texture handle"**/**"Border texture handle"**
  (`fill_texture_arrow_check`/`stroke_texture_arrow_check`,
  `uiItems.h`), drawn last in `drawForeground()` and dragged through
  `HitTextureArrow()`/`DragTextureArrow()` with one `StoreUndo()` at press.

**The arrow drives a texture AND a gradient, through completely different
fields.** `QBrush::transform()` is a *texture's* placement, but a gradient keeps
its own in its geometry (`start`/`finalStop`, or `centre`/`radius`/`focus`) and
leaves its transform at the identity — and a gradient must not be edited through
that transform, because for an ObjectMode gradient it acts in the **0..1 box
space**, where a 40-unit offset means forty whole bounding boxes rather than
forty scene pixels. So there are two accessor pairs in `stickdata.h`:
`ReadTextureXf`/`WriteTextureXf` and `ReadGradientXf`/`WriteGradientXf`, and
`MyView::ReadArrow()`/`WriteArrow()` convert either to one **scene-unit**
`ArrowXf` so the drawing, hit test and drag are written once.

Mapping for a gradient: linear → the axis's midpoint, direction and LENGTH;
radial → the circle's centre, its DIAMETER (comparable with the linear axis) and
the direction of its focal highlight, re-aimed at its existing *distance* so
dragging the angle spins the highlight instead of collapsing it onto the centre.
`WriteGradientXf()` has to REBUILD the gradient (`QBrush` only hands out a
`const QGradient*`) and therefore copies the stops, spread and **coordinate
mode** across explicitly — dropping the last would silently un-anchor the fill.
`ReadGradientXf()` normalizes the angle to (-180, 180]: `QLineF::angle()` reports
in [0, 360), so negating it for `QTransform`'s clockwise convention turns +33°
into -327°, the same direction but a value the dialog would display and a
round-trip would fail on. `ReadTextureXf()` never had that problem — `qAtan2()`
already answers in the normalized range.

`ArrowUnit()` is what makes one arrow fit both: the tile's longer side for a
texture, and for a gradient the size of the box its 0..1 coordinates span — the
stick's length when it follows the stick, the **canvas** when it doesn't, since
an un-anchored gradient really does span the scene rect. Measuring an unanchored
gradient against the stick instead would move the fill a whole canvas per
stick-length dragged.

`ArrowFrameAngle()` is the other half of the same conversion, and the one that was
missing: `ArrowUnit()` converts LENGTHS, but a stick-anchored placement is also
stored in **the stick's own axes**, not the scene's — a texture's offset is a
translation `StickLocalFrame()` then rotates, and an ObjectMode gradient's is a
displacement inside a box `ObjectGradientTurn()` turns. Reading one as though it
were scene coordinates made a drag move the paint off along the **stick's angle**
instead of along the drag: harmless for a stick pointing down (`img_angle == 0`,
which is why every earlier test missed it), perpendicular for a horizontal one and
exactly **backwards for a stick pointing up** (`img_angle == 180`). So `ReadArrow()`
rotates the stored offset by `StickGraphicsItem::BrushFrameAngle()` and adds it to
the angle, `WriteArrow()` undoes both. `BrushFrameAngle()` is `0` for a
scene-anchored paint — not a special case but the same statement, since
`AlignBrush()` has already cancelled the stick's frame out of one. It is a
`static` on `StickGraphicsItem` and shares `StickFrameAngle()` with
`ObjectGradientTurn()` precisely so the handle cannot point somewhere the fill
isn't. `TextureArrowDragMovesThePaintTheWayItWasDragged` pins it by RENDERING a
radial spot and requiring it to move the way it was dragged (checked to fail with
the rotation neutralised).

**The arrows are gated on the stick TYPE, not only on the brush**:
`StickData::PaintsFill()`/`PaintsStroke()` (`stickdata.h`, next to
`IsImageType()`) are the types whose `paint()` actually hands `brush`/`pen` to the
painter — a raster type IS its image, and LINE/MULTILINE/TEXT are stroke-only — so
for the others a gradient or a tile sitting in `brush` renders nothing and the
handle was a grabbable arrow aiming at an invisible fill (the same for its check
box). Both the arrow (`MyView::TextureArrowPoints()`) and the two check boxes
(`UpdateStyleControls()`) test it now, and `UpdateSliders()`' own per-type
enablement of the two type combos — which was the same list, written inline — is
that predicate too rather than a second copy of it.

**`Animation::UpdateStyleControls()`** exists because these three check boxes are
enabled per *what the selected stick is painted with*, and that changes **with no
selection change at all**: an Apply button (`MyView::SetGraphics()`) swaps a
stick's solid fill for a gradient while the same stick stays selected.
`UpdateSliders()` only runs on a selection change, so the boxes kept the
enablement computed while the fill was still solid and stayed greyed out —
indistinguishable, from the outside, from a check box that ignores clicks. Both
paths refresh them now: `UpdateSliders()` for the selection, `SetGraphics()`
(both its branches) for the apply. `SetGraphics()` also refreshes the FRAME
thumbnail — `StickFigure::UpdateIcon()` updates its own layer row but nothing
touched `Frame::UpdateIcon()`, so the frame strip kept showing the previous fill
(or colour, or thickness) until some later canvas drag happened to repaint it
through `mouseReleaseEvent`. It is deliberately narrow — `setEnabled()`
plus one signal-blocked `setChecked()`, nothing that can re-enter an edit — since
the whole of `UpdateSliders()`, which drives every slider in the panel, is not
safe to call mid-edit. `ApplyingAGradientEnablesTheStyleCheckBoxes` pins it, and
was checked to fail with the `SetGraphics()` call removed.

### Opacity

**The brush colour's alpha is the paint's opacity** — one concept for both kinds
of paint it can mean anything for. For a solid fill it *is* the colour, which is
what the two opacity sliders always wrote. For a **texture** it multiplies the
tile: a texture brush ignores its `QBrush::color()` when rendering (Qt documents
`setColor()` as having no effect on one), which leaves that colour's ALPHA as a
field the brush already has, already streams, and otherwise never uses. So
`Widget::SetPaintOpacity()` writes there for both, `StickGraphicsItem::
FillOpacity()` reads it back at paint time, and nothing new is serialized. It is
absolute rather than compounding, exactly like the solid case.

**The sliders act on the SELECTED STICK, not only on the current paint.** They
used to write `my_brush`/`my_pen` alone, so nothing happened until an Apply
button was pressed — which, next to the anchoring and tiling boxes that apply at
once, read as the opacity simply being ignored. They now follow the same shape as
every other per-stick slider in the panel: one `StoreUndo()` at the first
`valueChanged` of a drag, latched by a `sliderFlags` member and released by
`sliderReleased()`, so a whole drag is ONE undo step.

**A gradient keeps its opacity in its STOPS instead**, scaled relative to the most
opaque one so the transparency dialled per stop keeps its ratios and one slider
value still describes it. Per-stop alpha is dialled in `GradientDialog` — a
**"Stop opacity"** spin box beside "Stop position", plus the colour picker's own
alpha channel — and that is the only way to build a gradient that FADES, which the
single-number slider cannot express. Both the stop chips and the preview are painted
over a **checkerboard** (`Checkerboard()`, `gradientdialog.cpp`): a semi-transparent
stop over an opaque background looks exactly like a paler solid one, i.e. like the
alpha did nothing. `SyncStopSpins()` echoes the selected stop back into both boxes
on every selection change and after the colour picker — without it the next nudge
would slam the newly selected stop to the previous one's alpha, the same echo
`UpdateStyleControls()` owes the sliders (`GradientStopAlphaIsEditablePerStop` pins
both halves, and was checked to fail with the echo removed). That is not a preference, it is the only place it can
live: `QDataStream` writes a `QBrush`'s colour for every style but on the way back
restores it only for a solid and a texture — a gradient brush is REBUILT from its
stops and the colour is dropped (measured with a throwaway probe), so an alpha
parked there would silently vanish on save/load. It also needs nothing at paint
time, since Qt already honours stop alpha when it renders one; `FillOpacity()`
therefore still returns 1.0 for a gradient, and fading it a second time with the
painter would square it.

The two places are hidden behind one pair of accessors, `ReadPaintOpacity()`/
`WritePaintOpacity()` (`stickdata.h/cpp`, next to the `*Xf` pair), so the slider,
its echo and the paint path cannot disagree about where a given kind of brush keeps
the number. Only `Qt::NoBrush` has nothing to fade.

That makes the echo mandatory: `UpdateStyleControls()` re-reads each slider from
the selected stick through `ReadPaintOpacity()`. Without the echo the first nudge
would slam the newly selected stick to whatever value was left over from the
previous one.

Three more consequences worth knowing:
- the STROKE goes through `setBrush()`, never `setColor()`, because
  `QPen::setColor()` REPLACES the pen's brush with a flat one and would turn a
  textured stroke into a plain colour;
- `ApplyStyleBrush()` re-applies the current slider value whenever it adopts a
  texture, because `QBrush(tile)` is born with an opaque black colour — without
  it the slider would sit at 50% while a freshly picked pattern painted solid. It
  passes `live = false`: it is adopting a brush, not editing the stick;
- `TextureOptionsDialog::Texture()` builds a fresh `QBrush(tile)` and so cannot
  carry an opacity of its own; the re-apply above is what restores it.

The **outline must not fade with the fill**, which is the second reason
`FillAndStroke()` splits into two passes (the first being the single-instance
clip): `painter->setOpacity()` on a shape's one draw call fades its outline too.
`TextureFillHonoursOpacityWithoutFadingTheOutline` renders both and checks the
outline comes out identical.

A gradient needs neither pass — its alpha is in the stops, so the outline (the pen's
own paint) was never involved.
`GradientOpacityLivesInTheStopsAndTheSliderIsEnabled` pins the stop scaling, the
ratio between two stops surviving it, the round trip through `StickData`'s stream
(including the counterproof that the brush COLOUR does not survive it) and the
slider coming out enabled where it used to be greyed.

One consequence of the stop scaling: alphas are 8-bit, so dragging down to a very
low opacity and back up quantizes the ratios between stops a little. Marked with a
`ponytail:` comment on `WritePaintOpacity()`; the upgrade, if it ever shows, is a
stored base.

Per spec the arrow is the **lowest-priority** thing on the canvas to grab:
`HitTextureArrow()` reports a miss whenever the click is also on a
transformation-box handle or within grabbing distance of any stick endpoint
handle of the selection, so a press there falls through to the ordinary rotate
exactly as if no arrow existed. That priority is what forces
**`TextureArrowAnchor()`** to be neither `P1` nor the bare midpoint: an anchor
coinciding with a higher-priority handle is permanently shadowed by it, and
since a fresh texture has offset (0,0), the offset could then never be dragged
at all. `P1` is a stick handle; the bare midpoint is where the transformation
box's side mid-handles land, because a single straight stick's bounding box is
a degenerate line. So the anchor is pushed off the stick along its normal by a
couple of **grab radii** (`PolyPickRadius()`, not scene units — the thing it
must clear is defined in grab radii and must stay clear at any zoom), on
**whichever side the arrow points**. That last part is not decoration: with a
fixed side, the one arrow length equal to the stand-off lands the tip back on
the stick — and that length is a scale value the user can perfectly well be
sitting on. The tail is latched at press (`texture_arrow_tail`) so a tip drag
that crosses the stick's own direction doesn't move the origin it is being
measured from. The anchor is a *display* anchor: the offset reported is always
exactly `tail - anchor`, so dragging is exact whatever the anchor is.

### The brush border: `StickData::stroke_brush`

The one Border type that is **not a QBrush**, and therefore the one that needed a real
`StickData` field (record `version >= 6`) instead of riding inside `QPen`'s own stream
operators: the outline is **stamped dab by dab with a brush tip** rather than traced by
the pen. A `QPen` has no way to say "walk this path with this tip at this spacing".

It is the freehand draw mode brush, pointed at a stick's outline. `BrushStroke`
(`abrbrush.h`) is the one description both use — tip, colour, size, opacity, spacing,
angle and the three jitters (size, rotation, opacity) — and `BrushDab()` builds one
jittered, tinted dab for both, so a "25% spacing" cannot come to mean two things.
`MyView::StampBrush()` composites that dab onto the freehand canvas (with pen pressure
and the eraser's `DestinationOut`); `StampAlongPath()` marches it along a
`QPainterPath` for a border. The pitch is measured over the WHOLE path, so a corner
doesn't restart the rhythm and re-stamp on itself.

Draw mode's own session undo chain (`draw_undo`, pushed per stroke by
`MyView::PushDrawUndo()`, never touching `undo_stack`) is capped at `MAXDRAWUNDO` = 20,
much shallower than `MAXUNDO`'s 50 because **one entry is a whole canvas** — a square
the size of the viewport's diagonal, so ~19 MB at 1080p — where an `undo_stack` entry is
one frame's worth of stick records. Uncapped it kept one per stroke for the whole
session and ran into the gigabytes. The committed drawing is cropped to its painted
bounds on exit (`ExitDrawMode()`), so only the live chain was ever the problem.

**Colour, dab diameter and opacity are the PEN's** (`StickData::StrokeBrush()` refills
them from `pen.brush().color()` / `pen.widthF()` / `ReadPaintOpacity(pen.brush())`) and
are deliberately NOT serialized with the rest — the thickness slider, the opacity slider
and a colour button already exist and keep driving a brush border exactly as they drive
a plain one, rather than each number existing twice and being free to disagree. Only the
stamp, spacing, angle and three jitters are stored. The colour button in question is
`BrushStrokeDialog`'s own: picking "Brush" leaves the pen a solid colour (a gradient or
tile under it would be traced by nobody), and the *other* control that opens a colour
picker for a border — the combo's "Solid" entry — drops the brush by definition, so
without that button a border's colour would be frozen at whatever the pen held when the
brush was picked.

Rendering hangs off **`FillAndStroke()`**, which for this took a `QPainterPath` instead
of a `DrawShape` lambda (and moved from `stick.h` into `stick.cpp`, since it no longer
needs to be a template): an outline is now something to be WALKED, not only drawn. The
type branches of `paint()` build their shape in the frame they always drew in and
`FillAndStroke()` installs that frame itself, from the same
`PainterIsLocalFor()`/`StickLocalFrame()` pair `AlignBrush()` reads — so the shape and
the paint filling it cannot end up in two frames. `StrokeWithBrush()` is the one place a
border chooses between the pen and the tip, which is why LINE and MULTILINE (an outline
with no fill to go with it) can call it directly. It lifts the path into **scene**
coordinates before walking it, because a dab is an IMAGE: stamped inside the stick's
rotated frame it would come out turned with the stick and stretched by any scale in the
frame, which is not what a tip's own angle and size mean. MULTILINE stamps its **centre
line**, so its per-vertex widths do not survive — which is what choosing a tip over a pen
means. `StickData::StrokesAPath()` (`PaintsStroke()` minus TEXT) gates both the renderer
and the combo entry: a text stick's "stroke" is its glyph fill, not a contour.

`BrushPicker` (`brushpicker.h/cpp`) is the library grid — collection combo over the
`Brushes` folder, thumbnails, import/refresh, the seven built-in round presets — lifted
verbatim out of `Widget`'s constructor when the border needed the same thing. Draw mode
keeps its own size/opacity/hardness/pressure controls (they write live drawing state a
border has no equivalent of) and now embeds a `BrushPicker` for the library half;
`BrushStrokeDialog` (same file) embeds a second one plus the five stored numbers and a
live preview painted with `StampAlongPath()` itself, so spacing and jitter cannot look
one way in the dialog and another on the canvas. `Widget::ApplyStyleBrush()` clears
`my_stroke_brush.stamp` for any stroke kind other than `STYLE_BRUSH` — one border at a
time, the same rule the fills follow, or the dabs would stay on top of the new pen and
the pick would look as though it had done nothing.

**A user's own brushes are a THIRD kind of collection, `.mbrush`** (magic `'MPB1'`, a
`QDataStream` of `BrushPreset` + stamp per tip, in the same `Brushes` folder and listed in
the same combo). "Save brush" stores the selected stamp with the parameters as currently
dialled; "Delete brush" rewrites the file without the selected tip and removes the file
outright when the last one goes, so the combo never keeps an empty entry. It is a format of
our own rather than a write-back into the `.abr` because **an `.abr` carries a shape and no
parameters** — which is the whole thing being saved — so `.abr`/built-in collections stay
read-only and Save falls back to `My brushes.mbrush`. The picker holds no live parameters of
its own (draw mode's spin boxes write `MyView`'s state, `BrushStrokeDialog`'s write a
`BrushStroke`), so it asks its host for them through `SetParamsProvider()`; a saved tip then
re-emits them on selection exactly like a built-in preset does, which is why `BrushPreset`
grew `angle`/`opacity_jitter` and became `qint32`-typed. Both buttons connect through
*lambdas*: `clicked(bool)` would bind straight to `DeleteBrush(bool confirm)` and pass the
button's checked state, silently skipping the confirmation.
`UserBrushCollectionRoundTripsAndDeletes` pins the codec by reading the values back off the
grid — which `Populate()` has reloaded from the file — and was checked to fail with the
loader neutralised.

**An onion render goes through `StrokeWithBrush()` too**, in flat grey — the same treatment
`paint()` gives the pen. Excluding `onion_render` from it (the first cut did) left the pen
tracing every ghost frame of a stick whose real border is stamped, i.e. **a solid outline
visible behind the brush** on a canvas with onion skins on. That is what the report of a
"solid border still visible behind the brush" was: measured at an identical 2528 px traced
vs stamped before the fix.

**`StampAlongPath()` runs inside `paint()`, so it runs on every repaint** — unlike a
freehand drag, which stamps a few dabs per mouse event, once. The first cut was ~20x too
slow (measured: 252 ms for 20 repaints of one rect outline, vs 13 ms now including the
whole scene render) for two reasons, both fixed:
- the tip was scaled and tinted **per dab**, and a real `.abr` tip is several hundred pixels
  square. `TintedStamp()` now does it **once per stroke**, and with neither jitter varying it
  the rotation is hoisted too — the common case does no per-dab image work at all, just a
  `drawImage()` of the same `QImage`. With jitter the per-dab rescale starts from the
  already-small tinted dab rather than from the full-resolution tip.
- the walk used `QPainterPath::pointAtPercent()`/`percentAtLength()`, each of which
  **re-flattens the whole path**. It is flattened once now (`toSubpathPolygons()`) and walked
  segment by segment, carrying the leftover pitch across segments and subpaths so the rhythm
  is still measured along the whole outline.

**Opacity lives in `BrushStroke::opacity` and nowhere else** — `TintedStamp()` forces the
tint colour opaque. The colour comes from `pen.brush().color()`, whose alpha IS the pen's
opacity, so leaving it in tinted the dab AND faded the painter: a border at 50% rendered at
25%.

`BrushBorderStampsTheOutlineInsteadOfTracingIt` pins all of it by rendering: that spacing
acts on a border, that the pen does NOT also trace its own outline underneath — for **every**
type with a path, with a fill behind it, counting only pen-coloured pixels — that an onion
skin is stamped rather than traced (both checked to fail with the respective code
neutralised), the version-6 round trip, and that the three derived numbers come back from
the pen.

### Image-folder patterns and Krita bundles

**A folder of `.png`/`.jpg`/`.jpeg`/`.bmp` files under `myPatterns` is a pattern collection**, next
to the `.pat` files (`PatternLibrary::Rescan()`), because that is how free packs ship. Tiles load
in natural order (`QCollator` numeric mode, so `pattern_2` comes before `pattern_10`). "Import
patterns" copies loose images into an `Imported` folder.

**A Krita `.bundle` is converted on import, never read in place**, because nothing else here
understands it (`kritabundle.h/cpp`, ZIP through Qt's private `QZipReader`, hence
`QT += core-private` in both .pro files). "Import brush" writes its presets as a `.mbrush` of the
same name through `ImportKritaBundle()` (`brushpicker.h`), so the result is an ordinary user
collection. "Import patterns" writes the bundle's `patterns/` as PNGs into a folder. Only the
`paintbrush` engine converts. The tip can be an auto brush, which becomes `MakeRoundBrush()`, or a
hard rectangle, or a `.gbr`, the first cell of a `.gih`, or a `.png` inside the bundle. Coverage:
a 1-byte `.gbr` **is** the mask (255 = paint); an image tip paints where it is **dark**
(alpha × (255 − grey)), Krita's rule. Mapped: size (diameter, or tip size × `scale`), spacing ×100,
angle (radians → degrees), opacity × flow, and a **random** (`fuzzy`/`fuzzystroke`) rotation, size
or opacity sensor as the jitter. Pressure, tilt and drawing-angle sensors are not jitter.
Erasers, other engines and tips the bundle doesn't carry (Krita's own) are counted as skipped and
reported in one message. Tips are capped at 512 px, since draw mode's size stops at 500.
`KritaBundleImportConvertsPresetsAndTips` builds a bundle byte by byte (`QZipWriter`) and reads
the result back through the picker. It was checked to fail with the darkness rule neutralised.

### Tiling: `StickData::texture_tiled`

Record `version >= 5`, default true, **fill only**, and toggled by the **"Tile
texture"** check box in the Style column beside "Texture follows stick"
(`texture_tiled_check`). It started life only inside `TextureOptionsDialog`, which
was wrong for the same reason `my_brush_follows_stick` needs a visible control:
`SetGraphics()` copies `my_texture_tiled` onto the stick on EVERY fill apply, so a
setting made once for an image then rode along onto every later fill — including a
pattern picked from the library, which had no control for it at all and could
therefore be silently clipped to a single tile with no way back. The dialog keeps
its box (it belongs next to the scale it interacts with); both write the same flag
and `UpdateStyleControls()` echoes it, so they cannot drift.

Qt has no non-repeating
brush style, so this is enforced by clipping rather than by the brush:
`StickGraphicsItem::SingleInstanceClip()` clips to the tile rectangle mapped
through `TextureToScene()`, i.e. to precisely the one instance the user's
scale/angle/offset places, and every repeat outside it is cut away.

**A single Qt draw call paints a shape's fill and its outline together**, so
that clip chops the outline down with the repeats — measured, and the reason
`FillAndStroke()` (a template in `stick.h`) exists: when and only when the clip
is in force it runs the type's own drawing **twice**, first clipped and penless
for the fill, then unclipped and brushless for the outline. The three fill
branches of `paint()` (RECT/CIRCLE, TRAPEZOID/TAPER, POLYGON) each pass their
existing draw code to it as a lambda — including their translate/rotate, since
it may run twice and must produce the same geometry both times. The clip itself
is installed while the painter is still in scene coordinates and survives those
later transforms, because `setClipPath()` converts to device space at the call.
The STROKE always tiles: an outline clipped to one tile is a fragment of an
outline, not an outline. `NonTilingTextureFillPaintsOneInstanceOnly` renders
both states and checks the outline is identical in each.

`LoadPat()` lives in `abrbrush.cpp` (declared in `abrbrush.h`) rather than a
file of its own because `.pat` is the same big-endian PSD container as `.abr`
and ends each colour plane with the same bitmap tail: `DecodeRle()` (PSD
PackBits) and `ReadUcs2Name()` are reused verbatim. Three things about the
real on-disk format cost the first cut of this reader a total failure to load
anything, so they are worth stating plainly:
- the file opens with a **`"8BPT"` signature**, then an **int16** version
  (like `.abr`, not int32), then an int32 pattern count;
- the VMA list's `channels` field is a **capacity, not a count** — real files
  write 24 (Photoshop's maximum) and then 3 or 4 planes followed by ~22 empty
  slots, so the arrays must be read until the list's own end;
- packs are full of **CMYK** (`image_mode == 4`), stored INVERTED (0 = full
  ink), which is why `PlanesNeededFor()` keys the plane→pixel mapping off the
  mode: four planes are CMYK's inks in mode 4 but RGB-plus-alpha in mode 3,
  and guessing from the count alone silently wrecks the colours.
Scope is 8-bit Grayscale/RGB/CMYK, raw or PackBits, alpha honoured where
present; other modes are skipped per tile, not per file. Both writer dialects
(with and without a per-record length prefix) work, probed via the record's
`version == 1`. Verified against two shipped packs — one CMYK, one RGB+alpha.

The foreign exporters drop all of this, as they must: `StkFormat`'s
`FlattenFill()` reduces any brush to a gradient's first stop or a texture's
average pixel — **not** `QBrush::color()`, which is an unset black for those
styles and would blacken every gradient-filled figure — and both `ExportStk()`
and `ExportNodes()` append one `"N stick(s): ... exported as a flat color."`
warning. `.stck` is the *native* single-figure path and keeps everything.
`InterpolateStickData()` is deliberately untouched: its `brush.setColor()` is
that same no-op, so a gradient fill simply doesn't tween between keyframes.

