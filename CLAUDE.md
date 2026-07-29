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
`InbetweenUndoCommand` (`myview.cpp`, the second and only other
`QUndoCommand` in the project), pushed by
`MyView::GenerateInbetweens(start_index, count)` — the "generate
inbetweens" button. `FrameUndoCommand` snapshots exactly ONE frame, so it
cannot express "N frames appeared at once"; this command instead owns the
N ready-made `FrameData` snapshots and is a textbook `QUndoCommand`: its
`redo()` is what actually creates the frames (`QUndoStack::push()` calls
it), `undo()` deletes them again, and neither direction touches the two
keyframes. So `GenerateInbetweens()` does NOT call `StoreUndo()` and does
NOT mutate anything inline — it validates, builds the snapshots, and
pushes. It also passes `attach_to_scene = false` to `PopulateLiveFrame()`
(like the file-load path) because attaching would put every generated
frame's sticks on the scene at once, while `MoveToFrame()` only ever
removes one frame's worth.

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
loads unchanged. `Animation::operator>>` resets `version` to 2 after
reading, so re-saving a v1 file writes a complete v2 one rather than a v1
header with v2 content.

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
suite, REFACTOR_PLAN.md Phase 0, 87 tests as of the brush presets). Note the slots share
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
- `StickFigure::SetStepChildAsMaster()` has an unused local (`oldParent`,
  harmless compiler warning) left over from the safety-sweep pass —
  cosmetic, not fixed since it wasn't part of that pass's actual findings.

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
