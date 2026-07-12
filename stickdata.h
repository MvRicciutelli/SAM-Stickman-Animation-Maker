#ifndef STICKDATA_H
#define STICKDATA_H
#include <QBrush>
#include <QDataStream>
#include <QImage>
#include <QLineF>
#include <QList>
#include <QPen>
#include <QPointF>
#include <QString>

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
        TAPER = 18      ///< trapezoid with rounded (pie) ends
    };

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

    /**
     * @brief True only for the invisible bridge stick MergeStickFigures() creates to
     *        connect a joined figure's former master into the host figure's tree.
     *
     * In-memory only — deliberately NOT part of the operator<</operator>> byte layout
     * (see the field-order note below), since it only needs to survive Clone()-based
     * copies (undo/redo, copy/paste, library — Phase 3 confirmed none of those touch
     * the stream anymore). SplitStickFigures() uses this to recognize and garbage-collect
     * a linker that a split has left with no remaining children, rather than leaving it
     * (and its endpoint handles, re-added by MyView::AddHandles()) behind forever.
     */
    bool is_linker = false;

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

    int version = 1;                ///< serialization format version of this record
    int stick_type = StickType::LINE;///< StickType value; drives painting and serialization
    qreal z = 0;                    ///< depth used for scene z-ordering between sticks
    bool stepchild = false;         ///< true when attached to the parent's origin (P1) instead of its free end
    QLineF my_line;                  ///< the segment the stick is built on, in scene coordinates
    int parent_idx = 0;              ///< index of the parent stick inside its figure's stick_list; -1 for the master (only meaningful during (de)serialization)
    QPen pen;                       ///< outline color/thickness
    QBrush brush;                   ///< fill for the shape types (CIRCLE/RECT/TRAPEZOID/TAPER)
    int order = 0;                  ///< position inside Frame::total_sticks (frame-wide draw order)
    float img_angle = 0;             ///< orientation of the image/shape, follows the line direction
    float img_w_scale = 1;            ///< horizontal scale of the image, or shape width
    float img_h_scale = 1;            ///< vertical scale of the image, or second shape width
    int img_angle_offset = 0;         ///< extra user rotation on top of img_angle (IMAGE only)
    QPointF img_offset = QPointF(0, 0); ///< user translation of the image relative to the stick (IMAGE only)
    bool hard_top = false;           ///< TAPER: draw the far end flat instead of rounded
    bool hard_bottom = false;        ///< TAPER: draw the near end flat instead of rounded
    QImage *stick_img = nullptr;     ///< currently displayed image; points into stick_img_list, not separately owned
    QList<QImage*> stick_img_list;    ///< owned image variants of an IMAGE stick
    QList<QString> img_name_list;     ///< display names of stick_img_list entries, parallel by index
};

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

#endif // STICKDATA_H
