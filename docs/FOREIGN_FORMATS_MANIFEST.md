# Foreign Stickfigure Formats — Implementation Manifest

How SAM imports and exports Pivot Animator `.stk` and Stick Nodes
`.nodes` stickfigures. All conversion code lives in `stkformat.h` / `stkformat.cpp`
(pure model layer, no scene/widget dependency); the only UI entry points are the
two file dialogs in `mymainwindow.cpp` (`on_actionload_triggered` /
`on_actionsave_triggered`). Byte-level findings and mapping rationale:
STK_IMPORT_EXPORT_PLAN.md. Tests: `tests/tst_characterization.cpp` (`Stk*`/`Nodes*`
slots, sample files under `tests/data/`).

```
                 IMPORT                                   EXPORT
  .stk  (Pivot 3/4/5) ──┐                       ┌── .stk (Pivot 5)         full fidelity
  .stk  (Pivot ≤2)    ──┼──► StickFigureData ───┼── .stk (Pivot 2)         Stick Nodes-importable
  .nodes (SN ≤4.2.3)  ──┘          │            └── .nodes (v403 build 72) newest dialect only
                                   ▼
                     PopulateLiveStickFigure()  /  ExtractStickFigureData()
                     (the same bridges the native .stck path uses)
```

Everything converts **conservatively and defensively**: foreign features with no
local equivalent degrade to plain sticks or are skipped, each with a warning; no
parser ever guesses past an unknown layout, and every read is bounds-checked
(`ByteCursor` latches a fail flag instead of reading out of bounds).

---

## 1. File format structures

### 1.1 Pivot "modern" .stk (Pivot 3/4/5) — read + written

```
FILE ──────────────────────────────────────────────────────────────────────
│ 0x7A ('z') │ raw zlib stream (level 9, header 78 DA) ...................│
└────────────┴─────────────────────────────────────────────────────────────
                     │ inflate
                     ▼
PAYLOAD (all little-endian; f32 floats, f64 angles) ────────────────────────
┌───────────┬─────────────────┐
│ flags  u8 │ seg_count  i32  │            HEADER
├───────────┴─────────────────┴──────────────────────────────────────────┐
│ SEGMENT 1 (root: NO parent field)                                      │
│ SEGMENT 2..N (each prefixed by parent_joint u16)                       │
├─────────────────────────────────────────────────────────────────────────┤
│ section A   count_a u16 + count_a × { index u16, angle f64 }           │
│             (semantics unknown — skipped on import, 0 on export)       │
├─────────────────────────────────────────────────────────────────────────┤
│ section B   poly_count u16                                             │
│             per polyfill: vert_count u16, RGB+opacity u8×4,            │
│                           vert_count × joint u16                       │
├─────────────────────────────────────────────────────────────────────────┤
│ section C   draw order: (seg_count+poly_count) × u16 permutation       │
│             PRESENT ONLY when flags bit4 is cleared (polyfill feature) │
└─────────────────────────────────────────────────────────────────────────┘

SEGMENT record (fields conditional on flags — bit CLEARED ⇒ field present):
┌──────────────┬────────┬────────┬───────────┬──────┬────────┬─────────┬─────────┐
│ parent_joint │ length │ angle  │ thickness │ type │ static │ color   │ opacity │
│ u16 (seg≥2)  │ f32    │ f64    │ f32       │ u8   │ u8     │ u8×3    │ u8      │
│              │        │ rad,   │ 0 = invi- │ bit0 │ bit1   │ R G B   │ bit3    │
│              │        │ abs.   │ sible     │      │        │ bit2    │ 0=opaq. │
└──────────────┴────────┴────────┴───────────┴──────┴────────┴─────────┴─────────┘
type: 0 line/round caps · 6 line/square caps · 1 circle/white · 3 circle/solid
      4 circle/clear    (circle segment line = DIAMETER)
joints: 0 = origin, k = free end of segment k; parent always precedes child.
flags (u8, observed 0xBF/BE/BA/B9/AA/A8/A0): bit set = feature ABSENT.
      bits 5,7 always set, bit 6 always cleared ⇒ any deviation = reject.
```

### 1.2 Pivot legacy "v2" .stk (Pivot ≤ 2.2.5) — read + written (Stick Nodes-importable)

```
FILE (uncompressed, little-endian; size MUST equal 2 + 24×count) ───────────
┌────────────┬───────────┬───────────────────────────────────────────────┐
│ version u8 │ count u8  │ count × 24-byte SEGMENT records               │
│  = 0x01    │ (max 255) │                                               │
└────────────┴───────────┴───────────────────────────────────────────────┘

SEGMENT record (24 bytes, fixed):
offset 0        1         3        4        8        16         20     21       22
┌────────────┬─────────┬────────┬────────┬────────┬───────────┬──────┬────────┬────────┐
│ parent u8  │ id u16  │ ?? u8  │ length │ angle  │ thickness │ type │ static │ ?? u16 │
│ joint ref  │ 1-based │ always │ f32    │ f64    │ f32       │ u8   │ u8     │ always │
│ 0 = origin │ sequen- │ 0      │        │ [0,2π) │           │ 0/1  │        │ 0      │
│            │ tial    │        │        │ abs.   │           │      │        │        │
└────────────┴─────────┴────────┴────────┴────────┴───────────┴──────┴────────┴────────┘
type: 0 stick · 1 circle.  No colors, no caps, no fills, no sections.
Same joint model and angle reference as modern, just normalized to [0, 2π).
```

### 1.3 Stick Nodes .nodes — read (any version 160..403 b72) + written (v403 b72 only)

```
CONTAINER DETECTION ────────────────────────────────────────────────────────
  first i32, read BIG-endian:
    160 ≤ value ≤ 403 ──► file is UNCOMPRESSED, all fields BIG-endian (old SN)
    anything else     ──► whole file is a bare zlib stream (no marker);
                          inflate, re-read header LITTLE-endian (new SN)

PAYLOAD ─────────────────────────────────────────────────────────────────────
┌─────────────┬──────────────────┬───────────┬────────────────┐
│ version i32 │ build i32        │ scale f32 │ color u8×4     │   HEADER
│             │ (only if v≥403)  │ (figure)  │ A B G R order! │
├─────────────┴──────────────────┴───────────┴────────────────┴───────────┐
│ NODE TREE — depth-first pre-order, ONE root:                            │
│                                                                          │
│   ROOT (type −1, pure origin joint) ── child_count = 3                  │
│     ├─ node ── child_count = 1                                          │
│     │    └─ node ── child_count = 0                                     │
│     ├─ node ── ...          each node record ENDS with child_count i32; │
│     └─ node ── ...          its children follow immediately, recursively│
├──────────────────────────────────────────────────────────────────────────┤
│ if v≥230:      poly_count i32 + polyfill records                        │
│ if v≥403 b≥38: connector_count i32 + connector_count × (i32, i32)       │
└──────────────────────────────────────────────────────────────────────────┘

NODE record — ~45 fields, most gated on (version, build); the full ordered
gate table is STK_IMPORT_EXPORT_PLAN.md §2.9. The fields the converter USES:
┌───────────┬────────────┬────────┬───────────┬─────────────┬───────────┬─────────┐
│ node_type │ draw_order │ static │ local_x/y │ thickness   │ colors    │ child_  │
│ i8        │ i32        │ u8     │ f32×2     │ i32         │ ABGR u8×4 │ count   │
│ −1 root   │ →z-order   │ →warn  │ offset    │ (int!)      │ + circle  │ i32     │
│ 0/1 line  │            │        │ from      │             │ outline   │         │
│ 2/4 circle│            │        │ parent    │             │ color     │         │
│ 3/6 trap. │            │        │           │             │           │         │
│ 5/7 other │            │        │           │             │           │         │
└───────────┴────────────┴────────┴───────────┴─────────────┴───────────┴─────────┘
POSE: the format carries the pose TWICE — local_x/y offsets AND a local_angle
(degrees) chain relative to the parent (+ length). Stick Nodes itself derives
geometry from the ANGLE chain; real files keep both consistent. Import reads
the offsets; export writes BOTH, consistent by construction.
```

---

## 2. Import logic

### 2.1 Entry and routing (`on_actionload_triggered`)

```
file dialog: "Stickfigure (*.stck) ; Pivot Animator figure (*.stk) ; Stick Nodes figure (*.nodes)"
        │
        ├─ *.stck ───────────────► native path (StickFigure::LoadStickFigure), unchanged
        │
        ├─ *.stk ────► ImportStk()  ─┐   conversion runs BEFORE any UI mutation:
        └─ *.nodes ──► ImportNodes()─┤   a corrupt file leaves the animation untouched
                                     ▼
                    nullptr? ──yes──► QMessageBox error, STOP
                                     │no
                                     ▼
        StoreUndo() → Widget::AddStick() → PopulateLiveStickFigure(figure, data, true, true)
                    → append sticks to Frame::total_sticks → MyView::AddHandles()
                    → one QMessageBox listing all conversion warnings (if any)
```

### 2.2 `ImportStk()` — both Pivot dialects

```
read file bytes
     │
     ├─ byte[0] == 0x7A ──► inflate rest ──► ParseModernStk(payload)
     │                        (fail ⇒ error)      │
     │                                            ├─ flags sanity: bits 5,7 set & 6 clear, else REJECT
     │                                            ├─ seg_count ∈ [1, 65535], else REJECT
     │                                            ├─ per segment: parent_joint ≤ index, floats finite
     │                                            │  and plausible, else REJECT
     │                                            ├─ section A: skip count_a × 10 bytes
     │                                            ├─ section B: polyfills → POLYGON sticks (see below)
     │                                            ├─ section C (flags bit4 clear): permutation → draw ranks
     │                                            │  (segments AND polyfills)
     │                                            └─ trailing bytes → warning (kept)
     │
     └─ else ───────────────► ParseV2Stk(raw)
                                  ├─ version==1, size == 2+24×count, else REJECT
                                  └─ per record: id sequential, parent < id,
                                     floats sane, else REJECT
     both produce the same intermediate:
     StkSegment { parent_joint, length, angle(abs rad), thickness, type, static, color, draw_pos }
                                     │
                                     ▼
BuildFigure():  joints by forward kinematics (angles are ABSOLUTE, no chain):
                joint[0]=(0,0);  joint[k] = joint[parent] + len·(cos θ, sin θ)
                one StickData per segment:
                  my_line = QLineF(joint[parent], joint[k])
                  parent_joint==0 & not root ⇒ parent=master, stepchild=true
                  else parent = stick[parent_joint], stepchild inherited
                  children appended to EVERY ancestor (flattened invariant)
                  type/pen/brush via mapping table; thickness 0 ⇒ Qt::NoPen
                  z = draw_position / 100.0
                statics counted → ONE summary warning
                polyfills → one StickData::POLYGON stick each (MakePolygonStick()):
                  vertices = the referenced joints' positions at import time, stored
                  in the stick's normalized-local polygon_points; base line from the
                  first joint (weld point / parent) to the first distinct vertex;
                  fill = polyfill color, Qt::NoPen outline; z from section C.
                  APPROXIMATION (warned): the foreign fill deforms with every joint it
                  references, the POLYGON stick moves rigidly with its one parent.
                RecenterFigure(): translate bbox center → canvas center
```

### 2.3 `ImportNodes()`

```
read file bytes
     │
     ├─ BE i32 ∈ [160,403] ──► big-endian cursor over raw bytes
     └─ else ────────────────► inflate whole file (fail ⇒ REJECT), little-endian cursor
                                     │
                                     ▼
header: version/build gate check (v>403 or 403/b>72 or v<160 ⇒ REJECT "newer than supported")
        scale sanity (finite, 0 < s ≤ 1000), figure color (ABGR)
                                     │
                                     ▼
ITERATIVE depth-first walk (explicit stack — a corrupt child_count cannot
overflow the C++ stack; total node budget 65535):
    read root (type −1, pure joint, produces NO stick)
    per node:  ReadNode() consumes every version-gated field at its exact size
               (unmapped fields are read-and-dropped, never guessed)
               position = parent_pos + scale·(local_x, local_y)
               ┌─ root's 1st child ⇒ master_stick
               ├─ root's other children ⇒ stepchildren of master (origin-attached)
               └─ others ⇒ child of parent's stick, stepchild inherited
               type mapping (0/1→LINE round/square · 2→CIRCLE hollow · 4→CIRCLE filled
                             5→CIRCLE+warn · 3→TRAPEZOID zero tip · 6→TRAPEZOID/TAPER
                             7→POLYGON as a regular num_polygon_vertices-gon inscribed
                               in the circle whose diameter is the segment (assumed
                               convention, counted as approximation; <3 verts → LINE)
                             unknown→LINE+warn)
               effective color = use_segment_color ? node color : figure color (alpha as-is)
               z = draw_order_index / 100.0
                                     │
                                     ▼
trailing sections parsed TOLERANTLY (any inconsistency stops the section, keeps the figure):
    polyfills (v≥230) → POLYGON sticks: anchor/vertex node draw-indices resolved to
    positions via the walk's draw-index→slot map; same MakePolygonStick() mapping as
    STK (anchor node = parent, z just behind it); unresolvable references → skip+warn
    connectors → "trailing data ignored" warning
summary warnings: statics, stretchy, curved, gradients, locks, connectors, shapes
RecenterFigure() as above
```

---

## 3. Export logic

### 3.1 Entry and routing (`on_actionsave_triggered`)

```
save dialog: "Stickfigure (*.stck) ; Pivot 5 figure (*.stk) ;
              Pivot 2 figure - Stick Nodes compatible (*.stk) ; Stick Nodes figure (*.nodes)"
        │        (the SELECTED FILTER picks the format — both Pivot dialects share .stk)
        │
        ├─ "Stickfigure"      ──► native .stck (SaveStickFigure), unchanged
        ├─ "Pivot 5"          ──► ExtractStickFigureData() ─► ExportStk(PIVOT5)
        ├─ "Pivot 2"          ──► ExtractStickFigureData() ─► ExportStk(PIVOT2)
        └─ "Stick Nodes"      ──► ExtractStickFigureData() ─► ExportNodes()
                                        │
                        failure ⇒ QMessageBox error; success + warnings ⇒ one joined info box
```

### 3.2 `ExportStk()` — shared front, two serializers

```
guards: non-empty; ≤65535 sticks (PIVOT2: ≤255, suggests Pivot 5 instead)
        │
        ▼
ResolveExportSegments()  — ONE topological pass shared by both dialects:
    emit master first (STK segment 1 IS the origin segment), then repeatedly
    the LOWEST-Z stick whose parent is already emitted
      ⇒ file order == draw order, so no section C is ever needed
    per stick: length=|my_line|, angle=atan2(dy,dx),
      attach joint: master's own stepchildren ⇒ 0 (origin), else parent's index
      geometry guard: p1 vs parent joint mismatch > 0.5px ⇒ ONE warning (STK re-welds)
      type/color per mapping table (LINE→0/6 · CIRCLE→1/3/4 · RECT→6 · TAPER→0 ·
      TRAPEZOID→6 avg width · IMAGE→0 · POLYGON→0 thickness 0, the invisible skeleton ·
      MULTILINE→a CHAIN of welded type-0 segments, one per path edge, thickness = mean
      of the edge's two vertex widths, children attach to the chain's last joint;
      corner curvature has no STK equivalent → warned; approximations counted → warning)
    then, PIVOT5 only, per POLYGON stick (the standard Pivot solid-shape idiom):
      one invisible zero-thickness carrier segment per vertex, parented at the
      skeleton's free-end joint, + one section-B polyfill (brush color, carrier
      joints in outline order); PIVOT2 has no fills ⇒ skeleton only + warning
        │
        ├────────────── PIVOT5 ──────────────┐      ┌───────────── PIVOT2 ─────────────┐
        │ WriteModernPayload():              │      │ WriteV2Payload():                │
        │   flags MINIMIZED like Pivot does  │      │   0x01, count u8                 │
        │   (clear bit0/2/4/8 only if used,  │      │   24-byte records, sequential ids│
        │    bit4 cleared iff polyfills)     │      │   angle normalized to [0,2π)     │
        │   segments, section A = 0,         │      │   type collapses to 0/1          │
        │   section B = polyfills; section C │      │   colors DROPPED → one warning   │
        │   (iff polys): file order with each│      │   uncompressed                  │
        │   fill right after its skeleton    │      └──────────────────────────────────┘
        │   'z' + zlib(level 9)              │
        └─────────────────────────────────────┘
byte-pinned by tests: a default stick reproduces Pivot's own files byte-for-byte
in BOTH dialects (StkExportMatchesPivotBytes / StkExportV2MatchesPivotBytesAndLimits)
```

### 3.3 `ExportNodes()` — newest dialect only (v403 build 72, compressed LE)

Older `.nodes` dialects are deliberately never emitted: one modern writer, no
version switches, maximum compatibility with current Stick Nodes.

```
guards: non-empty; ≤65535 sticks
        │
        ▼
1  draw_order_index = rank of each stick by (z, list position); root joint = 0
2  build the node tree:
        root (type −1, at master's P1)
          ├─ master ..................... FORCED first root child (a reader takes
          ├─ master's stepchildren        the first child as the main branch)
          └─ orphans (warn)               everyone else under its parent's node
3  per stick, resolve BOTH pose representations from one offset:
        offset      = p2 − parent_node_position
        local_x/y   = offset                      (cached representation)
        length      = |offset|
        global θ    = atan2(offset.y, offset.x) in degrees
        local_angle = θ(self) − θ(parent node)    (authoritative representation)
     ⇒ identical geometry whichever one Stick Nodes trusts
4  type mapping: LINE→0/1 by cap · CIRCLE→2 (NoBrush) / 4 (filled; fill color =
   brush, outline color = pen, use_circle_outline=1) · RECT/TRAPEZOID/TAPER→6
   with exact f32 start/end widths (+rounded-end flags for TAPER) · IMAGE→0+warn ·
   POLYGON→0 thickness 0 (invisible skeleton) + one invisible carrier node per
   vertex (child of the skeleton node) + one polyfill trailer record referencing
   them by draw index (anchor = skeleton, use_polyfill_color=1, brush color) ·
   MULTILINE→a CHAIN of type-0 nodes (one per path edge, thickness = mean of the
   edge's vertex widths, curvature warned away; children attach to the last node).
   NOTE: whether Stick Nodes itself reads the polyfill trailer from compressed
   files is still on the §7.13 manual-check list (sticknodes-rs only does so in
   its uncompressed branch); our own importer round-trips it.
   thickness is an INTEGER: sub-pixel pen widths round UP to 1 (+warning)
5  WriteNodeRecord(): every v403/b72 field in exact stream order; features we
   don't have are written NEUTRAL, not zero-blind — multipliers = 1.0, so a
   reader that applies them anyway cannot zero the figure out; one i32 connector
   flag (0) per child
6  emit depth-first pre-order with an EXPLICIT stack; emitted-node count must
   equal stick count + 1 or the tree had a cycle ⇒ REJECT
7  trailer: poly_count i32 = 0, connector_count i32 = 0
8  container: bare zlib stream (no marker byte) — the modern SN container
```

---

## 4. Defensive rules (all formats)

| # | rule |
|---|------|
| 1 | Every read goes through the bounds-checked `ByteCursor`; overrun latches a fail flag checked at section boundaries — clean error, never UB. |
| 2 | Unknown layout ⇒ reject, never guess: modern-STK unknown flag bits; `.nodes` version/build above 403/72; non-`z`, non-v2, non-plausible containers. |
| 3 | Structural sanity: counts within budget (65535; v2 = 255 by construction), parent references only to earlier segments, v2 ids sequential, exact v2 size equation, `.nodes` child_count ≥ 0 and cumulative budget. |
| 4 | Numeric sanity: lengths/thicknesses/angles finite and within plausible magnitude, `.nodes` scale ∈ (0, 1000]. |
| 5 | No recursion over untrusted data: `.nodes` import AND export walk the tree with explicit stacks. |
| 6 | Import converts before any UI mutation; failures leave the animation untouched. |
| 7 | Unknown-but-skippable content (STK section A, `.nodes` trailing sections, trailing bytes) is skipped with a warning, keeping the figure. |
| 8 | Export writes neutral (not zero) values into foreign-only behavior fields, and refuses inconsistent trees (unreachable/cyclic sticks) instead of writing garbage. |

## 5. Warning catalog (shown once per file, joined)

| trigger | direction |
|---|---|
| N static segments → normal sticks | import (all formats) |
| N polygon fills imported as polygon sticks (rigid, one-joint attach) | import (STK modern, .nodes) |
| N polygon fills skipped (unresolvable node references) | import (.nodes) |
| N stretchy / curved / gradient / locked / connector nodes degraded | import (.nodes) |
| N unknown-type or shape segments approximated | import (both) |
| trailing/unrecognized data ignored | import (both) |
| images / rects / tapers / trapezoids exported as plain segments | export (.stk) |
| N polygon sticks exported as polygon fill + invisible vertex segments/nodes | export (PIVOT5, .nodes) |
| polygon fills unsupported: polygons exported as invisible segments | export (PIVOT2) |
| N multi-line sticks exported as chains of welded segments | export (all) |
| curved multi-line corners exported as sharp angles | export (all) |
| squashed circles (ellipses) exported as plain circles | export (both) |
| colors discarded / square caps lost / invisible sticks | export (PIVOT2) |
| sub-pixel widths rounded up to 1 | export (.nodes) |
| detached stick geometry re-welded | export (both) |

## 6. Verification

- 14 automated tests (`Stk*`, `Nodes*`) over 16 real sample files: per-feature
  import assertions, corrupt-file rejection (truncation, bombs, bad references,
  unknown variants), full export↔import round-trips (including the 91-stick
  Zed-Stickman through the `.nodes` writer, endpoint drift < 0.75 px), and two
  byte-identity pins against files Pivot itself wrote.
- Manual checks that still need a real app (plan §7.13): open exports in Pivot
  Animator 5 / Stick Nodes; confirms the STK opacity direction, draw-order
  direction, and hollow-circle hypotheses documented in the plan.
