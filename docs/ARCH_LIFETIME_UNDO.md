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

