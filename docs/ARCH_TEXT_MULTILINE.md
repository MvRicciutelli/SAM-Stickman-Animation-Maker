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
fields. **Placement is the Stick Transform** (see below), like every
other type's; `TextFrame()` turns it by +90° so the text runs along the
stick. The skeleton is painted
only while `text` is empty, which is exactly what makes the 2-click
preview visible; the text colour IS the stick's pen colour, and the wrap
width IS `my_line.length()` (the font size does not scale with the
figure). `stick.cpp`'s `TextFrame()`/`LayoutText()` are the single source
of truth shared by `paint()` and `ConstructBr()`. Selecting one shows the
code-built `advanced_text_tab` (content, font, size, bold/italic/underline/
strikeout, justification, V/H spacing, H/V stretch, wrap — wired in `Widget`'s
ctor, echoed back by `refresh_text_panel` — one `std::function` global in
`uiItems.h` instead of an extern per control) plus the Stick Transform tab.
A font missing on load is replaced with Arial by
`StickData::operator>>`, which records the family in the
`missing_text_fonts` list that `MyView::ReportMissingTextFonts()` (called
by every load path) turns into one prompt. The foreign `.stk`/`.nodes`
exporters have no text: a TEXT stick leaves as a plain segment.

### The MULTILINE outline

A MULTILINE is not stroked by the pen: per-vertex widths (`vertex_widths`) are something
`QPainterPathStroker` cannot do, so `paint()` builds the band's outline itself and fills it.
It follows the stroker's own rules, and each one fixed a visible bug: offset along the
bisector of the two segments' **unit** directions (the old prev→next chord is length-weighted
and points backwards at a sharp turn into a longer segment — the band flipped and "twisted on
itself"); a true miter (`h / cos(half turn)`) up to QPen's default limit; past it, a bevel on
the outer side with the inner side running through the vertex; and `Qt::WindingFill`, which
fills the inner overlap that rule produces (odd-even left a hole). A miter merely *clamped*
to the limit is wrong: it bends both edges toward the tip and thins both segments.
`MultilineSharpTurnKeepsAFullWidthBand` pins it by rendering.

### MULTILINE branches

A MULTILINE is a **tree** of vertices, not only a chain: `StickData::vertex_parents` (record
`version >= 7`) holds each virtual vertex's parent (P1 = -1), an edge is `(parent, child)`, and
the main line is P2's ancestry back to P1 — so P1/P2, rotation and scaling are untouched. It is
empty for a plain chain, and **every reader goes through `MultilineParents()`**, which returns
the stored list only if it is a valid tree over the CURRENT vertex count and the chain `i-1`
otherwise. That one guard is what keeps a corrupt file, or any code that edits `polygon_points`
without the parents (imports, `ConvertStick()`), from ever handing a cycle to a tree walk. Fetch
it BEFORE changing `polygon_points`, since it validates against the count.

Branch vertices are always inserted at the END of the interior (just before P2), so only P2
shifts: a branch tip the drawing session keeps extending stays valid across appends
(`BranchMultilineVertex()`). `AddPolygonPoint()` splits the edge INTO its index.

**`DeletePolygonPoint()` takes every branch growing out of the vertex along**, as specified:
the rest of its own branch, every branch it is the origin of, and their branches in turn. A
vertex `u` goes if the first main-line vertex above it is the deleted one. Only the main line
closes up: its child on the main line reattaches to a hub, which is its parent, or for P1 that
child itself. When an endpoint would collapse onto the other one (a one-edge main line with
branches), that end goes to the highest remaining index. When fewer than 2 vertices would be
left, it refuses and changes nothing, and `MyView::DeleteVertex()` deletes the whole stick.
That helper is the one rule behind Alt+click (in AND out of a drawing session) and the Delete
key. The key first removes every Ctrl-selected vertex in the frame, as one undo step
(`DeleteSelectedVertices()`), and only falls back to deleting sticks when none is selected.
Alt+click used to be handled only outside a session. Since a plain handle click reopens one,
it looked simply broken. `StickData::ReRootTree()` re-roots after a deletion, and after a
master change reverses the array (`ReanchorVertexScenePositions()`).

`paint()` splits the tree into polylines (`next[v]` follows the main line, any other child opens
a branch) and runs the outline code above per polyline. **A branch starts flat AT its junction,
and the junction is round by default, so its disc is the join.** The first cut started a branch
at the midpoint of the junction's incoming edge, to get a miter at the fork. That carried the
incoming edge past the junction, and wherever the main line does not continue straight (a
corner, an endpoint) a branch turning back by more than 90° stuck out as a spike. A flat start
plus a disc stays inside the junction's radius at every angle. All bands and discs go into ONE
`QPainterPath` filled once with `WindingFill`. Every band has the same orientation, so overlaps
add up instead of cancelling, and a semi-transparent line does not darken at a fork. The discs
go in **reversed**: `addEllipse()` runs clockwise and the bands counter-clockwise, so an
unreversed disc cancels to a hole.

**Per-vertex caps: `StickData::vertex_caps`** (record `version >= 8`), a `VertexCap` per virtual
vertex. `CAP_SHARP` gives a corner a miter, falling back to a bevel past QPen's miter limit as
before, and an end a flat stop. `CAP_BEVEL` always uses the bevel construction. `CAP_ROUND` uses
the bevel construction plus a disc, which on an end is a round cap. Read the list through
`MultilineCaps()`. It returns the historical look when the list is missing or the wrong size:
sharp corners, round branch origins, and ends following `pen.capStyle()`. Old lines therefore
render unchanged, and that is also why the separately drawn RoundCap ellipses are gone. They were
the historical ends, and now they are discs in the same path, so no double alpha. Every vertex
operation keeps the list parallel, exactly like widths and curvatures:
- `AppendMultilineVertex(pt, cap)` takes the new end's cap; the old end keeps its own.
- **`BranchMultilineVertex()` forces `CAP_ROUND` on the new vertex AND on the one it hangs
  from** (spec: no spikes at forks).
- A split vertex copies the cap of the edge it lands on.
- Deletions and the master-change reversal permute the list.

Read it, like the parents, before touching `polygon_points`. The panel's "Vertex cap" combo
(inside `vertex_width_row`, which now holds the two MULTILINE-only rows) calls
`MyView::SetVertexCap()`. That sets `my_vertex_cap` for the next vertices drawn and, as one undo
step, the cap of the Ctrl-selected vertices. Like the width and curvature sliders, the combo
does not echo a selected vertex's own cap.

The DRAWMULTI tool: mid-session, a click within `PolyPickRadius()` of a handle only picks
`poly_branch_from`, with no data change and no undo step. The next clicks grow the branch, and
the P2 handle goes back to the main line. On a finished line, a handle press defers its
`StoreUndo()` to the first move past `QApplication::startDragDistance()`. A release with no drag
reopens the session (`poly_session_resumed`) from that vertex, so a plain click leaves no empty
undo step. In a resumed session Esc only ends it; in a fresh one Esc still deletes the stick.
The `.stk` exporter emits edges parent-first (BFS) and hangs each from its parent vertex's joint.
`.nodes` keeps index order, so P2's node stays `last_node_of`, and references the parent vertex's
node (forward references are fine, since the tree is written from children lists).

`DrawBranchedMultilineViaRealMouseEvents` (real clicks, plus a render probe through the fake
tip→P2 edge a flat-chain render would draw), `MultilineBranchJoinHasNoSpike` (the spike
geometry, rendered semi-transparent: no spike, no hole, no double alpha at the fork),
`MultilineBranchTopologyEdits`, `DeleteMultilineVerticesWithAltAndDeleteKey`,
`MultilineVertexCapsRender` (each cap's corner and end, rendered), `MultilineVertexCapDrawingAndSelection`, the branch in
`ChangeMasterKeepsMultilineShape` and the branched half of `StkAndNodesExportMultilineChain`
pin it. Each was checked to fail with its piece neutralized.

