#ifndef STICK_H
#define STICK_H
#include <QGraphicsItem>
#include <math.h>
#include <style.h>
#include <QPainter>
#include <QGraphicsScene>
#include <QLabel>
#include <QSet>
#include <stickdata.h>

#define STEPCHILD -1    ///< legacy marker for "attached to the parent's origin" (kept for old call sites)
#define STKTYPE 90000   ///< QGraphicsItem::type() id, lets scene code recognize stick items (see SceneRemover)
#define MAXDEPTH 99.9f  ///< upper clamp for the user-editable depth (Z) of a stick
#define MINDEPTH -99.9f ///< lower clamp for the user-editable depth (Z) of a stick

extern QLabel *dbg_lbl;
class StickFigure;

/**
 * @brief The view: paints a StickData, handles mouse-driven interaction, owns nothing
 *        persistent.
 *
 * REFACTOR_PLAN.md Phase 2c completes the split Phase 1a started: `StickGraphicsItem`
 * (formerly `stick`) no longer derives from `StickData` (that bridge is gone) — it
 * holds a non-owning `StickData*` (`data`) and adds only what genuinely needs a
 * `QGraphicsScene` or reflects a live UI session: endpoint handles, selection/highlight
 * state, and the geometry/angle caches used while dragging.
 * `FramePresenter::AttachView()`/`DetachView()` (framepresenter.h) are the only code
 * that constructs or destroys one for a stick actually on screen — everywhere else
 * (undo, copy/paste, the library) works with `StickData` alone and never touches this
 * class at all.
 */
class StickGraphicsItem:public QGraphicsItem
{

public:
    enum { Type = STKTYPE };
//-------- methods ----------//

    /**
     * @brief Qt item-type id so scene iteration code can tell sticks from handles/frames.
     * @return always STKTYPE.
     */
    int type() const override { return Type; }

    /**
     * @brief Wraps a StickData for display; the data is not copied or owned.
     * @param stick_data record to present; must outlive this view.
     *
     * Only constructor since Phase 2c — call sites that used to build a `stick` and
     * its data together now build (or `Clone()`) a `StickData` first, then wrap it via
     * `FramePresenter::AttachView()`.
     */
    StickGraphicsItem(StickData *stick_data);

    /**
     * @brief Frees the endpoint handles. Does NOT touch `data` — non-owning.
     */
    ~StickGraphicsItem();

    StickData *data = nullptr; ///< non-owning; the persistent record this view presents

    /**
     * @brief Empties this stick's rows from the image list widget.
     *
     * Must be called by whoever detaches a LIVE (on-screen) IMAGE stick, right before
     * destroying its view; scratch/buffer deletions must not call it (they have no
     * view to call it on). Replaces the flag-guarded cleanup the destructor performed
     * before Phase 1b.
     */
    void RemoveImagesFromListWidget();

    // GetP2() è stato eliminato nel safety sweep: nessun chiamante rimasto, e passava
    // gradi (QLineF::angle()) a cos()/sin() che si aspettano radianti

    /**
     * @brief Scene bounding box of the item.
     * @return the cached rect (br), kept "positive" by UpdateBr().
     */
    QRectF boundingRect() const ;

    QRectF br;                   ///< cached bounding rect served to the scene by boundingRect()

    /**
     * @brief Scales the current image variant to the stick's length and user scale factors.
     * @return the scaled image, ready to be painted.
     */
    QImage CalcImg();

    /**
     * @brief The size CalcImg() scales to, without scaling anything.
     * @return the painted size in scene units; empty when there is no usable image.
     */
    QSizeF CalcImgSize() const;

    /**
     * @brief Paints the stick according to its StickData (type, pen, brush, image).
     */
    void paint(QPainter *painter, const QStyleOptionGraphicsItem *option, QWidget *widget);

    /**
     * @brief Recomputes bounding rect and angle, then schedules a scene repaint.
     * @param mode 1 while dragging (merges old+new rects to avoid ghosting), 0 otherwise.
     */
    void Refresh(int mode);

    /**
     * @brief Replaces the stick's segment.
     * @param line new segment; copied into `data->my_line`.
     */
    void SetLine(QLineF* line);

    /**
     * @brief Finalizes an interactive rotation, refreshing this stick and its children.
     */
    void EndRotation();

    /**
     * @brief Interactive rotation: points the stick towards the dragged position.
     * @param point mouse position in scene coordinates.
     */
    void Rotate(QPointF* point);

    /**
     * @brief Interactive manipulation: moves the free end, dragging children along.
     * @param point mouse position in scene coordinates.
     */
    void Manipulate(QPointF* point);

    

    /**
     * @brief Tells whether this stick's data is its figure's master (tree root).
     * @return true when the owning figure's master_stick is this stick's data.
     */
    bool IsMaster() const;
    // ------- POLYGON / MULTILINE vertex editing (all positions in SCENE coordinates; -
    // ------- the normalized-local storage conversion happens inside, via -------------
    // ------- StickData::PolygonTransform()). Vertex indices are VIRTUAL: for a -------
    // ------- POLYGON they index polygon_points directly; for a MULTILINE index 0 is --
    // ------- my_line.p1(), the last index is my_line.p2(), and everything between ----
    // ------- maps to polygon_points[i-1] (plus the parallel width/curvature lists). --

    /**
     * @brief The editable vertex chain in scene coordinates (no preview vertex).
     * @return POLYGON: every polygon_points entry mapped; MULTILINE: P1 + mapped
     *         interior vertices + P2; empty for any other type.
     */
    QPolygonF SceneVertices() const;

    /**
     * @brief Adds a vertex to a POLYGON/MULTILINE stick and repaints.
     * @param scene_point click position in scene coordinates.
     * @param insert_idx virtual insertion index; -1 (default) appends (POLYGON) /
     *        inserts before P2 (MULTILINE). MULTILINE endpoints cannot be displaced:
     *        the index is clamped to the interior range, and the new vertex splits the
     *        edge that ends in vertex insert_idx (StickData::vertex_parents).
     * @return true on success.
     */
    bool AddPolygonPoint(const QPointF &scene_point, int insert_idx = -1);

    /**
     * @brief MULTILINE only: extends the path — the current P2 becomes an interior
     *        vertex and the new point becomes the path's end (my_line.p2()).
     * @param scene_point the new endpoint.
     * @param cap StickData::VertexCap of the new endpoint (the old one keeps its own).
     * @return true on success.
     */
    bool AppendMultilineVertex(const QPointF &scene_point, qint32 cap = StickData::CAP_ROUND);

    /**
     * @brief MULTILINE only: adds a branch vertex hanging from an existing vertex, without
     *        touching P1/P2 (the main line extends through AppendMultilineVertex()).
     * @param from_idx virtual index of the vertex the new one hangs from; P2 is refused.
     * @param scene_point the new vertex, in scene coordinates.
     * @return the new vertex's virtual index (it stays valid across later appends), or -1.
     */
    int BranchMultilineVertex(int from_idx, const QPointF &scene_point);

    /**
     * @brief Finds the vertex nearest to a scene position.
     * @param scene_pos position to test.
     * @param min_dist_out optional out: distance to the returned vertex, scene units.
     * @return virtual vertex index, or -1 if the stick has no editable vertices.
     */
    int SelectPolygonPoint(const QPointF &scene_pos, float *min_dist_out = nullptr);

    /**
     * @brief Moves one vertex to a scene position and repaints. Moving a MULTILINE
     *        endpoint rebuilds the stick's frame (my_line/img_angle) while keeping
     *        every other vertex fixed in the scene.
     * @param index virtual vertex to move.
     * @param scene_pos new position in scene coordinates.
     * @return true on success.
     */
    bool EditPolygonPoint(int index, const QPointF &scene_pos);

    /**
     * @brief Removes one vertex and repaints. Removing a MULTILINE endpoint promotes
     *        its neighbor to being the new P1/P2 (frame rebuild, other vertices stay
     *        put). The caller enforces any minimum-count rule (e.g. MyView deletes the
     *        whole stick when a MULTILINE would drop below 2 vertices).
     * @param index virtual vertex to remove.
     * @return true on success.
     */
    bool DeletePolygonPoint(int index);

    /**
     * @brief Where a new vertex should be inserted so it lands on the nearest edge.
     *        POLYGON edges include the closing edge; MULTILINE edges are its tree's
     *        (parent, child) pairs.
     * @param scene_pos position to test.
     * @param dist_out optional out: distance from scene_pos to that edge, scene units.
     * @return virtual insertion index (see AddPolygonPoint()).
     */
    int NearestEdgeInsertIndex(const QPointF &scene_pos, float *dist_out = nullptr);

    /**
     * @brief Shows/moves the transient next-vertex preview that follows the mouse
     *        during a DRAWPOLY session; paint() renders it as one more vertex.
     * @param scene_pos current mouse position.
     * @param insert_idx where the vertex would be inserted (see NearestEdgeInsertIndex);
     *        ignored for a MULTILINE, whose preview always goes after the last vertex.
     * @param preview_parent MULTILINE only: the vertex the preview hangs from; -1 = P2.
     * @param preview_cap MULTILINE only: the StickData::VertexCap the click would give it.
     */
    void SetPolygonPreview(const QPointF &scene_pos, int insert_idx, int preview_parent = -1,
                           qint32 preview_cap = StickData::CAP_ROUND);

    /**
     * @brief Hides the next-vertex preview and repaints.
     */
    void ClearPolygonPreview();

    /**
     * @brief SceneVertices() plus the preview vertex (when active) at its insertion
     *        index — the exact chain paint() renders.
     */
    QPolygonF ScenePolygon() const;

    /**
     * @brief Re-anchors a pattern/gradient/image brush so it honours
     *        StickData::brush_follows_stick, whatever coordinate frame this stick type
     *        paints in.
     * @param source the brush as stored on the record (fill, or a pen's paint).
     * @return the same brush with its transform adjusted; @p source unchanged when there
     *         is nothing to re-anchor.
     *
     * A brush paints in the painter's CURRENT logical frame, and the stick types disagree
     * about what that is: RECT/CIRCLE/TRAPEZOID/TAPER/TEXT/IMAGE fill inside the stick's
     * own translated+rotated frame, so a texture or gradient follows the stick for free,
     * while LINE/POLYGON/MULTILINE draw straight in scene coordinates, where it does not.
     * This is why a gradient-stroked LINE looked pinned to the scene while a gradient-filled
     * RECT rotated correctly.
     *
     * Both directions are a single pre-multiplication of the brush transform by the stick's
     * local frame, or by its inverse. How much of that frame goes in depends on the brush,
     * and the difference is not cosmetic:
     * - a **texture** lives in logical coordinates, so it needs the WHOLE frame
     *   (translation included) to move and turn with the stick;
     * - a **QGradient::ObjectMode gradient** that must FOLLOW the stick is already anchored to
     *   the shape's bounding box, so it needs the ROTATION ONLY, and taken **about the centre
     *   of that box**. Composing the translation as well slides the gradient off the shape and
     *   the fill collapses to one flat colour; rotating about the box's ORIGIN instead of its
     *   centre is the same mistake in miniature — the transform of an ObjectMode gradient acts
     *   in the box's own 0..1 space, whose origin is its top-left CORNER, so a corner-pivoted
     *   rotation swings the gradient axis partly (or, at -90°, entirely) outside the shape.
     *   Both were measured, not reasoned about.
     * - a **QGradient::ObjectMode gradient that must NOT follow the stick** cannot be handled
     *   by a transform at all, in either direction: ObjectMode *means* "relative to the painted
     *   shape's bounding box", so the box travels with the stick and the gradient with it no
     *   matter what is composed. Compensating the rotation fixes the gradient's DIRECTION and
     *   leaves its POSITION riding along, which is exactly how the bug looked. Such a gradient
     *   is therefore REBUILT as a LogicalMode one anchored to the scene rect — see
     *   SceneAnchoredGradient() in stick.cpp. The returned brush is then a different brush from
     *   @p source, not merely a re-transformed one; it is the only case where that happens.
     *
     * A solid colour and Qt::NoBrush are returned untouched: they have no geometry to anchor.
     */
    QBrush AlignBrush(const QBrush &source) const;

    /**
     * @brief Where a texture brush's tile lands, mapped from the tile's own pixels to scene
     *        coordinates.
     * @param source the brush as stored on the record (fill, or a pen's paint).
     * @return the full tile-pixel -> scene mapping, i.e. the brush transform composed with
     *         whatever part of the stick's frame AlignBrush() decided the brush paints in.
     *
     * The one place that knows where a texture actually ends up on the canvas. Two callers
     * need that, for opposite reasons: SingleInstanceClip() maps the tile's rectangle FORWARD
     * to find the one instance it must keep, and MyView's texture arrow handle maps the
     * user's drag BACK to figure out which scale/angle/offset it asks for.
     */
    QTransform TextureToScene(const QBrush &source) const;

    /**
     * @brief How much a brush's own placement frame is turned relative to the SCENE, in degrees
     *        (QTransform's clockwise-positive convention).
     * @param data the stick record the brush belongs to.
     * @return the stick's frame rotation when the paint is anchored to the stick, 0 when it is
     *         anchored to the scene.
     *
     * The angle half of what TextureToScene() answers for a texture's position, and the piece a
     * GRADIENT has no TextureToScene() to ask (its numbers are in a normalized box, not in tile
     * pixels). One value serves both, because AlignBrush() ends up applying the same rotation to
     * either: the whole local frame for a texture, and its rotation about the box centre for an
     * ObjectMode gradient (ObjectGradientTurn()) — and for the types that fill in scene
     * coordinates it is the box itself that is already unturned. So anything that has to convert
     * between a brush's stored placement and the canvas — MyView::ReadArrow()/WriteArrow(), i.e.
     * the texture arrow handle — needs exactly this and nothing else.
     */
    static qreal BrushFrameAngle(const StickData *data);

    /**
     * @brief The single tile instance a non-tiling texture fill paints, in SCENE coordinates.
     * @param source the fill brush.
     * @return the tile's rectangle mapped through TextureToScene(), or an EMPTY path when the
     *         brush tiles, isn't a texture, or has no tile — i.e. when there is nothing to clip.
     *
     * Qt has no non-repeating brush style, so StickData::texture_tiled is enforced by clipping
     * rather than by the brush: the shape is still filled with the ordinary tiling brush and
     * every repeat except the one the texture transform places is clipped away. Returns the path
     * instead of touching the painter so the caller can decide the painter state in one place
     * (and so it can be tested without one).
     */
    QPainterPath SingleInstancePath(const QBrush &source) const;

    /**
     * @brief How opaque a fill should be painted, over and above whatever alpha its own paint
     *        carries.
     * @param source the fill brush.
     * @return 1.0 for everything except a texture, whose opacity is the alpha of
     *         QBrush::color().
     *
     * A texture brush ignores its QBrush::color() when rendering — Qt documents setColor() as
     * having no effect on one — which leaves that colour's ALPHA as a field the brush already
     * has, already streams, and otherwise never uses. So the opacity sliders write there for a
     * texture exactly as they write into the colour itself for a solid fill, and there is one
     * opacity concept rather than two: **the brush colour's alpha is the paint's opacity** — for
     * a solid it IS the colour, for a texture it multiplies the tile.
     *
     * A gradient is excluded because it needs nothing here: its opacity is in its stops (see
     * ReadPaintOpacity(), stickdata.h), which Qt already honours when it renders one — fading it a
     * second time with the painter would square it.
     */
    static qreal FillOpacity(const QBrush &source);

    /**
     * @brief Paints a shape with a fill and an outline, splitting the two into separate passes
     *        when they must not share the painter's state.
     * @param painter the painter, in SCENE coordinates (the clip is stored in device space at
     *        the setClipPath() call, so the local frame installed below cannot disturb it); its
     *        pen is already the stick's and is left as it was found.
     * @param fill the fill brush, as stored on the record.
     * @param shape the shape to paint, in the frame this stick type paints in — the stick's own
     *        translated+rotated one for RECT/CIRCLE/TRAPEZOID/TAPER, scene coordinates for
     *        POLYGON. Which of the two it is comes from the type, not from an argument, so the
     *        answer cannot drift from the one AlignBrush() uses.
     *
     * A single Qt draw call paints a shape's fill and its outline together, so anything applied
     * for the fill's sake — the single-instance clip, a fill opacity — lands on the outline too:
     * the clip chops it into a fragment, the opacity fades it. Hence two passes when (and only
     * when) either is in force: the fill clipped/faded and penless, then the outline plain and
     * brushless. Every other case is one pass, exactly as before.
     *
     * A shape rather than a callable because the outline is now also something to be WALKED, not
     * only drawn: a brush border (StickData::stroke_brush) stamps a tip along this very path.
     */
    void FillAndStroke(QPainter *painter, const QBrush &fill, const QPainterPath &shape) const;

    /**
     * @brief Draws the outline of @p shape the way this stick's record asks for it: stamped with
     *        the border brush when it has one, traced by the pen otherwise.
     * @param painter the painter, in SCENE coordinates.
     * @param shape the outline, in the same frame FillAndStroke() takes it.
     * @return true when the brush border painted it, false when the caller must draw it itself.
     *
     * Called by FillAndStroke() for the shape types and directly by paint()'s LINE and MULTILINE
     * branches, which have an outline but no fill to go with it. Never stamps during an onion
     * render — a ghost frame is flat grey by definition.
     */
    bool StrokeWithBrush(QPainter *painter, const QPainterPath &shape) const;

    //-------- variables----------//
    /**
     * @brief True while MyView::UpdateOnionSkins() is rendering ghost frames: paint()
     *        switches to a gray pen and grayscale images. One render at a time exists
     *        app-wide, hence static — formerly the free global `onion_render`
     *        (stick.cpp), now scoped to the class whose paint() reads it.
     */
    static bool onion_render;
    bool highlight = false;          ///< visual emphasis when the owning figure is selected
    bool selected = false;           ///< yellow endpoint markers when this is the current stick
    int rotation_spinbox_buffer = 0;   ///< last value pushed to the rotation spinbox (UI echo suppression)
    int scale_spin_box_buffer = 100;    ///< last value pushed to the scale spinbox (UI echo suppression)
    float scale_buffer = 1;           ///< stick length at drag start, reference for interactive scaling
    float width_buffer = 1;           ///< pen width at drag start, reference for interactive scaling
    float angle_buffer = 0;           ///< usato per disegnare lo stick nei Refresh
    float angle_buffer2 = 0;          ///< usato nella rotazione di tutto lo stickfigure
    QRectF img_rect;                  ///< legacy image placement rect (derived, not serialized)
    QGraphicsEllipseItem* stick_handle[2]; ///< red/green endpoint handles shown on the scene
    StickFigure* my_stick_figure = nullptr; ///< owning figure; needed by IsMaster()

    // ------- POLYGON UI-only state: never serialized, so vertex squares and the -------
    // ------- preview vertex show on the live canvas but not in onion skins, icons -----
    // ------- or exports (those all render from view-less clones) ----------------------
    QSet<int> selected_vertices;      ///< vertices picked with Ctrl(+Shift) on the select tool; drawn orange
    bool poly_preview_active = false; ///< true while the next-vertex preview follows the mouse
    int poly_preview_idx = 0;         ///< insertion position of the preview vertex
    int poly_preview_parent = -1;     ///< MULTILINE: vertex the preview hangs from; -1 = P2
    qint32 poly_preview_cap = StickData::CAP_ROUND; ///< MULTILINE: VertexCap the preview is drawn with
    QPointF poly_preview_point;       ///< preview vertex, normalized stick-local coords

    /**
     * @brief Recomputes the cached bounding rect.
     * @param mode 1 merges the previous rect (anti-ghosting while dragging), 0 replaces it.
     * @return the new cached rect.
     */
    QRectF UpdateBr(int mode);

    /**
     * @brief Rotates the stick by an absolute angle around its origin (figure rotation).
     * @param angle target angle in degrees, relative to angle_buffer2.
     */
    void Rotate(float angle);

    /**
     * @brief Scales length (and proportionally the pen width) around the origin.
     * @param scale_factor multiplier relative to scale_buffer/width_buffer.
     */
    void Scale(float scale_factor);

    /**
     * @brief Appends an image variant to an IMAGE stick and shows it in the list widget.
     * @param img_to_add image to take ownership of.
     * @param name display name for the list widget.
     */
    void AddImage(QImage *img_to_add, QString name);

    /**
     * @brief Rebuilds the image list widget from this stick's image variants.
     */
    void PopulateImageListWidget();

    /**
     * @brief Removes one image variant (and its list widget entry).
     * @param idx index into stick_img_list/img_name_list.
     */
    void RemoveImgFromList(int idx);

    /**
     * @brief Distance metric used to pick the stick closest to a click.
     * @param p point to test.
     * @param l candidate stick segment.
     * @return distance between p and l's free end (P2).
     */
    float MeasureDistanceFromLine(QPointF p, QLineF l);
private:
    //-------- methods----------//

    /**
     * @brief Caches the segment's current angle into angle_buffer.
     */
    void  CalcAngle();

    /**
     * @brief MULTILINE: re-anchors the local frame on new endpoints while keeping the
     *        interior vertices fixed in the scene (recomputes my_line, img_angle and
     *        re-normalizes polygon_points).
     */
    void RebuildMultilineFrame(const QPointF &new_p1, const QPointF &new_p2);

    /**
     * @brief MULTILINE: writes a whole virtual vertex list (scene coordinates) back —
     *        first/last become my_line's P1/P2, the rest the normalized interior.
     */
    void SetMultilineVertices(const QPolygonF &scene_verts);

    /**
     * @brief Draws the red/orange vertex preview squares over `verts` (shared by the
     *        POLYGON and MULTILINE paint branches).
     */
    void PaintVertexSquares(QPainter *painter, const QPolygonF &verts);

    /**
     * @brief Builds the raw bounding rect from the segment and shape/image extents.
     * @return rect before pen/handle padding is applied.
     */
    QRectF ConstructBr();
    /// @brief ConstructBr() of an image, shape or LINE before the Stick Transform moves it.
    QRectF UntransformedBr();
};

/**
 * @brief Removes every item except the canvas frame (my_rect) from a scene.
 * @param scene_to_clear scene to empty without deleting the items.
 */
void SceneRemover(QGraphicsScene *scene_to_clear);

/**
 * @brief Computes where an IMAGE stick's picture sits relative to its segment.
 * @param l stick segment.
 * @param s source image size.
 * @return placement rect centered on the segment origin.
 */
QRectF CalcImgRect(QLineF l,QSizeF s);
#endif // STICK_H
