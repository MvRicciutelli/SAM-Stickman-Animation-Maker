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

Each `*Data` class has its own `QDataStream operator<<`/`operator>>`,
**byte-compatible with the pre-refactor format** (existing `.mAnim`/`.stck`
files still load), and a `Clone()` that deep-copies in memory with no
stream involved. These operators have no flag guards — they read/write
data and nothing else.

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
suite, REFACTOR_PLAN.md Phase 0, 24 tests as of Phase 2c). It links the
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

## Known unresolved items

- "Rescale animation" has a width-only scale-factor bug (non-uniform
  stretch instead of uniform best-fit) — diagnosed, fix withheld pending a
  design decision on desired behavior (uniform vs. redesigned stretch).
- Frame/layer list widgets have no native "grow bottom-to-top" mode in Qt;
  a scroll-to-latest-change behavior was proposed as a pragmatic
  alternative but never implemented.

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
