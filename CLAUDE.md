# MarionettaPlus

Qt5/C++ desktop app (qmake project) for drawing and frame-by-frame animating
stick figures, with video export via ffmpeg. Windows/MinGW 6.6.1 Qt kit.
Build directories (`build-MarionettaPlus-*-Debug/Release`) live as siblings
of this folder, not inside it (qmake shadow build).

## Data model

`Animation` owns `QList<Frame*> frameList`.
`Frame` owns `QList<StickFigure*> stickFigures` plus a flattened
`QList<stick*> totalSticks` (all sticks across all figures in the frame,
used for Z-ordering).
`StickFigure` owns `QList<stick*> stickList`.
`stick` (a `QGraphicsItem` + `StickData`) forms an intra-figure tree via
`parent`/`children`, which since refactor Phase 2a live on `StickData` and
are typed `StickData*` — `children` is the flattened list of ALL
descendants, not just direct children. Bridge invariant until Phase 2c:
every `StickData` inside a LIVE figure is actually a `stick`, so view-side
code may `static_cast<stick*>` a tree pointer (such casts carry a
"bridge: dati di figura viva" comment).

Since refactor Phase 2b, every scratch/buffer object (undo/redo snapshots,
frame copy/paste, stickfigure copy/paste, the stickfigure library) is a
pure-data `FrameData`/`StickFigureData`/`StickData` instead of a live
`Frame`/`StickFigure`/`stick` — see "Serialization" below.

`QGraphicsScene::addItem()` transfers ownership to the scene. A
`QGraphicsItem`'s destructor auto-detaches from its scene safely on its own,
regardless of whether `removeItem()` was called first — never assume you
must call `removeItem()` before `delete`.

## The flag-guard system (read this before touching lifetime/serialization code)

Historically, six global booleans gated whether a `StickFigure`/`Frame`/
`stick` under construction or destruction was "live" (attached to the real
scene/UI) or "scratch" (an invisible buffer that must not touch the scene
or UI widgets): `undoFlag`, `copyFlag`, `libFlag`, `loadingAnimationFlag`,
`clearUndoFlag`, `switchFrameFlag`. This was the single most bug-dense
subsystem in the codebase.

As of refactor Phase 2b, **four of the six flags are gone**: `copyFlag`,
`libFlag`, `clearUndoFlag`, `loadingAnimationFlag` have zero remaining
declarations or assignments. They existed only to disambiguate which of
several behaviors a shared serialize/clone function should run; now that
scratch objects (undo snapshots, the copy-paste buffer, the library) are
pure `StickFigureData`/`FrameData` instead of live `StickFigure`/`Frame`,
there is nothing left for those flags to disambiguate — a `StickFigureData`
has no scene, no list widget, no icon to guard. See "Serialization" below
for the data classes and their bridge functions.

Two flags remain, both purely at the live-object level:
- **`undoFlag`** — true while `myView::undoRedo()` is restoring a frame.
  `StickFigure::~StickFigure()` checks it to skip removing sticks from the
  scene when `Frame::clearFrame()` (called just before repopulating) has
  already freed them explicitly.
- **`switchFrameFlag`** — true while a frame is being populated
  programmatically (paste, undo restore) rather than through direct user
  clicks; `Frame::addStickFigure()` checks it to skip list-widget selection
  side effects that `moveToFrame()` will redo anyway a moment later.

`stick.cpp` itself has been flag-free since Phase 1b: `~stick` never
touches `imgListWidget`; the code paths that delete a LIVE stick
(`deleteStick`, `weld`, `cancelDrawing`, `~StickFigure`'s live branch) call
`stick::RemoveImagesFromListWidget()` explicitly instead.

## Command flow: how a UI edit reaches the data model and becomes undoable

Every mutating UI handler in `widget.cpp` follows the same shape:

```cpp
void Widget::on_someBtn_clicked(){
    view->storeUndo(CMD_XXX);   // snapshot PRE-change state, BEFORE mutating
    // ...directly mutate the live Animation/Frame/StickFigure/stick objects...
}
```

`storeUndo()` always runs *before* the mutation — since Phase 2b it takes
an in-memory snapshot via `ExtractFrameData(*currentFrame)` (a plain
`FrameData*`, no stream, no scratch `Frame`) and pushes it, wrapped in an
`undoInfoStruct{command, frame}`, onto `myView::undoBuffer`. The `command`
tag (`CMD_SIMPLE` — default, `CMD_ADDFRAME`, `CMD_DELETEFRAME`,
`CMD_MOVETOFRAME`, see `myview.h`) records *what kind* of structural change
is about to happen, not the change itself — `undoRedo()` uses it later to
decide how to re-integrate the snapshot rather than replaying an action. On
a genuine new user edit (`undoFlag` false — i.e. not called recursively
from inside `undoRedo()`) `storeUndo()` also discards (`delete`) and clears
`redoBuffer`, since any pending redo history is now stale. `storeUndo()`
doubles as the autosave trigger point: it increments `opCount` and emits
`autoSaveTrigger()` every `autoSaveInterval` ops if `animationPath` points
at a real file. Both buffers are capped at `MAXUNDO` entries, evicting the
oldest via a plain `delete`.

`myView::undoRedo(mode)` (`mode` = `MODE_UNDO`/`MODE_REDO`) is the reverse
path, called from the Undo/Redo buttons:
1. Pop one `undoInfoStruct` off the relevant buffer — `myUndo.frame` is
   already a ready-to-use `FrameData*`, no deserialization step.
2. Immediately call `storeUndo(poppedCommand, oppositeMode)` — this snapshots
   the *current* (about-to-be-overwritten) live frame into the *other*
   buffer, which is what makes undo/redo symmetric (undoing pushes a redo
   entry and vice versa).
3. Dispatch on `command` to decide *where* in `frameList` the snapshot
   belongs and whether a frame needs to be created/removed first:
   - `CMD_ADDFRAME`+undo or `CMD_DELETEFRAME`+redo → the frame at
     `undoFrame->frameNumber` must disappear: `deleteFrame(currentFrame)`,
     then clear and re-populate the (now-shifted) frame at that index.
   - `CMD_DELETEFRAME`+undo or `CMD_ADDFRAME`+redo → the opposite: a frame
     must be (re)created at `undoFrame->frameNumber` via `setUpFrame()`
     before populating it.
   - `CMD_MOVETOFRAME` → just clear + repopulate the frame at
     `undoFrame->frameNumber` (no add/remove, only content changed).
   - `CMD_SIMPLE` (else) → clear + repopulate `currentFrame` directly; used
     for in-place edits (stick moves, scale/rotate, add/remove stick or
     stickfigure) that don't change which frame is current.
   Every branch repopulates the *live* target frame via
   `PopulateLiveFrame(target, *undoFrame, /*attach_to_scene=*/true,
   /*update_icons=*/true)` — attaching to the scene is now an explicit
   parameter instead of something `operator>>` inferred from a global flag
   (this used to be exactly the kind of place a left-set flag caused a
   `QGraphicsScene::removeItem: item's scene (0x0)...` crash).
4. `delete undoFrame` frees the snapshot — it is pure data, so a plain
   delete is correct; there is no scratch-`.scene` trap to avoid anymore.

Net effect: the undo system doesn't store or replay *commands* in the usual
sense — it stores whole-`FrameData` snapshots and a tag describing the
structural shape of the change, then uses that tag purely to figure out
which slot in `frameList` the restored snapshot belongs in.

## Serialization

Since refactor Phase 1a/2a/2b, every layer's persistent state lives in a
`*Data` class with no scene/widget dependency, mirroring the live class it
was extracted from:
- `StickData` (`stickdata.h/cpp`) — position, type, image list, and (since
  Phase 2a) the figure-tree `parent`/`children` pointers (typed
  `StickData*`). `stick` derives from both `QGraphicsItem` and `StickData`
  — a deliberate bridge kept until Phase 2c's `StickGraphicsItem` split.
  Bridge invariant: every `StickData` inside a LIVE figure is actually a
  `stick`, so view-side code may `static_cast<stick*>` a tree pointer
  (marked with a "bridge: dati di figura viva" comment).
- `StickFigureData` (`stickfiguredata.h/cpp`) — name, `stickList`,
  `masterStick`/`currentStick`.
- `FrameData` (`framedata.h/cpp`) — `stickFigures`, the flattened
  `totalSticks` z-order list, `frameNumber`/`numOfItems`.

Each `*Data` class has its own `QDataStream operator<<`/`operator>>`,
**byte-compatible with the pre-refactor format** (existing `.mAnim`/`.stck`
files still load), and a `Clone()` that deep-copies in memory with no
stream involved. These operators have no flag guards — they read/write
data and nothing else.

The live classes (`stick`, `StickFigure`, `Frame`) keep thin
`operator<<`/`operator>>` wrappers (used only by the single-`.stck`-file
and whole-`.mAnim`-file paths) that delegate to the data operators, plus
explicit **bridge functions** that replace what the old flag-guarded shared
operators used to do implicitly:
- `ExtractStickFigureData(figure)` / `PopulateLiveStickFigure(target, data,
  attach_to_scene, update_icon)`
- `ExtractFrameData(frame)` / `PopulateLiveFrame(target, data,
  attach_to_scene, update_icons)`

`attach_to_scene`/`update_icon(s)` are plain bool parameters now — undo
restore and paste pass `true` (the target is genuinely on screen), whole-
file load passes `false` (only the frame `moveToFrame()` actually displays
needs attaching/rendering, which it does itself right after). This is the
same behavior the old `undoFlag`/`loadingAnimationFlag` checks produced,
just selected explicitly by the caller instead of inferred from global
state. Every scratch/buffer object in the codebase — `frameCopyBuffer`,
`stickLibraryBuffer`, `copyStickFigureBuffer`, both undo/redo buffers — is
one of the `*Data` classes now, so none of them need the historical
uninitialized-`.scene` care; freeing one is a plain `delete`.

## Conventions

- Comments are a mix of Italian and English; match whichever the
  surrounding function uses. Comments explain *why* (a guard's reason, a
  workaround), never *what*.
- Refactored/new code (from refactor Phase 1 on): plain, navigable C++ (no
  clever tricks), English Doxygen on every touched symbol — `@brief`/
  `@param`/`@return` in headers, `@details` (the symbol's relevance) in
  source files. See REFACTOR_PLAN.md "Code style requirements".
- Terse, minimal-diff style (ponytail-lite even without the plugin active):
  no speculative abstractions, no unrequested refactors alongside a bug fix.
- Before adding a null/bounds check "just in case," trace actual callers
  (grep + read) to confirm the state is reachable — this codebase has
  several call sites where a value that *looks* nullable is actually
  guaranteed non-null a few lines up (e.g. `Frame::addStickFigure()`
  unconditionally sets `currentStickFigure` before returning).

## Tests

`tests/tests.pro` is a standalone qmake QtTest project (characterization
suite, REFACTOR_PLAN.md Phase 0). It links the model/undo/serialization
sources directly and does NOT link `widget.cpp`/`mymainwindow.cpp` — the
globals those files own are re-defined in `tests/test_globals.cpp`, and the
fixture in `tst_characterization.cpp` replicates the parts of `Widget`'s
constructor the model layer needs. Test helpers replicate UI handlers
verbatim and cite the call site they copy — if a handler changes, update the
matching helper. Build/run (kit: Qt 6.6.1 MinGW 64-bit, shadow build dir):

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
