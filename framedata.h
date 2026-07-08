#ifndef FRAMEDATA_H
#define FRAMEDATA_H
#include <QDataStream>
#include <QList>
#include "stickfiguredata.h"

/**
 * @brief Pure-data half of a frame: the stickfigures plus frame-wide z-order, with no
 *        scene, list-widget or icon dependency.
 *
 * Since Phase 2b this is what the undo buffers, the frame copy buffer and the library
 * buffer hold — the "scratch Frame with an uninitialized scene" trap is gone because a
 * FrameData has no scene at all. Conversion to/from the live Frame goes through
 * ExtractFrameData()/PopulateLiveFrame() (declared in frame.h).
 */
class FrameData
{
public:
    /**
     * @brief Builds an empty frame record.
     */
    FrameData();

    /**
     * @brief Frees the owned stickfigures (totalSticks only references their sticks).
     */
    virtual ~FrameData();

    /**
     * @brief Allocates an independent deep copy (figures cloned, z-order remapped).
     * @return heap-allocated copy; caller takes ownership.
     */
    FrameData *Clone() const;

    QList<StickFigureData*> stickFigures;          ///< owned figures of this frame
    QList<StickData*> totalSticks;                 ///< frame-wide draw order; references sticks owned by stickFigures
    StickFigureData *currentStickFigure = nullptr; ///< remembered selection; points into stickFigures
    int frameNumber = 0;                           ///< position of the frame in the animation
    int numOfItems = 0;                            ///< count of sticks meant to be on scene (legacy semantics)
};

/**
 * @brief Serializes a frame record (same byte layout as the pre-refactor operator).
 * @param stream destination stream.
 * @param frame_data record to write.
 * @return the stream, for chaining.
 */
QDataStream &operator<<(QDataStream &stream, const FrameData &frame_data);

/**
 * @brief Deserializes a frame record, rebuilding totalSticks from each stick's order.
 * @param stream source stream.
 * @param frame_data record to fill (existing figures are freed defensively).
 * @return the stream, for chaining.
 */
QDataStream &operator>>(QDataStream &stream, FrameData &frame_data);

#endif // FRAMEDATA_H
