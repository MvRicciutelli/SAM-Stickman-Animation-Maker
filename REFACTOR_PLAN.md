# MarionettaPlus — Architecture Refactor Plan

**Read `CLAUDE.md` first.** It documents the *current* architecture and its
known pain points (the flag-guard system, shared serialization operators,
scratch-object lifetime rules). This document is the plan to systematically
dissolve those pain points without changing what the app does.

**Audience:** a fresh Claude Code session picking this up cold. Each phase
is self-contained: read its "Context" line, do the steps, verify, check the
box, stop. Do not jump ahead to a later phase in the same session unless
the user asks for it — land one phase, let the user build/smoke-test, then
continue in a follow-up session.

## Goals

- Keep the conceptual hierarchy exactly as-is: `Animation` → `Frame` →
  `StickFigure` → `stick`. This refactor reshapes *how* each layer is
  implemented, not what it conceptually owns.
- Keep every existing feature and property working identically: undo/redo,
  frame/stickfigure/stick copy-paste, the stickfigure library, onion skin,
  merge/split, master-stick reassignment, rescale, video/image export,
  serialization file formats (`.mAnim`/`.stck` files must stay readable).
- Reduce spaghettification: eliminate the six global flag-guards
  (`undoFlag`, `copyFlag`, `libFlag`, `loadingAnimationFlag`,
  `clearUndoFlag`, `switchFrameFlag`), the shared five-purpose
  `operator<<`/`operator>>`, and the scratch-object-with-uninitialized-
  `.scene` lifetime trap — not by patching them further, but by restructuring
  so the *reason they were needed* goes away.
- Improve performance: undo/redo and copy/paste currently pay a full
  `QDataStream` serialize+deserialize round-trip on every single operation,
  even a one-pixel stick drag. Once the model is decoupled from Qt scene
  objects, most of that becomes a plain in-memory deep-copy (or a small
  command delta), no stream involved.
- Improve testability and extensibility: once the data model has no
  `QGraphicsScene`/`QListWidget` dependency, it can be unit-tested headless
  and new features can be built against it without wiring up the GUI.
- Harmonize naming: variables in `snake_case`, functions and classes in
  `UpperPascalCase`, consistently across the codebase (today it's a mix —
  mostly `camelCase` for both, e.g. `currentStickFigure`, `addStickFigure`,
  `myView`). See the "Naming convention" section below for the exact rule
  and its Qt-imposed exceptions, and Phase 7 for the sweep that applies it
  to whatever code isn't already rewritten by an earlier phase.

## Non-goals (do not do these unless separately requested)

- Do not migrate the build system off qmake. CMake migration is orthogonal
  and adds risk for no architectural benefit here — skip it.
- Do not change the on-disk file formats' *meaning* (field semantics), only
  which code writes/reads them. Files saved before the refactor must still
  load after it.
- Do not redesign the UI/UX (menus, dialogs, toolbars) — this is a backend/
  architecture refactor. Visual behavior should be indistinguishable to the
  user except where a phase explicitly says otherwise (there are none).
- Do not attempt this as one big-bang rewrite. No phase should leave the
  app in a non-building, non-runnable state for more than one sitting.

## Root diagnosis (why the current code is spaghetti)

Three structural causes, all traced by hand this project's history (see
`CLAUDE.md`):

1. **Data and Qt presentation are the same object.** `stick` *is* a
   `QGraphicsItem`; `Frame`/`StickFigure` directly hold and mutate
   `QGraphicsScene*`/`QListWidget*` pointers inside what should be pure
   data operations. This means every data-model object needs to know
   whether it's "really" on screen or a disconnected scratch copy — hence
   the flags.
2. **One shared serialize function, five unrelated call sites.** Undo/redo
   snapshotting, frame copy/paste, stickfigure copy/paste, library save/
   load, and whole-animation file I/O all funnel through the same
   `operator<<`/`operator>>`, each needing slightly different behavior
   (attach to scene or not? update list widget or not? update icon or
   not?) — selected via the same six global bools.
3. **Undo/redo stores whole-`Frame` snapshots via that same serializer**,
   rather than minimal reversible deltas, which is both slow and is what
   forces the "scratch frame with an uninitialized `.scene`" object shape
   to exist at all.

The plan below removes each cause at its root: split data from
presentation (cause 1), give each serialization use case its own function
(cause 2), and replace snapshot-based undo/redo with a command pattern
(cause 3). Once causes 1–3 are gone, the six flags have nothing left to
guard and phase 5 deletes them outright.

## Naming convention (applies to all new code from Phase 1 onward)

- **Variables** (locals, parameters, member fields): `snake_case` —
  `current_stick_figure`, `frame_list`, `stick_type`.
- **Functions and classes** (free functions, methods, class/struct names):
  `UpperPascalCase` — `AddStickFigure`, `ClearFrame`, `StickData`,
  `FramePresenter`.
- **Macros/`#define` constants** are neither — leave them as the project
  already writes them, `UPPER_SNAKE_CASE` (`CMD_SIMPLE`, `MAXUNDO`). The
  user's instruction covers variables and functions/classes specifically;
  don't extend it to macros without asking.
- Apply this to every *new* symbol introduced starting in Phase 1, so the
  codebase doesn't accumulate a second wave of renames later. Don't rename
  old, not-yet-touched code just because you're nearby — that's Phase 7's
  job, kept as one reviewable diff instead of scattered across unrelated
  structural changes.

## Code style requirements (user-mandated, apply to all refactored code from Phase 1 onward)

- **Plain, navigable C++.** No clever template metaprogramming, no dense
  one-liner tricks, no exotic language features where a simple class,
  explicit loop, or plain function does the job. The code must be easy to
  grep, read top-to-bottom, and step through in a debugger.
- **Doxygen comments in English on every symbol a phase creates or
  refactors** (functions, variables, defines, enums, classes):
  - In **headers**: `@brief` on each declaration, plus `@param`/`@return`
    where applicable.
  - In **source files**: the definition carries `@details` explaining the
    *relevance* of the symbol — why it exists and what role it plays in the
    architecture, not a restatement of the code.
  - Old, untouched code keeps its existing comments (Italian or English) —
    this rule applies to what a phase touches, same scoping as the naming
    convention above.

### Qt-imposed exceptions — do not rename these, in any phase

Qt's own ecosystem dictates several names outside this project's control.
Renaming them breaks compilation or, worse, breaks *silently* at runtime.
Before renaming anything, check it isn't one of these:

- **Overridden Qt virtuals** — `paint`, `boundingRect`, `shape`, `type`,
  `itemChange`, `mousePressEvent`, `mouseMoveEvent`, `mouseReleaseEvent`,
  `wheelEvent`, `keyPressEvent`, `run` (QThread), `redo`/`undo`
  (QUndoCommand), etc. — must keep the exact name/case the Qt base class
  declares. These are overrides, not this codebase's naming choice to make.
- **Qt Designer auto-connect slots** — every `on_<objectName>_<signalName>`
  method (there are dozens across `widget.cpp`/`mymainwindow.cpp`, e.g.
  `on_addStickBtn_clicked`) is wired up by *name-matching* against the
  widget's `objectName` property in the corresponding `.ui` file. Recommend
  leaving these alone entirely — renaming would require renaming every
  matching widget `objectName` inside the `.ui` XML too, for no functional
  benefit, across a very large surface (`widget.ui` alone is ~250KB). Scope
  the naming sweep to hand-written logic, not Designer-generated
  auto-connect plumbing.
- **The `.ui` `<class>` tag** — renaming a class like `myMainWindow` to
  `MyMainWindow` (a legitimate target of this convention, since it's a
  hand-written class name, not an auto-connect slot) requires also updating
  the `<class>` element inside `mymainwindow.ui` — that's what
  `ui_mymainwindow.h` uses to generate the `Ui::myMainWindow` type that
  `mymainwindow.h` declares a member of. Qt Creator's built-in rename
  refactor handles `.ui`+`.h`+`.cpp` together; a hand/script-driven rename
  must touch all three or the build breaks.
- **String-based old-style connections** — before renaming any signal or
  slot function, grep for `SIGNAL(` and `SLOT(` across the codebase. Qt5's
  new-style `connect(&obj, &Class::signal, ...)` is compile-time checked
  and safe to rename through (the compiler catches a mismatch); the old
  `connect(&obj, SIGNAL(foo()), &other, SLOT(bar()))` string form is not —
  it fails silently at runtime if a rename misses one string literal.
- **Qt container/API surface itself** (`QList`, `QGraphicsItem`, etc.) is
  obviously out of scope — this only concerns MarionettaPlus's own symbols.

## Target shape (end state, conceptual)

```
Model layer   (no Qt-widget, no QGraphicsScene, no QListWidget dependency)
  AnimationData -> FrameData -> StickFigureData -> StickData
  Pure values + parent/child relationships. Deep-copyable via a plain
  Clone(), not via serialize-to-bytes.

Command layer (drives all mutation of the model)
  QUndoStack + QUndoCommand subclasses, one per user action.
  Widget's on_xBtn_clicked() handlers build a command and push it.

View layer    (Qt-specific, owns and syncs against the model)
  StickGraphicsItem : QGraphicsItem   — paints from a StickData*, no data
  FramePresenter                      — owns the QGraphicsScene sync,
                                         list-widget icons, onion skins
  myView / Widget                     — same role as today, thinner

Serialization layer (purpose-built, no shared flags)
  FileSerializer   — .mAnim / .stck / library file I/O, versioned
  (clone/copy is no longer serialization — it's a deep-copy method on the
  model classes)
```

`Animation`/`Frame`/`StickFigure`/`stick` as class *names* can stay; what
changes is that each splits into a `*Data` class (model) and, where visual
representation is needed, a thin view counterpart. Naming is a per-phase
judgment call — don't bikeshed it, follow the existing project's naming
voice (see `CLAUDE.md` conventions).

## How to work through this plan

- One phase at a time. Each phase has explicit entry/exit criteria — don't
  start a phase whose entry criteria aren't met, don't call a phase done
  until its exit criteria are.
- After each phase: build, run the app, manually smoke-test the features
  listed in that phase's "Verify" section, then commit on a dedicated
  branch (e.g. `refactor/phase-2-frame-model-split`) before starting the
  next phase.
- Use `tokensave_context`/`tokensave_search` for exploration per the
  project's standing tooling policy (see global CLAUDE.md and this
  project's `CLAUDE.md`) — don't re-derive file locations by hand if the
  index already has them.
- When a phase is complete, update `CLAUDE.md`'s relevant section (the
  flag-guard system, serialization, or command-flow sections) to describe
  the *new* shape, and check the box below. Future sessions rely on
  `CLAUDE.md` staying accurate — a stale doc after a completed phase is
  worse than no doc.
- If a phase reveals the plan is wrong in some concrete way (a dependency
  you didn't foresee, a feature this plan forgot to account for), stop,
  explain the discrepancy to the user, and propose a revision — don't
  silently improvise a different design.

---

## Phase 0 — Safety net (characterization tests)

**Context:** there is currently no automated test coverage. Every change
this project has made so far was verified by hand-tracing plus the user
manually rebuilding and clicking through the app. A structural refactor
without tests is flying blind — write tests *against the current
behavior* first, so later phases can be verified automatically.

**Steps:**
1. Add a `tests/` subdirectory and a second qmake `.pro` target (or a
   `SUBDIRS` project) using `QtTest`, alongside the existing app target.
2. Write characterization tests (assert current behavior, not idealized
   behavior) for the highest-risk areas:
   - Round-trip serialization: construct a `StickFigure`/`Frame`/
     `Animation`, save (in-memory `QByteArray`, not disk) then reload,
     assert every field matches (this is exactly the class of bug found
     and fixed earlier in the project — `imgAngleOffset`, the library
     byte-stream desync — a regression test here has direct precedent).
   - Undo/redo: perform a sequence of `storeUndo`/`undoRedo` calls
     matching each `CMD_*` type, assert the resulting `frameList` state.
   - Merge/split stickfigure, master-stick reassignment.
   - Clone functions (`cloneFrame`, `cloneStickFigure`).
3. These tests currently need a `QGraphicsScene` in scope (the model isn't
   split out yet) — that's expected and fine, this phase tests the
   *current* architecture as a baseline, not the target one.

**Verify:** `qmake && make` builds a test binary; all tests pass against
the current, unrefactored code.

**Exit criteria:** enough coverage that phases 1–4 below can be validated
by running this suite instead of manually re-testing every feature by hand
each time.

- [x] Phase 0 complete — `tests/tests.pro` (QtTest, standalone qmake project);
  15 characterization tests, all passing against the unrefactored code. See
  `CLAUDE.md` "Tests" for how to build/run.

---

## Phase 1 — Split `stick` into `StickData` + `StickGraphicsItem`

**Context:** `stick` currently inherits `QGraphicsItem` directly, so a
"stick" object cannot exist without scene/painting concerns attached. This
is the single highest-payoff split: it's the innermost layer, touched by
every other class, and its destructor currently carries the most
convoluted flag-guard logic in the codebase (`stick::~stick()`'s
`onionRender`/`undoFlag`/`libFlag`/`copyFlag`/`clearUndoFlag`/
`iconUpdateFlag` checks around list-widget/image cleanup).

**Steps:**
1. Create `StickData`: all of `stick`'s non-graphical state — position,
   angle, `imgAngleOffset`, thickness, `stickType`, order/z, parent/children
   relationships (by pointer or index, keep whatever `stick` uses today),
   image list (`stickImgList`) and current image. No `QGraphicsItem` base,
   no scene pointer, no `imgListWidget` pointer.
2. Create `StickGraphicsItem : public QGraphicsItem`: holds a non-owning
   `StickData*`, implements `paint()`/`boundingRect()`/mouse handling by
   reading from the `StickData*`. This is the *only* class that touches
   `QGraphicsScene`.
3. Give `StickData` a `Clone()` deep-copy method (plain field copy, no
   `QDataStream`). This will later replace the serialize-round-trip used
   for cloning, but for now it can coexist alongside the old path.
4. `StickFigure` (still the old shape, untouched this phase) now holds
   `QList<StickData*> stickList` instead of `QList<stick*>`, and a
   *separate*, view-owned `QList<StickGraphicsItem*>` created/destroyed by
   whatever currently attaches sticks to the scene (`addHandles`,
   `Frame::addStickFigure`, etc.) — 1:1 with `stickList` by index while a
   figure is on screen.
5. Update `stick.cpp`'s serialization (`operator<<`/`operator>>`) to
   operate purely on `StickData` fields — this is a mechanical move, no
   behavior change yet.
6. Delete the flag checks inside the old `stick::~stick()` destructor
   entirely: `StickData`'s destructor has nothing scene/widget-related to
   guard, and `StickGraphicsItem`'s destructor only ever runs when it's
   genuinely attached to a scene (it's never constructed for scratch/
   buffer objects — those now only ever construct `StickData`).

**Verify:** Phase 0's serialization tests still pass (run against
`StickData` now). Manually smoke-test: draw sticks of each type (line,
circle, rect, trapezoid, taper, image), drag/rotate/scale them, undo/redo
each, copy/paste a stickfigure, save/load a `.stck` file.

**Exit criteria:** no code path constructs a `stick`/`QGraphicsItem`
subclass for a scratch/buffer/undo object — only `StickData` — and the six
flags are no longer referenced anywhere inside `stick.cpp`.

**Split into 1a/1b** (per the risk note below — the full split is too large
for one verifiable sitting):

- **1a — extract `StickData` (steps 1, 3, 5):** `stickdata.h/cpp` hold every
  persistent field (everything the stream operators touch) plus `Clone()`/
  `CopyFrom()`; the stick-level `operator<<`/`operator>>` move there,
  operating purely on `StickData`. `stick` temporarily derives from both
  `QGraphicsItem` and `StickData` so every call site keeps compiling
  unchanged — this multiple inheritance is a deliberate bridge that 1b
  dissolves into the plan's composition design (`StickGraphicsItem` holding
  a non-owning `StickData*`). Tree pointers (`parent`/`children`) and
  transient interaction state (selection, highlight, drag buffers, `br`,
  `imgRect`, handles) stay on `stick` for now.
- **1b — get the six flags out of stick.cpp (step 6):** the destructor's
  flag-guarded `imgListWidget` cleanup becomes an explicit
  `stick::RemoveImagesFromListWidget()` call made by the code paths that
  delete a LIVE stick (`deleteStick`, `weld`, `cancelDrawing`,
  `~StickFigure`'s live branch). A destructor cannot know whether its stick
  was on screen; the deleter always knows — so the flags become
  unnecessary *there* instead of being guarded better.
- **Folded into Phase 2 (steps 2 and 4, plus scratch-objects-as-data):**
  creating `StickGraphicsItem`, switching `StickFigure::stickList` to
  `QList<StickData*>` with view-owned graphics items, and moving
  `parent`/`children` onto `StickData`. Rationale: scratch objects are
  whole `Frame`/`StickFigure` containers (the undo buffer holds `Frame*`),
  so "scratch sticks are plain `StickData`" is only reachable once
  `FrameData`/`StickFigureData` exist; and graphics items need an owner
  that creates/destroys them on frame switches — that is exactly
  `FramePresenter`. Building the parallel lists before Phase 2 would wire
  graphics-item ownership ad hoc only to redo it one phase later.
  Consequently Phase 1's second exit criterion (scratch paths construct
  only `StickData`) moves into Phase 2's exit criteria.

- [x] Phase 1a complete — `StickData` extracted, serialization and clone
  are pure-data; app + tests build and pass unchanged.
- [x] Phase 1b complete — `stick.cpp` no longer references any of the six
  flags (grep-verified); live-deletion sites call
  `RemoveImagesFromListWidget()` explicitly. Side fix: render/export
  clone deletions (onion skins, icons, preview thread) can no longer touch
  `imgListWidget` — the export-thread path previously reached it from a
  non-GUI thread because no flag covered it.

---

## Phase 2 — Extend the split to `StickFigure` and `Frame`

**Context:** with `StickData` proven out, apply the same pattern one
level up. `Frame` currently owns a live `scene`/`ListWidget`/
`myCurrentLibraryWidget` and mutates them directly from inside what should
be pure data methods (`addStickFigure`, `clearFrame`, `minMaxZ`,
`updateIcon`). This is also where the uninitialized-`.scene`-on-scratch-
objects trap lives — removing `Frame`'s `scene` member removes the trap
structurally, not by remembering to guard it.

**Steps:**
0. (Folded in from Phase 1b — see there for rationale.) Create
   `StickGraphicsItem : public QGraphicsItem` holding a non-owning
   `StickData*`; move `parent`/`children` onto `StickData` (as
   `StickData*`); the presenter from step 3 owns the graphics items, and
   scratch/buffer/undo paths construct only data objects from here on.
1. Create `StickFigureData` (name, `QList<StickData*> stickList`,
   `masterStick`/`currentStick` as `StickData*`, tree-manipulation methods
   — `mergeStickFigures`, `splitStickFigures`, `setStepChildAsMaster`,
   `setDirectChainAsMaster`, `weld` — reimplemented purely in terms of
   `StickData`, no scene/list-widget touches). Give it a `Clone()` method.
2. Create `FrameData` (`QList<StickFigureData*> stickFigures`, the
   flattened `QList<StickData*> totalSticks` z-order list, frame number,
   `numOfItems`). No `scene`, no `ListWidget`, no `myCurrentLibraryWidget`
   members. `clearFrame()` becomes a trivial `qDeleteAll` + `clear()` —
   there is no scene to guard against. Give it a `Clone()` method.
3. Create a `FramePresenter` (owned by `myView`, one instance reused
   across frame switches, *not* one per frame): given a `FrameData*`,
   attaches a `StickGraphicsItem` per `StickData` to the scene, wires up
   handles, updates the frame-list preview icon (including the baked-in
   frame-number text), and tears the previous frame's items down first.
   This centralizes what `moveToFrame`/`switchFrameFlag` currently does
   ad hoc.
4. `Animation` keeps its shape for now (still `QList<Frame*>`, but `Frame`
   now means `FrameData`) — full `Animation` split is Phase 3.
5. `myView`/`Widget` handlers that used to call `Frame`/`StickFigure`
   methods expecting scene side effects now call the data method (no
   side effect) followed by an explicit `frame_presenter->Sync(frame_data)`
   call. This is the point where `switchFrameFlag` becomes unnecessary:
   the presenter's sync function is the *only* place scene/list-widget
   updates happen, called explicitly, not triggered as a side effect deep
   inside a data mutation.

**Split into 2a/2b/2c** (same rationale as Phase 1's split — each lands
buildable and test-verified):

- **2a — tree pointers become data:** `parent`/`children` move from `stick`
  to `StickData` (typed `StickData*`); the tree algorithms
  (`weld`/`chop`/`mergeStickFigures`/`splitStickFigures`/`setMaster`
  family/`deleteStick`/serialization tree rebuild) have their chain lists
  and cursors re-typed to `StickData*` — they were already pure data logic.
  Where view operations on tree members remain (scene detach, `refresh`,
  `QList<stick*>` interop), an explicit
  `static_cast<stick*>(...)` bridge cast is used, always marked with a
  "bridge: dati di figura viva" comment. Bridge invariant: until 2c, every
  `StickData` inside a LIVE figure is actually a `stick`; scratch objects
  gain pure-`StickData` sticks only in 2b, and no tree algorithm runs on
  scratch containers.
- **2b — scratch objects become pure data:** create `StickFigureData`/
  `FrameData` (steps 1–2) with their own flag-free stream operators
  (byte-compatible with today's format); the undo buffers,
  `frameCopyBuffer`, `copyStickFigureBuffer` and `stickLibraryBuffer`
  switch from `Frame*`/`StickFigure*` to data objects; live↔data
  conversion goes through explicit Extract/Populate bridge functions that
  replace the flag-gated behavior of the shared operators.
- **2c — the presenter (steps 3 and 5):** `FramePresenter` +
  `StickGraphicsItem`, `stickList` becomes `QList<StickData*>`, the bridge
  casts from 2a and the Populate bools from 2b are deleted, and
  `switchFrameFlag` dies per the exit criteria below.

- [x] Phase 2a complete — tree pointers and tree algorithms are
  data-typed; app + tests build and pass unchanged.
- [x] Phase 2b complete — `StickFigureData`/`FrameData` (with byte-compatible
  flag-free stream operators) exist; the undo buffers, `frameCopyBuffer`,
  `copyStickFigureBuffer` and `stickLibraryBuffer` all hold data objects;
  `ExtractStickFigureData`/`PopulateLiveStickFigure` and
  `ExtractFrameData`/`PopulateLiveFrame` are the sole live↔data bridge.
  `copyFlag`, `libFlag`, `clearUndoFlag`, `loadingAnimationFlag` have zero
  remaining declarations or assignments (grep-verified); their names only
  survive in comments explaining what was removed and why. App + tests
  build and pass unchanged (20 characterization tests).
- [~] Phase 2c **descoped, deferred to an optional future phase** — decided
  with the user 2026-07-08 after sizing the blast radius: retyping
  `stickList` to `QList<StickData*>` plus building `StickGraphicsItem`/
  `FramePresenter` touches 195+ call sites across `myview.cpp`/
  `widget.cpp`/`frame.cpp`/`StickFigure.cpp`, including mouse-event
  handling and `paint()` — code the QtTest suite cannot exercise (headless,
  no `QApplication` event loop), so this diff would be the first in the
  whole refactor not caught by an automated regression net. Crucially, by
  this point 2c is no longer fixing a live bug source: the three root
  causes in this plan's "Root diagnosis" are already resolved for every
  *scratch* object (cause 1's uninitialized-`.scene` trap cannot occur —
  `FrameData`/`StickFigureData` have no `.scene` member, ever; cause 2 is
  gone — each data class has exactly one serialize behavior; cause 3 is
  gone — undo/redo already clones in memory via `Extract`/`Populate`, no
  stream). What 2c would still buy is architectural: `stick` stays a fused
  data+view object for *live* figures only, which is a maintainability/
  testability goal, not a correctness one. Given the size and the missing
  test coverage for its riskiest surface, see "Phase 2c (deferred)" below
  instead of executing it now.
- **Dead-code finding while closing out Phase 2:** analysis of
  `switchFrameFlag`'s only two set-sites (`Widget::addFrame`, both removed
  by Phase 2b's paste/copy rewrites) showed it could never be `true` at
  either of its two read-sites (`Frame::addStickFigure`, only ever called
  from `Widget::addStick()`/`myView::splitStickFigure()`) anymore — Phase
  2b's `PopulateLiveFrame()` bypasses `addStickFigure()` entirely by
  constructing `StickFigure` objects directly. Deleted outright (not
  converted to a local guard — there was nothing left to guard). This
  finding is exactly the kind Phase 6 ("re-run tokensave's dead-code scans
  now that flags are gone") anticipated, just found a few phases early.

**Verify:** all 20 characterization tests pass; full app builds and links.
Manually smoke-test add/delete/reorder frames, copy/paste a frame, onion
skin rendering, the stickfigure library (add/remove/load), depth/z-order
controls, layer up/down — none of Phase 2's changes should be visible to
the user.

**Exit criteria (as descoped):** `copyFlag`, `libFlag`, `clearUndoFlag`,
`loadingAnimationFlag`, `switchFrameFlag` have zero remaining reads
anywhere in the codebase (grep-verified) — met. `undoFlag` remains,
narrowly scoped to "inside `undoRedo()`" (documented in `CLAUDE.md`). The
original exit criteria's `FramePresenter`/`stick.cpp`-scene-call
conditions are **not** met and are carried forward to the deferred
Phase 2c below, should the project revisit it.

- [x] Phase 2 complete (as descoped — see Phase 2c note above)

---

## Phase 2c (deferred) — `StickGraphicsItem` + `FramePresenter`

**Status:** not scheduled. Revisit only if a concrete need arises (e.g. a
future feature that wants to unit-test view-adjacent logic headlessly, or
a maintainer decides the fused `stick` class is actively getting in the
way). Do not start this without re-confirming the blast radius hasn't
grown further and without a plan for exercising the mouse/paint paths
(manual test matrix at minimum, ideally a `QTest::mouseClick`-based GUI
test added first).

**Steps, if resumed:** create `StickGraphicsItem : public QGraphicsItem`
holding a non-owning `StickData*`, reading `paint()`/`boundingRect()`/
mouse handling from it. Switch `StickFigure::stickList` to
`QList<StickData*>` with a parallel, view-owned `QList<StickGraphicsItem*>`
kept 1:1 by index. Create `FramePresenter` (owned by `myView`) that,
given a `FrameData*`, attaches/detaches `StickGraphicsItem`s, wires
handles, and updates the frame-list preview icon — the one place
scene/list-widget updates happen for a frame switch, called explicitly
instead of triggered as a side effect of a data mutation. Delete the
`static_cast<stick*>` bridge casts introduced in Phase 2a once nothing
needs them.

- [ ] Phase 2c (deferred) — not started

---

## Phase 3 — Split `Animation`; retire the shared serialize operators

**Context:** `Animation` is the outermost layer and the simplest to split
(it mostly just owns `frameList` + canvas width/height/fps). This phase
also does the serialization consolidation this whole refactor is largely
motivated by: once every model class has a `Clone()` from phases 1–2,
in-memory copy/paste and undo/redo no longer need `QDataStream` at all.

**Steps:**
1. Create `AnimationData` (`QList<FrameData*> frameList`, `width`,
   `height`, fps/export settings). `myView` keeps the presentation-facing
   responsibilities it already has (current frame tracking, tool state)
   but delegates canvas resize (`sizeChange`) to work purely on
   `AnimationData` + ask the presenter to redraw.
2. Replace every in-memory clone/copy call site that currently goes
   through `QBuffer`+`QDataStream`+`operator<<`/`operator>>` (frame copy/
   paste, stickfigure copy/paste, undo/redo snapshotting, library
   add-from-scene) with direct calls to the `Clone()` methods added in
   phases 1–2. Grep for `QBuffer`/`QDataStream` in `widget.cpp`/
   `myview.cpp`/`frame.cpp`/`StickFigure.cpp` to find every site.
3. Keep exactly one `QDataStream`-based path per data class, used only for
   *file* I/O (`FileSerializer` — can literally be the old
   `operator<<`/`operator>>` bodies, trimmed of every flag check, since
   file save/load is the one use case that still needs byte serialization
   and never needs to skip attaching to a scene — there is no scene
   reference left in the model to skip). Keep the on-disk field order/
   format identical so existing `.mAnim`/`.stck` files still load.
4. Update the library save/load path (`saveLibrary`, `loadLibrary`,
   `stickLibraryBuffer`) the same way — it becomes file I/O for the
   library file, and in-memory `Clone()` for "add current selection to
   library" / "add library item to scene."

**Verify:** Phase 0's serialization round-trip tests still pass against
`FileSerializer`. Add new tests asserting `Clone()` produces an
independent deep copy (mutate the clone, assert the original is
unaffected) for each data class. Manually smoke-test every save/load path:
whole-animation save/load, single-stickfigure save/load, library save/
load, and confirm a `.mAnim` file saved *before* this phase still loads
correctly after it.

**Exit criteria:** `libFlag`, `loadingAnimationFlag`, and `copyFlag` have
no remaining reads anywhere in the codebase (their only reason to exist
was disambiguating *which* of five behaviors the shared operator should
run — there's now only one behavior per operator). `grep -rn
"QDataStream\|QBuffer" widget.cpp myview.cpp` should only turn up the
`FileSerializer` call sites, nothing in the UI action handlers.

**Status: descoped — steps 2–4's exit criteria were already met by Phase
2b, step 1 (`AnimationData`) is skipped as unneeded.** Re-checked on
2026-07-09:
- `libFlag`/`loadingAnimationFlag`/`copyFlag` — zero reads (Phase 2b/2c).
- `grep -rn "QBuffer" *.cpp` — zero constructions anywhere in the
  codebase; the `#include <QBuffer>` lines were dead and removed from
  `widget.cpp`/`myview.cpp`/`frame.cpp`/`StickFigure.cpp`/`animation.cpp`.
  Also found and removed 5 vestigial `QDataStream myStream;` locals in
  `widget.cpp`'s library handlers (default-constructed, never attached to
  a device, never read or written — dead before this refactor touched
  them, just never noticed).
- `grep -rn "QDataStream" widget.cpp myview.cpp` — zero in `widget.cpp`;
  in `myview.cpp` only `saveLibrary`/`loadLibrary`'s genuine file I/O.
  Step 2 ("replace clone/copy call sites with `Clone()`") is done, just
  not literally via `Clone()` — Phase 2b's `Extract*`/`Populate*` bridge
  functions serve the same role (in-memory, no stream) and additionally
  handle the live↔data conversion `Clone()` alone can't.

Step 1 (`AnimationData`) is **not built**: unlike `StickData`/
`StickFigureData`/`FrameData`, nothing in the codebase ever needs a
scratch/buffer `Animation` — undo/redo and copy/paste both operate at
`Frame` granularity, never on the whole animation, and `Animation`'s
`operator<<`/`operator>>` already has exactly one call site (whole-file
save/load), so there's no "five-purpose shared serializer" problem at this
layer to fix. Building `AnimationData` now would be a speculative
abstraction with no consumer — skip per YAGNI; revisit only if a future
feature needs to clone/undo a whole animation in memory.

- [x] Phase 3 complete (as descoped — see status note above)

---

## Phase 4 — Replace hand-rolled undo/redo with `QUndoStack`

**Context:** `myView::undoBuffer`/`redoBuffer` currently store whole-
`Frame` clones (now `FrameData` clones, post phase 2–3) per operation,
which is correct but coarse: a single stick drag clones and stores an
entire frame's worth of stickfigures. `QUndoStack`/`QUndoCommand` is the
Qt-native answer and directly replaces `CMD_SIMPLE`/`CMD_ADDFRAME`/
`CMD_DELETEFRAME`/`CMD_MOVETOFRAME` + `MODE_UNDO`/`MODE_REDO` +
`undoInfoStruct` + `MAXUNDO` + `discardScratchFrame` with a single,
well-tested Qt class.

**Steps:**
1. Add a `QUndoStack` member to `myView` (replaces `undoBuffer`/
   `redoBuffer`). Wire the existing Undo/Redo buttons to
   `undo_stack->undo()`/`redo()`.
2. Create one `QUndoCommand` subclass per distinct user action currently
   tagged by a `CMD_*` constant, **plus** finer-grained commands for
   actions that today fall under the generic `CMD_SIMPLE` bucket but are
   common enough to deserve a precise, minimal undo (e.g.
   `TransformStickCommand` storing only the before/after transform of one
   `StickData`, not a whole cloned frame). Start with the coarse
   frame-level commands (`AddFrameCommand`, `DeleteFrameCommand`,
   `MoveToFrameCommand`) since they map 1:1 to the current `CMD_*` cases —
   these alone let you delete the old dispatch logic. Add finer-grained
   commands opportunistically afterward; it's fine if some actions still
   clone a whole `FrameData` as their undo payload initially.
3. Each `Widget::on_xBtn_clicked()` handler changes from
   `view->storeUndo(CMD_X); ...mutate...` to building the matching command
   object and `undo_stack->push(cmd)` (whose constructor/`redo()` performs
   the mutation — `QUndoStack::push` calls `redo()` once automatically).
4. Move autosave triggering (`opCount`/`autoSaveInterval`/
   `autoSaveTrigger()`) to a slot connected to `QUndoStack::indexChanged`,
   since that signal now fires on every push/undo/redo — replacing the
   manual `opCount++` inside the old `storeUndo`.
5. Set an undo limit via `undo_stack->setUndoLimit(MAXUNDO)` — replaces the
   hand-rolled eviction.
6. Delete `undoBuffer`, `redoBuffer`, `undoInfoStruct`, `storeUndo`,
   `undoRedo`, `discardScratchFrame`, the `CMD_*`/`MODE_UNDO`/`MODE_REDO`
   macros, and `undoFlag`/`clearUndoFlag` themselves — by this point in
   the plan nothing should still read them (double-check with grep before
   deleting).

**Verify:** Phase 0's undo/redo tests rewritten against `QUndoStack`
(construct commands directly, call `redo()`/`undo()`, assert model state).
Manually smoke-test a long undo/redo chain across every action type: add/
delete/move frames, draw/move/delete sticks, merge/split, paste, library
operations, rescale.

**Exit criteria:** `undoFlag` and `clearUndoFlag` have zero remaining
references in the codebase. Undo/redo is driven entirely by
`QUndoStack`.

**Status: complete (2026-07-09), with one deliberate, functionally-justified
deviation from the literal exit criteria — `undoFlag` remains.**

Initial sizing (59 non-test `storeUndo`/`undoRedo` call sites) flagged this
as comparable in risk to the descoped Phase 2c, on the assumption that
Step 2's per-action `QUndoCommand` design — `redo()` must *perform* the
mutation, since `QUndoStack::push()` calls it once — would require
restructuring each handler's control flow. Tracing every call site's
actual "undo unit" boundary before writing code found the load-bearing
detail that resolves this: **several tools are drag/multi-click gestures
spanning several event-handler calls** (`ROTATE`/`SCALE`/`MOVESCENE`/
`ROTATESCENE`/`SCALESCENE`/`ELONGATE`/`MANIPULATE`'s mutation happens across
repeated `mouseMoveEvent` calls after a single `mousePressEvent`; slider
tools the same way across repeated `valueChanged` calls after the first;
2-click drawing spans two `mousePressEvent` calls) — and critically, **the
pre-Phase-4 code never captured an explicit "after" snapshot for these
either.** It captured "before" once (at gesture start) and left the buffer
entry there; "after" was only ever computed lazily, at the moment the user
actually clicked Undo. This is directly compatible with `QUndoCommand`
without restructuring anything: `FrameUndoCommand` (`myview.cpp`) captures
`before_` at construction (exactly where `storeUndo()` already ran), lets
`push()`'s automatic first `redo()` be a no-op (the caller's inline
mutation code — completely unchanged — runs immediately after, exactly as
before), and captures `after_` lazily on the first `undo()` call. Every
`storeUndo(command)` call site in the codebase is therefore **unchanged**
— same signature, same call position, same inline mutation code after it.
Only `myView`'s undo *mechanism* changed: `undoBuffer`/`redoBuffer`/
`undoInfoStruct`/`MAXUNDO`-eviction/`undoRedo()` are gone, replaced by
`undo_stack` (`QUndoStack`, `setUndoLimit(MAXUNDO)`) + `FrameUndoCommand` +
`RestoreFrameSnapshot()` (the old `undoRedo()` dispatch body, verbatim,
now taking an explicit snapshot instead of a popped buffer entry).
Autosave triggering moved to `QUndoStack::indexChanged` per step 4 (a
lambda in the constructor) — this also makes autosave fire after undo/redo
too, not just fresh edits, arguably more correct than before.
`QUndoStack::push()` auto-discarding stale redo history on a fresh edit
replaced the old manual `!undoFlag` + `redoBuffer.clear()` dance.

**`undoFlag` initially survived Phase 4** for a reason unrelated to
`CMD_*`/`QUndoStack`: it guarded a real double-free between two functions
that both thought they owned stick cleanup. `Frame::clearFrame()`
explicitly freed each stick before deleting its owning `StickFigure`, and
`StickFigure::~StickFigure()` would try to free those same
(already-deleted) sticks again unless told not to — `undoFlag` was that
signal, raised for the exact duration of `RestoreFrameSnapshot()` (the
only caller of `clearFrame()` in the codebase).

**Follow-up (2026-07-09): eliminated `undoFlag` entirely** by fixing the
double-ownership at its root instead of guarding it, once Phase 2b's
precondition was recognized: every remaining `StickFigure` is guaranteed
live (scratch objects are `StickFigureData` since Phase 2b, never a
`StickFigure`), so there is no more "maybe scratch, maybe live" ambiguity
for the destructor to resolve. Fix:
- `StickFigure::~StickFigure()` now unconditionally owns cleanup (removes
  each stick from the scene, calls `RemoveImagesFromListWidget()`, deletes
  it) — no flag, no guard.
- `Frame::clearFrame()` no longer duplicates that cleanup; it's
  `qDeleteAll(stickFigures); stickFigures.clear();`, matching what
  `~Frame()` already did.
- `undoFlag`'s declaration and both write sites (`RestoreFrameSnapshot()`,
  `Widget::~Widget()`) are deleted. Two dead `!undoFlag` checks in
  `Frame::addStickFigure()` (same pattern as `switchFrameFlag` in Phase
  2c — that function is never reached while the flag was true) went with
  it.

This also fixed a **real, pre-existing memory leak**: `RestoreFrameSnapshot()`'s
`CMD_ADDFRAME`+undo/`CMD_DELETEFRAME`+redo branches call
`Animation::deleteFrame()` → `~Frame()` → `delete` each `StickFigure` →
(old) `~StickFigure()`, which skipped its cleanup loop entirely because
`undoFlag` was raised — so every undo of an add-frame or redo of a
delete-frame leaked that frame's sticks (never freed, and for `IMAGE`
sticks, left stale entries in `imgListWidget`). This predated Phase 4;
the old `undoRedo()` had the identical shape. A new test,
`UndoAddFrameFreesImageStickListWidgetEntries`, was written and confirmed
to **fail against the pre-fix code** (proving it genuinely detects the
bug) before the fix, then confirmed to pass after.

The one item flagged as unverifiable by static reading — whether
`Widget::~Widget()`'s `delete view` is safe now that `~StickFigure()`
always touches `scene`/`imgListWidget` — was checked two ways: (1) a new
test, `ViewDestructorHandlesImageStickWithoutCrash`, builds a standalone
`myView` with a live `IMAGE` stick and deletes it directly, exercising the
exact `myView → Animation → Frame → StickFigure → stick` cascade
`Widget::~Widget()` triggers; (2) the built `MarionettaPlus.exe` was
launched and cleanly closed via a real `WM_CLOSE` (`Process.CloseMainWindow()`),
exiting with code 0. Neither surfaced a crash.

Verify performed: all 22 characterization tests pass (20 original + the
2 above). Full app builds, links, and cleanly starts/closes. **Still not
covered by any of this:** a manual smoke-test of a long undo/redo chain
across drag-based tools (rotate/scale/move/rotate-scene/scale-scene/
elongate/manipulate) and slider-driven edits (image offset/scale/rotation,
rect/taper width/height) — these gesture-spanning call sites are what
Phase 4's redesign depends on most, and they're real mouse/slider
interaction the headless suite cannot drive.

- [x] Phase 4 complete, including the `undoFlag` follow-up — zero
  references to any of the original six flags remain anywhere in the
  codebase. Pending: your manual smoke-test of the drag/slider gestures
  noted above.

---

## Phase 5 — Delete the remaining flags and macros

**Context:** by this point `switchFrameFlag` should be the only flag with
any remaining reads (localized inside `FramePresenter`, if still needed
at all as a plain local bool rather than a global). This phase is cleanup
and macro removal — small, mechanical, but worth doing explicitly rather
than leaving as debt.

**Steps:**
1. Confirm via grep that `undoFlag`, `copyFlag`, `libFlag`,
   `loadingAnimationFlag`, `clearUndoFlag` are completely unreferenced;
   delete their declarations (`extern bool ...`) from every file.
2. If `switchFrameFlag` still exists, turn it into a local RAII scope
   guard inside `FramePresenter::Sync()` (set true on entry, false on
   exit via destructor) instead of a global — it should never need to be
   visible outside that one function anymore.
3. Replace the `CURRENTSTICKFIGURE`/`CURRENTFRAME` preprocessor macros
   (which currently expand to a *different* expression per file —
   `view->myAnimation->...` in some files, `myAnimation->...` in others,
   `V->myAnimation->...` in the advanced-*-widget files) with real methods
   — e.g. `Animation::currentFrame()`, `Frame::currentStickFigure()` —
   called consistently from every file. This gives the compiler a fighting
   chance at catching misuse that the macro form could never catch.

**Verify:** full rebuild with no macro-related warnings; run the full
Phase 0 test suite; manual smoke-test pass across all major features once
more (this is the last structural phase — treat this as the release-
candidate checkpoint).

**Exit criteria:** `grep -rn "undoFlag\|copyFlag\|libFlag\|
loadingAnimationFlag\|clearUndoFlag\|switchFrameFlag"` returns nothing.
`grep -rn "define CURRENTSTICKFIGURE\|define CURRENTFRAME"` returns
nothing.

- [ ] Phase 5 complete

---

## Phase 6 — Expand tests, sweep dead code, update docs

**Context:** with the model layer now fully Qt-widget-independent, this
is the point where testing and future-extension payoff actually
materializes — use it.

**Steps:**
1. Expand the `tests/` suite: since `AnimationData`/`FrameData`/
   `StickFigureData`/`StickData` no longer need a `QGraphicsScene` or
   `QApplication` event loop to construct and manipulate, add direct
   unit tests for tree operations (merge/split/master reassignment),
   clone independence, and file round-trips, run without any GUI.
2. Re-run tokensave's dead-code/unused-import scans now that flags and
   macros are gone — the refactor will likely have orphaned a few now-
   unreachable branches (e.g. any remaining `if(!undoFlag && ...)`-shaped
   conditions that got simplified to always-true/false along the way and
   were only partially cleaned up in earlier phases).
3. Rewrite `CLAUDE.md`'s "Command flow," "Serialization," and "flag-guard
   system" sections to describe the new architecture instead of the old
   one — those sections currently document exactly what this plan
   deletes; leaving them as-is after the refactor would actively mislead
   the next session.
4. Delete this file's completed checkboxes' sections if the user prefers
   a clean repo afterward, or leave it as a historical record — ask the
   user which they'd rather do once all phases are checked off.

**Verify:** `tokensave` re-index; new test suite passes; `CLAUDE.md`
accurately describes the post-refactor architecture (spot-check by
re-reading it as if you were a new session with no other context).

- [ ] Phase 6 complete

---

## Phase 7 — Naming convention harmonization sweep

**Context:** phases 1–6 write all *new* code in `snake_case`
variables / `UpperPascalCase` functions and classes per the "Naming
convention" section above, but leave old, not-yet-touched code (whatever
survives of the original `stick.cpp`/`frame.cpp`/`StickFigure.cpp`/
`myview.cpp`/`widget.cpp`/`mymainwindow.cpp`/`animation.cpp` by this point)
in the original mixed `camelCase` style. This phase is the final, isolated
sweep that brings the remaining old code up to the same convention — kept
last and separate so it's one reviewable rename-only diff, not noise
scattered across six structural phases.

**Steps:**
1. Re-grep the Qt-imposed-exceptions list above (Designer auto-connect
   slots, Qt virtual overrides, anything referenced by `SIGNAL(`/`SLOT(`
   strings, `.ui` `<class>` tags) and build an explicit exclude-list before
   renaming anything — do this once, up front, not per-file as you go.
2. Use `tokensave_rename_preview` (or the project's equivalent structural
   rename tool) to preview each rename across all references before
   applying it — a plain text find/replace risks matching inside strings,
   comments, or unrelated identifiers that happen to share a substring.
   Prefer renaming one class's members/methods at a time so each step is
   independently reviewable and buildable.
3. Rename hand-written classes still in the old style —
   `myView` → `MyView`, `myMainWindow` → `MyMainWindow` — remembering the
   companion `.ui` `<class>` tag edit for `myMainWindow`/`mymainwindow.ui`
   (see the Qt-imposed-exceptions note above). `myView` has no `.ui`
   counterpart, so it's a plain rename.
4. Rename remaining old-style member variables and function names file by
   file. Suggested order: leaf/low-dependency files first
   (`sleeper.cpp`/`checkupdate.cpp`/`imageexport.cpp`/`ffmpegexport.cpp`),
   then work up to the files most other code depends on
   (`animation.cpp`, `myview.cpp`, `widget.cpp`) last, so each step's blast
   radius is as small as possible.
5. Do **not** rename `on_<objectName>_<signalName>` methods or any `.ui`
   widget `objectName` — confirmed out of scope by the exceptions list.
6. After the sweep, grep once more for `SIGNAL(`/`SLOT(` to confirm no
   string-based connection now references a stale (pre-rename) name.

**Verify:** full rebuild; run the Phase 0/6 test suite; manual smoke-test
pass across all major features (this is a pure rename — any behavior
change surfacing here means a rename touched something it shouldn't have,
most likely a Designer auto-connect slot or a string-based connection).

**Exit criteria:** every hand-written variable is `snake_case` and every
hand-written function/class name is `UpperPascalCase`, except the
Qt-imposed exceptions, which remain exactly as Qt requires. `CLAUDE.md`'s
conventions section is updated to state the (now-realized) naming rule
instead of describing it as an aspiration.

- [ ] Phase 7 complete

---

## Risk notes

- Do each phase on its own branch off whatever the current default branch
  is at the time; don't stack unreviewed phases on top of each other in
  a way that makes it hard to bisect a regression back to a specific
  phase.
- The highest-risk single step in this entire plan is Phase 3's file-
  format preservation (an existing `.mAnim`/`.stck` file must still load
  correctly after `FileSerializer` replaces the old operators). Keep a
  handful of real saved files from before the refactor starts and
  explicitly re-test loading them at the end of Phase 3.
- If at any point a phase turns out to be bigger than expected (e.g.
  Phase 2's `Frame` split touches more call sites than anticipated), it's
  fine to split that phase into 2a/2b sub-steps — update this file to
  reflect the split rather than pushing forward with an oversized,
  hard-to-verify change.
- Phase 7's rename sweep is low semantic risk but high mechanical
  surface-area risk: the failure mode isn't a logic bug, it's silently
  renaming something Qt's moc/Designer/string-based-connect machinery
  still expects under the old name. Any behavior regression noticed during
  or after Phase 7 should be treated as a rename-scope mistake first, not
  a fresh bug — check it against the Qt-imposed-exceptions list before
  looking anywhere else.
