#ifndef ABRBRUSH_H
#define ABRBRUSH_H

#include <QImage>
#include <QList>
#include <QString>

/**
 * @brief One brush tip parsed from a Photoshop .abr file.
 *
 * @details `stamp` is an ARGB32 image whose ALPHA channel is the brush coverage
 * (RGB is left black; DrawMode tints the dab with the active brush color, exactly
 * like the built-in round stamp). This matches what MyView::StampBrush() expects
 * of draw_brush_stamp, so an AbrBrush stamp is a drop-in replacement for it.
 */
struct AbrBrush {
    QString name;
    QImage  stamp;
};

/**
 * @brief Parse a Photoshop .abr v1/v2 file, returning every *sampled* brush tip.
 * @param path Filesystem path to the .abr file.
 * @return List of sampled brushes. Empty if the file is missing, unreadable, not
 *         version 1/2, or contains only computed brushes. Computed brushes are
 *         skipped and version 6+ (the 8BIM-based CS format) is not handled — Phase 2
 *         is sampled-v2 scope by design.
 */
QList<AbrBrush> LoadAbr(const QString &path);

#endif // ABRBRUSH_H
