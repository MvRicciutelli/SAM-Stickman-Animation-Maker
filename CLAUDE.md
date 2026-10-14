# MarionettaPlus

Qt5/C++ desktop app (qmake project) for drawing and frame-by-frame animating
stick figures, with video export via ffmpeg. Windows/MinGW 6.6.1 Qt kit.
Build directories (`build-MarionettaPlus-*-Debug/Release`) live as siblings
of this folder, not inside it (qmake shadow build).

## Data model

`Animation` owns `QList<Frame*> frame_list`.
`Frame` owns `QList<StickFigure*> stick_figures` plus a flattened
`QList<StickData*> total_sticks` (all sticks across all figures in the
frame, used for Z-ordering).
`StickFigure` owns `QList<StickData*> stick_list`.

Since refactor Phase 2c, a "stick" is two separate objects instead of one
fused class: `StickData` (`stickdata.h/cpp`) is the persistent record —
no `QGraphicsScene`/widget dependency, exists for scratch/buffer objects
too — and `StickGraphicsItem` (`stick.h/stick.cpp` — class renamed, files
deliberately left named `stick.*`) is the `QGraphicsItem`-derived view: it
holds a non-owning `StickData *data` and adds only what genuinely needs a
scene or reflects live UI/drag state (endpoint handles, selection/
highlight, angle/scale caches). The intra-figure tree (`parent`/
`children`, flattened — `children` lists ALL descendants, not just direct
children) lives entirely on `StickData`, typed `StickData*`.

Not every `StickData` has a view: `StickData::view` is a non-owning
`StickGraphicsItem*` back-pointer, `nullptr` for every scratch/buffer/undo
object (always) and briefly `nullptr` for an in-progress draw until
`MyView::DrawCmd()` calls `FramePresenter::AttachView()`. Any code walking
`stick_list`/`total_sticks`/the tree that needs to reach the view (repaint,
select, rotate/scale interactively) must go through `->view->` guarded by
a null check — `FramePresenter::AttachView(stick_data, scene)` /
`DetachView(stick_data, scene)` (`framepresenter.h/cpp`) are the only code
that constructs or destroys a `StickGraphicsItem` for a stick actually on
screen; wrapping a LIVE `StickData` in a second `new StickGraphicsItem(...)`
(e.g. for a throwaway render/icon) steals its real view's back-pointer —
clone the `StickData` first (`Clone()`) and wrap the clone, never the
original (see `StickFigure::UpdateIcon()`/`Frame::UpdateIcon()` for the
pattern: destroy the render scene's views *before* `qDeleteAll()`-ing the
clones, since `~StickGraphicsItem()` reads `data->view` during its own
destruction).

Since refactor Phase 2b, every scratch/buffer object (undo/redo snapshots,
frame copy/paste, stickfigure copy/paste, the stickfigure library) is a
pure-data `FrameData`/`StickFigureData`/`StickData` instead of a live
`Frame`/`StickFigure`/`stick` — see "Serialization" below.

`StickData::is_linker` marks the invisible, `NoPen` bridge stick that
`MergeStickFigures()` (`StickFigure.cpp`) creates to graft a joined
figure's former master into the host figure's tree. `SplitStickFigures()`
walks back up a split stick's parent chain afterward and deletes any
linker left with no remaining children — without this a split left an
orphaned linker (and its endpoint handles, re-added forever by
`MyView::AddHandles()`) permanently attached to the host figure. The GC
loop handles chained merges (a linker whose parent is itself a
now-childless linker) by continuing up the chain.

`QGraphicsScene::addItem()` transfers ownership to the scene. A
`QGraphicsItem`'s destructor auto-detaches from its scene safely on its own,
regardless of whether `removeItem()` was called first — never assume you
must call `removeItem()` before `delete`.

Reach "the current frame/stickfigure/stick" via
`Animation::CurrentFrame()` / `Frame::CurrentStickFigure()` /
`Frame::CurrentStick()` (all trivial inline accessors, each returning a
**reference to the pointer** — e.g. `CurrentStickFigure() = someFigure;`
is a valid assignment, exactly like the raw member it wraps). These
replaced the `CS`/`CURRENTSTICKFIGURE`/`CURRENTFRAME` preprocessor macros
(Phase 5) that used to expand to a different hand-written expression per
file (`my_animation->...` inside `MyView`'s own methods,
`view->my_animation->...` in `widget.cpp`/`mymainwindow.cpp`,
`V->my_animation->...` in the `advanced*widget.cpp` files) — same
reachability, but as real, symbol-searchable, compiler-checked calls
instead of blind text substitution.

### The TEXT stick

`StickData::TEXT` (tool `DRAWTEXT`, both 25 — the tool-id == stick-type
convention) is a paragraph of text laid out along its stick. Drawing takes
three steps: origin click, free-end click, then a `QInputDialog`
multi-line prompt in `MyView::DrawCmd()` (cancel/empty deletes the stick
again through `DeleteWholeStick()`). That prompt runs BEFORE the
`UpdateIcon()`/`UpdateTab()`/`UpdateSelection()` trio at the end of the
second click, since those are what mirror the stick into the properties
panel — with the text set afterwards the box echoed an empty string.
Its own fields are `text`/`text_font`/`text_align`/`text_line_spacing`
(vertical)/`text_v_stretch`/`text_word_wrap` (`stickdata.h`); **bold,
italic, underline, strikeout, horizontal letter spacing
(`QFont::setLetterSpacing`, PercentageSpacing) and horizontal stretch
(`QFont::setStretch`) ride
inside `text_font` itself** — QFont streams them all, which is why the
field is a QFont and not a family+size pair. The VERTICAL stretch is the
one QFont has no equivalent for: it's a percent field applied as the
y-scale of `TextFrame()` (so it stretches the line pitch with the glyphs)
and is gated on record `version >= 3`, the same way version 2 gates the IK
fields. **Placement reuses the IMAGE fields**, `img_offset` for
the h/v offset and `img_angle_offset` for the extra rotation, so the
offset panel, `InterpolateStickData()` and the undo/clone paths needed no
new plumbing (same trick POLYGON already uses). The skeleton is painted
only while `text` is empty, which is exactly what makes the 2-click
preview visible; the text colour IS the stick's pen colour, and the wrap
width IS `my_line.length()` (the font size does not scale with the
figure). `stick.cpp`'s `TextFrame()`/`LayoutText()` are the single source
of truth shared by `paint()` and `ConstructBr()`. Selecting one shows two
tabs (`Animation::UpdateTab()`): the code-built `advanced_text_tab`
(content, font, size, bold/italic/underline/strikeout, justification, V/H
spacing, H/V stretch, wrap — wired in `Widget`'s
ctor, echoed back by `refresh_text_panel` — one `std::function` global in
`uiItems.h` instead of an extern per control) plus `advanced_img_tab` as
"Text Placement", of which `Animation::UpdateSliders()` disables the parts
a text stick has nothing to act on — the width/height SCALE controls (they
write `img_w_scale`/`img_h_scale`, which nothing reads for text) and the
image-variant list plus its swap/add/remove buttons (`img_list_widget` /
`img_buttons_row`, the latter being `ui->frame_44`, the one frame holding
all three) — re-enabling them for every other type. That lives in
UpdateSliders(), not UpdateTab(), because only the former runs on every
selection change rather than only on a type change. A font missing on load is replaced with Arial by
`StickData::operator>>`, which records the family in the
`missing_text_fonts` list that `MyView::ReportMissingTextFonts()` (called
by every load path) turns into one prompt. The foreign `.stk`/`.nodes`
exporters have no text: a TEXT stick leaves as a plain segment.

### The MULTILINE outline

A MULTILINE is not stroked by the pen: per-vertex widths (`vertex_widths`) are something
`QPainterPathStroker` cannot do, so `paint()` builds the band's outline itself and fills it.
It follows the stroker's own rules, and each one fixed a visible bug: offset along the
bisector of the two segments' **unit** directions (the old prev→next chord is length-weighted
and points backwards at a sharp turn into a longer segment — the band flipped and "twisted on
itself"); a true miter (`h / cos(half turn)`) up to QPen's default limit; past it, a bevel on
the outer side with the inner side running through the vertex; and `Qt::WindingFill`, which
fills the inner overlap that rule produces (odd-even left a hole). A miter merely *clamped*
to the limit is wrong: it bends both edges toward the tip and thins both segments.
`MultilineSharpTurnKeepsAFullWidthBand` pins it by rendering.

### Converting a stick's type

`MyView::ConvertStick()` turns any stick type into any other, in place, as one
`StoreUndo()`. It keeps `my_line`, the IK role/chain, the tree links, `pen` (colour,
thickness, border brush) and `brush`, and is **destructive by spec**: it first drops
everything only the old type reads (POLYGON/MULTILINE vertices, IMAGE/FREEHAND images —
IMAGE's `img_list_widget` rows too — TEXT's `text`, and the shared `img_offset`/
`img_angle_offset`/`img_*_scale` placement), then seeds the new type the way `DrawCmd()`
seeds a fresh one (RECT/TAPER/TRAPEZOID widths from their sliders, MULTILINE's two endpoint
widths from the pen). Two seeds are this function's own: a POLYGON starts as a **diamond
through both ends** (an empty one is invisible), and a FREEHAND enters draw mode on a blank
canvas. IMAGE and TEXT need user input, which the caller (the Widget lambda) asks for
BEFORE calling, so cancelling leaves neither a half-converted stick nor an empty undo step.

The UI is a single "Convert to" row (combo + button; `convert_type_combo` in `uiItems.h`,
its parent is the row). A widget has one parent, so `Animation::UpdateTab()` moves that row
to the top of the first advanced panel of whatever type it is showing, and preselects that
type.

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

## The flag-guard system (read this before touching lifetime/serialization code)

Historically, six global booleans gated whether a `StickFigure`/`Frame`/
`stick` under construction or destruction was "live" (attached to the real
scene/UI) or "scratch" (an invisible buffer that must not touch the scene
or UI widgets): `undoFlag`, `copyFlag`, `libFlag`, `loadingAnimationFlag`,
`clearUndoFlag`, `switchFrameFlag`. This was the single most bug-dense
subsystem in the codebase.

**All six flags are gone.** `copyFlag`, `libFlag`, `clearUndoFlag`,
`loadingAnimationFlag` had zero remaining declarations or assignments once
Phase 2b turned every scratch object into pure data — a `StickFigureData`
has no scene, no list widget, no icon to guard, so there was nothing left
for those flags to disambiguate. `switchFrameFlag` (Phase 2c) turned out
to already be unreachable dead state after Phase 2b (its only set-site no
longer wrapped any call that reads it). `undoFlag` (Phase 4 follow-up) was
the last: it guarded a double-free between `Frame::ClearFrame()` and
`StickFigure::~StickFigure()`, both of which thought they owned stick
cleanup. Fixed at the root instead of guarded: `~StickFigure()` now
unconditionally owns cleanup (removes each stick from the scene, calls
`RemoveImagesFromListWidget()`, deletes it — no flag needed since every
remaining `StickFigure` is guaranteed live post-Phase-2b), and
`ClearFrame()` no longer duplicates that work — it's a plain
`qDeleteAll(stick_figures)`. Fixing this also closed a real, pre-existing
leak: undoing an add-frame (or redoing a delete-frame) used to leak that
frame's sticks, since the old flag-guarded destructor skipped freeing them
during `Animation::DeleteFrame()`'s teardown. See "Serialization" below
for the data classes and their bridge functions.

`stick.cpp` itself has been flag-free since Phase 1b: `~StickGraphicsItem`
never touches `img_list_widget`; the code paths that delete a LIVE stick
(`DeleteStick`, `Weld`, `CancelDrawing`, `~StickFigure`'s live branch) call
`StickGraphicsItem::RemoveImagesFromListWidget()` explicitly instead — on
the stick's own view, not just its children's (a real bug this refactor's
retype surfaced and fixed: `DeleteStick()` called it for every child being
swept away but not for the selected stick itself, leaking rows in
`img_list_widget`; caught by the pre-existing
`DeleteImageStickClearsImageListWidget` characterization test).

### The second wave: dangling app-level globals (post-refactor safety sweep)

A later, separate pass (unrelated to the six historical flags above, which
were about live/scratch disambiguation) audited every remaining
file-scope global in the project — mostly single-purpose booleans/state
that only one class ever touched, left at file scope from before the
class existed or as a habit. Two outcomes per global:

- **Genuinely dead → deleted.** `change_type_flag` (never read anywhere),
  `load_file` (write-only — set around `Animation::LoadAnimation()`, never
  checked), `render_flag` (write-only — set around `ImageExport::run()`'s
  render, never checked), `busy_play` (always exactly mirrored
  `play != nullptr`; `Widget::on_PlayButton_clicked()` now checks `play`
  directly instead of a redundant twin).
- **Actually needed, but scoped wrong → folded into the one class that
  reads it**, as a member instead of a free global:
  - `clearing_animation` → private member of `Animation`.
  - `play_back` → member of `MyView` (read throughout `MoveToFrame()`).
  - `start_stick_figure_rotation`/`start_stick_figure_scale` → `static`
    members of `StickFigure` (one rotate/scale gesture at a time exists
    app-wide, hence `static` rather than per-instance).
  - `onion_render` → `static` member of `StickGraphicsItem` (same
    one-at-a-time reasoning; read by `paint()`).
  - the `sliderFlags` struct → private member of `Widget`; the analogous
    per-tab drag latches (`rect_w_slider_flag`, `taper_w_slider_flag`/
    `taper_h_slider_flag`) → members of `AdvancedTab`/
    `AdvancedTaperWidget` respectively (renamed to `snake_case` in the
    move).
  - `MySleeper`'s `pausePressed`/`intervalTime`/`previewGenerator` → members
    (`pause_pressed`/`interval_time`/`preview_generator`).
  - `imageexport.cpp`'s `render_option`/`export_path` → members of
    `ImageExport` (found later: a preview started during an export overwrote
    them, and the export finished without encoding its video).

**`ImageExport::run()` never reads the live animation.** The user keeps
editing while an export or a playback preview renders, so the constructor
(UI thread) clones every frame's `total_sticks` plus `W`/`H`/`my_rect`, and
the worker renders only that snapshot — cheap, since the images inside are
shared copy-on-write. PREVIEW mode is the same class (`MyView::PreparePreview()`
is gone): it returns `QImage`s, and `MySleeper::StartPlaying()` turns them into
pixmaps on the UI thread, because `QPixmap` is GUI-thread only. Preview frames
are rendered at the view's zoom (capped at 1:1, times the device pixel ratio)
and scaled back up by the item, so a zoomed-out preview no longer holds
full-canvas pixels per frame.

The re-scoping itself is behavior-preserving. The same pass also fixed a
number of unrelated memory-safety issues found along the way (out-of-bounds
reads on corrupt/malformed files, `nullptr` dereferences of
`Frame::CurrentStick()`/`CurrentStickFigure()` reachable after an
undo/redo restore, a stale `StickData*`→`QGraphicsItem*` cast in
`MyView::ChangeTool()` left over from before the Phase 2c stick/view
split, a thread-teardown race in the image-export/playback path) — see
git history (`Full-project safety sweep...`) for the complete list rather
than duplicating it here. If you go looking for one of the old flag names
(e.g. a stale memory of `start_stick_figure_rotation` as a bare global)
and don't find a file-scope declaration, check the owning class first —
it's now a member, not gone.

## Command flow: how a UI edit reaches the data model and becomes undoable

Since refactor Phase 4, undo/redo is driven by a `QUndoStack` member on
`MyView` (`undo_stack`), not hand-rolled buffers. Every mutating UI handler
still follows the exact same shape it always has — **this did not change**:

```cpp
void Widget::on_someBtn_clicked(){
    view->StoreUndo(CMD_XXX);   // snapshot PRE-change state, BEFORE mutating
    // ...directly mutate the live Animation/Frame/StickFigure/stick objects...
}
```

`StoreUndo(command)` still runs *before* the mutation, with the exact same
signature and call sites as before Phase 4. Internally it now constructs a
`FrameUndoCommand` (`myview.cpp`, not exposed in any header — only
`StoreUndo()`/`RestoreFrameSnapshot()` need it) and pushes it onto
`undo_stack`. `command` (`CMD_SIMPLE` — default, `CMD_ADDFRAME`,
`CMD_DELETEFRAME`, `CMD_MOVETOFRAME`, see `myview.h`) still records *what
kind* of structural change is about to happen, not the change itself.

**Why the call sites didn't need to change**, even though `QUndoCommand`'s
contract is normally "`redo()` performs the mutation": several tools in
this app are drag/multi-click gestures whose mutation spans multiple event
handler calls (`mouseMoveEvent` during a drag, `valueChanged` during a
slider drag, the second click of a 2-click draw) — for these, `StoreUndo()`
was always called once at gesture *start*, and the pre-Phase-4 code never
captured an explicit "after" state either; it only computed one lazily, at
the moment the user actually clicked Undo. `FrameUndoCommand` preserves
this exactly:
- **Constructor** captures `before_` (`ExtractFrameData` of the current
  frame) — this is what `StoreUndo()` always did.
- **`redo()`'s first call** (made automatically by `QUndoStack::push()`,
  before the caller's mutation has even run) is a deliberate no-op — the
  caller's own inline code, unchanged, performs the edit immediately after
  `StoreUndo()` returns, exactly as before Phase 4.
- **`undo()`'s first call** captures `after_` lazily (the live frame,
  whatever the edit produced) before restoring `before_`.
- Every subsequent `redo()`/`undo()` is a pure, symmetric snapshot restore.

`MyView::RestoreFrameSnapshot(command_tag, mode, snapshot)` is what
`FrameUndoCommand::undo()`/`redo()` call to actually restore a snapshot —
it is the pre-Phase-4 `undoRedo()`'s dispatch body, verbatim, just taking
an explicit snapshot argument instead of popping one off a buffer:
- `CMD_ADDFRAME`+undo or `CMD_DELETEFRAME`+redo → the frame at
  `snapshot->frame_number` must disappear: `DeleteFrame(current_frame)`,
  then clear and re-populate the (now-shifted) frame at that index.
- `CMD_DELETEFRAME`+undo or `CMD_ADDFRAME`+redo → the opposite: a frame
  must be (re)created at `snapshot->frame_number` via `SetUpFrame()` before
  populating it.
- `CMD_MOVETOFRAME` → just clear + repopulate the frame at
  `snapshot->frame_number` (no add/remove, only content changed).
- `CMD_SIMPLE` (else) → clear + repopulate `current_frame` directly; used
  for in-place edits (stick moves, scale/rotate, add/remove stick or
  stickfigure) that don't change which frame is current.

`QUndoStack` handles what the old code did by hand: pushing a new command
after an undo (without redoing first) automatically discards the stale
redo history (replacing the old manual `!undoFlag` + `redoBuffer.clear()`
check), and `undo_stack.setUndoLimit(MAXUNDO)` (set once, in the
constructor) replaces the hand-rolled per-push eviction. Autosave
(`op_count`/`auto_save_interval`/`AutoSaveTrigger()`) moved to a lambda
connected to `QUndoStack::indexChanged`, which fires on push *and* on
undo/redo — so autosave now also triggers after an undo/redo, not just
fresh edits.

`RestoreFrameSnapshot()` calls `Frame::ClearFrame()` with no flag needed
around it — see "The flag-guard system" above for how `undoFlag` (the
last of the original six) was eliminated by fixing the double-ownership
it guarded, rather than guarding it more carefully.

**The one exception to the `StoreUndo()` shape** is
`FrameBatchUndoCommand` (`myview.cpp`, the second and only other
`QUndoCommand` in the project): a contiguous BLOCK of frames appearing or
disappearing at once. `FrameUndoCommand` snapshots exactly ONE frame, so it
cannot express "N frames appeared at once"; this command instead owns the
N ready-made `FrameData` snapshots and is a textbook `QUndoCommand` —
`QUndoStack::push()` calls `redo()` and that is what actually performs the
edit, so the two directions are symmetric. Its two halves are `Insert()`
and `Remove()`, and a single `inserting_` flag decides which one is
`redo()`, **because "undo an insertion" and "do a deletion" are the same
operation**: one class covers all three callers instead of one per verb.
- `MyView::GenerateInbetweens(start_index, count)` (the "generate
  inbetweens" button) → `InsertFrames()`. It does NOT call `StoreUndo()`
  and does NOT mutate anything inline — it validates, builds the
  snapshots, and pushes; neither direction touches the two keyframes.
- `MyView::InsertFrames(insert_at, snapshots)` — also the multi-frame
  paste (see below). **Ownership of the snapshots passes to the undo
  stack**, which frees them when the entry is evicted, so a caller that
  keeps a copy buffer must hand over `Clone()`s.
- `MyView::DeleteFrames(first_index, count)` — the delete-frame button
  with several rows selected. It snapshots the whole block FIRST (that is
  what an undo restores it from) and refuses to empty the animation.

**`Remove()` empties the layer list up front** (`MyView::ClearLayerRows()`), and that is
not tidying: `Animation::DeleteFrame()` cleans up the layer rows for
`frame == current_frame` only — using "is the current frame" as a proxy for "its figures
own real rows" — yet it is **itself what reassigns `current_frame`** on every call. So from
the second iteration of the loop on, `current_frame` is a frame that was never displayed,
and if a later iteration deletes it, `QListWidget::row()` is handed a `QListWidgetItem`
that `MoveToFrame()` already destroyed → **segfault**. It needs the block to sit at the
END of the animation (there the reassignment falls back on `frame_list[count-1]`, i.e. the
very next frame the loop deletes), which is why inbetweens — always between two keyframes,
so never last — never hit it, and a paste at the end does. Clearing at the top of
`Remove()`, where `current_frame` is still genuinely the displayed one, is what makes every
iteration safe rather than making each one guess.

`ClearLayerRows()` is also what `MoveToFrame()` clears the list through, which closes the
underlying hole for good: the rows are the only `QListWidgetItem` a `StickFigure` may own,
and nothing used to null `StickFigure::linked_item` when they died, so **every frame that
was displayed and then left kept dangling ones**. Reachable by any future code that deletes
a non-displayed frame, not just this command. Every other reader of a non-displayed
figure's `linked_item` already null-checks (`StickFigure::UpdateIcon()`,
`MyView::UpdateSelectionDecorations()`, the layer-selection helpers), so nulling costs
nothing.

`Insert()` passes `attach_to_scene = false` to `PopulateLiveFrame()` (like
the file-load path) because attaching would put every frame of the block's
sticks on the scene at once, while `MoveToFrame()` only ever removes one
frame's worth. It also **rewrites each snapshot's `frame_number` to the
position it is entering at**, since `PopulateLiveFrame()` copies that field
from the data onto the live frame and would otherwise undo what
`SetUpFrame()` just numbered (a pasted snapshot carries its ORIGIN's
number). `Remove()` deletes from the last index down, so the earlier
indices stay valid, and lands on the frame before the block.

### Multiple frames: selection, copy/paste, delete

The frame list is a `QAbstractItemView::ContiguousSelection` (set in
`Widget`'s ctor) — shift-clicking a row extends the block, and that native
mode is the whole of the "select more than one frame" feature.
`ExtendedSelection` was not used on purpose: it also allows *sparse*
selections, which none of the three operations below can treat as a block.

**The previewed frame stays the one the selection started from**, which is
why `Widget::on_frameListWidget_itemClicked()` returns early on a shift
click: Qt has already extended the block by then, and `MoveToFrame()` would
both switch frame and collapse the selection back to one row through its
own `setCurrentItem()` call. Qt's shift anchor is the last *pressed*
position (not the current index), so it stays on the previewed frame across
repeated shift clicks on either side of it.

**`MoveToFrame()` passes `QItemSelectionModel::ClearAndSelect` explicitly** when it selects
the new frame's row, instead of the `linked_item->setSelected(true)` it used to: Qt takes a
different branch per selection mode inside `QListWidgetItem::setSelected()` and only clears
the previous selection in **SingleSelection** (measured with a throwaway probe — in
ContiguousSelection two rows end up selected, and a following bare `setCurrentItem()` does
not collapse them either). So the moment the list stopped being SingleSelection, every frame
switch left the frame it came FROM selected next to the new one — most visible on "duplicate
frame", where the source and the copy both stayed highlighted. A frame switch always defines
a fresh one-row selection; the multi-frame selection is built by shift-clicking, which never
reaches `MoveToFrame()` (`Widget::on_frameListWidget_itemClicked()` returns first).
`MoveToFrameSelectsOnlyTheNewFrame` pins it and was checked to fail with the old pair
restored — the frame-list selection mode is set in `Widget`'s ctor, which the test binary
does not link, so the slot sets it itself and restores it from a destructor.

`Widget::SelectedFrames()` is the one reader — rows walked in list order
(`selectedItems()` promises no order; the list is kept parallel to
`Animation::frame_list` by `Animation::SetupFrame()`/`MyView::DeleteFrame()`),
falling back to the current frame when nothing is selected.

`frame_copy_buffer` is now a `QList<FrameData*>` **member of `Widget`**
(it was a file-scope global, and leaked at exit): one snapshot per copied
frame. Copying a whole block costs almost nothing in memory — a snapshot
shares its images with the live sticks, `QImage` being copy-on-write (see
"Images are shared" below) — so no cap is needed on how many frames go in.

**Paste is two different operations, told apart by how many frames were
copied**, and deliberately so: a block enters AS frames after the current
one, in its original order (`InsertFrames()`, one undo step), while a
single frame keeps its historical behaviour of MERGING its figures into the
current frame as new layers. That merge is the only way to fold one frame's
figures into another, so unifying the two would remove a feature rather
than tidy one up.

## Serialization

Since refactor Phase 1a/2a/2b/2c, every layer's persistent state lives in a
`*Data` class with no scene/widget dependency, mirroring the live class it
was extracted from:
- `StickData` (`stickdata.h/cpp`) — position, type, image list, the
  figure-tree `parent`/`children` pointers (typed `StickData*`), and (since
  Phase 2c) the non-owning `StickGraphicsItem *view` back-pointer. No
  `QGraphicsItem` base — see "Data model" above for the full split.
- `StickFigureData` (`stickfiguredata.h/cpp`) — name, `stick_list`,
  `master_stick`/`current_stick`.
- `FrameData` (`framedata.h/cpp`) — `stick_figures`, the flattened
  `total_sticks` z-order list, `frame_number`/`num_of_items`.

Each `*Data` class has its own `QDataStream operator<<`/`operator>>` and a
`Clone()` that deep-copies in memory with no stream involved. These
operators have no flag guards — they read/write data and nothing else.
**Not byte-compatible with the original pre-refactor format** — backward
compatibility was explicitly waived when two format gaps were fixed:
`StickFigureData` now also streams `current_stick`'s index (previously
silently dropped on every save, so reloading/copying a figure always
reset selection to its first stick), and every serialized integer field
uses the fixed-width `qint32` instead of plain `int` (whose width is not
guaranteed by the language), with `StickData::z` streamed as an explicit
`double` regardless of whether `qreal` is built as `float` or `double`
locally. Every remaining `int`/`>>`/`<<` pair in a `*Data` operator was
audited for the same class of bug during that pass — if you add a new
serialized field, use `qint32`/explicit `double`, not `int`/`qreal`.

**Frame names are the one field that is NOT streamed by its own `*Data`
class.** `Frame::name`/`FrameData::name` (set by double-clicking a frame
row, shown by `Frame::UpdateLabel()`) travels through `Clone()`/
`ExtractFrameData()`/`PopulateLiveFrame()` like every other field, but
`FrameData::operator<<` deliberately ignores it: the names are written as
a trailer at the END of `Animation::operator<<` instead, gated on the new
animation format `version = 2`. A trailer is ignorable, so every existing
version-1 `.mAnim` (and every `.marlib`, which never carries names) still
loads unchanged. `Animation::LoadAnimation()` resets `version` to the current
format after reading, so re-saving an old file writes a complete current
one rather than an old header with new content.

### Files from a newer version are refused, not misread

Three versions are on the wire and each is checked: the `.mAnim` header
(`Animation::CURRENT_VERSION`, 3), `StickFigureData::version` (1) and `StickData::version`
(`StickData::CURRENT_VERSION`, 6). `FrameData` has none. **Raising a format means raising its
`CURRENT_VERSION` constant** — the in-class default of `version` is that constant, so a writer
and the reader's limit cannot drift apart. The stick record is the one that matters most: it
went 2→6 while the file header stayed at 3, so a newer build's `.mAnim` can hold a too-new
stick under a perfectly supported header, discoverable only mid-stream.

`RejectNewerVersion()` (`stickdata.h`) is called right after each `version` read. It puts the
stream into `ReadCorruptData` — every loop that already bails on `status() != Ok` then stops
instead of parsing the next record's bytes as this one's extra fields — and records the reason
in the global `file_load_error`, the same shape as `missing_text_fonts` (the data layer has no
UI). Every loader clears it BEFORE reading, so a stale message cannot fail a later load; the UI
layer shows it through `MyView::ReportRejectedFile()`, which also drops the load-time image pool
and the font list that the refused read filled (`ReportMissingTextFonts()` is skipped on this
path and would have drained them).

**A refused load changes nothing**, which is why the loaders now parse into pure data first:
- `Animation::LoadAnimation()` (returns `bool`) reads an `AnimationFileData` and only then
  `ClearAnimation()`s and applies it. It used to clear first, so any unreadable file had already
  destroyed the document; the same restructure makes a file with no readable frame a refusal
  instead of leaving a frame-less animation (`current_frame == nullptr` crashes most handlers).
  `operator>>(Animation)` is gone. A truncated file still keeps its leading frames, as before.
- `LoadStickFigureData()` replaces `StickFigure::LoadStickFigure()`, which streamed into a layer
  the caller had already added. The handler now reads, and only then does `StoreUndo()` +
  `AddStick()` — the order the foreign-format imports always used.
- `MyView::LoadLibrary()` (returns `bool`) swaps a freshly parsed buffer in only on success.

Two writes would have turned a refusal into data loss, and are guarded:
- `on_actionload_animation_triggered()` set `animation_path` **before** loading. Autosave and
  "Save" write to it, so a refused file would have been overwritten by the still-open (older)
  animation. It is now set, and undo cleared, only after a successful load.
- "Save current library" writes the buffer to the **selected list row's** file; after a refusal
  the buffer is some other library. `refused_library_files` makes `SaveLibrary()` refuse (with a
  warning) to overwrite such a file for the rest of the session — including an explicit
  "Save as" onto the same name.

Pinned by `NewerStickRecordVersionIsRefused`, `NewerStickFigureFileIsRefusedWithoutAnyLiveChange`,
`NewerAnimationFileIsRefusedAndTheOpenAnimationIsKept` (header-newer AND stick-newer, asserting
the very same `Frame` objects are still there) and `NewerLibraryIsRefusedKeptAndNeverOverwritten`
(byte-compares the file); each was checked to fail with its guard neutralized. NOT covered: the
two `mymainwindow.cpp` handlers (ordering of `animation_path`/`ClearUndo()`), since the test
binary doesn't link that file. QMessageBox is modal — a test that reaches one needs
`ModalDismisser` (file scope in the test source) or it hangs headless.

### Images are shared, never duplicated: the pool and the `ImageTable`

**`QImage` is copy-on-write, so every in-memory duplication this project
already does is free and must be left alone.** `StickData::CopyFrom()`'s
`new QImage(*image)` shares the pixel buffer; measured, 60 clones of an
11 MB picture cost **0 MB** (identical `constBits()`), and a write detaches
automatically. So `Clone()`, undo snapshots, frame/figure copy-paste,
inbetweens, icon/onion/export render clones cost nothing in pixels — the
doc comment on `CopyFrom()` describes the *semantics* ("never shares", i.e.
a write can't leak across), not the memory. Do not "optimize" those paths;
there is nothing there.

The duplication was entirely at the two ends of a **file**, where four
fields carry pixels: `stick_img_list` (IMAGE/FREEHAND), the texture inside
`brush`, the texture inside `pen`'s brush, and `stroke_brush.stamp` (a
border-brush tip, and by far the heaviest at ~900 KB encoded).

- **`operator>>` decoded an independent buffer per record**, so a figure
  that merely exists through N frames cost N copies of each of its images.
  Fixed by a load-time content-hash pool in `stickdata.cpp`
  (`PoolImage()`/`PoolBrushTexture()`/`PoolPenTexture()`): identical pixels
  read anywhere in one file end up sharing one buffer. Measured on the real
  operators: **898 MB → 19 MB** for 360 records. The pool holds a reference
  so sharing can span a whole file, and is drained by
  `ClearStickImagePool()` — called from `MyView::ReportMissingTextFonts()`,
  *before* its early return, because that is already the "a load just
  finished" hook every load path calls (same convention as
  `missing_text_fonts`). One choke point covers `.mAnim`, `.stck` and
  `.marlib`.
- **`operator<<` encoded an independent copy per record.** Fixed by
  `ImageTable` (`stickdata.h`) + animation format **`version = 3`**: the
  distinct images are written ONCE, ahead of the frames, and records write a
  `qint32` index. Measured: **512 MB → 6 MB** for a realistic 60-frame
  scene. `qCompress` is no alternative — zlib's 32 KB window cannot span
  PNG blocks megabytes apart (10 identical blocks: 14 MB → 14 MB).

`InstallImageTable()` installs/removes the table and returns the previous
one — **always restore it**, or a dangling table outlives its stack frame.
With none installed, records inline their pixels exactly as before, which
is what keeps `.stck`/`.marlib` byte-compatible; only `.mAnim` uses a table.
The write is **two passes** (frames into a scratch `QByteArray` first,
filling the table as a side effect, then table + payload) because the table
must be complete yet sit *before* the records that need it — a
`QDataStream` is read front to back, unlike the ignorable v2 name trailer.
Serializing into that buffer through the very same operators, rather than
walking the model to collect images separately, is what stops the table
from ever disagreeing with what the records reference. The table write is
**gated on `my_animation.version`**, not merely on a table being present:
writing one while the header says v2 desynchronizes the reader at the first
frame (found by a counterproof run, not in review).

A **texture inside a `QBrush` cannot be excluded from Qt's own operator**,
so a table write substitutes a 1x1 placeholder and appends the index;
`setTextureImage()` was measured to preserve style, transform and rendering
(and `QPen::setBrush()` to preserve width/cap/join — `setColor()` would
flatten the paint). v3 files do not open in a build that predates them;
v1/v2 files still load here.

**A brush tip is a mask** — only its alpha means anything by contract, and
`TintedStamp()` discards the RGB through `CompositionMode_SourceIn` — so
`MaskToStamp()` and `MyView::MakeRoundBrush()` build **`Format_Alpha8`**,
a 4x saving that a real 20-tip smoke pack turns into **163 MB → 40 MB**
resident. Every consumer renders identically from either format (measured:
`BrushDab()`, `StampAlongPath()`, `BrushPicker`'s un-tinted preview), and
the encoded size is unchanged, so this is purely memory. A tip read back
from a file arrives as ARGB32 (`QDataStream` writes PNG), so
`StickData::operator>>` re-converts it — without that the saving would
vanish at the first load.

Three findings from the same audit that are *not* fixed here, deliberately:
capping the resolution of pattern tiles is **lossy on real packs** (a
cut-out `.pat` shifts antialiased alpha edges: rmse 6-9, ~0.6% of pixels
past a diff of 16 — an opaque pack is untouched at max 1), JPEG would be
5-10x smaller than PNG but drops alpha, and this Qt build has **no WebP
writer** (`bmp cur ico jpeg jpg pbm pgm png ppm xbm xpm`).

`Frame::UpdateLabel()` exists because the frame number is *baked into the
thumbnail* (`UpdateIcon()` blanks the item's own `DisplayRole` label,
since it didn't render reliably in the frame list's layout). Anything that
renumbers frames — `Animation::SetupFrame()`'s shift loop,
`MyView::DeleteFrame()`'s renumber loop — must therefore repaint the band,
not just set `DisplayRole`; both now call `UpdateLabel()` (cheap: no scene
re-render, unlike `UpdateIcon()`). Its band is painted fully opaque
precisely so repeated calls overwrite the previous label.

The `operator>>` side of all three classes is also hardened against
corrupt/truncated files: `StickData::stick_img` indexing and
`StickFigureData::parent_idx` are bounds-checked (an out-of-range index
from a bad file used to read out of bounds or, for a parent-index cycle,
loop forever appending children), and image loops bail out on
`stream.status() != QDataStream::Ok` instead of continuing to allocate
from data that isn't there.

The live classes (`StickFigure`, `Frame`) keep thin `operator<<`/
`operator>>` wrappers (used only by the single-`.stck`-file and whole-
`.mAnim`-file paths) that delegate to the data operators, plus explicit
**bridge functions** that replace what the old flag-guarded shared
operators used to do implicitly:
- `ExtractStickFigureData(figure)` / `PopulateLiveStickFigure(target, data,
  attach_to_scene, update_icon)`
- `ExtractFrameData(frame)` / `PopulateLiveFrame(target, data,
  attach_to_scene, update_icons)`

`attach_to_scene`/`update_icon(s)` are plain bool parameters now — undo
restore and paste pass `true` (`PopulateLiveStickFigure` calls
`FramePresenter::AttachView()` per stick, so the target is genuinely on
screen with a live view), whole-file load passes `false` (every rebuilt
`StickData` starts view-less; only the frame `MoveToFrame()` actually
displays attaches views, for the whole frame at once, right after). This
is the same behavior the old `undoFlag`/`loadingAnimationFlag` checks
produced, just selected explicitly by the caller instead of inferred from
global state. Every scratch/buffer object in the codebase —
`frame_copy_buffer`, `stick_library_buffer`, `copy_stick_figure_buffer`,
both undo/redo buffers — is one of the `*Data` classes now, so none of them
need the historical uninitialized-`.scene` care; freeing one is a plain
`delete`.

**`StickFigure::linked_item` ownership**: the ONLY `QListWidgetItem` a
figure may own is the one actually INSERTED in the layer list, and only
`MoveToFrame()` (for the displayed frame) and `Frame::AddStickFigure()`
create it — `MoveToFrame()` reassigns `linked_item` unconditionally, and
`~StickFigure()` never frees it (only `Animation::DeleteFrame()` does, and
only for the *current* frame's figures, whose rows are real). So
`PopulateLiveFrame()` builds its figures with `linked_item = nullptr`:
the item it used to create here was never inserted anywhere, had no owner,
and was leaked on every undo/redo restore, paste and file load the moment
`MoveToFrame()` overwrote the pointer. Anything that may run on a figure
before it is displayed must therefore null-check `linked_item` —
`StickFigure::UpdateIcon()` does (it still refreshes
`stick_figure_icon`, which is what the row reads once it exists).

## Conventions

- Comments are a mix of Italian and English; match whichever the
  surrounding function uses. Comments explain *why* (a guard's reason, a
  workaround), never *what*.
- Refactored/new code (from refactor Phase 1 on): plain, navigable C++ (no
  clever tricks), English Doxygen on every touched symbol — `@brief`/
  `@param`/`@return` in headers, `@details` (the symbol's relevance) in
  source files. See REFACTOR_PLAN.md "Code style requirements".
- Naming (realized by REFACTOR_PLAN.md Phase 7): variables — locals,
  parameters, member fields — are `snake_case`; functions and classes are
  `UpperPascalCase`. Exceptions, never renamed: Qt virtual overrides
  (`paint`, `boundingRect`, `mousePressEvent`, etc.), Designer auto-connect
  `on_<objectName>_<signalName>` slots and any `.ui` `objectName`, and
  macros/`#define` constants (`UPPER_SNAKE_CASE`, unchanged). When a name
  is shared across unrelated classes or shadows a real Qt method (Phase 7
  hit this with `scale`/`QGraphicsView::scale`, `rotate`/
  `QPainter::rotate`, `currentRow`/`QListWidget::currentRow`,
  `fileName`/`QFileInfo::fileName`), check every call site's receiver
  before renaming — a shared name doesn't mean a shared meaning.
- Terse, minimal-diff style (ponytail-lite even without the plugin active):
  no speculative abstractions, no unrequested refactors alongside a bug fix.
- Before adding a null/bounds check "just in case," trace actual callers
  (grep + read) to confirm the state is reachable — this codebase has
  several call sites where a value that *looks* nullable is actually
  guaranteed non-null a few lines up (e.g. `Frame::AddStickFigure()`
  unconditionally sets `current_stick_figure` before returning).

  

## Tests

`tests/tests.pro` is a standalone qmake QtTest project (characterization
suite, REFACTOR_PLAN.md Phase 0, 120 tests as of the file-version guards —
119 passing plus the one font-dependent skip). Note the slots share
ONE animation and run in declaration order: a test that leaves the current
frame somewhere other than the last one breaks every later test that
assumes `AddFrame()` appends (see the closing `MoveToFrame()` in
`GenerateInbetweensInterpolatesAndIsOneUndoStep`). It links the
model/undo/serialization sources directly and does NOT link
`widget.cpp`/`mymainwindow.cpp` — the globals those files own are
re-defined in `tests/test_globals.cpp`, and the fixture in
`tst_characterization.cpp` replicates the parts of `Widget`'s constructor
the model layer needs. Test helpers replicate UI handlers verbatim and
cite the call site they copy — if a handler changes, update the matching
helper. `DrawViaRealMouseEvents`/`DragRotateViaRealMouseEventsIsUndoable`
are the exception: they drive real `QTest::mouseClick`/`mousePress`/
`mouseMove`/`mouseRelease` events through `MyView`'s actual
`mousePressEvent`/`mouseMoveEvent`/`mouseReleaseEvent` overrides instead of
calling model methods directly — added ahead of Phase 2c specifically to
cover the interactive mouse/paint path headlessly (`view->mapFromScene()`
converts scene coordinates to viewport pixels, so the tests don't hardcode
a coordinate transform). Build/run (kit: Qt 6.6.1 MinGW 64-bit, shadow
build dir):

```
qmake <repo>/tests/tests.pro && mingw32-make
release/tst_characterization.exe
```

**When you ADD a test slot, run the moc target first** — qmake writes the
`.moc` prerequisite of `tst_characterization.o` with a wrong relative path
(`../tests/release/...`), so a plain `mingw32-make` recompiles the .cpp
against the STALE metaobject and the new slot silently never runs (the
totals just don't grow):

```
mingw32-make -f Makefile.Release compiler_moc_source_make_all && mingw32-make -f Makefile.Release
```

**When you temporarily neutralize code to check that a test really fails
without it, `touch` the file and rebuild after restoring it too.** This
make setup compares timestamps at one-second resolution, so patch → build →
restore inside the same second leaves the OLD object file in place: the
suite then reports a failure that the source on disk no longer explains,
and the obvious conclusion (a bug in the new code) is wrong. Cost real
debugging time during the image-sharing pass. A `mingw32-make clean` first,
or `sleep 1` **then** `touch` (in that order — the touch has to land in a later
second than the object file, so a sleep after it does nothing), makes a
counterproof trustworthy.

A slot that fills in one of the `uiItems.h` UI globals (they are null in the test
binary, which doesn't link `widget.cpp`) must restore it to `nullptr` from a
**destructor**, not by an assignment at the end of the slot: a failing `QVERIFY`
returns immediately, so a stack-allocated widget is destroyed while the global
still points at it and every later slot segfaults dereferencing it — which
buries the one real failure under a crash. See the `Restore` guard in
`ApplyingAGradientEnablesTheStyleCheckBoxes`.

Running with `QT_QPA_PLATFORM=offscreen` works, but that platform has an
empty `QFontDatabase::families()` — anything font-dependent must skip
(see `TextStickRendersTextAndPreviewSkeleton`) or it fails only headlessly.

## Known unresolved items

- "Rescale animation" has a width-only scale-factor bug (non-uniform
  stretch instead of uniform best-fit) — diagnosed, fix withheld pending a
  design decision on desired behavior (uniform vs. redesigned stretch).
- Frame/layer list widgets have no native "grow bottom-to-top" mode in Qt;
  a scroll-to-latest-change behavior was proposed as a pragmatic
  alternative but never implemented.
- `Animation::current_frame` is never persisted — loading a `.mAnim` file
  always lands on frame 0 regardless of which frame was current when it
  was saved. Flagged during the serialization audit; whether load should
  instead restore the saved current frame is an open design question, not
  yet decided.
- `GradientDialog` cannot read an existing gradient back into its controls: it
  keeps whatever the user last built (the instance is held alive per kind), so
  re-opening it on a stick loaded from a file starts from that last gradient
  rather than from the stick's own one. Marked with a `ponytail:` comment;
  the upgrade is a `Seed(const QGradient&)` inverting `Gradient()`'s geometry.
- A **textured STROKE's opacity is written but never rendered**: the slider parks it
  in the pen brush's colour alpha (where a texture's opacity lives) but only
  `FillAndStroke()` ever turns that into `painter->setOpacity()`, and it runs for the
  FILL. A solid stroke works (the alpha is the colour Qt draws with) and so does a
  gradient stroke (its alpha is in the stops), so this is texture-only. Rendering it
  would mean a faded pass around each type's stroke drawing, which is why it is
  flagged here rather than bolted on.
- `StickFigure::SetStepChildAsMaster()` has an unused local (`oldParent`,
  harmless compiler warning) left over from the safety-sweep pass —
  cosmetic, not fixed since it wasn't part of that pass's actual findings.
- An `ObjectMode` gradient's bounding box stays **axis-aligned**, so a gradient
  that FOLLOWS the stick stretches slightly as well as turning when the stick
  rotates. Marked with a `ponytail:` comment in `ObjectGradientTurn()`; the
  upgrade is the same move `SceneAnchoredGradient()` already makes for the
  un-anchored case — a LogicalMode gradient — but built from the stick's own
  endpoints instead of the scene rect. Only the follows-the-stick path is
  affected; the un-anchored one is already LogicalMode and exact.
- The texture arrow's anchor sits on the side the arrow points to, so the whole
  arrow **jumps to the mirror side** when a tip drag takes the angle across the
  stick's own direction. The texture itself never moves (the tail is latched for
  the duration of the drag), and the alternative — a fixed side — has a
  permanently ungrabbable tip at one particular scale, so this is the better of
  the two rather than an oversight.
- A brush border is stamped at **scene scale**, so a dab is resampled when the view is
  zoomed in or the animation is exported larger — soft edges, not wrong geometry. The
  fix, if it shows, is to build the dab at device resolution in `StampAlongPath()`.
- The tinted dab is rebuilt on every `paint()` rather than cached across repaints (marked
  with a `ponytail:` comment) — one image scale per stick per repaint, fast enough now
  that the per-DAB rebuild is gone. If a crowded frame ever drags, the cache belongs on
  `StickGraphicsItem`, keyed by (tip, colour, diameter).
- **Tiling is a fill-only option.** A stroke texture always repeats, because a
  single Qt draw call paints fill and outline together and clipping the outline
  to one tile would leave a fragment of outline. Not a limitation worth lifting
  unless someone actually wants a one-instance stroke.

## Tooling

- This project has `tokensave` initialized (`.tokensave/`). Use
  `tokensave_context`/`tokensave_search` for structural code exploration
  before falling back to broad file reads — see the user's global
  instructions for the full policy. Note: tokensave's structural graph
  tools cannot do C++ ownership/lifetime semantic analysis (its
  inheritance/god-class/dead-code detectors are unreliable on this
  codebase, partly due to invisible Qt moc signal/slot connections) — deep
  lifetime/serialization bug hunts still require hand-tracing with
  Read/Grep.
