#ifndef MYVIEW_H
#define MYVIEW_H


#include <QGraphicsView>
#include <QPointF>
#include <QMouseEvent>
#include <QGraphicsItem>
#include <QLineF>
#include <QUndoStack>
#include <StickFigure.h>
#include <animation.h>

// --- Tool ids stored in `tool` and set by SetTool(); the toolbar button slots in
// --- widget.cpp map each button to one of these. mousePressEvent/mouseMoveEvent/
// --- ChangeTool() all switch() on the active id.
#define DRAW 5          ///< draw a straight segment (2 clicks: origin, then free end)
#define DRAWCIRCLE 6    ///< draw a circle-typed stick (StickData::CIRCLE)
#define ELONGATE 19     ///< drag the free end to change only a stick's length
#define DRAWIMG 7       ///< draw an image stick from image_draw_buffer (StickData::IMAGE)
#define MANIPULATE 8    ///< drag the free end, carrying the stick's children along
#define ROTATE 9        ///< rotate the whole selected figure around its master
#define SCALE 10        ///< scale the whole selected figure
#define NOTOOL 4        ///< default: click to select, drag to rotate a single stick/subtree
#define CLAMP 20        ///< like NOTOOL but rotates only the clicked stick (no children)
#define MOVESCENE 11    ///< translate every figure in the frame together
#define ROTATESCENE 12  ///< rotate every figure in the frame around the canvas centre
#define SCALESCENE 13   ///< scale every figure in the frame around the canvas centre
#define JOIN 14         ///< weld the current figure onto the one clicked next (MergeStickFigures)
#define SPLIT 15        ///< break the current figure at the selected stick (SplitStickFigures)
#define DRAWRECT 16     ///< draw a rectangle-typed stick (StickData::RECT)
#define DRAWTRAPEZOID 17 ///< draw a trapezoid-typed stick (StickData::TRAPEZOID)
#define DRAWTAPER 18    ///< draw a tapered stick (StickData::TAPER)
#define DRAWPOLY 21     ///< draw a polygon-typed stick (StickData::POLYGON): 2 clicks for the
                        ///< base line, then every click adds a vertex until Enter/Esc/double
                        ///< click/click-near-start; on an existing polygon it inserts vertices
                        ///< (19/20 would collide with ELONGATE/CLAMP)
#define DRAWMULTI 22    ///< draw a multi-line stick (StickData::MULTILINE): every click extends
                        ///< the path until Enter/double click (finalize) or Esc (cancel = the
                        ///< stick is deleted); on an existing multi-line it inserts a vertex at
                        ///< the middle of the nearest segment, drag adjusts, Alt+click deletes
#define DRAWFREEHAND 24 ///< draw an invisible 2-click stick, then enter draw mode to paint a
                        ///< raster image on it (StickData::FREEHAND); id == the stick type,
                        ///< same tool-id==type convention DRAWIMG/IMAGE use
#define DRAWTEXT 25     ///< draw a text stick (StickData::TEXT): 3 steps — origin click,
                        ///< free-end click, then a prompt for the paragraph. The skeleton
                        ///< is only visible during the 2-click preview; id == the stick
                        ///< type, same convention as DRAWIMG/DRAWFREEHAND
#define DRAW_IK 23      ///< creates/edits inverse-kinematics chains (StickData::IK_START/
                        ///< IK_NODE/IK_TARGET_ON/IK_TARGET_OFF): first click on an eligible
                        ///< stick's start handle begins a chain, second click on a
                        ///< descendant stick commits it; clicking an existing target
                        ///< toggles it on/off, Alt+click on one deletes the whole chain
// --- Layer reorder directions passed to MoveStickFigureZ().
#define MODE_DOWN 1     ///< move the selected layer one step down (towards the back)
#define MODE_UP 0       ///< move the selected layer one step up (towards the front)
#define MODE_TOP 2      ///< send the selected layer all the way to the front
#define MODE_BOTTOM 3   ///< send the selected layer all the way to the back

// --- Structural tag carried by every undo entry; StoreUndo() records which kind of
// --- change is about to happen and RestoreFrameSnapshot() dispatches on it.
#define CMD_SIMPLE 0        ///< in-place edit, current frame stays (move/scale/rotate/add-remove stick)
#define CMD_ADDFRAME 1      ///< a frame was created (undo must delete it again)
#define CMD_DELETEFRAME 2   ///< a frame was removed (undo must recreate it)
#define CMD_MOVETOFRAME 3   ///< a frame's whole content changed, no frame added/removed

// --- Direction passed to RestoreFrameSnapshot(), telling apart the two halves of the
// --- add/delete-frame pair (an undo of an add == a redo of a delete, and vice versa).
#define MODE_REDO 0
#define MODE_UNDO 1

// --- Which graphic attribute SetGraphics() copies from my_pen/my_brush onto a stick.
// --- The two "colour" ones copy the whole QPen paint / QBrush, not just a QColor: since
// --- the Style column's type combo boxes can make either a pattern, a gradient or an
// --- image, a setColor() call would be a no-op on exactly those. The names are kept as
// --- they were to leave every call site in widget.cpp untouched.
#define ATTRIBUTE_PENCOLOR 0    ///< the stroke's whole paint (brush + the Qt::NoPen state), via CopyPenPaint()
#define ATTRIBUTE_PENWIDTH 1    ///< the stroke's thickness only
#define ATTRIBUTE_BRUSHCOLOR 2  ///< the fill's whole QBrush, by plain assignment
#define ATTRIBUTE_BRUSHSIZE 3   ///< unused; kept so the numbering of the three above never shifts


/**
 * @brief The animation canvas: a QGraphicsView that owns the whole document and turns
 *        mouse/keyboard gestures into edits of the live Animation.
 *
 * MyView is the hub of the app. It owns the `Animation` (frames → figures → sticks,
 * see animation.h / frame.h / StickFigure.h / stickdata.h) and is the one place raw
 * input becomes a model change: the `mousePressEvent`/`mouseMoveEvent`/
 * `mouseReleaseEvent`/`keyPressEvent`/`wheelEvent` overrides switch on the active
 * `tool` and drive the matching StickFigure/StickData operation, wrapping each gesture
 * in a `StoreUndo()`/undo_stack entry.
 *
 * The surrounding UI talks to it from two sides: the toolbar/panels in widget.cpp
 * (`view->...`) trigger tools and edits, and the menu in mymainwindow.cpp handles
 * file/export. Only sticks actually on screen get a `StickGraphicsItem` view, attached
 * through `FramePresenter::AttachView()`; everything MyView stores for undo/copy/library
 * is pure data (`FrameData`/`StickFigureData`/`StickData`). See CLAUDE.md for the full
 * data-model and undo design.
 */
class MyView : public QGraphicsView
{
    Q_OBJECT
public:
    /**
     * @brief Builds the empty canvas: creates the Animation, the default pen/brush, the
     *        undo stack and the autosave hook.
     * @param parent parent widget (Qt ownership).
     */
    explicit MyView(QWidget *parent = nullptr);

    /**
     * @brief Tears the document down: detaches every handle, then deletes all frames,
     *        the Animation and the copy/library buffers.
     */
    ~MyView();

    unsigned int tool = NOTOOL; ///< active tool id (one of the DRAW/ROTATE/... constants)

    /**
     * @brief True while playback is running (raised by MySleeper/PreparePreview()):
     *        MoveToFrame() skips the interactive-only work (icons, handles, onion
     *        skins, list selection) for the duration. Formerly the free global
     *        `play_back` (widget.cpp), now owned by the class that consumes it.
     */
    bool play_back = false;

    /**
     * @brief Leaves the current tool cleanly, then activates a new one.
     * @param new_tool one of the tool-id constants above.
     *
     * Calls ChangeTool() first so a half-drawn stick or tool-specific UI is cancelled
     * before the switch. Called from the toolbar button slots in widget.cpp.
     */
    void SetTool(int new_tool);

    Animation * my_animation; ///< the document root: owns the frame list (animation.h)

    /**
     * @brief Deletes the selected figure layer from the current frame and its list row.
     *
     * Refuses to remove the last remaining layer (a frame must keep at least one
     * figure). Also reached from keyPressEvent() when Delete is pressed on a master
     * stick, and from the JOIN tool after a merge. Selection is moved to a surviving
     * layer so a following Move-Layer works on the right row.
     */
    void DeleteStickFigure();

    QPen    my_pen;   ///< current pen pushed onto newly drawn sticks and applied by SetGraphics()
    QBrush  my_brush; ///< current brush, same role as my_pen; both edited from widget.cpp's panels
    /**
     * @brief Current value of StickData::brush_follows_stick: same role as my_pen/my_brush,
     *        i.e. what a newly drawn stick inherits and what SetGraphics() applies.
     *
     * One flag for the pen and the brush together, matching the field it feeds. Driven by
     * the "Texture follows stick" check box at the top of the Style column.
     */
    bool    my_brush_follows_stick = true;
    /**
     * @brief Current value of StickData::texture_tiled: same role as my_pen/my_brush, i.e.
     *        what a newly drawn stick inherits and what SetGraphics() applies.
     *
     * Driven by the "Tile" check box of TextureOptionsDialog, the only control that edits it.
     */
    bool    my_texture_tiled = true;
    /**
     * @brief Current value of StickData::stroke_brush: same role as my_pen/my_brush, i.e. what a
     *        newly drawn stick inherits and what SetGraphics()'s ATTRIBUTE_PENCOLOR applies.
     *
     * A null `stamp` — the default — is "no brush border", so until the Border type combo's
     * "Brush" entry is used this rides along costing nothing. Driven by BrushStrokeDialog, which
     * is also where the tip is picked (from the same .abr library draw mode uses).
     */
    BrushStroke my_stroke_brush;

    // ---- maniglia a freccia della texture (fill e contorno) --------------------------
    // Un pattern/gradiente/immagine ha tre numeri che nessuna maniglia mostrava: offset,
    // angolo e scala. La freccia li rende un solo gesto — la coda è l'offset, la direzione
    // l'angolo, la lunghezza la scala — e ne mostra il valore corrente anche a riposo.
    // Vivono qui e non su StickData perché sono stato dell'INTERFACCIA (quali maniglie
    // mostrare), non del disegno: nulla di ciò che si salva o si esporta cambia.
    bool show_fill_texture_arrow = false;   ///< draw/grab the fill's texture arrow ("Show texture handle", Fill group)
    bool show_stroke_texture_arrow = false; ///< same for the stroke's ("Show texture handle", Border group)

    /**
     * @brief The two arrow endpoints for one of the current stick's textures, in scene coords.
     * @param for_fill true for StickData::brush, false for the pen's paint.
     * @param tail out: the arrow's start — the texture's offset.
     * @param tip out: the arrow's end — direction is the texture's angle, length its scale.
     * @return false when there is no arrow to draw: no current stick, no live view, that group's
     *         check box off, or a brush with no geometry to point at (a solid colour or
     *         Qt::NoBrush). Both a texture and a gradient get one, but NOT through the same
     *         field: ReadArrow()/WriteArrow() go to QBrush::transform() for a texture and to the
     *         gradient's own geometry for a gradient, because a gradient's transform acts in its
     *         0..1 box space, where a scene-unit offset would mean that many whole bounding
     *         boxes. The numeric editors (TextureOptionsDialog, GradientDialog) stay the place
     *         for exact values; this is for aiming.
     *
     * Public because Widget's check boxes and the tests both need to ask about the arrow;
     * MyView itself uses it from drawForeground() and from the hit test.
     */
    bool TextureArrowPoints(bool for_fill, QPointF *tail, QPointF *tip) const;

    /**
     * @brief Scene point an offset of (0,0) puts the arrow's tail on: beside the stick's
     *        midpoint, pushed out along its normal by a grab radius or two, on the side the
     *        arrow points to.
     * @param stick the stick the arrow belongs to; must be non-null.
     * @param angle the texture's rotation in degrees — it decides which of the two sides.
     *
     * A display anchor, not a claim about the texture: the offset the arrow reports is always
     * exactly `tail - anchor`, so dragging it is exact whatever the anchor is. It is NOT P1 and
     * NOT the bare midpoint because the arrow is the lowest-priority thing on the canvas to
     * grab, so an anchor coinciding with any higher-priority handle would be permanently
     * shadowed by it and the offset could never be dragged at all — see TextureArrowPoints().
     */
    QPointF TextureArrowAnchor(const StickData *stick, qreal angle) const;

    StickFigureData* copy_stick_figure_buffer = nullptr; ///< owned copy-paste buffer (pure data since Phase 2b)

    // ---- Freehand draw mode (myview.cpp) ---------------------------------------------
    // A FREEHAND stick is an IMAGE-like raster stick painted interactively. Draw mode is
    // a modal state (independent of `tool`): while active, every mouse/key event is
    // intercepted here and the tool switch is bypassed. See EnterDrawMode()/ExitDrawMode().
    bool draw_mode = false;                 ///< true between EnterDrawMode() and ExitDrawMode()
    StickData *draw_target = nullptr;       ///< the FREEHAND stick being painted (nullptr outside draw mode)
    QImage *draw_canvas = nullptr;          ///< == draw_target->stick_img during the session; painted in place (live)
    QRectF draw_region;                     ///< scene rect the canvas covers (the enter-time visible viewport)
    // brush state — edited live by DrawModeWidget's controls (widget.cpp):
    bool draw_eraser = false;               ///< eraser sub-tool (composites DestinationOut)
    bool draw_pressure_size = true;         ///< pen-pressure-drives-size checkbox (Phase 3); ignored on mouse input
    bool draw_pressure_opacity = true;      ///< pen-pressure-drives-opacity checkbox (Phase 3); ignored on mouse input
    double draw_pressure_val = 1.0;         ///< live pen pressure 0..1 from tabletEvent; 1.0 on mouse (reset each stroke)
    int draw_brush_size = 20;               ///< stamp diameter in scene units
    int draw_brush_opacity = 100;           ///< 0..100
    int draw_brush_spacing = 25;            ///< dab pitch, % of the (pressured) stamp diameter; lower = smoother/slower stroke
    QColor draw_brush_color = Qt::black;     ///< brush tint
    QImage draw_brush_stamp;                ///< grayscale-alpha stamp mask (built-in soft round in Phase 1)
    // built-in round tip, Photoshop-style: hardness/roundness are baked into the stamp by
    // MakeRoundBrush() (so the panel rebuilds it when they change), angle and both jitters
    // are applied per dab in StampBrush() and therefore work on imported .abr tips too.
    int draw_brush_hardness = 75;           ///< % of the radius that stays fully opaque before the feathered edge
    int draw_brush_roundness = 100;         ///< % of the tip's minor axis (100 = circle, lower = flatter ellipse)
    int draw_brush_angle = 0;               ///< tip rotation, degrees
    int draw_brush_size_jitter = 0;         ///< % of random per-dab size reduction (0 = every dab at the dialled size)
    int draw_brush_angle_jitter = 0;        ///< % of random per-dab rotation (100 = anywhere in the full circle)

    /** @brief Builds a soft round grayscale-alpha stamp: the built-in brush, and the panel's
     *  preview icons. Takes hardness/roundness explicitly (rather than reading the members) so
     *  the panel can render a preset's tip without first making it the active brush.
     *  @param size stamp diameter in pixels.
     *  @param hardness 0..100, % of the radius that stays fully opaque before the feather.
     *  @param roundness 1..100, % of the tip's minor axis (100 = circle).
     *  @return ARGB32 premultiplied mask, black with the coverage in its alpha channel. */
    static QImage MakeRoundBrush(int size, int hardness, int roundness);

    bool stickfig_lasso = false;            ///< figure-selection rubber band: false=rect, true=lasso (menu checkbox)

    // ---- Pixel selection & free-transform (draw-mode only) ---------------------------
    // Two selection sub-tools (rectangle, lasso) mark a set of canvas pixels; the selection
    // is then wrapped in a move/scale/rotate gizmo. Entirely separate from stick/frame
    // selection and alive only while draw_mode is: any exit, a switch to brush/eraser, or
    // Ctrl+D discards it. Shift adds to / Alt subtracts from the current selection.
    /// @brief Draw-mode sub-tools, selected by SetDrawTool() (draw-mode panel buttons,
    /// widget.cpp). DT_SEL_* drive the pixel-selection gizmo; DT_BRUSH/DT_ERASER paint.
    enum DrawTool {
        DT_BRUSH = 0,   ///< paint dabs with the current brush (StampBrush)
        DT_ERASER,      ///< erase (brush composited DestinationOut)
        DT_SEL_RECT,    ///< rectangle pixel-selection marquee (DrawSelPress/Move/Release)
        DT_SEL_LASSO    ///< freehand (lasso) pixel-selection marquee
    };
    int draw_tool = DT_BRUSH;               ///< active draw-mode sub-tool (a DrawTool value)
    QRegion draw_selection;                 ///< selected pixels in canvas space (empty = no selection)
    QRect   draw_sel_bounds;                ///< bbox of draw_selection at (re)definition, canvas px
    // gizmo: transform about draw_sel_bounds.center(), applied to the selected pixels
    QPointF draw_sel_t;                     ///< gizmo translation (canvas px)
    double  draw_sel_r = 0;                 ///< gizmo rotation (degrees)
    double  draw_sel_sx = 1, draw_sel_sy = 1; ///< gizmo scale (about box centre)
    bool    draw_float_active = false;      ///< selected pixels lifted into draw_float and being transformed
    QImage  draw_float;                     ///< lifted pixels, origin = draw_sel_bounds.topLeft()

    /** @brief Switch the active draw-mode sub-tool; leaving a selection tool for brush/eraser
     *  discards (committing) any pixel selection. */
    void SetDrawTool(int t);
    /** @brief Bake a floating (transformed) selection back into the canvas, then clear the
     *  selection and gizmo. Safe when nothing is selected. */
    void DiscardPixelSelection();

    /**
     * @brief Enters draw mode on a FREEHAND stick: bakes its current transform into a
     *        fresh viewport-sized canvas and starts the interactive paint session.
     * @param stick the FREEHAND stick to edit.
     * @param take_undo push a frame-undo snapshot first (true from the re-edit button;
     *        false right after creation, where DrawCmd already snapshotted).
     */
    void EnterDrawMode(StickData *stick, bool take_undo);

    /**
     * @brief Leaves draw mode: crops the canvas to the painted bounds, re-anchors the
     *        stick, and discards the draw-local undo chain. Safe to call when not in
     *        draw mode (no-op). Reached from Esc/Enter, any tool switch, or file/frame ops.
     */
    void ExitDrawMode();

    /**
     * @brief Steps the draw-session-local (image-snapshot) undo/redo chain — separate
     *        from `undo_stack`, alive only for the duration of one draw session.
     */
    void DrawStrokeUndo();
    void DrawStrokeRedo();

    /**
     * @brief Undo/redo entry point for every UI trigger (toolbar buttons + Ctrl+Z/Y
     *        shortcuts). In draw mode they drive the session-local stroke chain; otherwise
     *        the global frame undo_stack. Single place so both triggers stay consistent.
     */
    void Undo();
    void Redo();

    /**
     * @brief Switches the on-screen frame: clears the scene/layer list, repopulates
     *        them from `frame`, refreshes onion skins and selection.
     * @param frame the frame to display; becomes current_frame.
     *
     * The single routine every frame change goes through — the frame list slot in
     * widget.cpp, undo/redo (RestoreFrameSnapshot()), file load (mymainwindow.cpp) and
     * playback all call it. Attaches a live view to any stick that doesn't have one yet
     * (a freshly loaded frame), via FramePresenter::AttachView().
     */
    void MoveToFrame(Frame *frame);

    /**
     * @brief Rebuilds the semi-transparent "ghosts" of the previous frames.
     *
     * Clears the old onion pixmaps, then renders the N frames before the current one
     * (N = onion_skin_sb spinbox) onto faded pixmaps. Works on Clone()d StickData so it
     * never steals a live stick's `view` back-pointer (same pattern as
     * StickFigure::UpdateIcon()). Skipped during playback.
     */
    void UpdateOnionSkins();
    QList<QGraphicsPixmapItem*> onion_skins; ///< pixmaps currently drawn as onion skins (owned while shown)

    /**
     * @brief Undo/redo history, replacing the hand-rolled undoBuffer/redoBuffer
     *        (REFACTOR_PLAN.md Phase 4). Every entry is a FrameUndoCommand
     *        (myview.cpp) holding before/after `FrameData` snapshots of a single
     *        frame; `MAXUNDO` is enforced via `setUndoLimit()` in the constructor.
     */
    QUndoStack undo_stack;

    /**
     * @brief Restores a frame from a snapshot, dispatching on the command tag.
     * @param command_tag CMD_SIMPLE / CMD_ADDFRAME / CMD_DELETEFRAME / CMD_MOVETOFRAME —
     *        which structural change (if any) accompanied the edit being reverted/
     *        reapplied, and therefore whether a frame needs to be created/removed
     *        first (mirrors the pre-Phase-4 `undoRedo()` dispatch).
     * @param mode MODE_UNDO or MODE_REDO — which direction the CMD_ADDFRAME/
     *        CMD_DELETEFRAME pair goes (add un-does a delete and vice versa).
     * @param snapshot the frame content to restore; not modified or taken over.
     *
     * Called only by FrameUndoCommand::undo()/redo() (myview.cpp) — public because
     * that class lives outside MyView, not because other callers are expected.
     */
    void RestoreFrameSnapshot(int command_tag, int mode, FrameData *snapshot);

    /**
     * @brief Snapshots the current frame as the "before" state of a new edit and
     *        pushes it onto `undo_stack`.
     * @param command CMD_SIMPLE (default) / CMD_ADDFRAME / CMD_DELETEFRAME /
     *        CMD_MOVETOFRAME — see RestoreFrameSnapshot().
     *
     * Call BEFORE mutating, exactly like the pre-Phase-4 version — callers are
     * unchanged by this refactor. The "after" state is captured lazily, the first
     * time this edit is undone (see FrameUndoCommand in myview.cpp), since until
     * then the live frame already *is* the after-state and needs no separate copy.
     */
    void StoreUndo(int command = CMD_SIMPLE);

    /**
     * @brief Removes a frame from the animation and renumbers the ones after it.
     * @param frame frame to delete (its list row is taken first).
     *
     * The list-widget/renumber half of a frame deletion; the model half lives in
     * Animation::DeleteFrame(). Used by the delete-frame button (widget.cpp) and by
     * RestoreFrameSnapshot() when undoing an add / redoing a delete.
     */
    void DeleteFrame(Frame *frame);

    /**
     * @brief Inserts a new empty frame at a position.
     * @param pos index where the frame is created.
     * @return the created Frame.
     *
     * Thin pass-through to Animation::SetupFrame(); kept as a member so undo restore
     * and the add-frame button share one entry point.
     */
    Frame *SetUpFrame(int pos);

    /**
     * @brief Fills the gap between two adjacent frames with linearly interpolated ones.
     * @param start_index index in Animation::frame_list of the FIRST of the two frames;
     *        the second is always `start_index + 1` (the UI only offers the current frame
     *        paired with its previous or its next one).
     * @param count how many frames to generate (N >= 1).
     * @return an empty string on success, or the reason the two frames cannot be
     *         interpolated — the caller (Widget::on_generateInbetweenBtn_clicked()) shows
     *         it to the user. Nothing is modified when a check fails.
     *
     * Both frames must exist and must match structurally: same number of stickfigures,
     * same number of sticks in each corresponding figure, and the same StickData::stick_type
     * for each corresponding stick — without that there is no pairwise correspondence to
     * interpolate. The two frames themselves are left untouched; the generated frames are
     * inserted between them, each a copy of the CURRENT frame whose sticks are then moved
     * to their interpolated pose (InterpolateStickData(), stickdata.h) at t = i/(N+1).
     * The whole batch is ONE undo step: undoing removes all N frames, redoing rebuilds
     * them from the same snapshots.
     */
    QString GenerateInbetweens(int start_index, int count);

    /**
     * @brief Converts a viewport point to scene coordinates, clamped to the canvas.
     * @param my_coord in/out: the point to clamp (kept within my_rect + a 10% margin).
     * @param starting_point raw viewport position from the mouse event.
     *
     * Every mouse handler runs the cursor through this so an edit can't drag a stick
     * arbitrarily far off the page. Writes the global `coord` as a side effect.
     */
    void MapMyCoords(QPointF &my_coord, QPointF starting_point);

    /**
     * @brief Wipes the undo/redo history (QUndoStack::clear()).
     *
     * Called when a new document replaces the current one — file load and
     * New-Blank-Animation in mymainwindow.cpp — so stale frames can't be restored onto
     * a different animation.
     */
    void ClearUndo();

    /**
     * @brief Shows (once) the prompt listing the TEXT-stick fonts a load didn't find on
     *        this system and replaced with Arial, then clears the list.
     *
     * StickData::operator>> does the substitution and fills `missing_text_fonts`
     * (stickdata.h) — it has no UI to prompt from. Every load entry point calls this
     * when it's done: LoadLibrary() here, LoadAnimation()/LoadStickFigure() from their
     * callers in mymainwindow.cpp. No-op when nothing was missing.
     */
    void ReportMissingTextFonts();

    /**
     * @brief Loads a stick-figure library file into the buffer and its list widget.
     * @param file_name .stck-style library file to read.
     * @param mode 0 (default) rebuilds the widget entries; non-zero loads data only
     *        (legacy path, currently unused).
     */
    void LoadLibrary(QString file_name, int mode = 0);

    /**
     * @brief Copies the current figure into the library as a data record.
     * @param name when non-empty, overrides the copied StickFigureData's name (used by
     *        the "Add to library" prompt); ignored for a multi-selection, where each
     *        figure keeps its own live name. Empty by default: keeps the live figure's name.
     * @return the live source figure, or nullptr if it has no sticks.
     */
    StickFigure *AddStickFigureToLibrary(const QString &name = QString());

    /**
     * @brief Saves ONE figure as a new, self-contained *.marlib file (a FrameData holding
     * just that figure) — independent of, and without touching, the currently-open library
     * buffer (stick_library_buffer). Used by File > Save StickFigure's "Marionetta Library"
     * filter, so a single figure can be handed off as a ready-to-load mini-library.
     * @param file_name destination *.marlib path.
     * @param figure live figure to export; no-op if null or empty.
     */
    void SaveStickFigureAsLibrary(const QString &file_name, StickFigure *figure);
    QList<QGraphicsPixmapItem*> preview_list; ///< rendered frames shown one-by-one during playback (PreparePreview/DisplayPreview)

    /**
     * @brief Removes the selected entry from the library (widget + data buffer).
     */
    void RemoveFromLibrary();

    /**
     * @brief Applies the current pen/brush attribute to one stick or the whole figure.
     * @param all false: only the current stick; true: every stick in the current figure.
     * @param attribute which property to copy (ATTRIBUTE_PENCOLOR/PENWIDTH/BRUSHCOLOR).
     *
     * Reads my_pen/my_brush (set by the Style column's controls in widget.cpp) and pushes
     * the chosen attribute onto the model, then refreshes the affected views and icon.
     * The fill/stroke KIND — solid, pattern, gradient, image, none — travels inside those
     * two objects, so no extra parameter was needed when the type combo boxes were added.
     */
    void SetGraphics(bool all, int attribute);

    /**
     * @brief Splits the current figure at the selected stick into a new figure.
     *
     * The selected stick and its subtree become a new "_split" figure layer; the rest
     * stays. Refuses to split at the master. The model work is done by SplitStickFigures()
     * (StickFigure.cpp); this method handles the layer list, handles and z-order list.
     */
    void SplitStickFigure();

    /**
     * @brief Empties the current library (widget + buffer) and starts a fresh one.
     */
    void ClearCurrentLib();

    /**
     * @brief Re-applies the canvas size (W/H) to every frame, optionally repositioning.
     * @param option REPO (recentre only), REPOSCALE (recentre + rescale) or NOREPO.
     *
     * Called from MyMainWindow::GetAnimationParams() after the user changes the
     * animation dimensions in the options dialog.
     */
    void SizeChange(int option);

    /**
     * @brief Uniformly scales one frame's figures about the canvas centre.
     * @param F frame to scale.
     * @param scale_amount linear scale factor.
     * @param R reference rect whose centre is the scaling pivot.
     *
     * Helper for SizeChange()'s REPOSCALE path. NOTE: "Rescale animation" currently has
     * a width-only scale-factor bug (see CLAUDE.md "Known unresolved items").
     */
    void ScaleFrame(Frame *F, float scale_amount, QRectF R);

    /**
     * @brief Pre-renders every frame to a pixmap for smooth playback.
     *
     * Fills preview_list with one flattened image per frame (rendered from Clone()d
     * data, so live views are untouched). DisplayPreview() then flips through them.
     */
    void PreparePreview();

    /**
     * @brief Shows the pre-rendered frame at `idx` during playback.
     * @param idx index into preview_list; the previous frame's pixmap is removed first.
     */
    void DisplayPreview(int idx);

    /**
     * @brief Selects the stick/figure nearest the last click and syncs the layer list.
     *
     * The picking routine behind the NOTOOL/CLAMP/ELONGATE/MANIPULATE tools: asks the
     * Animation which stick is under `coord`, then mirrors that into the layer list
     * widget selection and the image list. No-op on an empty frame.
     */
    void ArrowSelection();

    /**
     * @brief Moves the selected figure layer in the Z (drawing) order.
     * @param increment signed step (used for MODE_UP/MODE_DOWN).
     * @param mode MODE_UP / MODE_DOWN / MODE_TOP / MODE_BOTTOM.
     *
     * Keeps the model z-order (Frame::Stickfigure...()) and the layer list widget in
     * sync. Driven by the move-layer buttons in widget.cpp.
     */
    void MoveStickFigureZ(int increment, int mode);

    /**
     * @brief Renumbers the current frame's `order` fields and refreshes num_of_items.
     * @param f retained for call-site compatibility; ignored (always uses the current frame).
     *
     * Run by StoreUndo() (and a couple of widget.cpp edits) right before snapshotting,
     * so the saved z-order matches the on-screen stacking.
     */
    void UpdateFrameOrder(Frame *f);

    // ------- multi-selezione di stickfigure (rettangolo/Shift-click/lista layer) ------

    /**
     * @brief True while two or more stickfigures are selected together.
     */
    bool MultiSelectionActive() const { return multi_selection.count() > 1; }

    /**
     * @brief Re-anchors the selection bounding box, pivot and transform handles onto the
     *        CURRENT figure after the active layer is switched from the list widget. Without
     *        this the scene-item decorations and the drawForeground overlay stay on the
     *        previously selected figure (only partially cleared). No-op during a multi-
     *        selection (its UI is driven separately) or a programmatic list-sync echo.
     */
    void RefreshSelectionForCurrentLayer(){
        if(multi_widget_sync || MultiSelectionActive()) return;
        UpdateSelectionDecorations(true);
    }

    QList<StickFigure*> multi_selection; ///< the selected figures (UI state; cleared on frame change/undo restore)
    QList<StickFigureData*> multi_copy_buffer; ///< copy buffer for a multi-selection (pure data)
    bool multi_widget_sync = false; ///< raised while THIS code drives the layer list's selection, so Widget's itemSelectionChanged handler can tell echo from user input

    /**
     * @brief The transform pivot: where the handle is drawn and the origin every
     *        multi-selection transformation (rotate/scale/flip) uses. Recentered on
     *        the selection's centroid whenever the SELECTION changes; carried along
     *        by translation drags; freely repositionable by dragging the handle or
     *        holding Space (released at the mouse position).
     */
    QPointF multi_pivot;

    /**
     * @brief Replaces the multi-selection wholesale (rubber band / layer list) and
     *        refreshes highlight, handle and layer-list rows. Passing fewer than 2
     *        figures clears everything.
     */
    void SetMultiSelectionList(const QList<StickFigure*> &figures);

    /**
     * @brief Adds one figure (Shift+click); the current single selection seeds the set.
     */
    void AddToMultiSelection(StickFigure *figure);

    /**
     * @brief Removes one figure (Alt+click); dropping to 1 clears the whole selection.
     */
    void RemoveFromMultiSelection(StickFigure *figure);

    /**
     * @brief Drops the multi-selection: un-highlights, deletes the transform handle
     *        and the selection rectangle. Safe to call at any time.
     */
    void ClearMultiSelection();

    /**
     * @brief Commits the set of figures a drag rubber band currently encloses/touches to
     *        the actual selection state (called every mouseMove while band_dragging).
     * @param touched the figures the band hit this move.
     * @details Bridges the band gesture to the two selection representations: 2+ figures →
     *          multi-selection (transform box + pivot, via UpdateMultiSelectionUI); exactly
     *          1 → a normal SINGLE selection (that figure becomes CurrentStickFigure with its
     *          handle/bounding box and its layer-list row) — the missing case that made a
     *          band enclosing one figure appear to do nothing, since multi_selection with a
     *          single entry is not an "active" selection and was wiped on release; 0 → nothing
     *          selected. Keyboard-modifier click add/remove is handled separately at press.
     */
    void ApplyBandSelection(const QList<StickFigure*> &touched);

    /**
     * @brief Center of the multi-selection (mean of every selected stick endpoint) —
     *        the transform handle's position and the origin of every transformation.
     */
    QPointF MultiSelectionCenter() const;

    /**
     * @brief The selection as row indices into current_frame->stick_figures, for the
     *        undo machinery (pointers die on restore, indices survive).
     */
    QList<int> MultiSelectionIndices() const;

    /**
     * @brief Rebuilds the multi-selection from indices after an undo/redo restore.
     */
    void SetMultiSelectionByIndices(const QList<int> &indices);

    /**
     * @brief Deletes every selected figure (keeps the last remaining layer rule).
     *        One undo snapshot for the whole batch.
     */
    void DeleteMultiSelection();

    /**
     * @brief Repaints highlight, transform handle and layer-list rows.
     * @param recenter_pivot true when the SELECTION SET changed (the pivot snaps back
     *        to the selection centroid, per spec); false after operations, which keep
     *        or carry the pivot themselves.
     */
    void UpdateMultiSelectionUI(bool recenter_pivot = false);

    /**
     * @brief Adds every stick's endpoint handles of a figure to the scene.
     * @param S figure whose handles become visible (master gets a green origin handle).
     */
    void AddHandles(StickFigure*S);

    /**
     * @brief Forces a viewport repaint so the IK chain markers (drawForeground) refresh.
     *
     * The markers are painted in drawForeground(), which runs on every viewport paint —
     * so most edits refresh them for free. This just nudges a repaint for the cases where
     * only StickData geometry changed (e.g. a FABRIK drag) without otherwise dirtying the
     * viewport. Public because the IK-properties toggle (widget.cpp) also calls it.
     */
    void UpdateIkOverlay();

    /**
     * @brief Removes a figure's endpoint handles from the scene (without deleting them).
     * @param S figure whose handles are hidden.
     */
    void RemoveHandles(StickFigure*S);
public slots:
    /**
     * @brief Writes the current library buffer to a file.
     * @param file_name destination path.
     */
    void SaveLibrary(QString file_name);
signals:
    /**
     * @brief Fired when it's time to autosave; wired to MyMainWindow::AutoSave() in
     *        MyMainWindow::DoConnections(). Emitted from the undo_stack indexChanged
     *        hook every auto_save_interval operations.
     */
    void  AutoSaveTrigger();

    /**
     * @brief Fired when draw mode starts (true) or ends (false), however it ends
     *        (Esc/Enter, tool switch, or the panel button). widget.cpp uses it to keep
     *        the draw panel's Edit/Exit button label and state in sync.
     */
    void DrawModeChanged(bool active);


    // QWidget interface
protected:
    virtual void mouseReleaseEvent(QMouseEvent *event) override;
    virtual void mouseMoveEvent(QMouseEvent *event) override;
    virtual void mousePressEvent(QMouseEvent *event) override;
    virtual void tabletEvent(QTabletEvent *event) override;
    virtual void mouseDoubleClickEvent(QMouseEvent *event) override;
    virtual void keyPressEvent(QKeyEvent *event) override;
    virtual void keyReleaseEvent(QKeyEvent *event) override;
    /**
     * @brief Draws every committed IK chain's marker (dotted line start->target + a cross
     *        on the effector) on top of everything, like the endpoint handles. In the
     *        interactive view only: export/render/icons paint a separate render scene
     *        (ImageExport, StickFigure/Frame::UpdateIcon), never this view's foreground.
     */
    virtual void drawForeground(QPainter *painter, const QRectF &rect) override;
private:
    // ------- POLYGON tool state. Deliberately no StickData* is stored: the session ----
    // ------- target is always CurrentStick() (re-checked for type/view each use), so --
    // ------- an undo/frame-switch mid-session can never leave a dangling pointer ------

    bool poly_session_active = false; ///< true between the base line's completion and Enter/Esc/double-click/close-click
    bool poly_session_extends = false;///< session started on an existing polygon: clicks insert at the nearest edge instead of appending
    int poly_drag_idx = -1;           ///< vertex being dragged by the DRAWPOLY tool; -1 when none
    bool poly_vertex_drag = false;    ///< true while a Ctrl-drag of the selected vertices (NOTOOL/CLAMP) is in progress

    // ------- DRAW_IK tool state. Like the POLYGON session above, no StickData* is ------
    // ------- stored for the pending chain start: IkPendingStart() re-derives it from ---
    // ------- CurrentStick() each time, so an undo/frame-switch mid-gesture can't leave -
    // ------- a dangling pointer. The two preview items ARE genuinely transient scene ---
    // ------- state (there is no committed chain yet for paint() to draw from) ----------
    bool ik_drawing = false; ///< true between a chain's first click and its second (commit/reject) or Esc/tool-change (abort)
    QGraphicsLineItem *ik_preview_line = nullptr;  ///< dotted line, pending start handle to mouse
    QGraphicsPathItem *ik_preview_cross = nullptr; ///< cross following the mouse (previews the target position)

    /**
     * @brief The chain's not-yet-committed start stick during an DRAW_IK session.
     * @return CurrentStick() if ik_drawing is true, its role is IK_START and it has no
     *         target yet (still pending), else nullptr.
     */
    StickData *IkPendingStart();

    /**
     * @brief Removes the two transient preview items, if present.
     */
    void ClearIkPreviewItems();

    /**
     * @brief Aborts an in-progress chain: reverts the pending start back to IK_NONE
     *        (safe unconditionally — see the eligibility rule in FindIkStartCandidate(),
     *        which guarantees a stick can only reach IK_START through this one path) and
     *        clears the session state/preview. Esc and ChangeTool() both route here.
     */
    void CancelIkDrawing();

    /**
     * @brief "Near a stick's start handle" (P1-only proximity, not a whole-segment
     *        test) among sticks eligible to begin a NEW chain.
     * @param figure figure to search (always CurrentStickFigure() at call sites, same
     *        scoping DRAWPOLY/DRAWMULTI already use for their own first click).
     * @param scene_pos position to test.
     * @return the nearest eligible stick within the pick radius, or nullptr. Eligible
     *         means ik_role == IK_NONE (covers the spec's explicit "not IK_TARGET_ON"
     *         exclusion, and more broadly any stick already committed to another chain
     *         — reusing one would desync that chain's own role bookkeeping) and at
     *         least one child (spec: "has one or more child items").
     */
    StickData *FindIkStartCandidate(StickFigure *figure, const QPointF &scene_pos);

    /**
     * @brief "Near an existing chain's target handle" (P2-only proximity) among
     *        sticks already committed as a target, either polarity.
     * @param figure figure to search.
     * @param scene_pos position to test.
     * @return the nearest such stick within the pick radius, or nullptr.
     */
    StickData *FindIkTargetHandle(StickFigure *figure, const QPointF &scene_pos);

    /**
     * @brief The second click's validity test: a whole-segment proximity hit (the same
     *        metric FigureAtPoint() uses) that carries no IK role of its own yet and for
     *        which a valid chain start exists, anchored at the FIRST click's point and
     *        reached by an unbroken free-end-to-origin path (every link's P1 on the
     *        previous link's P2 — FABRIK's joint-sharing assumption). The start is
     *        RE-derived here (ResolveIkStartForTarget), not taken as `provisional_start`:
     *        when several sticks (e.g. two limbs) share the start anchor point, the first
     *        click cannot tell which branch the user means — the target's branch decides.
     * @param figure figure to search.
     * @param provisional_start the stick tentatively picked by the first click; only its
     *        P1 (the anchor point) and identity (as an allowed start) matter here.
     * @param scene_pos position to test.
     * @return the nearest valid target within the pick radius, or nullptr — "not valid,
     *         try again" per spec.
     */
    StickData *FindIkTargetCandidate(StickFigure *figure, StickData *provisional_start, const QPointF &scene_pos);

    /**
     * @brief Walk up from `target` and return the chain start anchored at ~`anchor_p1`,
     *        reachable by a geometrically continuous parent path (each link's P1 on its
     *        parent's P2). Resolves the branch ambiguity a shared start anchor creates
     *        (Group D follow-up): the branch that actually reaches the clicked target
     *        wins over the first click's arbitrary tie-break.
     * @param target the clicked effector.
     * @param anchor_p1 the first click's anchor (the provisional start's P1).
     * @param provisional_start allowed as a start even though the first click already
     *        marked it IK_START; every OTHER stick must be IK_NONE to qualify.
     * @return the resolved start (closest such ancestor to the target), or nullptr.
     */
    StickData *ResolveIkStartForTarget(StickData *target, const QPointF &anchor_p1, StickData *provisional_start);

    // ------- multi-selezione: stato privato -------------------------------------------
    QGraphicsEllipseItem *multi_handle = nullptr; ///< thin light-blue transform handle drawn on the pivot (any selection, 1+ figures)
    QGraphicsRectItem *selection_bbox = nullptr;  ///< thin light-blue no-fill bounding box of the current selection (spec)
    QGraphicsRectItem *selection_band = nullptr;  ///< dotted selection rectangle; survives until the selection breaks
    bool band_dragging = false;       ///< true while the selection rectangle is being spanned
    bool multi_dragging = false;      ///< true while the whole selection is being translated by drag
    bool pivot_dragging = false;      ///< true while the transform handle/pivot is being relocated (handle drag or Ctrl+drag on empty canvas)
    bool space_dragging = false;      ///< true while Space is held: the selection follows the cursor, dragged by its master stick(s)
    QPointF multi_center_buffer;      ///< pivot captured at gesture start (rotate/scale origin)
    // ---- transformation box (stile del gizmo della selezione pixel, applicato alle figure) ----
    int sel_box_gesture = 0;          ///< 0 nessuno, 1 scala (uniforme), 2 rotazione: drag di una maniglia della box
    int texture_arrow_gesture = 0;    ///< 0 nessuno, 1 coda (offset), 2 punta (angolo + scala): drag della freccia della texture
    bool texture_arrow_is_fill = true;///< quale delle due frecce è sotto trascinamento
    /**
     * @brief The arrow's tail, latched at press for the duration of a tip drag.
     *
     * The tail is what the tip's angle and length are measured FROM, and the anchor it sits on
     * depends on the texture's angle (TextureArrowAnchor() picks the side the arrow points to).
     * Recomputing it on every mouse move would therefore move the very origin the drag is being
     * measured against, the moment the drag crosses the stick's own direction.
     */
    QPointF texture_arrow_tail;

    /**
     * @brief Which texture arrow a scene click grabs.
     * @param scene the click position.
     * @param for_fill out: which of the two arrows was hit.
     * @param on_tip out: true for the arrow's tip (angle + scale), false for its tail (offset).
     * @return true on a hit.
     *
     * Deliberately the LOWEST-priority pick of the selection tool (spec): it reports a miss
     * whenever the click is also on a transformation-box handle or within grabbing distance
     * of any stick endpoint handle of the selection, so those two keep working exactly as
     * before wherever an arrow happens to lie over them.
     */
    bool HitTextureArrow(const QPointF &scene, bool *for_fill, bool *on_tip) const;

    /**
     * @brief Applies a texture-arrow drag to the current stick's fill or stroke brush.
     * @param scene the current cursor position.
     *
     * Rewrites one TextureXf: the tail sets the offset, the tip sets the angle and the scale
     * together (its distance from the tail IS the tile's painted size). The undo snapshot was
     * taken once, at press — this runs on every mouse move of the gesture.
     */
    void DragTextureArrow(const QPointF &scene);

    /**
     * @brief The arrow's geometry in SCENE units — the one form both brush kinds are converted
     *        to, so the drawing, the hit test and the drag are written once instead of twice.
     */
    struct ArrowXf {
        QPointF offset;      ///< displacement of the tail from TextureArrowAnchor(), scene units
        qreal   angle  = 0;  ///< direction in degrees, clockwise (QTransform's sign)
        qreal   length = 0;  ///< tail-to-tip distance in scene units, i.e. the scale
    };

    /**
     * @brief Scene length one unit of the brush's own placement measures.
     * @param brush the brush being pointed at.
     * @return the tile's longer side for a texture (so the arrow's length IS the painted size of
     *         one tile); the size of the box a gradient's 0..1 coordinates span otherwise.
     */
    qreal ArrowUnit(const QBrush &brush) const;

    /**
     * @brief Degrees the current stick's brush frame is turned from the scene's axes.
     * @return StickGraphicsItem::BrushFrameAngle() of the current stick, 0 when there is none.
     *
     * The rotational half of the same conversion ArrowUnit() does for lengths: without it a
     * stick-anchored placement, which is stored in the stick's own frame, is read as though it
     * were scene coordinates and a drag moves the paint along the stick's angle instead of along
     * the drag.
     */
    qreal ArrowFrameAngle() const;

    /**
     * @brief Reads the arrow's geometry out of whichever placement this brush kind keeps —
     *        QBrush::transform() for a texture, the gradient's own geometry for a gradient.
     */
    ArrowXf ReadArrow(const QBrush &brush) const;

    /**
     * @brief Writes the arrow's geometry back; the exact inverse of ReadArrow().
     */
    void WriteArrow(QBrush *brush, const ArrowXf &arrow) const;

    /**
     * @brief The current stick's fill or stroke brush, or an empty brush when there is none.
     * @param for_fill true for StickData::brush, false for the pen's paint.
     *
     * One accessor for the three places that need "whichever of the two brushes this control
     * acts on" — the arrow's geometry, its hit test and its drag.
     */
    QBrush CurrentTextureBrush(bool for_fill) const;

    /**
     * @brief The stick the texture arrows act on — the current one — or nullptr when there
     *        is no frame or nothing is selected.
     */
    StickData *TextureArrowStick() const;

    bool band_is_lasso = false;       ///< il gesto rubber band in corso è un lazo (senza selection_band scene item)
    QPolygonF band_poly;              ///< punti del lazo (coordinate di scena) accumulati durante il drag

    /**
     * @brief The figures the handle/pivot/bbox act on: the multi-selection when 2+ are
     *        selected, otherwise the single current figure (if any). Empty on an empty
     *        frame or a figure with no sticks.
     */
    QList<StickFigure*> SelectedFigures() const;

    /**
     * @brief True when SelectedFigures() is non-empty (a handle/pivot is meaningful).
     */
    bool HasActiveSelection() const { return !SelectedFigures().isEmpty(); }

    /**
     * @brief True for the tools that show a selection handle/bbox (pick/transform tools);
     *        the drawing/scene-wide tools hide them.
     */
    bool ToolUsesSelection() const;

    /**
     * @brief Default pivot position: the master-stick origin (P1) of the single selected
     *        figure, or the average of the masters' origins for a multi-selection (spec).
     */
    QPointF SelectionPivotDefault() const;

    /**
     * @brief The pivot the transform tools actually use: for a single figure the
     *        persistent handle (master origin + StickFigure::pivot_offset), for a
     *        multi-selection the group pivot (multi_pivot). This is what ROTATE/SCALE
     *        turn/scale around (bug 3), not the bare master origin.
     */
    QPointF SelectionPivotCurrent() const;

    /**
     * @brief Endpoint bounding box (union of every selected stick's P1/P2) of the current
     *        selection, in scene coordinates.
     */
    QRectF SelectionBoundingBox() const;

    /**
     * @brief (Re)draws the transform handle and selection bounding box for the current
     *        selection, or removes them when nothing is selected / the tool doesn't use a
     *        selection. Optionally re-centers the pivot on SelectionPivotDefault().
     */
    void UpdateSelectionDecorations(bool recenter_pivot = false);

    /**
     * @brief Removes the handle and bounding-box items from the scene, if present.
     */
    void RemoveSelectionDecorations();

    /**
     * @brief Transformation-box handle positions in scene coords: 0-3 corners
     *        TL,TR,BR,BL; 4-7 edge mids top,right,bottom,left; 8 rotation knob above
     *        the top edge. Sits on SelectionBoundingBox().
     */
    void SelectionHandles(QPointF out[9]) const;

    /**
     * @brief Which transformation-box handle a scene point hits (within PolyPickRadius):
     *        0-7 a scale handle, 8 the rotation knob, -1 none.
     */
    int HitSelectionHandle(const QPointF &scene) const;

    /**
     * @brief Draws the shared transform-box gizmo: 8 white square scale handles (pts 0-7)
     *        and a round rotation knob (pt 8) with a stem to the top-mid handle (pt 4), in
     *        the light-blue cosmetic style used by BOTH the pixel-selection gizmo and the
     *        stickfigure transformation box.
     * @param painter the drawForeground painter (scene transform already applied).
     * @param pts 9 handle centres in SCENE coordinates (from GizmoHandles()/SelectionHandles()).
     * @param draw_outline also stroke the box quad (pts 0-3). true for the pixel gizmo (its
     *        box rotates with the gizmo, so it has no separate outline item); false for the
     *        stickfigure box, whose outline is already the selection_bbox scene item — passing
     *        true there would double-draw it. This bool is the ONLY difference between the two
     *        call sites; everything else (colour, handle size, shapes) is shared.
     */
    void DrawTransformHandles(QPainter *painter, const QPointF pts[9], bool draw_outline);

    /**
     * @brief True when the transformation box is shown and its handles are actionable —
     *        the NOTOOL selection tool with an active selection.
     */
    bool SelectionHandlesActive() const;

    /**
     * @brief Snapshot + per-figure buffers for a rotate gesture (shared by the ROTATE tool
     *        and the transformation-box rotation knob). Pivot = SelectionPivotCurrent().
     */
    void BeginSelectionRotate();

    /**
     * @brief Snapshot + per-figure buffers for a scale gesture (shared by the SCALE tool
     *        and the transformation-box scale handles). Pivot = SelectionPivotCurrent().
     */
    void BeginSelectionScale();

    /**
     * @brief Rotates SelectedFigures about multi_center_buffer by the drag increment
     *        (starting_coord -> coord); the ROTATE-tool move body, reused by the box knob.
     */
    void ApplySelectionRotate();

    /**
     * @brief Uniformly scales SelectedFigures about multi_center_buffer by the drag ratio
     *        (dist(coord)/dist(starting_coord)); the SCALE-tool move body, reused by the box.
     */
    void ApplySelectionScale();

    /**
     * @brief The figure owning a stick close enough to the position (segment distance
     *        within the pen width or the pick radius), or nullptr for empty canvas.
     */
    StickFigure *FigureAtPoint(const QPointF &scene_pos);

    /**
     * @brief The stick a DRAWPOLY/DRAWMULTI session is feeding, or nullptr.
     * @param wanted_type the StickType the active tool edits (POLYGON or MULTILINE).
     * @return CurrentStick() if it matches and has a live view, else nullptr.
     */
    StickData *PolySessionStick(qint32 wanted_type);

    /**
     * @brief Removes one stick (and its subtree) from the model, the scene and the
     *        frame's z-order list — the Delete-key path for a stick the code (not the
     *        user) picked: DRAWMULTI's Esc-cancel and the delete-last-vertex rule.
     * @param stick stick of the current figure to remove; must be non-null.
     */
    void DeleteWholeStick(StickData *stick);

    /**
     * @brief Zoom-aware pick radius for vertices/edges: handle_size screen pixels
     *        converted to scene units, so "very close" feels the same at any zoom.
     * @return the radius in scene coordinates.
     */
    float PolyPickRadius();

    /**
     * @brief Ends a DRAWPOLY point-adding session: clears the session flags and the
     *        next-vertex preview, repaints the polygon.
     */
    void FinishPolygonSession();

    /**
     * @brief Clears every polygon vertex selection in the current frame (the Ctrl
     *        multi-select of the NOTOOL/CLAMP tools).
     */
    void DeselectAllVertices();

    /**
     * @brief Finds the POLYGON/MULTILINE stick of the current frame with a vertex
     *        within the pick radius of a position.
     * @param scene_pos position to test.
     * @param vertex_idx out: (virtual) index of the found vertex.
     * @param type_filter restrict to one StickType, or -1 for both editable types.
     * @return the stick, or nullptr if no vertex is close enough.
     */
    StickData *FindPolygonVertex(const QPointF &scene_pos, int *vertex_idx, qint32 type_filter = -1);

    /**
     * @brief Finds the POLYGON/MULTILINE stick of the current frame with an edge
     *        within the pick radius of a position (used to insert vertices).
     * @param scene_pos position to test.
     * @param insert_idx out: (virtual) insertion index for a vertex on that edge.
     * @param type_filter restrict to one StickType, or -1 for both editable types.
     * @return the stick, or nullptr if no edge is close enough.
     */
    StickData *FindPolygonEdge(const QPointF &scene_pos, int *insert_idx, qint32 type_filter = -1);
    /**
     * @brief Performs one click of the 2-click drawing gesture.
     * @param point click position in scene coordinates.
     * @param mode the active DRAW* tool id (decides the resulting StickData type).
     *
     * First click starts a stick (StickFigure::StartDrawing() + AttachView()); second
     * click finishes it (EndDrawing()) and files it in the frame's z-order list.
     */
    void DrawCmd(QPointF* point, int mode);

    // ---- Freehand draw-mode internals ------------------------------------------------
    QList<QImage> draw_undo;      ///< per-stroke canvas snapshots (session-local undo chain)
    int draw_undo_idx = -1;       ///< current position in draw_undo (-1 = empty)
    bool draw_stroking = false;   ///< true between brush press and release
    QPointF draw_last_pos;        ///< last stamped scene position, for stroke interpolation
    double draw_dab_carry = 0;    ///< canvas distance walked since the last dab, carried across segments so spacing is measured along the whole stroke instead of restarting at every mouse-move
    QLineF draw_orig_line;        ///< the stick's skeleton (the 2-click line) stashed on entry, restored on exit so the freehand keeps a normal-stick origin/rotation
    QTransform draw_disp;         ///< canvas display transform, mirrors paint(): scene = draw_disp.map(pixel - canvasCenter). The canvas is painted in the stick's ROTATED frame, so the drawing is born at the stick's position/rotation and needs no re-rotation on exit
    QTransform draw_disp_inv;     ///< inverse of draw_disp, for scene -> canvas-pixel mouse mapping
    double draw_theta = 0;        ///< the stick's rotation (its img_angle) during the session

    /** @brief Pressure multiplier centred on 1.0 at a "normal" (half-way) pen press: the
     *  dialled-in brush size/opacity comes out exactly at draw_pressure_val==0.5, a harder
     *  press scales above it (up to 2x at full pressure) and a lighter one scales below. */
    double PressureFactor() const;
    /** @brief The loose draw_brush_* fields gathered into the BrushStroke that BrushDab() takes. */
    BrushStroke DrawBrush() const;
    /** @brief Stamps one brush dab (tinted/eraser) onto draw_canvas at a canvas-pixel point. */
    void StampBrush(const QPointF &canvas_px);
    /** @brief Stamps the brush along a scene-space segment (interpolated by brush spacing). */
    void PaintStrokeSegment(const QPointF &from_scene, const QPointF &to_scene);
    /** @brief Truncates any redo tail and pushes a copy of the current canvas onto draw_undo. */
    void PushDrawUndo();

    // ---- Pixel-selection internals ---------------------------------------------------
    int     draw_sel_gesture = 0; ///< 0 none, 1 new-rect, 2 new-lasso, 3 move, 4 scale, 5 rotate
    int     draw_sel_handle = -1; ///< scale-handle index (0..7) during a scale gesture
    int     draw_sel_op = 0;      ///< pending new-selection op: 0 replace, 1 add, 2 subtract
    QPointF draw_sel_p0;          ///< gesture anchor, canvas px (rubber-band start / move-prev)
    QPolygonF draw_lasso;         ///< lasso points, canvas px
    QPointF draw_sel_t0;          ///< gizmo translation at gesture start
    double  draw_sel_r0 = 0;      ///< gizmo rotation at gesture start
    double  draw_sel_sx0 = 1, draw_sel_sy0 = 1; ///< gizmo scale at gesture start
    double  draw_sel_ang0 = 0;    ///< pointer angle (deg) at rotate-gesture start

    QPointF SceneToCanvas(const QPointF &scene) const; ///< scene coord -> raw canvas pixel
    QPointF CanvasToScene(const QPointF &canvas) const; ///< raw canvas pixel -> scene coord
    QTransform PixelGizmo() const;         ///< current gizmo transform (about bounds centre)
    void GizmoHandles(QPointF out[9]) const; ///< 8 box handles + rotation handle, canvas px (post-gizmo)
    int  HitPixelHandle(const QPointF &scene) const; ///< handle idx under a scene point, 8=rotate, -1 none
    void LiftPixelFloat();                 ///< cut selected pixels out of the canvas into draw_float
    void CommitPixelFloat();               ///< stamp draw_float (transformed) back, push a draw-undo step
    void SetPixelSelection(const QRegion &r, int op); ///< replace/add/subtract, recompute bounds+reset gizmo
    /** @brief Pixel-selection mouse-down: starts a new marquee (rect/lasso, Shift=add/Alt=subtract)
     *  or a move/scale/rotate gesture when a handle/box is grabbed. Driven by mousePressEvent. */
    void DrawSelPress(const QPointF &scene, Qt::KeyboardModifiers mods);
    /** @brief Pixel-selection mouse-drag: extends the marquee or applies the active gizmo gesture.
     *  Driven by mouseMoveEvent while draw_sel_gesture is set. */
    void DrawSelMove(const QPointF &scene);
    /** @brief Pixel-selection mouse-up: commits a freshly-drawn rect/lasso into the selection;
     *  a move/scale/rotate leaves its float active. Driven by mouseReleaseEvent. */
    void DrawSelRelease(const QPointF &scene);
    /** @brief Tight bounding rect of the non-transparent pixels of an image (empty if none). */
    static QRect OpaqueBounds(const QImage &img);

    /**
     * @brief Cleanup run when leaving a tool (called by SetTool before switching).
     *
     * Mostly cancels a half-finished draw so an unfinished stick doesn't survive a tool
     * change, and frees the image draw buffer when leaving DRAWIMG.
     */
    void ChangeTool();

    // QWidget interface


protected:
    virtual void wheelEvent(QWheelEvent *event) override;
};

#endif // MYVIEW_H
