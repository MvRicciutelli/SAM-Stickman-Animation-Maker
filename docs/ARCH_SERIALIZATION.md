## Serialization

`.sam` (animation) and `.samlib` (library) were `.mAnim`/`.marlib` before the rebrand from
MarionettaPlus. Only the extension changed, not a byte of the format, so the open dialogs,
File > Load StickFigure and `Widget::DetectLibraries()` accept both; saves write the new one.

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
version-1 `.sam` (and every `.samlib`, which never carries names) still
loads unchanged. `Animation::LoadAnimation()` resets `version` to the current
format after reading, so re-saving an old file writes a complete current
one rather than an old header with new content.

### Files from a newer version are refused, not misread

Three versions are on the wire and each is checked: the `.sam` header
(`Animation::CURRENT_VERSION`, 3), `StickFigureData::version` (1) and `StickData::version`
(`StickData::CURRENT_VERSION`, 11). `FrameData` has none. **Raising a format means raising its
`CURRENT_VERSION` constant** — the in-class default of `version` is that constant, so a writer
and the reader's limit cannot drift apart. The stick record is the one that matters most: it
went 2→11 while the file header stayed at 3, so a newer build's `.sam` can hold a too-new
stick under a perfectly supported header, discoverable only mid-stream.

**A stick record read from an older version is raised to `CURRENT_VERSION` at the end of
`operator>>`.** `operator<<` gates every newer field on the record's own `version`, and
`CopyFrom()` copies it, so a record used to keep the version it was read with forever: a stick
from an old file that was then branched, tiled or given a brush border silently dropped that
on the next save. Once read, the fields the old file lacked hold their defaults, i.e. the record
IS a complete current one — the same reasoning `Animation::LoadAnimation()` applies to the header.

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
  `missing_text_fonts`). One choke point covers `.sam`, `.stck` and
  `.samlib`.
- **`operator<<` encoded an independent copy per record.** Fixed by
  `ImageTable` (`stickdata.h`) + animation format **`version = 3`**: the
  distinct images are written ONCE, ahead of the frames, and records write a
  `qint32` index. Measured: **512 MB → 6 MB** for a realistic 60-frame
  scene. `qCompress` is no alternative — zlib's 32 KB window cannot span
  PNG blocks megabytes apart (10 identical blocks: 14 MB → 14 MB).

`InstallImageTable()` installs/removes the table and returns the previous
one — **always restore it**, or a dangling table outlives its stack frame.
With none installed, records inline their pixels exactly as before, which
is what keeps `.stck`/`.samlib` byte-compatible; only `.sam` uses a table.
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
`.sam`-file paths) that delegate to the data operators, plus explicit
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

