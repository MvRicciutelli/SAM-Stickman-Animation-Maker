#include "stickdata.h"

/**
 * @details All defaults live as in-class initializers in stickdata.h so they are visible
 * next to each field's documentation; the constructor has nothing left to do.
 */
StickData::StickData()
{
}

/**
 * @details Image ownership rule: a StickData owns the QImage copies in stick_img_list
 * (stick_img only points into that list). Freeing them here — unconditionally — replaces
 * the flag-guarded cleanup the legacy stick destructor needed, because a pure data
 * object has no widget or scene to protect.
 */
StickData::~StickData()
{
    qDeleteAll(stick_img_list);
    stick_img_list.clear();
}

/**
 * @details This is the in-memory replacement for the historical clone idiom
 * (serialize to a QBuffer, deserialize into the target). It copies exactly the field
 * set the stream operators handle, so both paths stay interchangeable while call sites
 * migrate. Images are duplicated so the copy never shares pixel buffers with the
 * source; stick_img is remapped onto the copied list entry.
 */
void StickData::CopyFrom(const StickData &source)
{
    version = source.version;
    stick_type = source.stick_type;
    is_linker = source.is_linker;
    z = source.z;
    stepchild = source.stepchild;
    my_line = source.my_line;
    parent_idx = source.parent_idx;
    pen = source.pen;
    brush = source.brush;
    order = source.order;
    img_angle = source.img_angle;
    img_w_scale = source.img_w_scale;
    img_h_scale = source.img_h_scale;
    img_angle_offset = source.img_angle_offset;
    img_offset = source.img_offset;
    hard_top = source.hard_top;
    hard_bottom = source.hard_bottom;

    // difesa in profondità come in operator>>: oggi il target è sempre appena costruito,
    // ma un riuso futuro non deve far trapelare le immagini precedenti
    qDeleteAll(stick_img_list);
    stick_img_list.clear();
    img_name_list = source.img_name_list;
    stick_img = nullptr;
    for (QImage *image : source.stick_img_list) {
        QImage *copy = new QImage(*image);
        stick_img_list.append(copy);
        if (image == source.stick_img)
            stick_img = copy;
    }
}

/**
 * @details Convenience wrapper over CopyFrom() for the call sites (Phase 2/3) that
 * want a fresh heap object instead of filling an existing one.
 */
StickData *StickData::Clone() const
{
    StickData *copy = new StickData();
    copy->CopyFrom(*this);
    return copy;
}

/**
 * @details Moved verbatim from StickFigure.cpp; this operator was already pure-data (no
 * flag guards) and is the single write path for undo snapshots, copy/paste, the library
 * and .mAnim/.stck files. The field order IS the on-disk format — never reorder it.
 * Image-family types append their extra block; IMAGE additionally streams every image
 * variant plus the index of the displayed one.
 */
QDataStream &operator<<(QDataStream &stream, const StickData &stick_data)
{
    stream << (double)stick_data.z; // explicit double: qreal itself is float on some Qt builds
    stream << stick_data.version;
    stream << stick_data.stick_type;
    stream << stick_data.is_linker;
    stream << stick_data.stepchild;
    stream << stick_data.my_line;
    stream << stick_data.parent_idx;
    stream << stick_data.pen;
    stream << stick_data.brush;
    stream << stick_data.order;
    if ((stick_data.stick_type == StickData::CIRCLE) || (stick_data.stick_type == StickData::IMAGE) || (stick_data.stick_type == StickData::RECT) || (stick_data.stick_type == StickData::TRAPEZOID) || (stick_data.stick_type == StickData::TAPER)) {
        if (stick_data.stick_type == StickData::IMAGE) {
            // stick_img può essere nullo solo per un record ricostruito da un file corrotto:
            // meglio serializzare una QSize vuota che dereferenziare un puntatore nullo
            QSize imgSize = stick_data.stick_img ? stick_data.stick_img->size() : QSize();
            stream << imgSize;
        }
        stream << stick_data.img_angle;
        stream << stick_data.img_angle_offset; // mancava: si azzerava a ogni save/load/clone (vedi paint(): img_angle+img_angle_offset)
        stream << stick_data.img_h_scale;
        stream << stick_data.img_w_scale;
        stream << stick_data.img_offset;
        stream << stick_data.hard_top;
        stream << stick_data.hard_bottom;

        if (stick_data.stick_type == StickData::IMAGE) {
            stream << (qint32)stick_data.stick_img_list.length();
            stream << (qint32)stick_data.stick_img_list.indexOf(stick_data.stick_img);
            int j = 0;
            for (QImage *i : stick_data.stick_img_list) {
                stream << *i;
                stream << stick_data.img_name_list[j];
                j++;
            }
        }
    }

    return stream;
}

/**
 * @details Mirror of operator<< (moved verbatim from StickFigure.cpp). Fills a
 * freshly-constructed record; IMAGE sticks get their image list rebuilt with newly
 * allocated copies, so the record owns everything it points to. No scene attach, no
 * widget update happens here anymore — that is the whole point of the Phase 1 split.
 */
QDataStream &operator>>(QDataStream &stream, StickData &stick_data)
{
    double z_value = 0;
    stream >> z_value; // matches the explicit double written by operator<<, regardless of qreal's local width
    stick_data.z = z_value;
    stream >> stick_data.version;
    stream >> stick_data.stick_type;
    stream >> stick_data.is_linker;
    stream >> stick_data.stepchild;
    stream >> stick_data.my_line;
    stream >> stick_data.parent_idx;
    stream >> stick_data.pen;
    stream >> stick_data.brush;
    stream >> stick_data.order;
    if ((stick_data.stick_type == StickData::IMAGE) || (stick_data.stick_type == StickData::CIRCLE) || (stick_data.stick_type == StickData::RECT) || (stick_data.stick_type == StickData::TRAPEZOID) || (stick_data.stick_type == StickData::TAPER)) {
        QSize mysize;
        if (stick_data.stick_type == StickData::IMAGE) {
            stream >> mysize;
        }
        stream >> stick_data.img_angle;
        stream >> stick_data.img_angle_offset;
        stream >> stick_data.img_h_scale;
        stream >> stick_data.img_w_scale;
        stream >> stick_data.img_offset;
        stream >> stick_data.hard_top;
        stream >> stick_data.hard_bottom;
        if (stick_data.stick_type == StickData::IMAGE) {
            qint32 numOfImgs;
            qint32 stickImgIdx;
            stream >> numOfImgs;
            stream >> stickImgIdx;
            for (int i = 0; i < numOfImgs; i++) {
                QImage *img = new QImage(mysize, QImage::Format_ARGB32);
                stream >> *img;
                QString name;
                stick_data.stick_img_list.append(img);
                stream >> name;
                stick_data.img_name_list.append(name);
                if (stream.status() != QDataStream::Ok)
                    break; // file troncato: non continuare ad allocare immagini da dati inesistenti
            }
            // l'indice viene dal file: su un file corrotto può essere -1 o fuori range, e
            // indicizzare la lista senza controllo scrive/legge fuori dai limiti
            if (stickImgIdx >= 0 && stickImgIdx < stick_data.stick_img_list.count())
                stick_data.stick_img = stick_data.stick_img_list[stickImgIdx];
            else if (!stick_data.stick_img_list.isEmpty())
                stick_data.stick_img = stick_data.stick_img_list[0];
        }
    }
    return stream;
}
