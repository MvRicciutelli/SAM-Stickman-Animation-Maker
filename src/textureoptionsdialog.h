#ifndef TEXTUREOPTIONSDIALOG_H
#define TEXTUREOPTIONSDIALOG_H

#include <QDialog>
#include <QBrush>

class QCheckBox;
class QDoubleSpinBox;
class QLabel;
class QSpinBox;

/**
 * @file textureoptionsdialog.h
 * @brief The numeric options of an image fill/stroke, behind the "Image" entry of the Fill
 *        and Border type combo boxes.
 *
 * An image fill is a Qt::TexturePattern QBrush whose placement lives in QBrush::transform()
 * (see TextureXf in stickdata.h). This dialog is the numeric editor of that placement —
 * scale, offset, angle — plus the one thing the transform cannot express, whether the tile
 * repeats (StickData::texture_tiled). Its on-canvas counterpart is MyView's texture arrow
 * handle, which drags the same three numbers directly on the drawing; both exist because a
 * texture needs coarse aiming (the arrow) and exact values (here).
 *
 * Like GradientDialog it is held alive by its owner between uses, but it does not need to be:
 * every control is re-seeded from the brush on each opening, so nothing is carried over.
 */

/**
 * @brief Modal editor for a texture brush's image, placement and tiling.
 */
class TextureOptionsDialog : public QDialog
{
    Q_OBJECT
public:
    /**
     * @brief Builds the editor. The controls are meaningless until Seed() has run.
     * @param parent owner window.
     */
    explicit TextureOptionsDialog(QWidget *parent = nullptr);

    /**
     * @brief Fills the controls from an existing brush.
     * @param brush the brush being edited. A non-texture brush (or one with a null tile)
     *        leaves the dialog with no image, which Texture() then reports as invalid.
     * @param tiled current value of StickData::texture_tiled for the target.
     * @param tiling_editable false for a STROKE texture, which always tiles (clipping an
     *        outline to one instance would chop it into a fragment): the Tile box is then shown
     *        checked but disabled, rather than offering a choice that does nothing.
     *
     * Called before every exec(), so re-opening the dialog on a stick always shows that
     * stick's real values rather than the last ones edited — the gap GradientDialog still
     * has (see CLAUDE.md "Known unresolved items").
     */
    void Seed(const QBrush &brush, bool tiled, bool tiling_editable = true);

    /**
     * @brief The brush the controls currently describe.
     * @return a Qt::TexturePattern brush holding the chosen image with the edited placement
     *         written into its transform, or Qt::NoBrush when no image was ever loaded.
     */
    QBrush Texture() const;

    /**
     * @brief Whether the tile should repeat across the shape.
     * @return the "Tile" check box state, to be stored in StickData::texture_tiled.
     */
    bool Tiled() const;

    /**
     * @brief Asks the user for an image file and adopts it.
     * @return true when an image was loaded, false on cancel or an unreadable file (which
     *         is reported to the user).
     *
     * Public because the caller uses it to get the FIRST image before the dialog is even
     * shown — picking "Image" with no texture yet has to open a file chooser, not an
     * options box for an image that doesn't exist.
     */
    bool ChooseImage();

private:
    /**
     * @brief Repaints the preview swatch with the current image, placement and tiling.
     *
     * The swatch is filled with the very brush Texture() returns, so what it shows is what
     * a shape will show — including, when tiling is off, the single instance and the empty
     * space around it.
     */
    void RefreshPreview();

    QImage tile;              ///< the chosen image at full resolution; null until ChooseImage() succeeds
    QString tile_name;        ///< file name of `tile`, shown next to the Image button

    QLabel *preview_label;    ///< live swatch, repainted by RefreshPreview() on every change
    QLabel *name_label;       ///< shows tile_name and the image's pixel size
    QDoubleSpinBox *scale_spin; ///< tile size as a % of the image's own pixels (100 = 1:1)
    QSpinBox *angle_spin;     ///< tile rotation in degrees, clockwise
    QSpinBox *offset_x_spin;  ///< horizontal displacement of the tile, in scene units
    QSpinBox *offset_y_spin;  ///< same, vertically
    QCheckBox *tile_check;    ///< off = paint one instance instead of repeating (texture_tiled)
};

#endif // TEXTUREOPTIONSDIALOG_H
