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
     *        the index is clamped to the interior range.
     * @return true on success.
     */
    bool AddPolygonPoint(const QPointF &scene_point, int insert_idx = -1);

    /**
     * @brief MULTILINE only: extends the path — the current P2 becomes an interior
     *        vertex and the new point becomes the path's end (my_line.p2()).
     * @param scene_point the new endpoint.
     * @return true on success.
     */
    bool AppendMultilineVertex(const QPointF &scene_point);

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
     *        POLYGON edges include the closing edge; MULTILINE edges are the open
     *        chain only.
     * @param scene_pos position to test.
     * @param dist_out optional out: distance from scene_pos to that edge, scene units.
     * @return virtual insertion index (see AddPolygonPoint()).
     */
    int NearestEdgeInsertIndex(const QPointF &scene_pos, float *dist_out = nullptr);

    /**
     * @brief Shows/moves the transient next-vertex preview that follows the mouse
     *        during a DRAWPOLY session; paint() renders it as one more vertex.
     * @param scene_pos current mouse position.
     * @param insert_idx where the vertex would be inserted (see NearestEdgeInsertIndex).
     */
    void SetPolygonPreview(const QPointF &scene_pos, int insert_idx);

    /**
     * @brief Hides the next-vertex preview and repaints.
     */
    void ClearPolygonPreview();

    /**
     * @brief SceneVertices() plus the preview vertex (when active) at its insertion
     *        index — the exact chain paint() renders.
     */
    QPolygonF ScenePolygon() const;

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
     * @brief Draws the red/orange vertex preview squares over `verts` (shared by the
     *        POLYGON and MULTILINE paint branches).
     */
    void PaintVertexSquares(QPainter *painter, const QPolygonF &verts);

    /**
     * @brief Builds the raw bounding rect from the segment and shape/image extents.
     * @return rect before pen/handle padding is applied.
     */
    QRectF ConstructBr();
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
