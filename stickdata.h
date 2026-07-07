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

/**
 * @brief Pure-data half of a stick: every field that is saved to disk or duplicated by
 *        undo snapshots, copy/paste and the library, with no scene or widget dependency.
 *
 * This is the model side of the stick/StickGraphicsItem split (REFACTOR_PLAN.md Phase 1).
 * A StickData can exist without a QGraphicsScene, so scratch/buffer/undo objects built
 * from it never need the global flag guards that plague the legacy `stick` class.
 * Field names keep their historical spelling (`myLine`, `Pen`, ...) until the Phase 7
 * naming sweep, so existing call sites compile unchanged.
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
        LINE = 5,       ///< plain segment, drawn with Pen only
        CIRCLE = 6,     ///< ellipse whose radius is the stick line
        IMAGE = 7,      ///< user-imported image, needs stickImgList/stickImg
        RECT = 16,      ///< rectangle extruded along the stick line
        TRAPEZOID = 17, ///< four-point polygon with independent end widths
        TAPER = 18      ///< trapezoid with rounded (pie) ends
    };

    /**
     * @brief Builds an empty stick record with safe defaults (LINE type, no images).
     */
    StickData();

    /**
     * @brief Frees the owned image copies in stickImgList.
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
     * directly — rebuilt from parentIdx on load.
     */
    QList<StickData*> children;

    // ------- persistent fields (exactly what operator<</operator>> read and write) ------

    int version = 1;                ///< serialization format version of this record
    int stickType = StickType::LINE;///< StickType value; drives painting and serialization
    qreal Z = 0;                    ///< depth used for scene z-ordering between sticks
    bool stepchild = false;         ///< true when attached to the parent's origin (P1) instead of its free end
    QLineF myLine;                  ///< the segment the stick is built on, in scene coordinates
    int parentIdx = 0;              ///< index of the parent stick inside its figure's stickList; -1 for the master (only meaningful during (de)serialization)
    QPen Pen;                       ///< outline color/thickness
    QBrush Brush;                   ///< fill for the shape types (CIRCLE/RECT/TRAPEZOID/TAPER)
    int order = 0;                  ///< position inside Frame::totalSticks (frame-wide draw order)
    float imgAngle = 0;             ///< orientation of the image/shape, follows the line direction
    float imgWScale = 1;            ///< horizontal scale of the image, or shape width
    float imgHScale = 1;            ///< vertical scale of the image, or second shape width
    int imgAngleOffset = 0;         ///< extra user rotation on top of imgAngle (IMAGE only)
    QPointF imgOffset = QPointF(0, 0); ///< user translation of the image relative to the stick (IMAGE only)
    bool hardTop = false;           ///< TAPER: draw the far end flat instead of rounded
    bool hardBottom = false;        ///< TAPER: draw the near end flat instead of rounded
    QImage *stickImg = nullptr;     ///< currently displayed image; points into stickImgList, not separately owned
    QList<QImage*> stickImgList;    ///< owned image variants of an IMAGE stick
    QList<QString> imgNameList;     ///< display names of stickImgList entries, parallel by index
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
