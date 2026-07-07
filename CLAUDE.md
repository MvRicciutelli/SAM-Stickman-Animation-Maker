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
`stick` (a `QGraphicsItem`) forms an intra-figure tree via `parent`/
`children` — `children` is the flattened list of ALL descendants, not just
direct children.

`QGraphicsScene::addItem()` transfers ownership to the scene. A
`QGraphicsItem`'s destructor auto-detaches from its scene safely on its own,
regardless of whether `removeItem()` was called first — never assume you
must call `removeItem()` before `delete`.

## The flag-guard system (read this before touching lifetime/serialization code)

Six global booleans gate whether a `StickFigure`/`Frame`/`stick` under
construction or destruction is "live" (attached to the real scene/UI) or
"scratch" (an invisible buffer that must not touch the scene or UI widgets):
`undoFlag`, `copyFlag`, `libFlag`, `loadingAnimationFlag`, `clearUndoFlag`,
`switchFrameFlag`. This is the single most bug-dense subsystem in the
codebase — most crashes found in this project traced back to one of these
guards being incomplete at some call site (missing a flag, or a flag left
set too broadly). When adding a new code path that creates/destroys
scratch objects, check every existing guard of this shape nearby and match
it rather than inventing a new condition.

As of refactor Phase 1b, `stick.cpp` itself is flag-free: `~stick` no
longer touches `imgListWidget`; the code paths that delete a LIVE stick
(`deleteStick`, `weld`, `cancelDrawing`, `~StickFigure`'s live branch) call
`stick::RemoveImagesFromListWidget()` explicitly instead. The flags still
gate everything at the `StickFigure`/`Frame` level (`operator>>`,
destructors, `addStickFigure`).

Scratch/buffer objects — `frameCopyBuffer`, `stickLibraryBuffer`,
`copyStickFigureBuffer`, undo/redo buffer `Frame*` entries, `undoFrame` —
have an **uninitialized `.scene` member for their entire lifetime** (never
wired via `Animation::addFrame`, unlike real frames). Never call
`Frame::clearFrame()` or `scene->removeItem()` on one of these — it
dereferences the null/garbage `.scene`. Free their contents with
`qDeleteAll(...)` directly and let `QGraphicsItem`'s auto-detach handle the
rest. See `discardScratchFrame()` / `freeLibraryFrame()` in `myview.cpp` for
the established pattern.

## Command flow: how a UI edit reaches the data model and becomes undoable

Every mutating UI handler in `widget.cpp` follows the same shape:

```cpp
void Widget::on_someBtn_clicked(){
    view->storeUndo(CMD_XXX);   // snapshot PRE-change state, BEFORE mutating
    // ...directly mutate the live Animation/Frame/StickFigure/stick objects...
}
```

`storeUndo()` always runs *before* the mutation — it clones `currentFrame`
(via the same serialize/deserialize round-trip described below) into a
scratch `Frame` and pushes it, wrapped in an `undoInfoStruct{command, frame}`,
onto `myView::undoBuffer`. The `command` tag (`CMD_SIMPLE` — default,
`CMD_ADDFRAME`, `CMD_DELETEFRAME`, `CMD_MOVETOFRAME`, see `myview.h`) records
*what kind* of structural change is about to happen, not the change itself —
`undoRedo()` uses it later to decide how to re-integrate the snapshot rather
than replaying an action. On a genuine new user edit (`undoFlag` false —
i.e. not called recursively from inside `undoRedo()`) `storeUndo()` also
discards and clears `redoBuffer`, since any pending redo history is now
stale. `storeUndo()` doubles as the autosave trigger point: it increments
`opCount` and emits `autoSaveTrigger()` every `autoSaveInterval` ops if
`animationPath` points at a real file. Both buffers are capped at
`MAXUNDO` entries, evicting the oldest via `discardScratchFrame()`.

`myView::undoRedo(mode)` (`mode` = `MODE_UNDO`/`MODE_REDO`) is the reverse
path, called from the Undo/Redo buttons:
1. Pop one `undoInfoStruct` off the relevant buffer.
2. Immediately call `storeUndo(poppedCommand, oppositeMode)` — this snapshots
   the *current* (about-to-be-overwritten) live frame into the *other*
   buffer, which is what makes undo/redo symmetric (undoing pushes a redo
   entry and vice versa).
3. Deserialize the popped snapshot into a scratch `Frame* undoFrame`.
4. Dispatch on `command` to decide *where* in `frameList` the snapshot
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
   Every branch clears `undoFlag` around the specific `cloneFrame()` call
   that populates the *live* target frame, then sets it back to `true`
   immediately after — `operator>>` checks `undoFlag` to decide whether to
   actually `scene->addItem()` a deserialized stick, so leaving it `true`
   here would silently produce sticks that exist in the model but never
   attach to the scene (this was the root cause of a `QGraphicsScene::
   removeItem: item's scene (0x0)...` crash chain earlier in the project's
   history).
5. `discardScratchFrame(undoFrame)` frees the scratch frame — never
   `clearFrame()`/`delete` directly, per the uninitialized-`.scene` rule
   above.

Net effect: the undo system doesn't store or replay *commands* in the usual
sense — it stores whole-`Frame` snapshots and a tag describing the
structural shape of the change, then uses that tag purely to figure out
which slot in `frameList` the restored snapshot belongs in.

## Serialization

Since refactor Phase 1a, `stick`'s persistent state lives in `StickData`
(`stickdata.h/cpp`): `stick` derives from both `QGraphicsItem` and
`StickData` (a deliberate bridge until Phase 1b splits view from data), the
stick-level stream operators live in `stickdata.cpp` and are pure-data (no
flag guards), and `cloneStick()` is a plain `StickData::CopyFrom()` field
copy instead of a stream round-trip. `~StickData` frees the image list
unconditionally; only the `imgListWidget` cleanup remains flag-guarded in
`~stick`.

`QDataStream operator<<`/`operator>>` on `StickData`, `StickFigure`,
`Frame`, `Animation` are the SAME functions used for: undo/redo snapshots,
frame copy/paste, stickfigure copy/paste, library save/load, and
whole-animation file save/load. A bug in one of these operators affects all of those
features simultaneously — when fixing or extending one, check it against
all five use sites, not just the one you're working on. Every caller today
passes a freshly-constructed, empty target; `operator>>` implementations
defensively `qDeleteAll` the target's existing list first in case that ever
changes.

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
