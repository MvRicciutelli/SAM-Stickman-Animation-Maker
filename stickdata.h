#ifndef STICKDATA_H
#define STICKDATA_H
#include <abrbrush.h>   // BrushStroke: the border-brush settings live on the record
#include <QBrush>
#include <QDataStream>
#include <QFont>
#include <QHash>
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
     * @brief True for the types whose paint() actually hands `brush` to the painter.
     *
     * A raster type IS its image (pen and brush are never looked at), and LINE, MULTILINE and
     * TEXT are stroke-only — for all of them the brush is a value the stick carries and nothing
     * renders. Everything the fill can be edited or aimed with therefore has to be gated on
     * this, not merely on the brush having a gradient/texture in it: Animation::UpdateSliders()
     * greys the fill type combo, UpdateStyleControls() the fill's texture-handle box, and
     * MyView::TextureArrowPoints() hides the handle itself.
     */
    bool PaintsFill() const
    {
        return !IsImageType() && stick_type != LINE && stick_type != MULTILINE
                               && stick_type != TEXT;
    }

    /**
     * @brief True for the types whose paint() actually hands `pen` to the painter.
     *
     * Only the raster types are excluded — see PaintsFill(); a TEXT stick strokes its glyphs
     * with the pen, so it counts.
     */
    bool PaintsStroke() const { return !IsImageType(); }

    /**
     * @brief True for the types whose border is a PATH that a brush tip can be walked along.
     *
     * PaintsStroke() minus TEXT: a text stick's "stroke" is the glyph fill, not a contour, so
     * there is nothing to stamp along. Gates both the renderer (paint()) and the "Brush" entry
     * of the Border type combo.
     */
    bool StrokesAPath() const { return PaintsStroke() && stick_type != TEXT; }

    /**
     * @brief The border brush ready to stamp with: the stored tip and dynamics, plus the three
     *        numbers that are the PEN's (colour, dab diameter, opacity).
     * @return a BrushStroke whose stamp is null when this stick has no brush border.
     *
     * The one place the derived half is filled in, so the thickness slider, the colour button and
     * the opacity slider keep driving a brush border exactly as they drive a plain one — see the
     * note on BrushStroke (abrbrush.h) for why those three are not stored twice.
     */
    BrushStroke StrokeBrush() const;

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

    /// The newest record format this build reads AND writes; operator>> refuses anything above
    /// it (RejectNewerVersion()), because a newer record's extra fields would be read as the
    /// start of the next record and desync the whole stream.
    static constexpr qint32 CURRENT_VERSION = 6;
    qint32 version = CURRENT_VERSION; ///< serialization format version of this record; 2 adds the
                                     ///< IK fields below, 3 adds TEXT's text_v_stretch, 4 adds
                                     ///< brush_follows_stick, 5 adds texture_tiled, 6 adds
                                     ///< stroke_brush — all gated in operator<</operator>> so
                                     ///< files saved before those fields existed still load
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

    /**
     * @brief Whether a pattern/gradient/image fill or stroke is anchored to the stick
     *        (rotating, moving and stretching with it) or to the scene (staying put while
     *        the stick moves over it). True — anchored to the stick — is the default.
     *
     * Meaningless for a plain colour, which has no geometry to anchor. It governs the pen
     * AND the brush together: one visual decision per stick, not two.
     *
     * The flag is needed because the stick types do not agree on which coordinate frame
     * they paint in: RECT/CIRCLE/TRAPEZOID/TAPER/TEXT/IMAGE paint inside the stick's own
     * rotated frame (so a texture follows for free), while LINE/POLYGON/MULTILINE paint in
     * scene coordinates (so it does not). StickGraphicsItem::AlignBrush() reconciles the
     * two by pre-multiplying the brush transform, in whichever direction that type needs —
     * which is why this is one bool here and no per-type special-casing anywhere else.
     *
     * Serialized only from record version 4 on.
     */
    bool brush_follows_stick = true;

    /**
     * @brief Whether a texture FILL repeats across the shape (true, the default) or is
     *        painted as a single instance (false).
     *
     * Only a Qt::TexturePattern fill has anything to repeat, so this is meaningless for a
     * plain colour, a gradient and an empty brush. Qt has no non-repeating brush style, so
     * StickGraphicsItem clips the fill to the one instance the texture transform places —
     * see StickGraphicsItem::SingleInstanceClip(). The STROKE always tiles: clipping a
     * stroke to one instance would chop the outline into a fragment instead of drawing it.
     *
     * Serialized only from record version 5 on.
     */
    bool texture_tiled = true;

    /**
     * @brief The "Brush" border: the outline is STAMPED with a brush tip, dab by dab, instead of
     *        being traced by the pen.
     *
     * A null `stamp` — the default — means no brush border, and the pen draws the outline exactly
     * as it always has; everything else here is inert until a tip is picked. The tip and the
     * spacing/jitters are the same ones freehand draw mode uses, read from the same .abr library
     * (see BrushPicker), so a border can be given the texture of any imported Photoshop brush.
     *
     * Only the stored half of a BrushStroke is meaningful here: `color`, `size` and `opacity` are
     * the pen's and are refilled by StrokeBrush(), so they are neither edited nor serialized.
     *
     * Serialized only from record version 6 on.
     */
    BrushStroke stroke_brush;
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
 * @brief Why the file being loaded was refused, in words for the user; empty when nothing was.
 *
 * The data layer cannot show a dialog, so a refusal travels the way missing_text_fonts does: the
 * operators and loaders record it here and the UI layer that started the load reports it
 * (MyView::ReportRejectedFile()). Every loader clears it before it reads, so a stale message can
 * never fail a later, valid load. The first refusal wins.
 */
extern QString file_load_error;

/**
 * @brief Refuses a record or file whose format version is newer than this build understands.
 * @param stream the stream being read; put into QDataStream::ReadCorruptData so every
 *        status-checking loop downstream stops instead of parsing the rest as garbage.
 * @param what the kind of record, for the message ("stick", "stickfigure", "animation").
 * @param found the version read from the file.
 * @param supported the newest version this build reads.
 * @return true when @p found is newer (and file_load_error is set), false when it is readable.
 */
bool RejectNewerVersion(QDataStream &stream, const char *what, qint32 found, qint32 supported);

/**
 * @brief Releases the load-time image pool's references.
 *
 * operator>> shares one pixel buffer between every record that reads identical pixels —
 * an image stick, a texture fill/stroke or a border-brush tip repeated across N frames
 * costs one copy instead of N. The pool holds a reference to each distinct image so that
 * sharing can span the whole file; this drops those references once the load is over,
 * leaving the loaded records as the only owners.
 *
 * Called by MyView::ReportMissingTextFonts(), i.e. by every file-load entry point, for
 * the same reason and at the same moment missing_text_fonts is drained.
 */
void ClearStickImagePool();

/**
 * @brief File-wide table of distinct images, so a repeated one is written once.
 *
 * Without it every record inlines its own copy of its pixels: the same picture, texture
 * or brush tip is re-encoded once per stick PER FRAME, which is what made a .sam grow
 * to hundreds of megabytes. With a table installed the record writes a qint32 index into
 * @c images instead, and the container writes the table once.
 *
 * Installed by InstallImageTable() around a whole-file read/write and null otherwise, so
 * a record written with no table still inlines its images (that is what the single-figure
 * .stck path does, and it keeps StickData's own record format unchanged).
 */
struct ImageTable {
    QList<QImage> images;               ///< distinct images, in first-written order
    QHash<size_t, int> index_of;        ///< content key -> index into images (write side only)

    /**
     * @brief Index of @p image in the table, appending it if this is the first sighting.
     * @param image the image to look up; a null one is a valid entry (index reused).
     * @return the index to write into the record.
     */
    qint32 IndexFor(const QImage &image);
};

/**
 * @brief Installs (or removes, with nullptr) the table StickData's operators index into.
 * @param table the table to use for the duration of one whole-file read or write.
 * @return the previously installed table, to restore — always restore it, so a nested
 *         or aborted load cannot leave a dangling table behind.
 */
ImageTable *InstallImageTable(ImageTable *table);

/**
 * @brief Writes an image table (the container's half of the indexing above).
 */
QDataStream &operator<<(QDataStream &stream, const ImageTable &table);

/**
 * @brief Reads an image table, pooling each entry so the sharing survives the load.
 */
QDataStream &operator>>(QDataStream &stream, ImageTable &table);

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

/**
 * @brief The placement of a texture or gradient inside a QBrush, as three editable numbers.
 *
 * A QBrush already carries its placement, in QBrush::transform() — and QDataStream writes
 * that transform, so it persists with no field of its own (see CLAUDE.md "Fill and stroke").
 * What a transform cannot do is be edited: neither the numeric options dialog nor the
 * on-canvas arrow handle can act on a 3x3 matrix. This struct is that matrix in the form
 * both of them speak, and ReadTextureXf()/WriteTextureXf() convert losslessly in both
 * directions — every brush this app builds is a plain scale+rotate+translate, which is
 * exactly what these three fields describe.
 */
struct TextureXf {
    qreal   scale  = 1.0;         ///< uniform scale factor applied to the tile / gradient box
    qreal   angle  = 0.0;         ///< rotation in degrees, clockwise (QTransform::rotate's sign)
    QPointF offset = QPointF(0,0);///< translation, applied after the rotation
};

/**
 * @brief Reads a brush's placement out of its transform.
 * @param brush brush to inspect; a brush with no transform reads back as the identity
 *        placement (scale 1, angle 0, no offset).
 * @return the placement. Only meaningful for a texture or gradient brush — a solid colour
 *         has no geometry, and its transform is always the identity.
 */
TextureXf ReadTextureXf(const QBrush &brush);

/**
 * @brief Writes a placement back into a brush's transform.
 * @param brush brush to modify; must not be null. Its style, colour, tile and gradient
 *        stops are left alone — only QBrush::setTransform() is called.
 * @param xf the placement to store. The scale is clamped to a small positive floor: a zero
 *        (or near-zero) scale collapses the transform to a matrix Qt treats as singular,
 *        which renders as nothing at all and which ReadTextureXf() could then never recover
 *        an angle from. The floor is on the DETERMINANT's terms, not merely non-zero — see
 *        the implementation.
 */
void WriteTextureXf(QBrush *brush, const TextureXf &xf);

/**
 * @brief Reads a GRADIENT brush's placement, as the same three numbers a texture's is.
 * @param brush brush to inspect; a non-gradient brush reads back as the identity placement.
 * @return the placement, in the gradient's OWN coordinate space — 0..1 of whatever box it is
 *         anchored to, since that is what a QGradient's geometry is expressed in. The offset is
 *         measured from the box's CENTRE, so a centred gradient reads offset (0,0).
 *
 * The gradient counterpart of ReadTextureXf(), and it exists because the two brush kinds keep
 * their placement in completely different places: a texture's is QBrush::transform(), while a
 * gradient's is its own geometry (start/finalStop for a linear one, centre/radius/focus for a
 * radial one) and its transform is normally the identity. Editing a gradient through its
 * transform is not an option — for an ObjectMode gradient the transform acts in that 0..1 box
 * space, so an offset in scene units would mean that many whole bounding boxes.
 *
 * The mapping onto the three numbers is: linear — the axis's midpoint, direction and LENGTH;
 * radial — the circle's centre, its DIAMETER (so it compares with the linear axis) and the
 * direction of its focal highlight, which is the one thing an angle can mean for a circle.
 */
TextureXf ReadGradientXf(const QBrush &brush);

/**
 * @brief Writes a placement back into a gradient brush, rebuilding its geometry.
 * @param brush brush to modify; must not be null. A non-gradient brush is left alone. The
 *        stops, the spread, the coordinate mode and the brush transform are all preserved —
 *        only the geometry is rebuilt, which is why the gradient has to be reconstructed rather
 *        than mutated (QBrush hands out only a const QGradient*).
 * @param xf the placement, in the gradient's own 0..1 space. A radial gradient keeps its focal
 *        DISTANCE and is only re-aimed, so dragging the angle spins the highlight around the
 *        centre instead of flattening it onto it.
 */
void WriteGradientXf(QBrush *brush, const TextureXf &xf);

/**
 * @brief How opaque a fill or stroke paint is, as one number for all three kinds of paint.
 * @param brush brush to inspect.
 * @return 0..1. A solid colour's or a texture's is the alpha of QBrush::color(); a gradient's is
 *         the alpha of its MOST OPAQUE stop.
 *
 * Two storage places rather than one, and measured rather than chosen: QDataStream writes a
 * QBrush's colour for every style, but on the way back in it only restores it for a solid and a
 * texture — a gradient brush is rebuilt from its stops and the colour is dropped, so an opacity
 * parked there would silently vanish on save/load. The stops, which do persist, are therefore
 * where a gradient's opacity lives (and where Qt already looks when rendering it, so unlike a
 * texture it needs no painter opacity at paint time — see StickGraphicsItem::FillOpacity()).
 */
qreal ReadPaintOpacity(const QBrush &brush);

/**
 * @brief Sets a paint's opacity, in whichever of the two places that kind of brush keeps it.
 * @param brush brush to modify; must not be null. Qt::NoBrush has no paint to fade — callers
 *        (Widget::SetPaintOpacity(), Animation::UpdateStyleControls()) exclude it.
 * @param opacity 0..1, absolute: setting the same value twice changes nothing.
 *
 * A gradient's stops are scaled RELATIVE to the most opaque one, which is what keeps the
 * transparency the user dialled per stop: the ratios between the stops survive, and the number
 * the slider shows is the peak. ponytail: alphas are 8-bit, so a round trip through a very low
 * opacity quantizes the ratios a little; the upgrade, if it ever shows, is a stored base.
 */
void WritePaintOpacity(QBrush *brush, qreal opacity);

#endif // STICKDATA_H
