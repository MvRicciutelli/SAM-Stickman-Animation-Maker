#ifndef STICK_H
#define STICK_H
#include <QGraphicsItem>
#include <math.h>
#include <style.h>
#include <QPainter>
#include <QGraphicsScene>
#include <QLabel>
#include <stickdata.h>

#define STEPCHILD -1    ///< legacy marker for "attached to the parent's origin" (kept for old call sites)
#define STKTYPE 90000   ///< QGraphicsItem::type() id, lets scene code recognize stick items (see sceneRemover)
#define MAXDEPTH 99.9f  ///< upper clamp for the user-editable depth (Z) of a stick
#define MINDEPTH -99.9f ///< lower clamp for the user-editable depth (Z) of a stick

extern bool onionRender;
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

    /**
     * @brief Endpoint of a segment measured trigonometrically.
     * @param line segment to measure.
     * @return P2 recomputed from P1 + length/angle.
     */
    QPointF getP2(QLineF* line);

    /**
     * @brief Scene bounding box of the item.
     * @return the cached rect (br), kept "positive" by updateBr().
     */
    QRectF boundingRect() const ;

    QRectF br;                   ///< cached bounding rect served to the scene by boundingRect()

    /**
     * @brief Scales the current image variant to the stick's length and user scale factors.
     * @return the scaled image, ready to be painted.
     */
    QImage calcImg();

    /**
     * @brief Paints the stick according to its StickData (type, pen, brush, image).
     */
    void paint(QPainter *painter, const QStyleOptionGraphicsItem *option, QWidget *widget);

    /**
     * @brief Recomputes bounding rect and angle, then schedules a scene repaint.
     * @param mode 1 while dragging (merges old+new rects to avoid ghosting), 0 otherwise.
     */
    void refresh(int mode);

    /**
     * @brief Replaces the stick's segment.
     * @param line new segment; copied into `data->my_line`.
     */
    void setLine(QLineF* line);

    /**
     * @brief Finalizes an interactive rotation, refreshing this stick and its children.
     */
    void endRotation();

    /**
     * @brief Interactive rotation: points the stick towards the dragged position.
     * @param point mouse position in scene coordinates.
     */
    void rotate(QPointF* point);

    /**
     * @brief Interactive manipulation: moves the free end, dragging children along.
     * @param point mouse position in scene coordinates.
     */
    void manipulate(QPointF* point);

    /**
     * @brief Tells whether this stick's data is its figure's master (tree root).
     * @return true when the owning figure's master_stick is this stick's data.
     */
    bool isMaster() const;

//-------- variables----------//
    bool highlight = false;          ///< visual emphasis when the owning figure is selected
    bool selected = false;           ///< yellow endpoint markers when this is the current stick
    int rotationSpinboxBuffer = 0;   ///< last value pushed to the rotation spinbox (UI echo suppression)
    int scaleSpinBoxBuffer = 100;    ///< last value pushed to the scale spinbox (UI echo suppression)
    float scaleBuffer = 1;           ///< stick length at drag start, reference for interactive scaling
    float widthBuffer = 1;           ///< pen width at drag start, reference for interactive scaling
    float angleBuffer = 0;           ///< usato per disegnare lo stick nei refresh
    float angleBuffer2 = 0;          ///< usato nella rotazione di tutto lo stickfigure
    QRectF imgRect;                  ///< legacy image placement rect (derived, not serialized)
    QGraphicsEllipseItem* stickHandle[2]; ///< red/green endpoint handles shown on the scene
    StickFigure* my_stick_figure = nullptr; ///< owning figure; needed by isMaster()

    /**
     * @brief Recomputes the cached bounding rect.
     * @param mode 1 merges the previous rect (anti-ghosting while dragging), 0 replaces it.
     * @return the new cached rect.
     */
    QRectF updateBr(int mode);

    /**
     * @brief Rotates the stick by an absolute angle around its origin (figure rotation).
     * @param angle target angle in degrees, relative to angleBuffer2.
     */
    void rotate(float angle);

    /**
     * @brief Scales length (and proportionally the pen width) around the origin.
     * @param scaleFactor multiplier relative to scaleBuffer/widthBuffer.
     */
    void scale(float scaleFactor);

    /**
     * @brief Appends an image variant to an IMAGE stick and shows it in the list widget.
     * @param imgToAdd image to take ownership of.
     * @param name display name for the list widget.
     */
    void addImage(QImage *imgToAdd, QString name);

    /**
     * @brief Rebuilds the image list widget from this stick's image variants.
     */
    void populateImageListWidget();

    /**
     * @brief Removes one image variant (and its list widget entry).
     * @param idx index into stick_img_list/img_name_list.
     */
    void removeImgFromList(int idx);

    /**
     * @brief Distance metric used to pick the stick closest to a click.
     * @param p point to test.
     * @param l candidate stick segment.
     * @return distance between p and l's free end (P2).
     */
    float measureDistanceFromLine(QPointF p, QLineF l);
private:
    //-------- methods----------//

    /**
     * @brief Caches the segment's current angle into angleBuffer.
     */
    void  calcAngle();

    /**
     * @brief Builds the raw bounding rect from the segment and shape/image extents.
     * @return rect before pen/handle padding is applied.
     */
    QRectF constructBr();
};

/**
 * @brief Removes every item except the canvas frame (my_rect) from a scene.
 * @param sceneToClear scene to empty without deleting the items.
 */
void sceneRemover(QGraphicsScene *sceneToClear);

/**
 * @brief Computes where an IMAGE stick's picture sits relative to its segment.
 * @param l stick segment.
 * @param s source image size.
 * @return placement rect centered on the segment origin.
 */
QRectF calcImgRect(QLineF l,QSizeF s);
#endif // STICK_H
