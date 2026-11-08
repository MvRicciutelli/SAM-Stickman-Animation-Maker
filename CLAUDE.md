# SAM - Stickman Animation Maker

Qt5/C++ desktop app (qmake project) for drawing and frame-by-frame animating
stick figures, with video export via ffmpeg. Windows/MinGW 6.6.1 Qt kit.
Build directories (`build-SAM-*-Debug/Release`) live as siblings
of this folder, not inside it (qmake shadow build).

Layout: C++ sources and headers in `src/` (on the include path, so `<myview.h>` works),
Designer forms in `ui/`, design documents (`REFACTOR_PLAN.md`, `STK_IMPORT_EXPORT_PLAN.md`,
`FOREIGN_FORMATS_MANIFEST.md`) in `docs/`, and icons plus `image2Avi.exe` in `resources/`
(that one through a qrc alias, so it is still `:/image2Avi.exe`). `SAM.pro`, `myres.qrc` and
the license files stay at the root, so the root listing stays short and the README sits
near the top of the GitHub page. Generated files (moc/uic output, `SAM.pro.user*`) are not
tracked.

Deep-dive design notes (rationale, measurements, pinning tests) live in `docs/ARCH_*.md`. They are NOT
loaded automatically: **read the matching file before touching that area.** The rules below are the
ones that break things silently if you don't know them.

## Data model

`Animation` owns `QList<Frame*> frame_list`. `Frame` owns `QList<StickFigure*> stick_figures` plus a
flattened `QList<StickData*> total_sticks` (all sticks of all figures, used for Z-ordering).
`StickFigure` owns `QList<StickData*> stick_list`.

A "stick" is two objects:
- `StickData` (`stickdata.h/cpp`): the persistent record. No scene/widget dependency; also used for
  every scratch/buffer object (undo snapshots, copy/paste buffers, library) as pure-data
  `FrameData`/`StickFigureData`/`StickData`.
- `StickGraphicsItem` (`stick.h/stick.cpp`, files deliberately still named `stick.*`): the
  `QGraphicsItem` view. Holds a non-owning `StickData *data`, plus only what needs a scene or live
  UI/drag state.
- The intra-figure tree (`parent`/`children`) lives on `StickData`. `children` lists ALL
  descendants, not just direct children.

**`StickData::view`** (non-owning `StickGraphicsItem*`) is `nullptr` for every scratch/buffer/undo
object and briefly for an in-progress draw until `MyView::DrawCmd()` calls
`FramePresenter::AttachView()`. Code walking `stick_list`/`total_sticks`/the tree that needs the view
must go through `->view->` with a null check.
- `FramePresenter::AttachView()`/`DetachView()` (`framepresenter.h/cpp`) are the ONLY code that
  constructs or destroys a `StickGraphicsItem` for a stick on screen.
- Never wrap a LIVE `StickData` in a second `new StickGraphicsItem(...)`: it steals the real view's
  back-pointer. `Clone()` first and wrap the clone (pattern: `StickFigure::UpdateIcon()`). Destroy the
  render scene's views BEFORE `qDeleteAll()`-ing the clones, since `~StickGraphicsItem()` reads
  `data->view`.
- `StickData::is_linker`: invisible `NoPen` bridge stick made by `MergeStickFigures()`;
  `SplitStickFigures()` deletes linkers left childless (walking up chained linkers).

`QGraphicsScene::addItem()` transfers ownership to the scene. A `QGraphicsItem` destructor detaches
itself from its scene, so never assume `removeItem()` must precede `delete`.

Reach "the current X" through `Animation::CurrentFrame()` / `Frame::CurrentStickFigure()` /
`Frame::CurrentStick()`: inline accessors returning a **reference to the pointer** (assignable).
They replaced the old `CS`/`CURRENTSTICKFIGURE`/`CURRENTFRAME` macros.

## Command flow and undo (details: `docs/ARCH_LIFETIME_UNDO.md`)

Every mutating UI handler has this shape:
```cpp
void Widget::on_someBtn_clicked(){
    view->StoreUndo(CMD_XXX);   // snapshot PRE-change state, BEFORE mutating
    // ...mutate the live Animation/Frame/StickFigure/stick directly...
}
```
- `StoreUndo()` pushes a `FrameUndoCommand` onto `MyView::undo_stack` (`QUndoStack`, limit `MAXUNDO`).
  Its first `redo()` is a deliberate no-op (the caller mutates inline right after); the first `undo()`
  captures the "after" state lazily. Drag/slider gestures call it once at gesture start.
- The one exception is `FrameBatchUndoCommand` (a block of frames appearing/disappearing, used by
  `InsertFrames()`/`DeleteFrames()`/`GenerateInbetweens()`): there `push()` performs the edit. Snapshot
  ownership passes to the stack. `Remove()` must `ClearLayerRows()` first (segfault otherwise).
- No lifetime flags exist any more (`undoFlag` etc. are gone): `~StickFigure()` owns stick cleanup and
  `Frame::ClearFrame()` is a plain `qDeleteAll(stick_figures)`.
- `StickFigure::linked_item` is the layer row; only `MoveToFrame()` and `Frame::AddStickFigure()`
  create it, `~StickFigure()` never frees it, and `PopulateLiveFrame()` leaves it `nullptr`. Null-check
  it anywhere that can run on a non-displayed figure.
- App-level globals were folded into their owning class (`play_back`, `onion_render`,
  `clearing_animation`, `sliderFlags`, ...). If a global you remember has no file-scope declaration,
  grep the owning class.
- The frame list is `ContiguousSelection`; `MoveToFrame()` selects with `ClearAndSelect`; shift-click
  extends a block without switching frame. Pasting several frames inserts them as frames; pasting one
  frame merges its figures into the current frame as layers.
- `ImageExport::run()` renders a snapshot cloned in its constructor and never reads the live animation.

## Serialization (details: `docs/ARCH_SERIALIZATION.md`)

- `.sam` = animation, `.samlib` = library (formerly `.mAnim`/`.marlib`; both still load, saves write the
  new ones). Persistent state lives in `StickData`/`StickFigureData`/`FrameData`, each with its own
  `operator<<`/`>>` and `Clone()`. Live `StickFigure`/`Frame` delegate through bridge functions
  `Extract*Data()`/`PopulateLive*()`.
- New serialized fields: use `qint32` and explicit `double`, never `int`/`qreal`.
- Raising a format means raising its `CURRENT_VERSION` (`Animation` 3, `StickFigureData` 1,
  `StickData` 11). `operator<<` gates newer fields on the record's version; `operator>>` raises an
  older record to current at its end.
- Newer-than-supported files are refused (`RejectNewerVersion()` + `file_load_error`, shown through
  `MyView::ReportRejectedFile()`). A refused load must change nothing: parse into pure data first, and
  never set `animation_path` or overwrite a library file before the load succeeded.
- `QImage` is copy-on-write: `Clone()`, undo snapshots and copy/paste are free, don't "optimize" them.
  Duplication only happened at file ends: load-time image pool (`ClearStickImagePool()`, drained from
  `ReportMissingTextFonts()`) and the save-time `ImageTable` (`.sam` v3 only; always restore the
  previous table from `InstallImageTable()`).
- Frame names are a trailer at the end of `Animation::operator<<` (format v2), not in `FrameData`.
- Brush tips are `Format_Alpha8` masks. `QMessageBox` is modal: a test that reaches one needs
  `ModalDismisser`.

## Feature rules (details in the linked files)

**Stick Transform, convert, copy/paste, view navigation** (`docs/ARCH_STICK_TRANSFORM.md`)
- Every type draws through `ShapeFrame() = LocalTransform · global_xf · spine · InheritedTransform()`.
  Local (`local_xf`) moves this shape; global (`global_xf`) moves this stick and descendants.
- Render clones (icons, onion skins, `ImageExport`) must use `CloneForRender()` so the tree, and with it
  inherited globals, survives.
- A LINE's frame comes from its line, never from `img_angle`. `img_*` fields are internal placement,
  not the user's. MULTILINE is edited where it is drawn (`SceneVertices()`/`SetDrawnLine()`).
- `ConvertStick()` is destructive by spec and one undo step; the caller asks for IMAGE/TEXT input
  BEFORE calling it. Stick-branch copy/paste is `CopyStickBranch()`/`PasteStickBranch()`.
- ZOOMVIEW/ROTATEVIEW/PANVIEW change only the `QGraphicsView` transform: no `StoreUndo()`, nothing a
  render reads. Not the same as `ROTATESCENE`/`MOVESCENE`.

**TEXT and MULTILINE** (`docs/ARCH_TEXT_MULTILINE.md`)
- `TextFrame()`/`LayoutText()` are the single source for `paint()` and `ConstructBr()`. The text colour
  is the pen colour and the wrap width is `my_line.length()`.
- MULTILINE is a vertex TREE. Read parents/caps only via `MultilineParents()`/`MultilineCaps()`, fetch
  them BEFORE changing `polygon_points`, and keep widths, curvatures, caps and parents parallel in every
  vertex operation. `DeletePolygonPoint()` takes its branches along. `paint()` fills all bands and
  (reversed) discs as one `WindingFill` path.

**VECTOR stick** (`docs/ARCH_VECTOR.md`)
- `vector_shapes` are values in normalized stick-local space (through `PolygonTransform()`); handles are
  OFFSETS from the anchor; the hand-written `CopyFrom()` must copy them.
- Per-shape paint reuses the stick's helpers through `paint_shape`/`PaintRef`: never read
  `data->brush_follows_stick`/`texture_tiled`/`stroke_brush` directly in a paint helper.
- Vector tools never store a `StickData*` (`VectorStick()` re-derives it); every gesture names its
  shape and anchor index.
- Inbetweens only between `VectorShapesCompatible()` shapes; the panel refresh connections stay queued.

**Fill, stroke, texture, opacity, brush border, layout** (`docs/ARCH_STYLE_PAINT.md`)
- `pen`/`brush` are whole `QPen`/`QBrush`: patterns, images and gradients need no new fields.
  `SetGraphics()` assigns the whole brush/pen paint (`CopyPenPaint()`), not a colour.
- Opacity = the brush colour's alpha for solid and texture fills, and the stop alphas for a gradient.
  Always go through `ReadPaintOpacity()`/`WritePaintOpacity()`. Set a stroke's paint with `setBrush()`,
  never `QPen::setColor()` (it flattens a texture).
- `AlignBrush()` anchors paint per type and per brush style; an ObjectMode gradient that must not
  follow the stick is rebuilt as LogicalMode (`SceneAnchoredGradient()`).
- Raster tiles are never used 1:1: scaled through the brush transform (`TextureBrush()`).
- A brush border (`stroke_brush`) is stamped by `StampAlongPath()` inside `paint()` via
  `StrokeWithBrush()`; the onion render goes through it too. Colour, size and opacity come from the pen.
- Icon buttons paint through the stylesheet `image:` property, never `background-image:`. Sidebar and
  shrink behaviour is set in `widget.ui` plus `BottomRowSplitter`; read the doc before changing sizes.

**Release** (`docs/ARCH_RELEASE.md`): `VERSION` in `SAM.pro` is the single source. The installer is
per-user on purpose. Attach the installer to the GitHub release as `SAM-Setup.exe`. Bundled brush and
pattern packs are third-party CC0/CC BY and stay outside SAM's license (verify a license and add a
NOTICE.md row before adding a pack).

## Conventions

- Comments should always be in english, if you find italian comments, translate them.
- Refactored/new code (from refactor Phase 1 on): plain, navigable C++ (no
  clever tricks), English Doxygen on every touched symbol — `@brief`/
  `@param`/`@return` in headers, `@details` (the symbol's relevance) in
  source files. See docs/REFACTOR_PLAN.md "Code style requirements".
- Naming (realized by docs/REFACTOR_PLAN.md Phase 7): variables — locals,
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

`tests/tests.pro` is a standalone qmake QtTest project (characterization suite, `docs/REFACTOR_PLAN.md`
Phase 0). The slots share ONE animation and run in declaration order: a test that leaves the current
frame somewhere other than the last one breaks later tests that assume `AddFrame()` appends. It links the
model/undo/serialization sources directly and does NOT link `widget.cpp`/`mymainwindow.cpp`; the globals
those own are re-defined in `tests/test_globals.cpp`, and the fixture replicates the parts of `Widget`'s
constructor the model layer needs. Test helpers replicate UI handlers verbatim and cite the call site:
if a handler changes, update the helper. Real-mouse tests (`DrawViaRealMouseEvents` etc.) drive
`QTest::mouse*` through `MyView`'s overrides; `view->mapFromScene()` converts coordinates.

Build/run (Qt 6.6.1 MinGW 64-bit, shadow build dir):
```
qmake <repo>/tests/tests.pro && mingw32-make
release/tst_characterization.exe
```
- **After ADDING a test slot, run the moc target first**, or the stale metaobject is used and the new slot
  silently never runs: `mingw32-make -f Makefile.Release compiler_moc_source_make_all && mingw32-make -f Makefile.Release`
- **To prove a test fails without some code** (counterproof): patch, build, restore, then `sleep 1`
  THEN `touch` the file and rebuild (or `mingw32-make clean`). Timestamps have 1 s resolution, so a
  same-second patch/restore leaves the OLD object file and the suite reports a failure the source no
  longer explains.
- A slot that fills in a `uiItems.h` UI global (null in the test binary) must restore it to `nullptr`
  from a **destructor**, not at the end of the slot: a failing `QVERIFY` returns early and every later
  slot would segfault on the dangling global. See `Restore` in `ApplyingAGradientEnablesTheStyleCheckBoxes`.
- `QT_QPA_PLATFORM=offscreen` works but has an empty `QFontDatabase::families()`: font-dependent tests
  must skip (see `TextStickRendersTextAndPreviewSkeleton`).

## Known unresolved items

- "Rescale animation" has a width-only scale-factor bug (non-uniform stretch instead of uniform
  best-fit): diagnosed, fix withheld pending a design decision.
- Frame/layer lists have no native bottom-to-top growth in Qt; scroll-to-latest was proposed, not built.
- `Animation::current_frame` is never persisted: loading a `.sam` always lands on frame 0. Open design
  question.
- `GradientDialog` can't read an existing gradient back into its controls (keeps the last one built).
  Upgrade: a `Seed(const QGradient&)` inverting `Gradient()`.
- A textured STROKE's opacity is written but never rendered (solid and gradient strokes are fine).
- `StickFigure::SetStepChildAsMaster()` has an unused local `oldParent` (harmless warning).
- A master change reverses sticks and inverts the tree without conjugating Stick Transforms, so
  transformed sticks on that chain jump (MULTILINE excepted: its drawn ends are carried across).
- Transform scale/flips aren't in the texture arrow's unit/angle, and don't scale a LINE's pen or a
  MULTILINE's vertex widths. Stick handles stay on the skeleton under any transform. The foreign
  `.stk`/`.nodes` exporters ignore transforms except where already in the geometry they read.
- An `ObjectMode` gradient's bounding box stays axis-aligned, so a follow-the-stick gradient stretches
  slightly when the stick rotates (`ObjectGradientTurn()` `ponytail:`; upgrade: a LogicalMode gradient
  built from the stick's endpoints).
- The texture arrow's anchor jumps to the mirror side when a tip drag crosses the stick's direction
  (deliberate; a fixed side has an ungrabbable tip at one scale).
- A brush border is stamped at scene scale, so a dab is resampled when zoomed in or exported larger.
  The tinted dab is rebuilt every `paint()`; if a crowded frame drags, cache it on `StickGraphicsItem`.
- Tiling is fill-only: a stroke texture always repeats.
- VECTOR shapes have no texture arrow handle (v1 scope).
- Vector layer thumbnails (`VectorShapeIcon`) ignore brush borders and texture placement.
- VECTOR is not a "Convert to" target.
- `.stk`/`.nodes` export a vector stick as a plain segment.

## Tooling

Don't trust tokensave's dead-code, inheritance, god-class or "no callers" results here: Qt moc,
`connect()` and Designer auto-connect wiring is invisible to the graph (verified on v7.15.0:
`Widget::on_PlayButton_clicked` shows zero callers). Lifetime and serialization bug hunts need
hand-tracing with Read/Grep.
