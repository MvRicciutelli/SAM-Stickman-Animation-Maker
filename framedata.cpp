#include "framedata.h"

/**
 * @details Defaults live as in-class initializers in the header.
 */
FrameData::FrameData()
{
}

/**
 * @details Owns only the figures; totalSticks holds non-owning references into them,
 * so freeing the figures is the whole cleanup. This one-liner replaces the old
 * discardScratchFrame()/freeLibraryFrame() flag dances.
 */
FrameData::~FrameData()
{
    qDeleteAll(stickFigures);
    stickFigures.clear();
}

/**
 * @details In-memory deep copy: figures are cloned and both the z-order list and the
 * current-selection pointer are remapped onto the copies by index.
 */
FrameData *FrameData::Clone() const
{
    FrameData *copy = new FrameData();
    copy->frameNumber = frameNumber;
    copy->numOfItems = numOfItems;
    for (StickFigureData *figure : stickFigures)
        copy->stickFigures.append(figure->Clone());
    // rimappa la lista di z-order sui cloni cercando (figura, indice stick) dell'originale
    for (StickData *ordered_stick : totalSticks) {
        for (int figure_index = 0; figure_index < stickFigures.count(); figure_index++) {
            int stick_index = stickFigures[figure_index]->stickList.indexOf(ordered_stick);
            if (stick_index >= 0) {
                copy->totalSticks.append(copy->stickFigures[figure_index]->stickList[stick_index]);
                break;
            }
        }
    }
    int current_index = stickFigures.indexOf(currentStickFigure);
    if (current_index >= 0)
        copy->currentStickFigure = copy->stickFigures[current_index];
    return copy;
}

/**
 * @details Byte-for-byte the layout the legacy Frame operator wrote. The fifth int
 * (current stick index) is always written — the historical libFlag branch wrote a 0
 * placeholder there, and a FrameData with no current figure does the same, so library
 * files and animation files share one code path with no flag.
 */
QDataStream &operator<<(QDataStream &stream, const FrameData &frame_data)
{
    stream << (int)frame_data.stickFigures.count();
    stream << frame_data.numOfItems;
    stream << frame_data.frameNumber;
    stream << (int)frame_data.stickFigures.indexOf(frame_data.currentStickFigure);
    if (frame_data.currentStickFigure)
        stream << (int)frame_data.currentStickFigure->stickList.indexOf(frame_data.currentStickFigure->currentStick);
    else
        stream << 0;
    for (StickFigureData *figure : frame_data.stickFigures)
        stream << *figure;
    return stream;
}

/**
 * @details Mirror of operator<<. totalSticks is rebuilt from each stick's order field
 * with the same out-of-range guard the legacy operator had (a non-permutation order
 * must not write out of bounds). The current selection is restored via the two indices
 * with the legacy bounds checks; on failure currentStickFigure stays null.
 */
QDataStream &operator>>(QDataStream &stream, FrameData &frame_data)
{
    int max = 0;
    int currentStickIdx = 0;
    int currentStickFigureIdx = 0;
    stream >> max;
    stream >> frame_data.numOfItems;
    stream >> frame_data.frameNumber;
    stream >> currentStickFigureIdx;
    stream >> currentStickIdx;

    qDeleteAll(frame_data.stickFigures);
    frame_data.stickFigures.clear();
    frame_data.totalSticks.clear();
    frame_data.currentStickFigure = nullptr;

    for (int i = 0; i < max; i++) {
        StickFigureData *figure = new StickFigureData();
        frame_data.stickFigures.append(figure);
        stream >> *figure;
        frame_data.totalSticks.append(figure->stickList);
    }
    for (StickFigureData *figure : frame_data.stickFigures) {
        for (StickData *stick_data : figure->stickList) {
            if (stick_data->order >= 0 && stick_data->order < frame_data.totalSticks.count())
                frame_data.totalSticks[stick_data->order] = stick_data;
        }
    }
    if (currentStickIdx >= 0 && currentStickFigureIdx >= 0 && currentStickFigureIdx < frame_data.stickFigures.length()) {
        if (currentStickIdx < frame_data.stickFigures[currentStickFigureIdx]->stickList.length()) {
            frame_data.currentStickFigure = frame_data.stickFigures[currentStickFigureIdx];
            frame_data.currentStickFigure->currentStick = frame_data.currentStickFigure->stickList[currentStickIdx];
        }
    }
    return stream;
}
