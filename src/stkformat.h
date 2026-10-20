#ifndef STKFORMAT_H
#define STKFORMAT_H
#include <QPointF>
#include <QString>
#include <QStringList>

class StickFigureData;

/**
 * @brief Import/export of foreign stickfigure formats: Pivot Animator .stk (modern
 *        compressed "Pivot 3/4/5" variant and legacy uncompressed "Pivot 2" variant)
 *        and Stick Nodes .nodes.
 *
 * Pure model layer: converts between files and StickFigureData, no scene or widget
 * dependency. The binary layouts, the mapping tables and every defensive rule
 * implemented here are specified in STK_IMPORT_EXPORT_PLAN.md — section references
 * in stkformat.cpp point into that document. Conversion is conservative: foreign
 * features with no local equivalent degrade to plain sticks (or are skipped) and are
 * reported through the warnings list; nothing is ever guessed past an unknown layout.
 */
namespace StkFormat {

/**
 * @brief Which .stk dialect ExportStk() writes.
 */
enum StkVariant {
    PIVOT5, ///< modern compressed format ('z' + zlib), full feature set
    PIVOT2  ///< legacy uncompressed format, the one Stick Nodes can import; max 255 sticks, no colors
};

/**
 * @brief Reads a Pivot .stk file (either variant, auto-detected) into a new figure record.
 * @param path file to read.
 * @param target_center scene point the imported figure's bounding-box center is moved to
 *        (STK coordinates are origin-relative, so the raw figure sits around (0,0)).
 * @param warnings receives one line per degraded/skipped foreign feature; never null.
 * @param error receives the failure reason when nullptr is returned; never null.
 * @return heap-allocated figure (caller owns), or nullptr on failure.
 */
StickFigureData *ImportStk(const QString &path, const QPointF &target_center,
                           QStringList *warnings, QString *error);

/**
 * @brief Reads a Stick Nodes .nodes file (uncompressed big-endian or zlib little-endian,
 *        auto-detected; versions 160..403 build 72) into a new figure record.
 * @param path file to read.
 * @param target_center same recentering as ImportStk().
 * @param warnings receives one line per degraded/skipped foreign feature; never null.
 * @param error receives the failure reason when nullptr is returned; never null.
 * @return heap-allocated figure (caller owns), or nullptr on failure.
 */
StickFigureData *ImportNodes(const QString &path, const QPointF &target_center,
                             QStringList *warnings, QString *error);

/**
 * @brief Writes a figure record as a Pivot .stk file.
 * @param path destination file.
 * @param figure figure to convert (typically from ExtractStickFigureData()).
 * @param variant output dialect; PIVOT2 additionally drops colors and caps at 255 sticks.
 * @param warnings receives one line per local feature the format cannot represent; never null.
 * @param error receives the failure reason when false is returned; never null.
 * @return true on success.
 */
bool ExportStk(const QString &path, const StickFigureData &figure, StkVariant variant,
               QStringList *warnings, QString *error);

/**
 * @brief Writes a figure record as a Stick Nodes .nodes file.
 *
 * Always emits the newest supported dialect (version 403 build 72, whole-file zlib,
 * little-endian — what current Stick Nodes itself writes); older .nodes dialects are
 * deliberately not emitted. Both pose representations the format carries (the
 * local-angle chain, which the Stick Nodes code treats as authoritative, and the
 * cached local x/y offsets) are written consistently, so the figure decodes
 * identically whichever one a reader trusts.
 * @param path destination file.
 * @param figure figure to convert (typically from ExtractStickFigureData()).
 * @param warnings receives one line per local feature the format cannot represent; never null.
 * @param error receives the failure reason when false is returned; never null.
 * @return true on success.
 */
bool ExportNodes(const QString &path, const StickFigureData &figure,
                 QStringList *warnings, QString *error);

} // namespace StkFormat

#endif // STKFORMAT_H
