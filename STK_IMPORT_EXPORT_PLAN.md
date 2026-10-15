# STK Import/Export Plan

Goal: import Pivot Animator `.stk` and Stick Nodes `.nodes` stickfigure files into
SAM (converting them to `StickFigureData`/live `StickFigure`), and export
SAM stickfigures as `.stk` and `.nodes` (newest dialect only, §9.6).
Conversion is **conservative and defensive**: any foreign feature with no local
equivalent is ignored (or approximated by a plain stick) with a warning; any local
feature with no foreign equivalent is exported as a plain stick with a warning.

This document is self-contained: the binary formats below were reverse-engineered
from the sample files in `C:\Users\MVR\OneDrive\Documents\stickfigures exmaples\`
(`basic\` — 10 minimal single-feature files, `advanced\` — 3 complex community
figures; all modern format) and `C:\Users\MVR\Documents\scratchpad\v2\` (5 legacy
Pivot-v2 files re-saving some of the same figures), and validated **byte-exactly**
against all of them (every file parses with zero leftover bytes and geometrically
sane values; the v2 files additionally satisfy an exact `2 + 24·count` size
equation). No public spec exists; official Pivot docs describe features but not
bytes.

---

## 1. Important finding about "the STK format" (read first)

There are several incompatible formats that use the `.stk` extension:

1. **Pivot ≤ 2.2.5 "v2" legacy format**: uncompressed, tiny fixed-size records —
   fully specified in §2.8 from the 5 samples in
   `C:\Users\MVR\Documents\scratchpad\v2\`. This is the *only* STK variant
   Stick Nodes can import (per the
   [Stick Nodes FAQ](https://sticknodes.com/faqs/), Pivot 3/4 figures must be
   down-saved to v2 first). **Covered by this plan**, both import and export.
2. **Pivot 3/4/5 "modern" format**: what all 13 modern sample files use. One `'z'`
   marker byte + a zlib stream (§2.1–2.6). Written by current Pivot Animator 5.x.
3. **Stick Nodes `.nodes`**: a completely different format — a recursive node tree,
   not a segment list. Specified in §2.9 from the
   [sticknodes-rs](https://github.com/vinceTheProgrammer/sticknodes-rs) parser
   (authoritative field list, supports Stick Nodes ≤ 4.2.3 build 72) and validated
   byte-exactly against `Zed-Stickman.nodes` (v224, 92 nodes, zero leftover
   bytes; now in `tests/data/nodes/`). Import reads every supported version;
   export emits only the newest dialect (v403 build 72) — see §9.6.

**Consequence**: import auto-detects the `.stk` variants by the first byte and
routes `.nodes` by extension (§4.0); export offers both "Pivot 5" (full fidelity)
and "Pivot 2 — Stick Nodes compatible" (reduced feature set, §5.2) as separate
save-dialog filters.

---

## 2. The binary formats (reverse-engineered, byte-validated)

§2.1–2.6 describe the modern Pivot 3/4/5 format; §2.8 the legacy v2 format.

### 2.1 Container

```
offset 0:  1 byte   0x7A (ASCII 'z')  — "zipped" marker
offset 1:  N bytes  raw zlib stream (RFC 1950; samples use level 9, header 78 DA)
```

Everything below describes the **decompressed** payload. All integers are
**little-endian**. Floats are IEEE-754 `f32`; angles are IEEE-754 `f64`.

### 2.2 Header

| field | type | meaning |
|---|---|---|
| `flags` | u8 | feature-absence bitmask, see below |
| `seg_count` | i32 | number of segments (≥ 1) |

`flags` — a bit **cleared to 0 means the feature's data is present**, set to 1 means
absent (the per-segment field is simply not written). Observed values: `0xBF`, `0xBE`,
`0xBA`, `0xB9`, `0xAA`, `0xA8`, `0xA0`. (`0xB9` — static+color present, type absent —
confirms the bits toggle independently.)

| bit | cleared ⇒ | adds per segment |
|---|---|---|
| 0 (0x01) | segment `type` byte present | 1 byte |
| 1 (0x02) | `static` flag present | 1 byte |
| 2 (0x04) | RGB color present | 3 bytes |
| 3 (0x08) | opacity byte present | 1 byte |
| 4 (0x10) | polyfills used ⇒ **draw-order section present** (see 2.5/2.6) | — |
| 5 (0x20) | *never observed cleared* | unknown |
| 6 (0x40) | **always cleared** in every sample (base data marker) | none extra |
| 7 (0x80) | *never observed cleared* | unknown |

**Defensive rule (mandatory)**: on import, if `flags` deviates from the observed
envelope — i.e. bit 5 or bit 7 cleared, or bit 6 set — abort with "unsupported STK
variant" instead of guessing field sizes. Never try to parse past an unknown layout.

### 2.3 Segment records (`seg_count` of them)

The figure is a joint tree. **Joint 0 is the origin** (the figure's orange move handle
in Pivot). **Joint k (1-based) is the free end of segment k.** Segment 1 is the root
and starts at the origin; it has NO parent field. Every subsequent segment starts a
u16 parent-joint reference. Parents always precede children in file order.

Per segment, in this exact field order:

| field | type | present | meaning |
|---|---|---|---|
| `parent_joint` | u16 | segments 2..N only | joint this segment starts from: 0 = origin, k = end of segment k (k < own index) |
| `length` | f32 | always | segment length in pixels (Pivot default limb ≈ 48) |
| `angle` | f64 | always | **absolute** (world) direction in radians, measured from +x axis, y-down screen coords. −π/2 = straight up. NOT relative to parent (verified: default stickman, Chuck outline arc) |
| `thickness` | f32 | always | line width in px. Pivot default 6.0. **0.0 = invisible segment** (used as handle-carrier, e.g. Chuck's root); fractional values occur (0.2) |
| `type` | u8 | flags bit0 clear | see type table below; absent ⇒ 0 |
| `static` | u8 | flags bit1 clear | 0/1. 1 = segment has no handle in Pivot and moves rigidly with its parent; absent ⇒ 0. Verified by the dedicated sample `three consecutive sticks with the middle of them being a static segment.stk` (flags `0xB9`, middle segment's byte = 1) |
| `color` | u8×3 | flags bit2 clear | **RGB order** (verified: red stick = `FF 00 00`, Chuck yellow = `F0 DB 21`); absent ⇒ black |
| `opacity` | u8 | flags bit3 clear | 0 = fully opaque (default). Non-zero = transparency amount, best hypothesis `alpha = 255 − value` (Hippie Van windows: 192, 228). Absent ⇒ opaque. Verify visually during implementation (§7) |

Segment `type` values observed:

| value | meaning | evidence |
|---|---|---|
| 0 | line, round end caps | default sticks |
| 6 | line, square end caps | "single squared stick" sample |
| 1 | circle, solid-white fill (Pivot's default circle) | "single circle" sample |
| 3 | circle, solid fill (segment color) | "circle with line and fill color changed" sample |
| 4 | circle, clear/no fill (best hypothesis; only seen in advanced figures) | Dark Lord, Hippie Van |

Pivot 5 documents exactly: lines (round/square caps) and circles with "three types of
circle fill: solid white, clear (no fill) and solid"
([creating_figure_types](https://pivotanimator.net/help5-2/creating_figure_types.htm)).
A **circle's segment line is its DIAMETER**: the parent joint and the segment's own
joint are opposite points on the circumference (verified by Chuck's chained-circle
outline). Unknown type values: import as a plain line + warning.

### 2.4 Section A — static-angle table (skippable)

| field | type |
|---|---|
| `count_a` | u16 |
| `count_a ×` { `index` u16, `angle` f64 } | 10 bytes each |

Semantics not decoded. An earlier hypothesis (per-static-segment angles) is
**disproved by counter-example**: the dedicated static sample contains a static
segment yet has `count_a = 0` — so statics do not require this section, and its
real meaning (only ever non-empty in the three advanced community figures) remains
unknown. **Import: skip `count_a × 10` bytes. Export: write `count_a = 0`.** All
basic samples have 0; figures render correctly ignoring it (segment `angle` fields
already fully define the pose).

### 2.5 Section B — polyfills

| field | type |
|---|---|
| `poly_count` | u16 (present in **every** file, even when flags bit4 is set — then it is 0) |
| per polyfill: `vert_count` u16, `color` u8×4 (RGB + 4th byte: 0 = opaque, else transparency like §2.3), `verts` u16 × vert_count | joint indices (0..seg_count), polygon outline in order |

Verified byte-exactly: triangle sample (1 poly, 3 verts `2,0,1`, red), Chuck (14
polys, first = 17 verts, Chuck-yellow `F0DB21`), Dark Lord (1 poly, 15 verts, black),
Hippie Van (83 polys).

### 2.6 Section C — draw order (only when flags bit4 cleared)

`(seg_count + poly_count)` × u16 — a permutation of the drawable ids
(verified a valid permutation in all 4 polyfill-bearing samples; e.g. Dark Lord:
63 segments + 1 poly = 64 entries covering 0..63). Working assumption: entry k = the
id drawn k-th, ids 0..seg_count−1 = segments in file order, then polyfills.
Files **without** polyfills have no section C at all — for them draw order is simply
segment file order.

After section C (or B when C is absent) the payload ends. Import: warn if trailing
bytes remain (do not fail); fail cleanly on truncation anywhere.

### 2.8 Legacy Pivot ≤ 2.2.5 format ("v2") — uncompressed

Reverse-engineered from the 5 samples in `C:\Users\MVR\Documents\scratchpad\v2\`
(same figures as some modern basics, re-saved as v2). Validated on all 5: file size
is **exactly** `2 + 24 × seg_count`, every field sane (the v2 stickman decodes to
torso-up at 263°, circle head parented to the neck joint, legs down; the 3-stick
chain has parents 0,1,2 with `static = 1` precisely on the middle segment).

No compression, no marker. All integers little-endian.

```
offset 0:  u8  version        — 0x01 in every sample
offset 1:  u8  seg_count      — u8! the v2 format cannot hold more than 255 segments
offset 2:  seg_count × 24-byte segment records
(nothing follows — no colors, no polyfills, no draw order, no trailer)
```

Segment record (24 bytes, fixed — no optional fields):

| off | type | field | notes |
|---|---|---|---|
| 0 | u8 | `parent_joint` | 0 = origin, k = end of segment k — same joint model as §2.3 |
| 1 | u16 | `id` | 1-based, always sequential (`id == record index + 1` in all samples). Defensive: reject if not sequential |
| 3 | u8 | unknown | always 0 observed; ignore on import, write 0, warn if ≠ 0 |
| 4 | f32 | `length` | pixels (default limb 48, like modern) |
| 8 | f64 | `angle` | absolute direction, radians, +x axis, y-down — **same convention as modern but normalized to [0, 2π)** instead of (−π, π]: the default up-stick is 3π/2 = 4.7124 here vs −π/2 in modern files |
| 16 | f32 | `thickness` | v2 default 5.0 |
| 20 | u8 | `type` | 0 = stick, 1 = circle (diameter = segment line, like modern). Only these two exist in v2 |
| 21 | u8 | `static` | 0/1 — v2 supports static segments (verified by sample) |
| 22 | u16 | unknown | always 0 observed; same handling as offset 3 |

Feature set relative to modern: **no** colors (everything black), no opacity, no
end-cap choice, no circle fill modes, no polyfills, no draw order (file order
draws), no section A. Parent-before-child ordering holds (`parent_joint < id`).

**Variant detection** (import sniffer): first byte `0x7A` ⇒ modern (§2.1);
`0x01` **and** `size == 2 + 24 × byte[1]` ⇒ v2; anything else ⇒ reject with the
unsupported-variant message (this also catches Pivot text-object files, §9.5).

### 2.9 Stick Nodes `.nodes` format (import only)

Sources: the [sticknodes-rs](https://github.com/vinceTheProgrammer/sticknodes-rs)
parser (`src/serialization/read.rs` — the authoritative, version-gated field list;
the library targets Stick Nodes ≤ 4.2.3 build 72), cross-validated byte-exactly on
`Zed-Stickman.nodes` (version 224: 92 nodes consume exactly 5072 bytes; every
non-root node satisfies `|(local_x, local_y)| == length`; `draw_order_index` is a
0..91 permutation).

**Container / endianness — twist**: read the first i32 as **big-endian** `version`.
- `160 ≤ version ≤ 403` ⇒ the file is **uncompressed and big-endian throughout**
  (older Stick Nodes; Zed-Stickman is this kind, v224).
- otherwise ⇒ the whole file (no marker byte) is a **zlib stream**; decompress and
  re-read the header as **little-endian**, all fields LE from there on (newer
  Stick Nodes). If zlib fails too ⇒ reject.
- after decompressing: `version > 403`, or `version == 403 && build > 72`, or
  `version < 160` ⇒ reject ("newer Stick Nodes format than supported") — field
  layouts beyond build 72 are unknown, same defensive philosophy as §2.2.

**Header**: `version` i32; **if version ≥ 403**: `build` i32; `scale` f32 (figure
display scale, Zed: 1.5); figure color 4×u8 in **A,B,G,R byte order**.

**Node tree** — recursive, depth-first: exactly one root node follows the header;
each node ends with `child_count` i32, immediately followed by its children,
recursively. The root has `node_type = −1`, `local_x/y = 0`, and is a pure joint
(the figure origin — its color/thickness are not rendered). A node's segment is
drawn from its **parent's position to its own position**;
`position = parent_position + (local_x, local_y)` — offsets, no trigonometry.

`node_type` (i8): −1 root, 0 rounded segment, 1 segment (flat caps), 2 circle,
3 triangle, 4 filled circle, 5 ellipse, 6 trapezoid, 7 polygon.

Per-node fields in exact order. **Gate** column: empty = all versions; otherwise
read the field only when the condition holds (v = version, b = build). Fields we
don't map are still consumed at their exact size (READ+SKIP):

| # | field | type | gate | use |
|---|---|---|---|---|
| 1 | connector prefix: `local_x` f32, `local_y` f32, `percent` f32, [b≥44: `percent_default` f32], `value` f32, `method` i32, `reversed` u8, [b≥65: `smart_stretch_ancestral_value` f32] | — | v≥403 ∧ b>38 ∧ this child was flagged as connector by its parent (field 26) | skip; count for warning |
| 2 | `node_type` | i8 | | map (table §4b) |
| 3 | `draw_order_index` | i32 | | → z-order |
| 4 | `is_static` | u8 | | warning (→ §9.1 later) |
| 5 | `is_stretchy` | u8 | | skip; count for warning |
| 6 | `is_floaty` | u8 | v≥403 ∧ b≥48 | skip |
| 7 | `is_smart_stretch` | u8 | v≥248 | skip |
| 8 | `do_not_apply_smart_stretch` | u8 | v≥252 | skip |
| 9 | `smart_stretch_reset_impulse` | u8 | v≥403 ∧ b≥50 | skip |
| 10 | `use_segment_color` | u8 | | 0 ⇒ node uses the FIGURE color |
| 11 | `use_circle_outline` | u8 | v≥256 | circle pen color source |
| 12 | `circle_is_hollow` | u8 | v≥403 ∧ b≥21 | circle brush = NoBrush |
| 13 | `use_gradient` u8 + `reverse_gradient` u8 | 2×u8 | v≥176 | skip; count for warning |
| 14 | `gradient_mode` | i16 | v≥403 ∧ b≥20 | skip |
| 15 | `use_segment_scale` | u8 | | skip |
| 16 | `local_x` f32, `local_y` f32, `scale` f32, `default_length` f32, `length` f32, `default_thickness` i32, `thickness` i32 | 28 B | | geometry: local_x/y, thickness |
| 17 | `segment_curve_radius` | i32 | v≥320 | ≠0 ⇒ count "curved segments straightened" |
| 18 | `curve_circulization` | u8 | v≥403 ∧ b≥20 | skip |
| 19 | `segment_curve_polyfill_precision` | i16 | v≥403 ∧ b≥21 | skip |
| 20 | `half_arc` u8 + `right_triangle_direction` i16 | 3 B | v≥256 | skip |
| 21 | `triangle_upside_down` | u8 | v≥300 | skip |
| 22 | `trapezoid_thickness_start` + `_end` | 2×i32 if b<64, else 2×f32 | v≥403 ∧ b≥36 | trapezoid widths |
| 23 | two i32 (discarded) + `use_trapezoid_thickness_start` u8 + `_end` u8 | 10 B | v≥403 ∧ b==36 | skip (format quirk of that one build) |
| 24 | `trapezoid_top_thickness_ratio` | f32 | v≥256 ∧ b≠36 | trapezoid tip width |
| 25 | `trapezoid_is_rounded_start` u8 + `_end` u8 | 2×u8 | v≥403 ∧ b≥36 | TAPER vs TRAPEZOID hint |
| 26 | `num_polygon_vertices` | i16 | v≥256 | skip |
| 27 | `default_local_angle` | f32 | v≥248 | skip |
| 28 | `local_angle` | f32 | | skip (redundant with local_x/y) |
| 29 | `default_angle` | f32 | v≥248 | skip |
| 30 | `color` | 4×u8 A,B,G,R | | pen/brush |
| 31 | `gradient_color` | 4×u8 | v≥176 | skip |
| 32 | `circle_outline_color` | 4×u8 | v≥256 | circle pen when use_circle_outline |
| 33 | `is_angle_locked` | u8 | v≥403 ∧ b≥39 | skip; count for warning |
| 34 | discarded f32 | f32 | v≥403 ∧ 39≤b≤50 | skip |
| 35 | `angle_lock_is_main_node` | u8 | v≥403 ∧ b≥51 | skip |
| 36 | two f32 (lock offsets) | 2×f32 | v≥403 ∧ 51≤b≤56 | skip |
| 37 | `angle_lock_offset` | f32 | v≥403 ∧ b≥57 | skip |
| 38 | `angle_lock_relative_start` | f32 | v≥403 ∧ b≥63 | skip |
| 39 | `angle_lock_stickfigure_start` | f32 | v≥403 ∧ b≥67 | skip |
| 40 | `angle_lock_relative_multiplier` | i8 | v≥403 ∧ b≥63 | skip |
| 41 | `is_drag_locked` | i16 if b≤40, else u8 | v≥403 ∧ b≥39 | skip |
| 42 | discarded i16 | i16 | v≥403 ∧ 41≤b≤45 | skip |
| 43 | `drag_lock_angle` | f32 | v≥403 ∧ b≥46 | skip |
| 44 | `smart_stretch_multiplier` | f32 | v≥403 ∧ b≥41 | skip |
| 45 | discarded u8 | u8 | v≥403 ∧ 41≤b≤45 | skip |
| 46 | `child_count` | i32 | | recursion |
| 47 | connector flags, one i32 per child | i32×child_count | v≥403 ∧ b>38 | marks which children get the field-1 prefix |

**After the tree**: if `version ≥ 230`, a polyfill section: `poly_count` i32; per
polyfill: `anchor_node_draw_index` i32, color 4×u8 (A,B,G,R), `use_polyfill_color`
u8, `attached_count` i32, then `attached_count` × i32 node draw-indices. If
`version ≥ 403 ∧ build ≥ 38`, a connector-endpoint section follows: `count` i32 +
`count` × (i32, i32). Caveat: sticknodes-rs only reads these sections in its
uncompressed branch — whether compressed files carry them is unverified; parse them
tolerantly (any inconsistency ⇒ stop, warn "trailing data ignored", keep the
figure).

---

## 3. The local model (SAM) — relevant recap

- `StickFigureData` ([stickfiguredata.h](stickfiguredata.h)): `name`,
  `QList<StickData*> stick_list`, `master_stick`, `current_stick`.
- `StickData` ([stickdata.h](stickdata.h)): `stick_type`
  (LINE=5, CIRCLE=6, IMAGE=7, RECT=16, TRAPEZOID=17, TAPER=18), `my_line` (QLineF,
  scene coords), `parent`/`children` (children = ALL descendants, flattened),
  `stepchild` (attached to parent's P1 instead of P2), `pen`, `brush`, `z`,
  `img_angle`/`img_w_scale`/`img_h_scale` (shape types), `is_linker`, images.
- Attachment invariant ([StickFigure.cpp:104-136](StickFigure.cpp#L104-L136)): a
  child's `my_line.p1()` is its parent's `p2()` — or the parent's `p1()` when
  `stepchild`. Stepchild only arises for sticks attached at the **master's** P1
  (= Pivot's origin), and propagates to all their descendants
  ([StickFigure.cpp:207-213](StickFigure.cpp#L207-L213)).
- Local CIRCLE ([stick.cpp:107-123](stick.cpp#L107-L123)): ellipse whose extent
  **along the line is the full line length** (my_line = diameter, laterally
  `length × img_w_scale`) — identical geometry to Pivot's circle when
  `img_w_scale == 1`. Shape types need
  `img_angle = -atan2(dx, dy) * 180/π` ([StickFigure.cpp:148](StickFigure.cpp#L148)).
- Z-order: per-stick `z` double; figure sticks get `base_z + i/100.0`
  ([StickFigure.cpp:125](StickFigure.cpp#L125)); views apply it via `setZValue`.
- Bridges: build a `StickFigureData`, then
  `PopulateLiveStickFigure(figure, data, true, true)` puts it on screen; and
  `ExtractStickFigureData(figure)` snapshots a live figure for export
  ([StickFigure.cpp:364-454](StickFigure.cpp#L364-L454)).
- UI anchors: `.stck` load/save handlers
  [mymainwindow.cpp:61-90](mymainwindow.cpp#L61-L90) — the STK paths mirror these
  exactly (StoreUndo → `Widget::AddStick()` → populate → append to
  `Frame::total_sticks` → `MyView::AddHandles()`).

STK ↔ local is nearly isomorphic: joint tree ↔ stick tree, origin-attached STK
segments ↔ `stepchild` children of the master, joint k = stick k's `p2()`.

---

## 4. Import mapping (STK → StickFigureData)

**§4.0 Variant detection first**: files with the `.nodes` extension go to the
`.nodes` parser (§2.9 container logic: BE-uncompressed vs whole-file-zlib+LE —
internal, not extension-based). For `.stk` (per §2.8): first byte `0x7A` ⇒
decompress and parse modern; `0x01` with the exact v2 size equation ⇒ parse v2;
else reject. The two STK parsers produce the same in-memory intermediate (parent
joint, length, absolute angle, thickness, type, static, color, opacity), so
everything below applies to both — v2 simply has fixed defaults for the fields it
lacks (type∈{0,1}, black opaque color, no polyfills/sections; normalize its
[0, 2π) angle if downstream code prefers signed, though `cos`/`sin` don't care).
The `.nodes` parser instead produces joints directly (§4b) and reuses the same
StickData-construction and post-processing steps below.

Compute joint positions first (forward kinematics — angles are absolute, so no
parent-chain accumulation is needed):

```
joint[0]   = QPointF(0, 0)                      // origin, translated later
end(seg k) = joint[parent_joint(k)] + length_k * (cos(angle_k), sin(angle_k))
joint[k]   = end(seg k)
```

Then, per segment k (1-based), create one `StickData`:

| STK | local |
|---|---|
| segment 1 | `master_stick`, `parent = nullptr` |
| `parent_joint = j > 0` | `parent =` stick j, `my_line = QLineF(joint[j], joint[k])`, `stepchild = parent->stepchild` |
| `parent_joint = 0` (non-root) | `parent = master_stick`, `my_line = QLineF(joint[0], joint[k])`, `stepchild = true` |
| `children` | flattened all-descendants form — append the new stick to *every* ancestor's `children`, exactly like `PopulateLiveStickFigure()` does |
| type 0 | `LINE`, `pen.setCapStyle(Qt::RoundCap)` |
| type 6 | `LINE`, `pen.setCapStyle(Qt::SquareCap)` |
| type 1 | `CIRCLE`, `brush = QBrush(Qt::white)`, `img_w_scale = 1`, `img_angle` per §3 formula |
| type 3 | `CIRCLE`, `brush = QBrush(color)`, rest as type 1 |
| type 4 | `CIRCLE`, `brush = Qt::NoBrush`, rest as type 1 |
| other type | `LINE` + warning *"segment N: unknown type T imported as plain stick"* |
| `thickness > 0` | `pen.setWidthF(thickness)` |
| `thickness == 0` | `pen.setStyle(Qt::NoPen)` (invisible handle-carrier segments; do NOT abuse `is_linker`, which has merge-GC semantics) |
| `color` + `opacity` | `QColor(r, g, b, opacity == 0 ? 255 : 255 − opacity)` into pen (and brush for type 3) |
| `static = 1` | no local equivalent — import as normal stick; ONE summary warning *"N static segments imported as normal sticks (they will get handles)"* (future: §9.1) |
| section A | skip silently (future: §9.1) |
| polyfills | skip + warning *"N polygon fills ignored (not supported)"* (future: §9.2) |
| draw order | `z = base-independent increasing value per draw position`: stick drawn p-th gets `z = p/100.0`; without section C use file order. Append to `stick_list` in **file order** (parents must precede children for `PopulateLiveStickFigure`), set z separately |

Post-processing:
- `name` = `QFileInfo(path).completeBaseName()`.
- Translate all `my_line`s so the figure's bounding-box center lands at the visible
  scene center (`scene->sceneRect().center()`), since STK coords are origin-relative.
- `current_stick = master_stick`.

Defensive requirements (all mandatory):
- Wrap payload access in a small bounds-checked LE reader (see §6.1); ANY read past
  the end ⇒ import fails with a clean error, never UB (same hardening philosophy as
  the existing `operator>>` bounds checks, see CLAUDE.md "Serialization").
- Reject: unrecognized first byte or zlib failure or v2 size mismatch (⇒ "not a
  supported STK file — supported: Pivot 2 and Pivot 3/4/5 stickfigures"),
  `seg_count < 1` or `> 65535` (v2: `> 255` is impossible by construction),
  `parent_joint ≥ own index` (forward/self reference), v2 non-sequential `id`,
  non-finite or absurd floats (|length| or |thickness| > 1e6, |angle| > 1e3 —
  reject file), unknown flag bits (§2.2 rule, modern only).
- Cap warnings collected per file (e.g. first 20 + "…and N more").

### 4b. Stick Nodes `.nodes` mapping

The node tree is already isomorphic to the local stick tree, with one shift: a
`.nodes` **node is a joint** whose segment connects it to its parent, and the root
node is a pure origin joint with no segment. So: **every non-root node ⇒ one
`StickData`**; the root itself produces nothing.

- Positions: `pos(node) = pos(parent) + scale · (local_x, local_y)` with
  `pos(root) = (0,0)` and `scale` = the header figure scale (multiply thicknesses
  by `scale` too — hypothesis, verified visually in §7.13; Zed uses 1.5).
- Tree: the root's **first** child becomes `master_stick`; the root's other
  children become the master's `stepchild` children with `my_line.p1() =
  master.p1()` (exactly the STK `parent_joint = 0` rule). Every other node is a
  normal child: `my_line = QLineF(pos(parent), pos(node))`. Children lists in
  flattened all-descendants form, as in §4. Depth-first file order already
  guarantees parents precede children in `stick_list`.
- Effective color: `use_segment_color ? node color : figure color` (byte order
  A,B,G,R; alpha used as-is — no inversion, unlike STK opacity).
- `draw_order_index` → z: stick k gets `z = draw_order_index(k)/100.0` (the index
  is a plain permutation, Zed: 0..91; root's index is unused). Append to
  `total_sticks` in ascending draw order.
- `thickness` (i32) → `pen.setWidthF(thickness · scale)`; 0 ⇒ `Qt::NoPen` as in §4.

`node_type` → local:

| .nodes type | local |
|---|---|
| 0 rounded segment | `LINE`, RoundCap |
| 1 segment | `LINE`, SquareCap |
| 2 circle | `CIRCLE`, `brush = Qt::NoBrush` (hollow — hypothesis §7.13; pen = `circle_outline_color` when `use_circle_outline`, else effective color), `img_w_scale = 1`, `img_angle` per §3 formula |
| 4 filled circle | `CIRCLE`, `brush =` effective color; `circle_is_hollow` (≥403 b≥21) forces NoBrush |
| 5 ellipse | `CIRCLE` + warning (lateral scale unknown pre-403; keep `img_w_scale = 1`) |
| 3 triangle | `TRAPEZOID`, `img_w_scale = thickness·scale` (base), `img_h_scale = 0` (tip) + warning (right-triangle/upside-down variants ignored) |
| 6 trapezoid | `TRAPEZOID`, base = `thickness·scale`, tip = `thickness·scale·trapezoid_top_thickness_ratio` (v≥256; ≥403 b≥36 use the explicit start/end widths; rounded ends ⇒ `TAPER`) + warning when fields are pre-256 defaults |
| 7 polygon | `LINE` + warning *"polygon segment imported as plain stick"* |
| unknown | `LINE` + warning |

Ignored with ONE summary warning each (counts, not per-node spam): statics
(§9.1), stretchy/smart-stretch/floaty, curved segments (`segment_curve_radius ≠
0`), gradients, angle/drag locks, connectors (their geometry still imports via
local_x/y — only the elastic behavior is lost), polyfill section (§9.2 shares the
solution), `is_stretchy`/`use_segment_scale` refinements.

Defensive additions for `.nodes`: recursion depth cap (= total node budget 65535,
matching the u16 limits elsewhere; the format nests arbitrarily deep — implement
the reader iteratively with an explicit stack, NOT actual recursion, to keep
corrupt files from overflowing the C++ stack); `child_count < 0` or cumulative
node count > budget ⇒ reject; every read bounds-checked as in §4.

## 5. Export mapping (StickFigureData → STK)

Segment emission order: **topological (parent before child), choosing among
ready sticks by ascending `z`** (then by `total_sticks`/list position as tiebreak).
This preserves draw order without writing section C, matching what Pivot itself does
for polyfill-less figures. Build `index_of[stick]` as sticks are emitted (1-based).
The emission order, joint computation (`parent_joint`, length, `atan2` angle) and
the guard rules at the end of this section are shared by both output variants.

### 5.1 Modern (Pivot 5) output — default

Per stick:

| local | STK |
|---|---|
| `length` | `my_line.length()` |
| `angle` | `atan2(dy, dx)` of `my_line` (double) |
| `parent_joint` | master's direct stepchildren ⇒ 0; otherwise `index_of[parent]`. (A stepchild whose parent is not the master attaches at its parent's P2 like any normal child ⇒ `index_of[parent]`) |
| LINE, RoundCap pen | type 0, thickness = `pen.widthF()` |
| LINE, other cap | type 6 |
| LINE with `Qt::NoPen` (incl. `is_linker`) | type 0, thickness 0 (invisible in Pivot too) |
| CIRCLE, brush white | type 1; warn if `img_w_scale != 1` (*"ellipse exported as circle"*) |
| CIRCLE, NoBrush | type 4 |
| CIRCLE, other brush | type 3 |
| RECT | type 6 (square-capped line), thickness = `img_w_scale` + warning *"rectangle exported as square-capped stick"* |
| TAPER | type 0 (round-capped line), thickness = `img_w_scale`; warn only if `img_w_scale != img_h_scale` (equal-width taper IS a round-capped thick line) |
| TRAPEZOID | type 6, thickness = `(img_w_scale + img_h_scale)/2` + warning |
| IMAGE | type 0, thickness = `pen.widthF()` + warning *"image stick exported as plain stick"* |
| color | shape types: brush color if brush ≠ NoBrush, else pen color; LINE: pen color. RGB bytes + opacity byte = `255 − alpha` (0 when opaque) |
| `static` | never written as 1 (no local concept; future: §9.1) |

Header `flags` — minimize like Pivot does (observed bytes reproduced exactly):
start from `0xBF`; clear bit 0 if any type ≠ 0; clear bit 2 if any color ≠ opaque
black; clear bit 3 if any alpha ≠ 255. Bits 1 and 4 stay set (no statics, no
polyfills ⇒ no section C). Trailer: `count_a = 0` (u16), `poly_count = 0` (u16).
Compress with zlib level 9, prepend `'z'`.

### 5.2 Legacy v2 output — "Stick Nodes compatible"

Same emission order and geometry, written into the §2.8 layout (uncompressed,
`0x01`, u8 count, 24-byte records: `parent_joint` u8, sequential u16 `id`, 0, f32
length, f64 angle **normalized to [0, 2π)**, f32 thickness, u8 type, u8 static
(0 until §9.1 lands), u16 0). Additional degradations on top of the §5.1 table,
each with a warning, since v2 has no color/caps/fills:

| local | v2 STK |
|---|---|
| any stick | type collapses to 0 (stick) or 1 (circle): CIRCLE ⇒ 1, everything else ⇒ 0 |
| non-black or non-opaque color | dropped (v2 is black-only) + ONE summary warning *"colors are not supported by the Pivot 2 format and were discarded"* |
| CIRCLE with brush ≠ white | type 1 + warning (v2 circles are always white-filled) |
| RECT / square caps | type 0 + warning (no square caps in v2) |
| `Qt::NoPen` / `is_linker` | thickness 0 (untested in real Pivot 2 — flag in the §7.13 manual check) |
| > 255 sticks | refuse export (u8 count/parent) with a clear error suggesting the Pivot 5 format |
| z-order ≠ file order | already handled by shared emission order (v2 draws in file order) |

Errors: no current figure (existing guard), > 65535 sticks (refuse; 255 for v2), a child whose
`p1()` doesn't coincide with its attachment joint within epsilon (shouldn't happen —
the app maintains the invariant; if it does, export anyway — STK's length/angle
representation silently re-welds the joint — and add a warning).

Round-trip property (test target): export → import must reproduce every stick's
endpoints (ε = 1e-3), type mapping, colors, z-relative-order.

---

## 6. Implementation steps

### 6.1 New files: `stkformat.h` / `stkformat.cpp` (model layer, no widgets)

```cpp
namespace StkFormat {
    enum StkVariant { PIVOT5, PIVOT2 };   // §2.1-2.6 vs §2.8
    // all return the warnings accumulated during conversion in *warnings
    // ImportStk auto-detects the STK variant (§4.0); export takes it explicitly.
    // target_center: scene point the figure's bounding-box center is moved to
    // (foreign coordinates are origin-relative) — the UI passes the canvas center.
    StickFigureData *ImportStk(const QString &path, const QPointF &target_center,
                               QStringList *warnings, QString *error);
    StickFigureData *ImportNodes(const QString &path, const QPointF &target_center,
                                 QStringList *warnings, QString *error); // §2.9/§4b
    bool             ExportStk(const QString &path, const StickFigureData &figure,
                               StkVariant variant, QStringList *warnings, QString *error);
}
```

- Internal: a tiny bounds-checked little-endian cursor over `QByteArray`
  (`ReadU8/ReadU16/ReadI32/ReadF32/ReadF64`, each sets a fail flag instead of reading
  out of bounds; one check at the end). Do **not** use `QDataStream` for the payload:
  its `setFloatingPointPrecision` makes `f32`+`f64` mixing error-prone.
- zlib via Qt built-ins, no new dependency:
  - decompress: `qUncompress` expects a 4-byte big-endian expected-size prefix before
    the raw zlib stream. Prepend a size hint and retry doubling it while the result
    is empty (start `compressed.size()*16`, cap 64 MB ⇒ reject file).
  - compress: `qCompress(payload, 9)` then strip its 4-byte prefix; write `'z'` +
    stream.
- English Doxygen per project convention; `snake_case` locals, `UpperPascalCase`
  functions.
- Add to `SAM.pro` `SOURCES`/`HEADERS`, and to `tests/tests.pro` (it links
  model sources directly).

### 6.2 UI integration ([mymainwindow.cpp](mymainwindow.cpp#L61-L90))

Extend the existing two handlers' file dialog filters — no new menu actions:
- `on_actionload_triggered`: filter
  `"Stickfigure (*.stck);;Pivot Animator figure (*.stk);;Stick Nodes figure (*.nodes)"`.
  If the chosen file ends in `.stk` or `.nodes` (case-insensitive): `StoreUndo()` →
  `central_w->AddStick()` → `ImportStk()`/`ImportNodes()` by extension → on success
  copy data into the fresh current figure via
  `PopulateLiveStickFigure(figure, *data, true, true)`, set `figure->name`, append
  sticks to `total_sticks` (existing loop), `AddHandles()`; on failure show the error
  (`QMessageBox::warning`) — and undo the empty added layer (call the same path a
  failed `.stck` load would leave; simplest: leave the empty layer and let undo
  remove it, matching current `.stck` behavior on empty file).
  If warnings are non-empty: one `QMessageBox::information` listing them.
- `on_actionsave_triggered`: filter
  `"Stickfigure (*.stck);;Pivot 5 figure (*.stk);;Pivot 2 figure - Stick Nodes compatible (*.stk)"`;
  route by the returned `selectedFilter` (both STK entries share the extension, so
  the filter string — not the suffix — picks `PIVOT5` vs `PIVOT2`); same warning
  presentation. Default-suffix handling: use the `selectedFilter` argument to
  append the right extension when the user types none.
- Both dialogs keep `my_stick_figures_folder` as start dir.

### 6.3 Conversion internals (order of work)

1. Cursor/reader + container (de)compress round-trip on all 19 samples (13 modern
   STK + 5 v2 STK + 1 `.nodes`; standalone check, no UI): variant detection,
   decompressed sizes, re-compressed→re-parsed stability.
2. Import parsers (modern STK + v2 STK) to `StickFigureData` (§4).
3. Export writers (§5.1, then §5.2 — they share the emission/geometry code, so the
   v2 writer is a thin second serializer) + round-trip self-check per variant.
4. `.nodes` import parser (§2.9/§4b — independent of the STK code except the
   bounds-checked cursor, which must support both endiannesses).
5. UI hookup (§6.2).
6. Tests (§7).

## 7. Tests (tests/tst_characterization.cpp additions)

Copy the 10 `basic\*.stk` samples into `tests/data/stk/`, the 5 v2 samples from
`C:\Users\MVR\Documents\scratchpad\v2\` into `tests/data/stk/v2/` (tiny files,
≤ 146 bytes), and `Zed-Stickman.nodes` into `tests/data/nodes/` (5 KB).

1. `ImportBasicStickman`: 6 sticks; master + tree shape (arms/head parented to torso
   joint via master P2, legs stepchildren at origin); circle head type CIRCLE.
2. `ImportSingleFeatureFiles`: for each basic file assert count, type, pen width,
   color, cap style, brush.
3. `ImportPolyfillTriangleWarns`: 2 sticks imported, 1 warning mentioning polygon.
4. `ImportRejectsCorruptStk`: truncated payload, bad marker, bad zlib, forward
   parent ref, absurd count — all fail cleanly with error, no crash/UB (mirrors the
   existing corrupt-file hardening tests).
5. `ExportImportRoundTrip`: build a figure in code (line+circle+rect+stepchild),
   export, re-import, compare endpoints (ε), types, colors, z order.
6. `ExportMatchesPivotBytes`: a single default stick (len 48, −π/2, width 6, black)
   must produce a decompressed payload byte-identical to `single stick.stk`'s
   (header `BF 01 00 00 00`, 16-byte segment, `00 00 00 00` trailer) — pins the
   writer to the observed format.
7. `ImportStaticSampleWarns` (modern *three consecutive sticks…* sample, flags
   `0xB9`): 3 sticks chained P2→P2, one warning naming 1 static segment. Becomes
   the acceptance test for §9.1 later (assert `is_static` on stick 2 instead of
   the warning).
8. `ImportV2Files`: for each v2 sample assert count/types/parents — stickman:
   parents {0,1,1,1,0,0}, seg 4 is the CIRCLE head at the neck joint, legs are
   stepchildren; 3-stick chain: parents 0,1,2 + static warning on the middle one;
   circles/sticks: thickness 5/15, angle 3π/2 normalized to the same direction as
   the modern −π/2 samples.
9. `ImportRejectsCorruptV2`: size not equal to `2 + 24·count`, non-sequential id,
   `parent_joint ≥ id` — clean errors.
10. `ExportV2MatchesPivotBytes`: a single default stick exported as `PIVOT2`
    (thickness 5) must be byte-identical to `v2\single stick.stk` (26 bytes); plus
    `ExportV2ImportRoundTrip` (colors dropped with warning, geometry ε-equal) and
    a >255-stick figure refused.
11. `ImportNodesZedStickman`: 91 sticks from 92 nodes (root dropped); master =
    root's first child, root's other 2 direct children are stepchildren; every
    stick's `my_line` length = 1.5 × the node's stored length (header scale);
    exactly one CIRCLE; 10 TRAPEZOIDs (triangles); z values follow the
    draw_order permutation; warnings mention 72 static and 33 stretchy nodes;
    dark-gray figure color inherited where `use_segment_color = 0`.
12. `ImportRejectsCorruptNodes`: truncated mid-node, negative `child_count`,
    node-count bomb (child_count huge), version 9999 (⇒ compressed path ⇒ zlib
    fail), v403 build 73 rejection — clean errors, no stack overflow (iterative
    reader).
13. Manual verification (not automatable here): open exported files of BOTH
    STK variants in Pivot Animator 5, and the v2 export in Stick Nodes; import
    `advanced\*.stk` and compare on screen with Pivot rendering — specifically to
    confirm the STK opacity hypothesis (§2.3), draw-order direction (§2.6), and
    whether Pivot 2/Stick Nodes accept thickness-0 invisible segments (§5.2).
    For `.nodes`: compare imported Zed-Stickman against its Stick Nodes
    rendering — confirms the header-scale hypothesis and hollow-vs-filled circle
    mapping (§4b). Adjust the marked hypotheses if the visual check contradicts
    them.

## 8. Open questions / accepted risks

- ~~Stick Nodes compatibility~~ **resolved**: the v2 format is now specified (§2.8)
  and in scope for both import and export (§5.2) — Stick Nodes reads exactly that
  variant. Residual risk: v2 semantics were derived from Pivot 5's *Save As → v2*
  output; verify once in real Stick Nodes and/or Pivot 2 (§7.13), especially
  thickness-0 segments.
- Opacity byte direction (`255 − value` vs raw alpha) and draw-order permutation
  direction — two explicitly marked hypotheses, verified visually in §7.13.
- Section A semantics unknown — and NOT static-related (disproved by the static
  sample, §2.4). Safely skipped (import) / zeroed (export); this is what
  "conservative" means here. If Pivot complains about exported files (it shouldn't
  — basic samples with `count_a = 0` exist), revisit.
- The two always-zero unknown bytes per v2 record (§2.8) — ignored/zeroed; warn if
  ever non-zero.
- Flags bits 5/7 unknown — defensively rejected if ever cleared (§2.2).
- Gradient colors (a documented Pivot 5 feature) never appeared in any sample —
  presumably one of the unknown flag bits; covered by the §2.2 rejection rule
  (roadmap: §9.3).
- `.nodes` version gates (§2.9) are transcribed from sticknodes-rs, not from
  first-hand samples of every version/build — only v224 is byte-validated locally.
  The defensive bounds-checked reader turns any gate mistake into a clean import
  error rather than corruption; if a real file fails, get its version/build from
  the error message and re-check that gate against the app.
- `.nodes` hypotheses to verify visually (§7.13): header scale applied to
  coordinates AND thicknesses; Circle(2) hollow vs FilledCircle(4) filled; whether
  compressed (newer) `.nodes` files carry the polyfill/connector trailing sections
  (§2.9 caveat).

---

## 9. Foreign-format features with no local equivalent — future implementation points

Everything in this section is **explicitly out of scope for the first
implementation**: the converter handles each item defensively (warn + degrade to a
plain stick, or skip) as specified in §4/§5. They are recorded here as self-contained
work items for later, ordered by expected value/effort ratio. Each item lists what
the feature is in Pivot, how the first implementation degrades it, and what a full
local implementation would require. None of them changes the container or section
layout of §2 — they only fill in fields/sections the first pass skips, so the
converter can be upgraded incrementally without rework.

### 9.1 Static segments

- **Pivot / Stick Nodes**: a segment marked *static* has no endpoint handle in the
  animator and moves rigidly with its parent. Heavily used by community figures to
  make complex shapes act as one piece (Chuck: 113 of 122 segments; Hippie Van:
  1012 of 1017; Zed-Stickman: 72 of 92 nodes). Stored as the per-segment `static`
  byte in **all three formats** (flags bit 1 in modern STK, fixed offset 21 in v2
  records, node field 4 in `.nodes`). Dedicated STK samples exist for both
  variants: `basic\three consecutive sticks…` (modern, flags `0xB9`) and
  `v2\three consecutive sticks…`. Note: section A is unrelated to statics (the
  modern static sample has `count_a = 0`), so this item does NOT need to decode it
  — export keeps writing `count_a = 0`.
- **First pass degrades to**: normal sticks with handles + summary warning;
  never exported.
- **Full implementation**:
  1. `StickData`: new `bool is_static` field. Serialize it in
     `stickdata.cpp` `operator<<`/`operator>>` — new trailing field, so bump
     `StickData::version` and read it conditionally (`if (version >= 2)`), keeping
     old `.stck`/`.mAnim` files loadable. Follow the CLAUDE.md serialization rules
     (fixed-width types).
  2. View layer: `FramePresenter::AttachView()` / `MyView::AddHandles()` skip
     handle creation for static sticks (the `is_linker` handling is the model to
     imitate); selection/rotation code must treat them as non-interactive (they
     already move rigidly with the parent — that part is free).
  3. Figure builder UI: a "static" toggle for the selected stick (Widget button or
     context action) + undo via the standard `StoreUndo(CMD_SIMPLE)` pattern.
  4. Converter: import `static` → `is_static` in all three importers (drop the
     §4/§4b warnings); export writes flags bit 1 + the byte (modern STK) and the
     offset-21 byte (v2). Acceptance: both STK static samples round-trip through
     both writers, Zed-Stickman imports with 72 `is_static` sticks; §7.7 flips
     from warning-asserting to `is_static`-asserting.

### 9.2 Polyfills (polygon fills)

- **Pivot**: filled polygons whose vertices are figure joints, with own RGB +
  opacity and a slot in the figure-wide draw order (§2.5/§2.6). This is how
  community figures get solid bodies (Chuck: 14 polys, Hippie Van: 83). The single
  biggest visual loss in the first pass — imported advanced figures appear as
  outlines only.
- **First pass degrades to**: skipped with a count warning; export writes
  `poly_count = 0`.
- **Full implementation**:
  1. Model: `PolyfillData` (list of member `StickData*` — the joints — plus
     `QColor`, `qreal z`); owned by `StickFigureData`/`StickFigure` in a
     `QList<PolyfillData*>`. Serialize into the figure operators (version bump as
     in 9.1); `Clone()`/`ExtractStickFigureData()`/`PopulateLiveStickFigure()`
     must remap the stick pointers by index exactly like the tree pointers.
  2. View: a `QGraphicsItem` that rebuilds its `QPolygonF` from the member sticks'
     current endpoints on every refresh (hook where stick views refresh after
     drag/rotate), `setZValue(z)`; attach/detach in `FramePresenter` alongside
     stick views.
  3. UI: creation tool ("select joints in order → close polygon"), color picker
     reuse, delete; undo via the existing frame-snapshot mechanism (polyfills ride
     inside the figure data, so `FrameUndoCommand` covers them for free once
     serialization exists).
  4. Converter: import section B verts → member sticks (joint k = stick k,
     joint 0 = master's P1 side — represent as master + "origin" flag), import
     section C into per-poly z interleaved with stick z; export the reverse,
     including writing section C (flags bit 4 cleared) when polys exist.
  5. Frame/video render path: polyfill items must render in `UpdateIcon()`'s
     clone-scene pattern too (clone polys with their sticks).

### 9.3 Gradient colors

- **Pivot**: per-segment two-color gradient (documented in the v5 figure builder:
  Ctrl+click on the color button). **Not present in any sample file**, so its binary
  encoding is unknown — almost certainly one of the never-observed-cleared flag bits
  (5 or 7) adding a second RGB(+opacity?) per segment.
- **First pass degrades to**: files using it are *rejected* by the §2.2 unknown-bits
  rule (safe: layout would be unparseable anyway).
- **Full implementation** (two independent stages):
  1. *Format decode*: obtain samples (save a one-stick gradient figure in Pivot 5,
     plus the same figure without gradient; diff the decompressed payloads — the
     same technique that produced §2). Update §2.2/§2.3 and relax the rejection
     rule for the newly-understood bit.
  2. *Local support*: local sticks paint with `QPen`/`QBrush`; a gradient stick
     needs a `QLinearGradient` from `p1()` to `p2()` set as brush/pen at paint time
     plus a second `QColor` field on `StickData` (serialization version bump).
     Alternatively (cheaper, import-only): map gradient → flat color of the first
     gradient stop + warning, which only requires stage 1.

### 9.4 Legacy Pivot ≤ 2.2.5 format (⇒ Stick Nodes interop) — FOLDED INTO MAIN PLAN

Originally a future item blocked on missing samples; the samples now exist
(`C:\Users\MVR\Documents\scratchpad\v2\`) and the format turned out trivial, so v2
support is part of the first implementation: spec in §2.8, import auto-detection in
§4.0, export variant in §5.2, tests in §7.8–7.10. Nothing remains here except the
§7.13 real-world check (open a v2 export in actual Stick Nodes / Pivot 2 once —
the spec was derived from Pivot 5's *Save As → v2* output, which could in principle
be a dialect).

### 9.5 Pivot text objects saved as STK

- **Pivot**: the v5 text designer saves text objects as `.stk` files too (per the
  [STK files help](https://www.pivotanimator.net/help5-2/stk_files.htm)); their
  internal layout is unknown and presumably starts with a different flags/marker
  pattern.
- **First pass degrades to**: rejected by the defensive rules (unknown variant).
- **Full implementation**: investigation item only — obtain a sample, decide
  whether a mapping even makes sense (SAM has no text primitive; nearest
  approximations are an IMAGE stick with rasterized text, or plain refusal with a
  clearer message "this STK contains a Pivot text object"). Low priority; do the
  clearer-error-message part for free in the §4.0 sniffer if the signature is easy
  to detect.

### 9.6 `.nodes` export — IMPLEMENTED (newest dialect)

Implemented as `StkFormat::ExportNodes()` (see FOREIGN_FORMATS_MANIFEST.md §3.3):
it emits **only the newest supported dialect** — version 403 build 72, whole-file
zlib, little-endian, every gated field of that build written explicitly — rather
than the older v224-style layout originally considered here; one modern writer, no
version switches, matching what current Stick Nodes itself produces. Key decisions:
- Both pose representations are written consistently from one offset (the
  local-angle chain Stick Nodes treats as authoritative, plus the cached
  local x/y), so the figure decodes identically whichever a reader trusts.
- Behavior fields we have no data for are written NEUTRAL, not zero
  (smart-stretch/angle-lock multipliers = 1.0), so a reader that applies them
  cannot zero the figure out.
- The master is forced to be the root's first child (readers take the first
  child as the main branch), draw order rides `draw_order_index` from z-ranks,
  and the DFS emitter is iterative with a reachability check (cyclic trees are
  refused, not serialized).
- Covered by `NodesExportRoundTrip` and `NodesExportZedRoundTrip` (91-stick
  figure, endpoint drift < 0.75 px). Remaining manual check: open one export in
  real Stick Nodes (§7.13).
- Still the natural home for exporting statics once §9.1 lands, and polyfills
  (v≥230 section) once §9.2 lands.

### Suggested order

9.1 (small, self-contained, big compatibility win for community figures — and it
now upgrades all three formats at once, importers AND exporters) → 9.2 (the big
visual payoff; largest work item) → 9.3 → 9.5 (9.4 and 9.6 are implemented).
Items 9.1+9.2 together make imported advanced figures visually faithful; until
then the import warnings tell the user exactly what was dropped.
