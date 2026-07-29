#ifndef STICKDATA_H
#define STICKDATA_H
#include <QBrush>
#include <QDataStream>
#include <QFont>
#include <QImage>
#include <QLineF>
#include <QList>
#include <QPen>
#include <QPointF>
#include <QString>
#include <QStringList>
#include <QTransform>

class StickGraphicsItem; // defined in stick.h; forward-declared to avoid a header cycle

/**
 * @brief Pure-data half of a stick: every field that is saved to disk or duplicated by
 *        undo snapshots, copy/paste and the library, with no scene or widget dependency.
 *
 * This is the model side of the stick/StickGraphicsItem split (REFACTOR_PLAN.md Phase 1,
 * completed by Phase 2c). A StickData can exist without a QGraphicsScene, so scratch/
 * buffer/undo objects built from it never need any global flag guard. Field names keep
 * their historical spelling (`my_line`, `pen`, ...) until the Phase 7 naming sweep, so
 * existing call sites compile unchanged.
 */
class StickData
{
public:
    /**
     * @brief Discriminates how a stick is drawn and which extra fields are serialized.
     *
     * The numeric values double as tool ids in myview.h (DRAW=5, DRAWCIRCLE=6, ...) and
     * are written to file as plain ints — they must never be renumbered or the on-disk
     * format breaks.
     */
    enum StickType{
        LINE = 5,       ///< plain segment, drawn with pen only
        CIRCLE = 6,     ///< ellipse whose radius is the stick line
        IMAGE = 7,      ///< user-imported image, needs stick_img_list/stick_img
        RECT = 16,      ///< rectangle extruded along the stick line
        TRAPEZOID = 17, ///< four-point polygon with independent end widths
        TAPER = 18,     ///< trapezoid with rounded (pie) ends
        POLYGON = 21,   ///< closed polygon; extra vertices live in polygon_points
                        ///< (19/20 are already taken by the ELONGATE/CLAMP tool ids)
        MULTILINE = 22, ///< open multi-segment path: endpoints are my_line's P1/P2,
                        ///< interior vertices live in polygon_points, with per-vertex
                        ///< width (taper) and corner curvature in vertex_widths/
                        ///< vertex_curvatures
        FREEHAND = 24,  ///< raster paint stick: a single stick_img painted freehand in
                        ///< draw mode (myview.cpp). Rendered/transformed/serialized
                        ///< exactly like IMAGE (see IsImageType()); its one image never
                        ///< enters the image-variant list widget (23 is the DRAW_IK tool id)
        TEXT = 25       ///< paragraph of text laid out along the stick; the skeleton is
                        ///< invisible (drawn only while `text` is empty, i.e. during the
                        ///< 2-click drawing preview). Content/font/layout live in the
                        ///< text_* fields below, placement in img_offset/img_angle_offset
    };

    /**
     * @brief True for the raster-image-backed types (IMAGE and its freehand-paint
     *        variant FREEHAND), which share the same paint/transform/serialize path.
     *
     * The distinction that remains IMAGE-only is the multi-image-variant list widget:
     * a FREEHAND stick holds exactly one image and never populates it.
     */
    bool IsImageType() const { return stick_type == IMAGE || stick_type == FREEHAND; }

    /**
     * @brief Builds an empty stick record with safe defaults (LINE type, no images).
     */
    StickData();

    /**
     * @brief Frees the owned image copies in stick_img_list.
     *
     * Virtual so a derived object (today the legacy `stick`) can be deleted through a
     * StickData pointer once containers switch to StickData* in Phase 1b.
     */
    virtual ~StickData();

    /**
     * @brief Deep-copies every persistent field from another stick record.
     * @param source record to copy from; its images are duplicated, never shared.
     *
     * Replaces the old serialize-to-QByteArray-and-back cloning round-trip. Tree
     * pointers, selection state and scene items are deliberately NOT copied — they are
     * not persistent data.
     */
    void CopyFrom(const StickData &source);

    /**
     * @brief Allocates a new independent copy of this record.
     * @return heap-allocated deep copy; caller takes ownership.
     */
    StickData *Clone() const;

    // ------- figure-tree relationships (Phase 2a: moved here from the legacy stick) ------

    StickData *parent = nullptr;    ///< parent stick inside the figure tree; nullptr for the master
    /**
     * @brief ALL descendants in the figure tree, flattened — not just direct children.
     *
     * Historical invariant kept as-is: every ancestor lists every descendant. Direct
     * children are the entries whose parent points back at this stick. Not serialized
     * directly — rebuilt from parent_idx on load.
     */
    QList<StickData*> children;

    /**
     * @brief The on-screen graphics item currently presenting this record, if any.
     *
     * Non-owning; nullptr when this record isn't attached to a scene (every scratch/
     * buffer/undo object, always) or between DetachView() and the next AttachView()
     * for a live one. Wired by FramePresenter::AttachView()/DetachView() (Phase 2c) —
     * nothing else should assign it. This is a deliberate, minimal exception to "the
     * model knows nothing about the view": one non-owning pointer buys O(1) lookup from
     * data to its presenter for interactive code (rotate/scale/drag) that already has a
     * StickData* in hand and needs to reach the view, without FramePresenter having to
     * keep a separate hash synchronized by hand.
     */
    StickGraphicsItem *view = nullptr;

    // ------- persistent fields (exactly what operator<</operator>> read and write) ------

    qint32 version = 3;              ///< serialization format version of this record; 2 adds the
                                     ///< IK fields below, 3 adds TEXT's text_v_stretch — both
                                     ///< gated in operator<</operator>> so files saved before
                                     ///< those fields existed still load correctly
    qint32 stick_type = StickType::LINE;///< StickType value; drives painting and serialization
    /**
     * @brief True only for the invisible bridge stick MergeStickFigures() creates to
     *        connect a joined figure's former master into the host figure's tree.
     *
     * SplitStickFigures() uses this to recognize and garbage-collect a linker that a
     * split has left with no remaining children, rather than leaving it (and its
     * endpoint handles, re-added by MyView::AddHandles()) behind forever.
     */
    bool is_linker = false;
    qreal z = 0;                    ///< depth used for scene z-ordering between sticks; streamed as an
                                     ///< explicit double (qreal itself can be float or double depending
                                     ///< on how Qt was built, so the field's own width isn't stable)
    bool stepchild = false;         ///< true when attached to the parent's origin (P1) instead of its free end
    QLineF my_line;                  ///< the segment the stick is built on, in scene coordinates
    qint32 parent_idx = 0;           ///< index of the parent stick inside its figure's stick_list; -1 for the master (only meaningful during (de)serialization)
    QPen pen;                       ///< outline color/thickness
    QBrush brush;                   ///< fill for the shape types (CIRCLE/RECT/TRAPEZOID/TAPER)
    qint32 order = 0;                ///< position inside Frame::total_sticks (frame-wide draw order)
    float img_angle = 0;             ///< orientation of the image/shape, follows the line direction
    float img_w_scale = 1;            ///< horizontal scale of the image, or shape width
    float img_h_scale = 1;            ///< vertical scale of the image, or second shape width
    qint32 img_angle_offset = 0;      ///< extra user rotation on top of img_angle (IMAGE only)
    QPointF img_offset = QPointF(0, 0); ///< user translation of the image relative to the stick (IMAGE only)
    bool hard_top = false;           ///< TAPER: draw the far end flat instead of rounded
    bool hard_bottom = false;        ///< TAPER: draw the near end flat instead of rounded
    QImage *stick_img = nullptr;     ///< currently displayed image; points into stick_img_list, not separately owned
    QList<QImage*> stick_img_list;    ///< owned image variants of an IMAGE stick
    QList<QString> img_name_list;     ///< display names of stick_img_list entries, parallel by index
    /**
     * @brief Extra vertices of a POLYGON or MULTILINE stick, in normalized
     *        stick-local coordinates.
     *
     * Local frame: origin at my_line.p1(), rotated by img_angle+img_angle_offset,
     * translated by img_offset, and scaled by my_line.length() — the exact transform
     * PolygonTransform() returns. Storing normalized coordinates makes the shape
     * follow the stick's move/rotate for free (like an IMAGE) and scale with the
     * figure when the stick's length changes.
     *
     * POLYGON: ALL the polygon's vertices (P1/P2 are only the skeleton).
     * MULTILINE: the INTERIOR vertices only — the path's endpoints are my_line's own
     * P1/P2, so vertex index i in "virtual" terms is: 0 = P1, 1..count = interior,
     * count+1 = P2 (see StickGraphicsItem's vertex helpers).
     */
    QList<QPointF> polygon_points;

    /**
     * @brief MULTILINE only: stroke width at each vertex (taper), endpoints included.
     *
     * Indexed by virtual vertex index (size = polygon_points.count() + 2). Initialized
     * to the pen width when the stick is drawn; edited per selected vertex via the
     * taper slider.
     */
    QList<float> vertex_widths;

    /**
     * @brief POLYGON and MULTILINE: corner smoothing at each vertex, 0 (sharp angle)
     *        to 1 (maximum rounding), rendered as quadratic curves via QPainterPath.
     *        Indexed by (virtual) vertex index; missing entries read as 0. For a
     *        MULTILINE the two endpoint entries exist but have no corner to smooth.
     */
    QList<float> vertex_curvatures;

    // ------- TEXT only. Placement reuses the IMAGE fields (img_offset for the h/v -----
    // ------- offset, img_angle_offset for the extra rotation), so the offset panel ----
    // ------- and InterpolateStickData() work on a text stick unchanged. -------------

    /**
     * @brief The paragraph content; '\\n' separates paragraphs (stick.cpp maps them to
     *        QChar::LineSeparator for QTextLayout). Empty means "not written yet": the
     *        stick paints its bare skeleton line instead, which is what makes the 2-click
     *        drawing preview visible before the text is typed.
     */
    QString text;

    /**
     * @brief Everything about the typeface: family, point size, bold, italic, HORIZONTAL
     *        letter spacing (QFont::setLetterSpacing, PercentageSpacing) and HORIZONTAL
     *        stretch (QFont::setStretch, percent).
     *
     * Stored as a whole QFont on purpose: QDataStream serializes all of those natively,
     * so the bold/italic/letter-spacing/stretch controls needed no extra fields and no
     * format change. Only the VERTICAL stretch has no QFont equivalent and lives in its
     * own field below. On load a family that is not installed is replaced with Arial and
     * reported through missing_text_fonts (see operator>>).
     */
    QFont text_font;

    qint32 text_align = 0x0001;      ///< Qt::AlignLeft/AlignRight/AlignHCenter/AlignJustify (stored as int so the format doesn't depend on Qt's enum type)
    float text_line_spacing = 1.0f;   ///< VERTICAL spacing: line height multiplier (1 = the font's own leading). The horizontal (letter) spacing lives in text_font
    /**
     * @brief VERTICAL stretch of the glyphs, in percent (100 = unstretched).
     *
     * QFont::stretch() is horizontal only, so this one is applied as a y-scale inside
     * stick.cpp's TextFrame() — which means it stretches the line pitch along with the
     * glyphs, i.e. the whole text block. Serialized only from record version 3 on.
     */
    qint32 text_v_stretch = 100;
    bool text_word_wrap = true;      ///< wrap the paragraph at the stick's length; false lets lines run past it

    /**
     * @brief Maps a normalized polygon_points entry to scene coordinates.
     * @return the local→scene transform; invert it to map a click back to a vertex.
     *
     * Mirrors the painter transform the IMAGE branch of paint() applies
     * (translate p1 → rotate img_angle+img_angle_offset → translate img_offset),
     * plus the length scaling that normalizes the stored points.
     */
    QTransform PolygonTransform() const;

    /**
     * @brief Role of a stick inside an inverse-kinematics chain (the DRAW_IK tool,
     *        myview.cpp). A chain is a straight ancestor line inside one stickfigure:
     *        one IK_START (the anchor, never moves), zero or more IK_NODE sticks in
     *        between, and one IK_TARGET_ON/IK_TARGET_OFF stick whose free end (P2) is
     *        the effector the user drags. IK_TARGET_ON/OFF encodes the chain's on/off
     *        toggle — there is no separate "active" flag to fall out of sync with it.
     *
     * Only DeleteStick() is currently aware that OTHER tree-restructuring operations
     * exist (Weld/Chop/SetMaster family/MergeStickFigures/SplitStickFigures are not):
     * one of those breaking a chain's straight ancestor line leaves the chain inert
     * (StickFigure.cpp's chain-walk helpers fail their consistency check and no-op)
     * rather than corrupting data, but does not clear the stale roles. Deliberate
     * scope boundary, not a bug — revisit if that gap turns out to matter in practice.
     */
    enum IkRole {
        IK_NONE = 0,     ///< not part of any chain (the vast majority of sticks)
        IK_START,        ///< the chain's fixed anchor; ik_target names its target stick
        IK_NODE,         ///< an intermediate link, carried along by the solve
        IK_TARGET_ON,    ///< the chain's effector; solved toward the dragged position
        IK_TARGET_OFF    ///< same stick, chain toggled off: behaves like a plain stick
    };

    qint32 ik_role = IK_NONE;      ///< one of the IkRole values above
    qint32 ik_start_idx = -1;      ///< scratch for (de)serialization only, like parent_idx — see StickFigureData's operator<<
    qint32 ik_target_idx = -1;     ///< scratch for (de)serialization only, like parent_idx

    /**
     * @brief IK_START only: the chain's target stick. Non-owning, not copied by
     *        CopyFrom() (same rule as `parent`/`children`) — resolved from
     *        ik_target_idx by whoever orchestrates a clone/load (ExtractStickFigureData/
     *        PopulateLiveStickFigure/StickFigureData::Clone(), mirroring how those
     *        already remap `parent`/`children`).
     */
    StickData *ik_target = nullptr;

    /**
     * @brief IK_TARGET_ON/OFF only: the chain's start stick. Same non-owning,
     *        remapped-externally rule as ik_target.
     */
    StickData *ik_start = nullptr;
};

/**
 * @brief Families of TEXT sticks that a load found missing on this system and replaced
 *        with Arial, accumulated by operator>> (data layer: it cannot prompt).
 *
 * Drained by MyView::ReportMissingTextFonts(), which every file-load entry point calls
 * once the load is done.
 */
extern QStringList missing_text_fonts;

/**
 * @brief Serializes one stick record (used by every save/copy/undo path).
 * @param stream destination stream.
 * @param stick_data record to write.
 * @return the stream, for chaining.
 */
QDataStream &operator<<(QDataStream &stream, const StickData &stick_data);

/**
 * @brief Deserializes one stick record previously written by operator<<.
 * @param stream source stream.
 * @param stick_data freshly-constructed record to fill.
 * @return the stream, for chaining.
 */
QDataStream &operator>>(QDataStream &stream, StickData &stick_data);

/**
 * @brief Linearly interpolates the animatable pose of a stick between two poses.
 * @param target record that receives the interpolated pose; every field that cannot be
 *        blended (stick_type, images, is_linker/stepchild, the IK role, the tree links)
 *        keeps whatever it already holds — callers seed it with a Clone() of one of the
 *        two poses.
 * @param start pose reached at t == 0.
 * @param end pose reached at t == 1.
 * @param t normalized position between the two poses (0 = start, 1 = end).
 *
 * Written for MyView::GenerateInbetweens() (myview.cpp), which fills the frames it
 * creates between two keyframes stick by stick. Blended fields: my_line (hence
 * img_angle, recomputed from it), z, pen width/colour, brush colour, img_offset,
 * img_w_scale/img_h_scale, img_angle_offset and — only when the two poses agree on the
 * vertex count — polygon_points/vertex_widths/vertex_curvatures. A raster image cannot
 * be blended, so an IMAGE/FREEHAND stick keeps the target's picture and only its
 * placement moves.
 */
void InterpolateStickData(StickData &target, const StickData &start, const StickData &end, float t);

#endif // STICKDATA_H
