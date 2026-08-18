#ifndef BRUSHPICKER_H
#define BRUSHPICKER_H

#include <abrbrush.h>
#include <QDialog>
#include <QMetaType>
#include <QWidget>
#include <functional>

class QComboBox;
class QLabel;
class QListWidget;
class QPushButton;
class QSpinBox;

/**
 * @file brushpicker.h
 * @brief The brush-tip library, and the dialog that turns a tip into a stick's BORDER.
 *
 * One grid, two users. It was built inline in Widget's constructor for freehand draw mode;
 * the "Brush" entry of the Border type combo needs exactly the same thing — the same folder,
 * the same .abr collections, the same built-in round presets — so the grid moved here rather
 * than being written a second time. Draw mode keeps its own size/opacity/hardness/pressure
 * controls (they act on live drawing state and are laid out among unrelated ones); what is
 * shared is the LIBRARY, which is the part with the file scanning, the .abr decoding and the
 * preview rendering in it.
 */

/**
 * @brief One built-in round tip: not a texture but a set of numbers, since the pencils differ
 *        by hardness/opacity/thickness rather than by shape.
 *
 * Also what a USER-saved brush stores beside its stamp, which is why the fields are @c qint32 and
 * why @c angle / @c opacity_jitter are here at all: the built-in presets leave both at 0, but a
 * saved brush has to come back exactly as it was dialled. They are last so the presets' positional
 * initializers stay valid.
 */
struct BrushPreset {
    QString name;
    qint32 size = 20, opacity = 100, spacing = 25, hardness = 75, roundness = 100;
    qint32 size_jitter = 0, angle_jitter = 0;
    qint32 angle = 0, opacity_jitter = 0;
};
Q_DECLARE_METATYPE(BrushPreset)   // sta nel UserRole+2 dell'item di una punta salvata

/**
 * @brief Collection selector + thumbnail grid + import/refresh over the app's Brushes folder.
 *
 * Each .abr file is one "collection"; the first combo entry is the built-in round tip in its
 * seven preset flavours. Selecting a thumbnail always emits brushChosen(); a built-in one also
 * emits presetChosen(), because a preset carries parameters and an imported tip carries only
 * its shape (whatever the user dialled stays put).
 *
 * A THIRD kind of collection is the user's own: "Save brush" writes the selected stamp plus the
 * host's live parameters into a `.mbrush` file in the same folder, so a tweaked brush comes back
 * next session — parameters included, which is exactly what an .abr cannot carry (and why saving
 * does not try to write one back). Those are the only collections "Delete brush" can edit.
 */
class BrushPicker : public QWidget
{
    Q_OBJECT
public:
    /**
     * @brief Builds the picker over a folder of .abr files.
     * @param brushes_path the Brushes folder (Widget's brush_folder); imports are copied into it.
     * @param parent Qt parent.
     */
    explicit BrushPicker(const QString &brushes_path, QWidget *parent = nullptr);

    /**
     * @brief Regenerates the selected BUILT-IN tip in place, stamp and thumbnail.
     * @param hardness,roundness the round-brush shape parameters.
     * @param size,opacity only affect the thumbnail, which is drawn at the size and opacity the
     *        tip will be used at (otherwise seven presets look like seven identical black discs).
     *
     * In place, without repopulating: repopulating would reset the selection to "Round" every
     * time the hardness spin box moved. A no-op on an .abr collection — there the stamp is the
     * file's and there is nothing to regenerate.
     */
    void RebuildBuiltIn(int hardness, int roundness, int size, int opacity);

    /** @brief The selected tip's stamp, or a null image when the grid is empty. */
    QImage CurrentStamp() const;

    /**
     * @brief Supplies the parameters "Save brush" stores beside the stamp.
     * @param provider called at save time; only the numbers are read, the name is asked for.
     *
     * The picker holds no live parameters of its own — they belong to the host (draw mode's
     * spin boxes write MyView's state, the border dialog's write a BrushStroke), so it asks.
     * Without a provider the Save button is disabled.
     */
    void SetParamsProvider(std::function<BrushPreset()> provider);

    /**
     * @brief Adds the selected stamp plus the provider's parameters to a `.mbrush` collection.
     * @param name the name to store it under; empty (the button's case) asks for one.
     *
     * The target is the CURRENT collection when that is already a user one, otherwise a
     * "My brushes.mbrush" created in the folder — an .abr is a read-only import here, so saving
     * into the Photoshop pack the tip came from is not on offer.
     */
    void SaveBrush(const QString &name = QString());

    /**
     * @brief Removes the selected tip from the current user collection, rewriting its file.
     * @param confirm ask first (what the button does); a collection left empty is deleted.
     */
    void DeleteBrush(bool confirm = true);

signals:
    /** @brief A tip was selected; @p stamp is its coverage mask (alpha), never null. */
    void brushChosen(const QImage &stamp);
    /** @brief A BUILT-IN tip was selected: apply its parameters as well as its shape. */
    void presetChosen(const BrushPreset &preset);

private:
    /** @brief Fills the grid: an empty @p collection means the built-in presets. */
    void Populate(const QString &collection);
    /** @brief Rebuilds the collection combo by scanning the folder for .abr files. */
    void Rescan();
    /** @brief Copies the chosen .abr files into the folder and selects the last one. */
    void Import();

    QString brushes_path;
    QComboBox *folder_combo = nullptr;
    QListWidget *brush_list = nullptr;
    QPushButton *save_btn = nullptr;
    QPushButton *delete_btn = nullptr;
    std::function<BrushPreset()> params_provider;
};

/**
 * @brief The "Brush" border chooser: pick a tip, then tune how it is laid along the outline.
 *
 * Everything a brush border stores lives here (spacing and the three jitters, plus the tip's own
 * angle); colour, thickness and opacity are deliberately absent, because for a border they are
 * the PEN's and the Style column already has a control for each — see BrushStroke (abrbrush.h).
 * Seed() therefore takes them so the preview can show the real thing.
 */
class BrushStrokeDialog : public QDialog
{
    Q_OBJECT
public:
    /**
     * @brief Builds the dialog over the same brushes folder BrushPicker takes.
     */
    explicit BrushStrokeDialog(const QString &brushes_path, QWidget *parent = nullptr);

    /**
     * @brief Loads an existing border brush into the controls.
     * @param brush the brush to show; a null stamp leaves the currently picked tip alone, so
     *        opening the dialog on a stick that has no brush border yet starts from the library
     *        selection rather than from nothing.
     *
     * Re-read on every opening (like TextureOptionsDialog::Seed(), unlike GradientDialog), so the
     * numbers shown are always the selected stick's.
     */
    void Seed(const BrushStroke &brush);

    /**
     * @brief The brush the controls describe.
     * @return the stored half from the controls, plus the size/opacity Seed() supplied and the
     *         COLOUR, which this dialog does edit — it is the pen's colour, and the only other
     *         control that opens a colour picker for a border is the combo's "Solid" entry, which
     *         drops the brush by definition. So without it a brush border's colour would be
     *         whatever the pen happened to hold when the brush was picked, permanently.
     */
    BrushStroke Brush() const { return brush; }

private:
    /** @brief Repaints the sample stroke; called by every control. */
    void RefreshPreview();

    BrushStroke brush;
    QSpinBox *spacing_spin = nullptr;
    QSpinBox *angle_spin = nullptr;
    QSpinBox *size_jitter_spin = nullptr;
    QSpinBox *angle_jitter_spin = nullptr;
    QSpinBox *opacity_jitter_spin = nullptr;
    QPushButton *color_button = nullptr;
    QLabel *preview = nullptr;
    BrushPicker *picker = nullptr;
};

#endif // BRUSHPICKER_H
