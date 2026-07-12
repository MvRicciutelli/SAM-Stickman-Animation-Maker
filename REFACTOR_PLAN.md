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
  `StickFigure` → `stick` (since Phase 2c, `stick` is split into
  `StickData` + `StickGraphicsItem` — see Phase 2c). This refactor reshapes
  *how* each layer is implemented, not what it conceptually owns.
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

**Status: complete.** `tests/tests.pro` (standalone qmake `QtTest`
project) holds the characterization suite. See `CLAUDE.md` "Tests" for
build/run instructions.

- [x] Phase 0 complete

---

## Phase 1 — Split `stick` into `StickData` + `StickGraphicsItem`

**Status: complete (1a, 1b).** `StickData` extracted with `Clone()`/
`CopyFrom()` and flag-free stream operators; `stick.cpp`'s live-deletion
sites (`deleteStick`, `weld`, `cancelDrawing`, `~StickFigure`'s live
branch) call `RemoveImagesFromListWidget()` explicitly instead of
flag-guarding the destructor. The composition split into
`StickGraphicsItem`/`FramePresenter` (originally this phase's steps 2/4)
was folded into and completed as part of Phase 2c.

- [x] Phase 1a complete
- [x] Phase 1b complete

---

## Phase 2 — Extend the split to `StickFigure` and `Frame`

**Status: complete (2a, 2b, 2c).** Tree pointers/algorithms are
`StickData*`-typed (2a); `StickFigureData`/`FrameData` with flag-free
stream operators replaced every scratch/buffer object, going through
`ExtractStickFigureData`/`PopulateLiveStickFigure` and
`ExtractFrameData`/`PopulateLiveFrame` as the sole live↔data bridge (2b);
the view split completed in Phase 2c below. `copyFlag`/`libFlag`/
`clearUndoFlag`/`loadingAnimationFlag`/`switchFrameFlag` have zero
remaining reads (grep-verified); `switchFrameFlag` itself turned out to
already be dead code by this point (its only set-sites were removed by
2b's paste/copy rewrites) and was deleted outright.

- [x] Phase 2 complete (2a, 2b, 2c)

---

## Phase 2c — `StickGraphicsItem` + `FramePresenter`

**Status: complete (2026-07-10).** Originally descoped/deferred
(2026-07-08) over blast-radius size and missing mouse/paint test
coverage; resumed once a compiler-guided grep pass found the real radius
much smaller (~76 sites, not 195+) and after adding `QTest`-driven mouse
tests (`DrawViaRealMouseEvents`, `DragRotateViaRealMouseEventsIsUndoable`)
to close that coverage gap. `StickData` gained a non-owning `view`
back-pointer; `stick`/`StickGraphicsItem` (class renamed post-split; file
kept as `stick.h`/`stick.cpp`) holds a non-owning `StickData*`;
`FramePresenter::AttachView`/`DetachView` (`framepresenter.h/cpp`, two
free functions rather than a stateful class) are the sole attach/detach
path. Surfaced and fixed one real bug: `deleteStick()` wasn't clearing
`imgListWidget` for the stick being deleted itself (only its children).
See `CLAUDE.md` "Data model" for the resulting architecture.

- [x] Phase 2c complete

---

## Phase 3 — Split `Animation`; retire the shared serialize operators

**Status: descoped (2026-07-09).** Steps 2–4's exit criteria were already
met by Phase 2b (no `QBuffer`/stray `QDataStream` left outside genuine
file I/O — also found and removed 5 dead `QDataStream` locals in
`widget.cpp`'s library handlers as a bonus). Step 1 (`AnimationData`) was
skipped: nothing needs a scratch/buffer `Animation` (undo/redo and
copy/paste both operate at `Frame` granularity), so building it would be
a speculative abstraction with no consumer (YAGNI).

- [x] Phase 3 complete (as descoped)

---

## Phase 4 — Replace hand-rolled undo/redo with `QUndoStack`

**Status: complete (2026-07-09), including a follow-up that eliminated
`undoFlag` itself.** Undo/redo is driven by `QUndoStack` + `FrameUndoCommand`
(`myview.cpp`); every `storeUndo(command)` call site is unchanged because
`FrameUndoCommand` captures `before_` at construction (where `storeUndo()`
already ran) and `after_` lazily on the first `undo()` — this matched the
pre-Phase-4 behavior for drag/multi-click gestures exactly, so no handler
needed restructuring. The `undoFlag` follow-up fixed the double-free it
guarded at the root (`StickFigure::~StickFigure()` now unconditionally
owns stick cleanup; `Frame::clearFrame()` no longer duplicates it) rather
than guarding it more carefully, which also fixed a real, pre-existing
leak on undo-add-frame/redo-delete-frame — caught by a new test
(`UndoAddFrameFreesImageStickListWidgetEntries`) written to fail against
the pre-fix code first.

- [x] Phase 4 complete, including the `undoFlag` follow-up

---

## Phase 5 — Delete the remaining flags and macros

**Status: complete (2026-07-09).** All six original flags confirmed gone
(grep-verified; names survive only in explanatory comments). Replaced the
`CURRENTSTICKFIGURE`/`CURRENTFRAME`/`CS` macros (~400 occurrences across 9
files, each expanding to a different hand-written expression per file)
with real accessor methods — `Animation::CurrentFrame()`,
`Frame::CurrentStickFigure()`/`CurrentStick()` — returning a **reference
to the pointer** so every existing assignment-through-macro call site
kept compiling unchanged.

- [x] Phase 5 complete

---

## Phase 6 — Expand tests, sweep dead code, update docs

**Status: complete (2026-07-10).** Added 3 pure-data tests exercising
`FrameData`/`StickFigureData` clone-independence and round-trips with no
`QGraphicsScene`/GUI involved (`FrameDataCloneRemapsReferences`,
`FrameDataRoundTrip`, `StickFigureDataCloneDeepCopiesImages`) — 27
characterization tests total. `tokensave`'s dead-code/unused-import scans
are clean outside test-only Qt-slot false positives (`QTest` methods look
"uncalled" to a static call graph; verified via `tokensave tool dead_code`/
`unused_imports`, excluding `tests/`); a manual grep for the six retired
flags and the old bridge-cast comment turned up nothing but explanatory
prose. `CLAUDE.md`'s "Data model," "flag-guard system," "Command flow,"
and "Serialization" sections describe the current, post-2c architecture.

- [x] Phase 6 complete

---

## Phase 7 — Naming convention harmonization sweep

**Status: in progress (2026-07-12).** Class renames done: `myView` →
`MyView`, `myMainWindow` → `MyMainWindow` (+ `.ui` `<class>` tag),
`mySleeper` → `MySleeper`, `checkUpdate` → `CheckUpdate`, `imageExport` →
`ImageExport`, `ffmpegExport` → `FfmpegExport` (+ `.ui`), `animationOptions`
→ `AnimationOptions` (+ `.ui`), `advancedTab` → `AdvancedTab` (+ `.ui`),
each with their own hand-written members renamed too (e.g.
`updatePresent`→`UpdatePresent`, `centralW`→`central_w`,
`advancedRectTab`→`advanced_rect_tab`). Confined so far to: `sleeper.*`,
`checkupdate.*`, `imageexport.*`, `ffmpegexport.*`, `animationoptions.*`,
`advancedtab.*`, `mymainwindow.*`, `main.cpp`, plus the touched
cross-references in `widget.cpp`/`animation.cpp`/`tests/*`. Verified via a
real 64-bit MinGW build (`/c/Qt/Tools/mingw1120_64`, matching Qt 6.6.1's
`mingw_64` — an earlier verification attempt against a stray 32-bit
toolchain on PATH produced misleading linker errors unrelated to the
rename, since corrected) plus a full `tst_characterization` run: 27/27
pass. One real mistake caught and fixed during this pass: a blanket
`advancedTab`→`AdvancedTab` sed also matched `ui->advancedTab` in
`widget.cpp` — the auto-generated accessor for an unrelated `.ui` tab-page
widget that happens to share the same objectName as the class. Reverted
those two call sites back to lowercase; left the `.ui` objectName itself
untouched (correct per the exceptions list — only `advancedtab.ui`'s own
`<class>`/root-widget were renamed, not the coincidentally-named child
widget living inside `widget.ui`). Lesson for the remaining sweep: after
each class rename, explicitly grep for `ui->` + the old name before
trusting a blanket substitution, since `.ui`-generated accessor names don't
distinguish "the class" from "a same-named objectName."

**Second checkpoint (2026-07-12, same day):** renamed the remaining three
`advanced*widget` classes (`advancedLineWidget`→`AdvancedLineWidget`,
`advancedCircleWidget`→`AdvancedCircleWidget`,
`advancedTaperWidget`→`AdvancedTaperWidget`, all + `.ui`) — no
Designer-objectName collisions this time, confirmed by grepping every
`.ui` file for each name before touching anything (the lesson from the
`advancedTab` mistake above, applied). Then renamed `uiItems.h`'s full set
of ~40 global widget-pointer variables to `snake_case`
(`fpsSpinBox`→`fps_spin_box`, `onionSkinSB`→`onion_skin_sb`,
`myStickFigureWidgetList`→`my_stick_figure_widget_list`, etc.), plus a
second batch of ~18 more globals in `widget.cpp`/`mymainwindow.cpp`
(folders, buffers, `animationPath`→`animation_path`,
`HANDLESIZE`→`handle_size`), plus the pervasive `myRect`→`my_rect` (81
references across 10 files). Most of these ~40 widget-pointer variables
turned out to be assigned directly from a same-named `.ui` accessor
(`fps_spin_box = ui->fpsSpinBox;`) — exactly the `advancedTab` trap, but
at scale. Handled safely with a negative-lookbehind rename
(`perl -pi -e 's/(?<!->)\bOLD\b/NEW/g'`) that renames the bare global but
never a `->`-prefixed accessor, verified by grepping every `.ui`
`objectName` against the full variable list first. Verified after each
batch with a real build + a full `tst_characterization` run: 27/27 pass
throughout.

**Third checkpoint (2026-07-12, same day):** renamed every `snake_case`-due
field across the whole model/live class family, in one coordinated pass
since `*Data` and their live counterparts intentionally share field names:
- `StickData` (`stickdata.h/cpp`): `stickType`→`stick_type`, `myLine`→
  `my_line`, `parentIdx`→`parent_idx`, `Pen`→`pen`, `Brush`→`brush`,
  `imgAngle*`/`imgWScale`/`imgHScale`/`imgOffset`→ snake_case, `hardTop`/
  `hardBottom`→snake_case, `stickImg*`/`imgNameList`→snake_case, and
  `Z`→`z` (handled separately — a bare single-letter name is too generic
  for a blanket word-boundary rename; scoped to `->Z`/`.Z` member access
  plus the declaration, which still missed one *implicit* same-class
  access (`Z = source.z;` inside `StickData::CopyFrom`, no `this->` or
  `->` prefix) — caught by the build, fixed by hand. Lesson: a field
  short enough to need scoped-rather-than-blanket renaming should also be
  grepped for implicit no-prefix self-access before trusting the build to
  catch it, not after.
- `StickFigureData`/`StickFigure` shared fields: `stickList`→`stick_list`,
  `masterStick`→`master_stick`, `currentStick`→`current_stick`.
- `StickFigure`'s own remaining fields (`drawCount`, `stickNum`,
  `lineBuffer`, `baseZ`, `linkedItem`, `stickFigureIcon`, `iconImg`,
  `originPosBuffer`, `sceneRotationBuffer`, `selectingOrigin`,
  `stickBuffer`, `parentBuffer`, `myScene`, plus the `myStickFigure`
  stream-operator parameter) → snake_case.
- `FrameData`/`Frame` shared fields: `stickFigures`→`stick_figures`,
  `stickFigureBuffer`→`stick_figure_buffer`, `currentStickFigure`→
  `current_stick_figure` (case-sensitive rename — the already-correct
  `CurrentStickFigure()` accessor method from Phase 5 was left untouched,
  confirmed by grep before and after), `numOfItems`→`num_of_items`,
  `frameNumber`→`frame_number`, `totalSticks`→`total_sticks`, and the
  `myFrame` stream-operator parameter.
- `Animation`: `frameList`→`frame_list`, `frameBuffer`→`frame_buffer`,
  `currentFrame`→`current_frame` (same case-sensitive care around
  `CurrentFrame()`), and `myAnimation`→`my_animation` (506 references —
  the largest single-symbol rename so far, but zero `.ui` collision risk
  since it's a plain member, not Designer-backed).

Every one of the above was `.ui`-objectName-collision-checked before
renaming (none had collisions except the `Z`/implicit-access lesson
above) and verified with a real build + full `tst_characterization` run
(27/27) after each batch.

**Not yet started:** member functions *inside* the core classes — `Widget`
(`widget.cpp`/`.h`), `MyView` (`myview.cpp`/`.h`), the remaining methods on
`StickFigure`/`Frame`/`Animation`, `stick.cpp`/`.h` (`StickGraphicsItem`),
and `framepresenter.cpp`/`.h`.

**Fourth checkpoint (2026-07-12, same day):** renamed `StickGraphicsItem`
(`stick.h`/`.cpp`) and `StickFigure`'s method set, plus
`StickGraphicsItem`'s remaining fields (`rotationSpinboxBuffer`→
`rotation_spinbox_buffer`, `scaleBuffer`/`widthBuffer`/`angleBuffer`/
`angleBuffer2`→snake_case, `imgRect`→`img_rect`, `stickHandle`→
`stick_handle`). Most methods (`getP2`, `calcImg`, `setLine`,
`endRotation`, `manipulate`, `isMaster`, `updateBr`, `addImage`,
`populateImageListWidget`, `removeImgFromList`, `calcAngle`,
`constructBr`, `sceneRemover`, `calcImgRect`, `refresh`,
`measureDistanceFromLine`) were safe blanket renames — but three names
needed the "explicit usage-scope check, not just `.ui`" this status note
previously flagged as the next risk, and all three actually hit it:
- `highlight` is a **field** on `StickGraphicsItem` (bool) but a
  **method** on `StickFigure`. Renamed only occurrences followed by `(`
  (`s/\bhighlight\b(?=\s*\()/Highlight/g`), leaving the one bare field
  read/write untouched.
- `rotate` collided with real `QPainter::rotate(qreal)` calls in
  `stick.cpp`'s `paint()`. Renamed everything except occurrences
  immediately preceded by `painter->` (6 call sites correctly excluded).
- `scale` collided with real `QGraphicsView::scale(qreal,qreal)` — both
  our custom `StickGraphicsItem::scale(float)` and Qt's view-zoom calls
  read as `X->scale(...)`, so a `->`-prefix check alone (which worked
  fine for every earlier rename) wasn't enough; only a **double**
  indirection (`->view->scale(`, meaning "go through a StickData/
  StickFigure to reach the view") is ours, while a **bare** `view->
  scale(...)` is Qt's. Renamed via a lookbehind requiring the literal
  `->view->` prefix. This still missed the method's own declaration/
  definition and one call site reached through `CurrentStickFigure()->
  scale(...)` (no `->view->` in front) — none of those are call-through-
  the-view patterns, so the lookbehind correctly didn't touch them, but
  they still needed the rename by hand. Caught immediately by the build
  (`'class StickFigure' has no member named 'scale'`) and fixed.

Lesson generalized for the remaining classes: **before renaming a method,
grep every call site of that exact name across the whole codebase and
group them by receiver, not just by textual prefix** — a name shared
across unrelated classes (or shadowing a real Qt method) needs the classes
distinguished, and a single regex rarely catches 100% of a method's own
declaration *and* every call-through-a-different-chain at once. Verified
after this batch with a real build + full `tst_characterization` run:
27/27 pass.

**Fifth checkpoint (2026-07-12, same day):** confirmed `framepresenter.h`/
`.cpp` was already fully compliant (`AttachView`/`DetachView` were named
correctly by the original refactor — no change needed). Renamed the
remaining `StickFigure` methods (`StartDrawing`, `PreviewDrawing`,
`EndDrawing`, `ReturnLine`, `CancelDrawing`, `DeleteStick`,
`SaveStickFigure`, `SelectStick`, `SetLineFromPoint`, `UpdateZ`,
`UpdateIcon`, `LoadStickFigure`, `RotateStickFigure`, `Traslate`, `Weld`,
`Chop`, `SetMaster`, `SetDirectChainAsMaster`, `SetStepChildAsMaster`,
`InvertMaster`, `StepDownMaster`, `UpdateBoundingRects`, `Elongate`,
`MaxZ`, `MinZ`, `SelectOrigin`), the free functions `MergeStickFigures`/
`SplitStickFigures`/`CloneStick`, `Frame`'s remaining methods
(`RemoveStickFigure`, `AddStickFigure`, `UpdateRender`, `ClearFrame`,
`NormalizeDepth`, `StickfigureToBottom`, `StickfigureToTop`, `MinMaxZ`,
`StickfigureAboveBelow`, `CleanTotalSticks`) plus its `frameIcon`→
`frame_icon` field, and `Animation`'s full method set (`AddFrame`,
`UpdateSelection`, `DeleteFrame`, `SetupFrame`, `SaveAnimation`,
`LoadAnimation`, `ClearAnimation`, `UpdateSliders`, `UpdateTab`).
`weld`/`chop` were checked for collision with `QString::chop()` before
renaming (none found — every call site resolves to a `StickFigure`
instance). Verified after each batch with a real build + full
`tst_characterization` run: 27/27 pass throughout, no new collisions this
round.

**Sixth checkpoint (2026-07-12, same day):** renamed `MyView`'s remaining
methods (`SetTool`, `DeleteStickFigure`, `MoveToFrame`,
`UpdateOnionSkins`, `StoreUndo`, `SetUpFrame`, `MapMyCoords`, `ClearUndo`,
`LoadLibrary`, `AddStickFigureToLibrary`, `RemoveFromLibrary`,
`SetGraphics`, `SplitStickFigure`, `ClearCurrentLib`, `SizeChange`,
`ScaleFrame`, `PreparePreview`, `DisplayPreview`, `ArrowSelection`,
`MoveStickFigureZ`, `UpdateFrameOrder`, `AddHandles`, `RemoveHandles`,
`SaveLibrary`, `DrawCmd`, `UpdateRotationBuffer`, `ChangeTool`), the
`AutoSaveTrigger` signal, and its fields (`myPen`→`my_pen`, `myBrush`→
`my_brush`, `copyStickFigureBuffer`→`copy_stick_figure_buffer`,
`onionSkins`→`onion_skins`, `previewList`→`preview_list`). All Qt virtual
overrides (`mouseReleaseEvent`, `mouseMoveEvent`, `mousePressEvent`,
`keyPressEvent`, `wheelEvent`) correctly left untouched. One more
parameter/member collision like `setTool`'s: `SetTool(int Tool)`'s
parameter would have collided with the `tool` member once lowercased
(`tool = tool;`, a silent no-op) — renamed the parameter to `new_tool`
instead of `tool`. Verified with a real build + full
`tst_characterization` run: 27/27 pass.

Member-level scope note: this and every checkpoint above renamed class
names, member fields, and member functions — not every local/loop
variable inside long method bodies (e.g. `myview.cpp`'s mouse-event
handlers are hundreds of lines with many local variables still in
`camelCase`). Treated as acceptable scope for Phase 7's core goal
(consistent public API surface); exhaustively renaming every local
variable in every function body is a much larger, lower-value tail that
can be a later pass if wanted, not a blocker on calling Phase 7 "done."

**Not yet started:** `Widget` (`widget.cpp`/`.h`) — the largest remaining
file (52KB), with the most Qt-inheritance surface (`Widget : QWidget`) and
therefore the highest remaining concentration of possible virtual-override
and generic-name collisions. Same discipline applies — `.ui`-objectName
collision check plus full call-site/receiver check for any short or
generic method name before renaming it.

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
