#include "stickdata.h"

/**
 * @details All defaults live as in-class initializers in stickdata.h so they are visible
 * next to each field's documentation; the constructor has nothing left to do.
 */
StickData::StickData()
{
}

/**
 * @details Image ownership rule: a StickData owns the QImage copies in stickImgList
 * (stickImg only points into that list). Freeing them here — unconditionally — replaces
 * the flag-guarded cleanup the legacy stick destructor needed, because a pure data
 * object has no widget or scene to protect.
 */
StickData::~StickData()
{
    qDeleteAll(stickImgList);
    stickImgList.clear();
}

/**
 * @details This is the in-memory replacement for the historical clone idiom
 * (serialize to a QBuffer, deserialize into the target). It copies exactly the field
 * set the stream operators handle, so both paths stay interchangeable while call sites
 * migrate. Images are duplicated so the copy never shares pixel buffers with the
 * source; stickImg is remapped onto the copied list entry.
 */
void StickData::CopyFrom(const StickData &source)
{
    version = source.version;
    stickType = source.stickType;
    Z = source.Z;
    stepchild = source.stepchild;
    myLine = source.myLine;
    parentIdx = source.parentIdx;
    Pen = source.Pen;
    Brush = source.Brush;
    order = source.order;
    imgAngle = source.imgAngle;
    imgWScale = source.imgWScale;
    imgHScale = source.imgHScale;
    imgAngleOffset = source.imgAngleOffset;
    imgOffset = source.imgOffset;
    hardTop = source.hardTop;
    hardBottom = source.hardBottom;

    // difesa in profondità come in operator>>: oggi il target è sempre appena costruito,
    // ma un riuso futuro non deve far trapelare le immagini precedenti
    qDeleteAll(stickImgList);
    stickImgList.clear();
    imgNameList = source.imgNameList;
    stickImg = nullptr;
    for (QImage *image : source.stickImgList) {
        QImage *copy = new QImage(*image);
        stickImgList.append(copy);
        if (image == source.stickImg)
            stickImg = copy;
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
    stream << stick_data.Z;
    stream << stick_data.version;
    stream << stick_data.stickType;
    stream << stick_data.stepchild;
    stream << stick_data.myLine;
    stream << stick_data.parentIdx;
    stream << stick_data.Pen;
    stream << stick_data.Brush;
    stream << stick_data.order;
    if ((stick_data.stickType == StickData::CIRCLE) || (stick_data.stickType == StickData::IMAGE) || (stick_data.stickType == StickData::RECT) || (stick_data.stickType == StickData::TRAPEZOID) || (stick_data.stickType == StickData::TAPER)) {
        if (stick_data.stickType == StickData::IMAGE) {
            QSize imgSize = stick_data.stickImg->size();
            stream << imgSize;
        }
        stream << stick_data.imgAngle;
        stream << stick_data.imgAngleOffset; // mancava: si azzerava a ogni save/load/clone (vedi paint(): imgAngle+imgAngleOffset)
        stream << stick_data.imgHScale;
        stream << stick_data.imgWScale;
        stream << stick_data.imgOffset;
        stream << stick_data.hardTop;
        stream << stick_data.hardBottom;

        if (stick_data.stickType == StickData::IMAGE) {
            stream << (int)stick_data.stickImgList.length();
            stream << (int)stick_data.stickImgList.indexOf(stick_data.stickImg);
            int j = 0;
            for (QImage *i : stick_data.stickImgList) {
                stream << *i;
                stream << stick_data.imgNameList[j];
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
    stream >> stick_data.Z;
    stream >> stick_data.version;
    stream >> stick_data.stickType;
    stream >> stick_data.stepchild;
    stream >> stick_data.myLine;
    stream >> stick_data.parentIdx;
    stream >> stick_data.Pen;
    stream >> stick_data.Brush;
    stream >> stick_data.order;
    if ((stick_data.stickType == StickData::IMAGE) || (stick_data.stickType == StickData::CIRCLE) || (stick_data.stickType == StickData::RECT) || (stick_data.stickType == StickData::TRAPEZOID) || (stick_data.stickType == StickData::TAPER)) {
        QSize mysize;
        if (stick_data.stickType == StickData::IMAGE) {
            stream >> mysize;
        }
        stream >> stick_data.imgAngle;
        stream >> stick_data.imgAngleOffset;
        stream >> stick_data.imgHScale;
        stream >> stick_data.imgWScale;
        stream >> stick_data.imgOffset;
        stream >> stick_data.hardTop;
        stream >> stick_data.hardBottom;
        if (stick_data.stickType == StickData::IMAGE) {
            int numOfImgs;
            int stickImgIdx;
            stream >> numOfImgs;
            stream >> stickImgIdx;
            for (int i = 0; i < numOfImgs; i++) {
                QImage *img = new QImage(mysize, QImage::Format_ARGB32);
                stream >> *img;
                QString name;
                stick_data.stickImgList.append(img);
                stream >> name;
                stick_data.imgNameList.append(name);
            }
            stick_data.stickImg = stick_data.stickImgList[stickImgIdx];
        }
    }
    return stream;
}
