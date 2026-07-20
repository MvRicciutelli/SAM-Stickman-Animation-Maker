#include "stickfiguredata.h"

/**
 * @details Defaults live as in-class initializers in the header.
 */
StickFigureData::StickFigureData()
{
}

/**
 * @details A figure record owns its sticks outright — there is no scene or widget to
 * detach from, which is precisely why scratch objects can be freed with a plain delete
 * instead of the flag-guarded dances the live classes needed.
 */
StickFigureData::~StickFigureData()
{
    qDeleteAll(stick_list);
    stick_list.clear();
}

/**
 * @details In-memory deep copy: sticks are cloned field-by-field and the tree pointers
 * (parent/children/master/current) are remapped onto the copies by index. Replaces the
 * historical serialize-and-back clone idiom for figure buffers.
 */
StickFigureData *StickFigureData::Clone() const
{
    StickFigureData *copy = new StickFigureData();
    copy->version = version;
    copy->name = name;
    for (StickData *source_stick : stick_list)
        copy->stick_list.append(source_stick->Clone());
    for (int i = 0; i < stick_list.count(); i++) {
        StickData *source_stick = stick_list[i];
        StickData *copied_stick = copy->stick_list[i];
        int parent_index = stick_list.indexOf(source_stick->parent);
        if (parent_index >= 0)
            copied_stick->parent = copy->stick_list[parent_index];
        for (StickData *child : source_stick->children) {
            int child_index = stick_list.indexOf(child);
            if (child_index >= 0)
                copied_stick->children.append(copy->stick_list[child_index]);
        }
        // ik_start/ik_target: stesso remapping per indice di parent/children (ik_start_idx/
        // ik_target_idx sono scratch solo per lo stream su file, vedi operator<< sotto)
        if (source_stick->ik_target != nullptr) {
            int target_index = stick_list.indexOf(source_stick->ik_target);
            if (target_index >= 0)
                copied_stick->ik_target = copy->stick_list[target_index];
        }
        if (source_stick->ik_start != nullptr) {
            int start_index = stick_list.indexOf(source_stick->ik_start);
            if (start_index >= 0)
                copied_stick->ik_start = copy->stick_list[start_index];
        }
    }
    int master_index = stick_list.indexOf(master_stick);
    if (master_index >= 0)
        copy->master_stick = copy->stick_list[master_index];
    int current_index = stick_list.indexOf(current_stick);
    if (current_index >= 0)
        copy->current_stick = copy->stick_list[current_index];
    return copy;
}

/**
 * @details version, stick count, name, current_stick's index (-1 if none — added when
 * this was found to be silently dropped by every load/populate path; no longer
 * byte-compatible with the pre-refactor format, which never carried this field at all),
 * then each stick (parent_idx travels inside the stick record; -1 marks the master).
 * No flag guards — this operator writes data and nothing else.
 */
QDataStream &operator<<(QDataStream &stream, const StickFigureData &figure_data)
{
    stream << figure_data.version;
    stream << (qint32)figure_data.stick_list.count(); // beware, stream types must correspond! qsizetype != qint32
    stream << figure_data.name;
    stream << (qint32)figure_data.stick_list.indexOf(figure_data.current_stick);
    for (StickData *stick_data : figure_data.stick_list) {
        if (stick_data != figure_data.master_stick)
            stick_data->parent_idx = (qint32)figure_data.stick_list.indexOf(stick_data->parent);
        else
            stick_data->parent_idx = -1;
        // ik_start_idx/ik_target_idx: stesso schema di parent_idx, ricalcolati qui appena
        // prima dello stream (StickData::operator<< li scrive solo se version >= 2)
        stick_data->ik_target_idx = stick_data->ik_target
                ? (qint32)figure_data.stick_list.indexOf(stick_data->ik_target) : -1;
        stick_data->ik_start_idx = stick_data->ik_start
                ? (qint32)figure_data.stick_list.indexOf(stick_data->ik_start) : -1;
        stream << *stick_data;
    }
    return stream;
}

/**
 * @details Mirror of operator<<: reads the sticks and rebuilds the parent/children
 * tree from parent_idx with the same ancestor walk the legacy operator used (children
 * lists are the flattened all-descendants form). Scene attach, handle colors and icon
 * updates — which the legacy operator did behind flag guards — belong to
 * PopulateLiveStickFigure() now.
 */
QDataStream &operator>>(QDataStream &stream, StickFigureData &figure_data)
{
    qint32 max = 0;
    qint32 current_stick_idx = -1;
    stream >> figure_data.version;
    stream >> max;
    stream >> figure_data.name;
    stream >> current_stick_idx;
    // difesa in profondità come nel vecchio operator>>: il target è sempre appena
    // costruito, ma un riuso futuro non deve far trapelare gli stick precedenti
    qDeleteAll(figure_data.stick_list);
    figure_data.stick_list.clear();
    figure_data.master_stick = nullptr;
    figure_data.current_stick = nullptr;
    for (int i = 0; i < max; i++) {
        StickData *loaded_stick = new StickData();
        figure_data.stick_list.append(loaded_stick);
        stream >> *loaded_stick;
    }
    // ricostruisci le gerarchie di parentela tramite gli indici; parent_idx viene dal
    // file, quindi su un file corrotto va trattato come non attendibile: fuori range
    // indicizzerebbe la lista fuori dai limiti, e un ciclo (a->b->a) non terminerebbe mai
    for (StickData *stick_data : figure_data.stick_list) {
        if (stick_data->parent_idx >= 0 && stick_data->parent_idx < figure_data.stick_list.count()) {
            stick_data->parent = figure_data.stick_list[stick_data->parent_idx];
            StickData *current_parent = stick_data->parent;
            int steps = 0;
            while (current_parent != nullptr && steps++ < figure_data.stick_list.count()) {
                current_parent->children.append(stick_data);
                if (current_parent->parent_idx >= 0 && current_parent->parent_idx < figure_data.stick_list.count())
                    current_parent = figure_data.stick_list[current_parent->parent_idx];
                else // arrivati allo stickmaster vai al prossimo
                    break;
            }
        } else {
            stick_data->parent = nullptr;
            if (stick_data->parent_idx < 0) // -1 è il marcatore legittimo del master; un indice oltre il range è solo corruzione
                figure_data.master_stick = stick_data;
        }
    }
    if (current_stick_idx >= 0 && current_stick_idx < figure_data.stick_list.count())
        figure_data.current_stick = figure_data.stick_list[current_stick_idx];
    else if (figure_data.stick_list.count() > 0)
        figure_data.current_stick = figure_data.stick_list[0];
    // risolvi ik_start/ik_target dagli indici, stessa fase in cui parent_idx è già
    // diventato parent sopra; indici fuori range (file corrotto) restano semplicemente
    // non risolti (nullptr), come l'analogo controllo su parent_idx
    for (StickData *stick_data : figure_data.stick_list) {
        if (stick_data->ik_target_idx >= 0 && stick_data->ik_target_idx < figure_data.stick_list.count())
            stick_data->ik_target = figure_data.stick_list[stick_data->ik_target_idx];
        if (stick_data->ik_start_idx >= 0 && stick_data->ik_start_idx < figure_data.stick_list.count())
            stick_data->ik_start = figure_data.stick_list[stick_data->ik_start_idx];
    }
    return stream;
}
