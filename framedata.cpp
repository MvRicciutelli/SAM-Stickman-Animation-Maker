#include "framedata.h"

/**
 * @details Defaults live as in-class initializers in the header.
 */
FrameData::FrameData()
{
}

/**
 * @details Owns only the figures; total_sticks holds non-owning references into them,
 * so freeing the figures is the whole cleanup. This one-liner replaces the old
 * discardScratchFrame()/freeLibraryFrame() flag dances.
 */
FrameData::~FrameData()
{
    qDeleteAll(stick_figures);
    stick_figures.clear();
}

/**
 * @details In-memory deep copy: figures are cloned and both the z-order list and the
 * current-selection pointer are remapped onto the copies by index.
 */
FrameData *FrameData::Clone() const
{
    FrameData *copy = new FrameData();
    copy->frame_number = frame_number;
    copy->num_of_items = num_of_items;
    for (StickFigureData *figure : stick_figures)
        copy->stick_figures.append(figure->Clone());
    // rimappa la lista di z-order sui cloni cercando (figura, indice stick) dell'originale
    for (StickData *ordered_stick : total_sticks) {
        for (int figure_index = 0; figure_index < stick_figures.count(); figure_index++) {
            int stick_index = stick_figures[figure_index]->stick_list.indexOf(ordered_stick);
            if (stick_index >= 0) {
                copy->total_sticks.append(copy->stick_figures[figure_index]->stick_list[stick_index]);
                break;
            }
        }
    }
    int current_index = stick_figures.indexOf(current_stick_figure);
    if (current_index >= 0)
        copy->current_stick_figure = copy->stick_figures[current_index];
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
    stream << (qint32)frame_data.stick_figures.count();
    stream << frame_data.num_of_items;
    stream << frame_data.frame_number;
    stream << (qint32)frame_data.stick_figures.indexOf(frame_data.current_stick_figure);
    if (frame_data.current_stick_figure)
        stream << (qint32)frame_data.current_stick_figure->stick_list.indexOf(frame_data.current_stick_figure->current_stick);
    else
        stream << (qint32)0;
    for (StickFigureData *figure : frame_data.stick_figures)
        stream << *figure;
    return stream;
}

/**
 * @details Mirror of operator<<. total_sticks is rebuilt from each stick's order field
 * with the same out-of-range guard the legacy operator had (a non-permutation order
 * must not write out of bounds). The current selection is restored via the two indices
 * with the legacy bounds checks; on failure current_stick_figure stays null.
 */
QDataStream &operator>>(QDataStream &stream, FrameData &frame_data)
{
    qint32 max = 0;
    qint32 currentStickIdx = 0;
    qint32 currentStickFigureIdx = 0;
    stream >> max;
    stream >> frame_data.num_of_items;
    stream >> frame_data.frame_number;
    stream >> currentStickFigureIdx;
    stream >> currentStickIdx;

    qDeleteAll(frame_data.stick_figures);
    frame_data.stick_figures.clear();
    frame_data.total_sticks.clear();
    frame_data.current_stick_figure = nullptr;

    for (int i = 0; i < max; i++) {
        StickFigureData *figure = new StickFigureData();
        frame_data.stick_figures.append(figure);
        stream >> *figure;
        frame_data.total_sticks.append(figure->stick_list);
    }
    for (StickFigureData *figure : frame_data.stick_figures) {
        for (StickData *stick_data : figure->stick_list) {
            if (stick_data->order >= 0 && stick_data->order < frame_data.total_sticks.count())
                frame_data.total_sticks[stick_data->order] = stick_data;
        }
    }
    if (currentStickIdx >= 0 && currentStickFigureIdx >= 0 && currentStickFigureIdx < frame_data.stick_figures.length()) {
        if (currentStickIdx < frame_data.stick_figures[currentStickFigureIdx]->stick_list.length()) {
            frame_data.current_stick_figure = frame_data.stick_figures[currentStickFigureIdx];
            frame_data.current_stick_figure->current_stick = frame_data.current_stick_figure->stick_list[currentStickIdx];
        }
    }
    return stream;
}
