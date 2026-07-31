#ifndef PATTERNLIBRARY_H
#define PATTERNLIBRARY_H

#include <QDialog>
#include <QImage>
#include <QString>

class QComboBox;
class QListWidget;
class QSpinBox;

/**
 * @file patternlibrary.h
 * @brief The floating pattern picker behind the "Pattern" entry of the Fill and Stroke
 *        type combo boxes.
 *
 * One instance per Widget, created lazily and kept alive for the session (see
 * Widget::ShowPatternLibrary()): the window is a picker, not a modal step, so it stays
 * open next to the canvas while the user tries tiles on different sticks. It is the only
 * consumer of LoadPat() (abrbrush.h) and of the myPatterns folder created by
 * Widget::CreatePaths().
 *
 * Its whole output is one signal carrying a QImage. Turning that into a fill is the
 * caller's one-liner — `QBrush(tile)` is already a Qt::TexturePattern brush that
 * StickData::brush stores, StickGraphicsItem::paint() draws and QDataStream serializes
 * with no extra field anywhere, which is why this class knows nothing about StickData.
 */

/**
 * @brief Floating, non-modal grid of pattern tiles read from the user's .pat libraries.
 *
 * Deliberately shaped after the .abr brush grid in Widget's constructor (collection combo
 * box + icon grid + Import/Refresh): each .pat file is one "collection", listed by its
 * path relative to the patterns folder so real subfolders show up as a prefix.
 */
class PatternLibrary : public QDialog
{
    Q_OBJECT
public:
    /**
     * @brief Builds the window and fills the collection list from @p patterns_path.
     * @param patterns_path the myPatterns folder to scan, recursively, for *.pat files —
     *        passed in rather than read from a global so the class has no dependency on
     *        widget.cpp's file-scope QDir set.
     * @param parent owner window; also the window this one floats above (Qt::Tool).
     */
    explicit PatternLibrary(const QString &patterns_path, QWidget *parent = nullptr);

signals:
    /**
     * @brief Emitted every time the user clicks a tile, and whenever the tile size changes
     *        while one is selected.
     * @param tile the ARGB32 tile, ready to be wrapped in a QBrush by the receiver.
     * @param tile_size how many pixels wide the tile should paint at — see tile_size_spin
     *        for why a raw tile is nearly always the wrong size on a stick.
     *
     * Re-emitting rather than emitting once is what lets the window stay open as a palette:
     * the receiver applies the tile as the current fill/stroke each time, so trying tiles
     * (or sizes) is a click each with no dialog round trip.
     */
    void patternChosen(const QImage &tile, int tile_size);

    /**
     * @brief Emitted when the user asks for the chosen tile to be put ON the selection —
     *        by double-clicking a thumbnail or pressing "Apply to stick".
     *
     * Always preceded by a patternChosen() for the same tile, so the receiver can simply
     * push the current fill/stroke onto the selected stick without re-deriving it. Picking
     * and assigning are separate because the window is a palette: a single click previews
     * into the Style column, and nothing reaches the drawing until this fires.
     */
    void patternApplied();

private:
    /**
     * @brief Rebuilds the collection combo box by walking the patterns folder for *.pat.
     *
     * Preserves the current selection when the same file is still there, so an Import or
     * Refresh does not throw the user back to the first collection.
     */
    void Rescan();

    /**
     * @brief Fills the grid with the tiles of one collection.
     * @param collection absolute path of a .pat file; empty (or unreadable/undecodable)
     *        leaves a single disabled placeholder item explaining what to do.
     */
    void Populate(const QString &collection);

    /**
     * @brief Copies user-picked .pat files into the patterns folder, then shows the last
     *        one imported.
     *
     * Importing means copying, exactly like the .abr brush import: the folder is the
     * library, so a pattern stays available in later sessions without the original file.
     */
    void ImportFiles();

    /**
     * @brief Emits patternChosen() for the grid's current item, if it holds a tile.
     *
     * Shared by the click handler and the tile-size spin box, so changing the size updates
     * the current fill without needing a second click on the tile.
     */
    void EmitCurrent();

    QString folder_path;         ///< the myPatterns folder this window lists (never empty)
    QComboBox *collection_combo; ///< one entry per .pat file found; UserData = its full path
    QListWidget *tile_grid;      ///< icon-mode preview grid; each item's UserRole = its QImage tile

    /**
     * @brief Side, in scene pixels, the chosen tile should paint at.
     *
     * Photoshop pattern tiles are authored for print-sized canvases — the ones in a typical
     * pack are several hundred pixels square — while a stick is a few dozen pixels across.
     * Painted 1:1 such a tile shows a single flat crop of itself and the fill reads as a
     * solid colour, so the receiver scales it (through the brush transform, which Qt
     * serializes) instead of leaving the user to conclude patterns don't work.
     */
    QSpinBox *tile_size_spin;
};

#endif // PATTERNLIBRARY_H
