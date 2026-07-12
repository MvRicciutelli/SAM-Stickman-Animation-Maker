#ifndef STICKFIGUREDATA_H
#define STICKFIGUREDATA_H
#include <QDataStream>
#include <QList>
#include <QMetaType>
#include <QString>
#include "stickdata.h"

/**
 * @brief Pure-data half of a stickfigure: the stick tree plus name and selection memory,
 *        with no scene, list-widget or icon dependency.
 *
 * This is what scratch/buffer objects hold since Phase 2b: undo snapshots, the
 * copy-paste buffer and the library keep StickFigureData instead of live StickFigure
 * objects, so none of the global flag guards apply to them. Conversion to/from the live
 * StickFigure goes through ExtractStickFigureData()/PopulateLiveStickFigure()
 * (declared in StickFigure.h).
 */
class StickFigureData
{
public:
    /**
     * @brief Builds an empty figure record.
     */
    StickFigureData();

    /**
     * @brief Frees the owned sticks.
     */
    virtual ~StickFigureData();

    /**
     * @brief Allocates an independent deep copy (sticks and tree pointers remapped).
     * @return heap-allocated copy; caller takes ownership.
     */
    StickFigureData *Clone() const;

    int version = 1;                    ///< serialization format version of this record
    QString name;                       ///< user-visible layer name
    QList<StickData*> stick_list;        ///< owned sticks forming the figure
    StickData *master_stick = nullptr;   ///< tree root; points into stick_list
    StickData *current_stick = nullptr;  ///< remembered selection; points into stick_list
};
Q_DECLARE_METATYPE(StickFigureData*)

/**
 * @brief Serializes a figure record (same byte layout as the pre-refactor operator).
 * @param stream destination stream.
 * @param figure_data record to write; per-stick parent_idx is recomputed here.
 * @return the stream, for chaining.
 */
QDataStream &operator<<(QDataStream &stream, const StickFigureData &figure_data);

/**
 * @brief Deserializes a figure record, rebuilding the stick tree from parent_idx.
 * @param stream source stream.
 * @param figure_data record to fill (existing sticks are freed defensively).
 * @return the stream, for chaining.
 */
QDataStream &operator>>(QDataStream &stream, StickFigureData &figure_data);

#endif // STICKFIGUREDATA_H
