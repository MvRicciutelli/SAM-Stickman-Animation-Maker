#ifndef GRADIENTDIALOG_H
#define GRADIENTDIALOG_H

#include <QDialog>
#include <QGradient>

class QComboBox;
class QLabel;
class QListWidget;
class QSpinBox;

/**
 * @file gradientdialog.h
 * @brief The gradient editor behind the "Linear gradient" / "Radial gradient" entries of
 *        the Fill and Stroke type combo boxes.
 *
 * Everything this dialog produces is one QGradient, which the caller wraps in a QBrush
 * (`QBrush(dialog.Gradient())`) and stores in StickData::brush or QPen::setBrush(). No
 * part of a gradient needs a new StickData field: QDataStream's own QBrush operator
 * already writes the stops, the spread, the coordinate mode and the geometry.
 *
 * @note Every gradient built here uses QGradient::ObjectMode, i.e. coordinates in the
 * 0..1 box of whatever shape is being painted. That is the single decision that makes a
 * gradient fill free at every other layer: the gradient stretches with the shape, so it
 * follows a stick's move, rotate, scale and length change with no per-frame
 * recomputation and nothing to keep in sync. All the geometry spin boxes below are
 * therefore percentages of the shape, never scene pixels.
 */

/**
 * @brief Modal editor for a linear or radial gradient: stops, spread and geometry.
 *
 * @note A stop's ALPHA is edited here too, by its own spin box as well as through the colour
 * picker's alpha channel, because that is the only place a gradient's transparency can live
 * (see Widget::SetPaintOpacity()) — and it is the only way to build a gradient that FADES,
 * which the one-number opacity slider cannot express. The stop chips and the preview are
 * painted over a checkerboard for the same reason: a semi-transparent stop over an opaque
 * background is indistinguishable from a paler solid one.
 *
 * Held alive by its owner between uses (see Widget's members) so the controls keep their
 * values from one opening to the next.
 */
class GradientDialog : public QDialog
{
    Q_OBJECT
public:
    /**
     * @brief Which of the two gradient shapes this instance edits.
     *
     * Fixed at construction: it decides which geometry controls are meaningful, and the
     * two kinds are separate dialog instances rather than one with a type switch.
     */
    enum Kind {
        LINEAR, ///< QLinearGradient — the axis is a direction (Angle) through the centre
        RADIAL  ///< QRadialGradient — a circle (Radius) with an off-centre highlight (Focus/Angle)
    };

    /**
     * @brief Builds the editor for one gradient kind, seeded with a plain white-to-black
     *        two-stop gradient.
     * @param kind LINEAR or RADIAL; cannot change afterwards.
     * @param parent owner window.
     */
    explicit GradientDialog(Kind kind, QWidget *parent = nullptr);

    /**
     * @brief Builds the gradient currently described by the controls.
     * @return an ObjectMode QGradient of this dialog's Kind, with the edited stops and
     *         spread. Safe to call whether or not the dialog was accepted — it only ever
     *         reads the controls.
     *
     * Returned by value: QGradient is a value class that carries the concrete linear or
     * radial geometry in its base, which is exactly what QBrush's constructor reads.
     */
    QGradient Gradient() const;

private:
    /**
     * @brief Repaints the stop list from `stops` and syncs the position spin box.
     *
     * `stops` is the model; the list widget is only ever a view of it, so every edit goes
     * through this rather than mutating items in place.
     */
    void RebuildStopList();

    /**
     * @brief Re-reads the selected stop's position and alpha into their spin boxes.
     *
     * Mandatory rather than cosmetic: without it the alpha box would keep the previously
     * selected stop's value, and the next nudge would slam the newly selected stop to it —
     * the same echo Animation::UpdateStyleControls() owes the opacity sliders.
     */
    void SyncStopSpins();

    /**
     * @brief Repaints the preview swatch with Gradient().
     *
     * Filling the swatch rectangle with the ObjectMode gradient shows precisely what a
     * shape will look like, since ObjectMode maps the gradient to whatever rectangle is
     * being filled.
     */
    void RefreshPreview();

    Kind kind;              ///< the shape this instance edits; set once, in the constructor
    QGradientStops stops;   ///< the model: colour stops at positions 0..1, kept sorted by position

    QListWidget *stop_list;   ///< view of `stops`; the current row is the stop the controls edit
    QSpinBox *position_spin;  ///< position of the selected stop, 0..100 % along the gradient
    QSpinBox *alpha_spin;     ///< opacity of the selected stop's colour, 0..100 % (its alpha)
    QSpinBox *angle_spin;     ///< LINEAR: axis direction. RADIAL: direction of the Focus highlight
    QSpinBox *focus_spin;     ///< RADIAL only: highlight distance from the centre, 0..100 % of Radius
    QSpinBox *offset_x_spin;  ///< centre displacement from the shape's middle, -100..100 % of its width
    QSpinBox *offset_y_spin;  ///< same, vertically, in % of the shape's height
    QSpinBox *radius_spin;    ///< RADIAL only: circle radius, 1..200 % of the shape's box
    QComboBox *spread_combo;  ///< what happens outside the stops: Pad / Repeat / Reflect
    QLabel *preview_label;    ///< live swatch, repainted by RefreshPreview() on every change
};

#endif // GRADIENTDIALOG_H
