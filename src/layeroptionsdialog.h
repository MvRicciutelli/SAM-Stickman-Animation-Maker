#ifndef LAYEROPTIONSDIALOG_H
#define LAYEROPTIONSDIALOG_H

#include <QDialog>
#include "layeroptions.h"

class QComboBox;
class QDoubleSpinBox;
class QGroupBox;
class QPushButton;
class QSpinBox;

/**
 * @file layeroptionsdialog.h
 * @brief Editor of one layer's LayerOptions, opened from the layer list's context menu.
 *
 * Each effect is a checkable group box whose check state is the effect's on/off switch. The
 * caller (Widget's layer list context menu) previews every Changed() on the canvas and restores
 * the previous options on Cancel; OK is one undo step.
 */

/**
 * @brief Modal editor for a stickfigure layer's opacity and effects.
 */
class LayerOptionsDialog : public QDialog
{
    Q_OBJECT
public:
    /**
     * @brief Builds the editor. The controls hold defaults until Seed() runs.
     * @param parent owner window.
     */
    explicit LayerOptionsDialog(QWidget *parent = nullptr);

    /**
     * @brief Fills the controls from a layer's options without emitting Changed().
     * @param options the options being edited.
     */
    void Seed(const LayerOptions &options);

    /**
     * @brief The options the controls currently describe.
     * @return a full LayerOptions value.
     */
    LayerOptions Options() const;

signals:
    /**
     * @brief Emitted on every user change of any control, for the live preview.
     */
    void Changed();

private:
    /// @brief The controls of one shadow (outer or inner): same fields, two boxes.
    struct ShadowControls {
        QGroupBox *box;            ///< checkable: the shadow's on/off switch
        QSpinBox *direction;       ///< LayerShadow::angle
        QDoubleSpinBox *distance;  ///< LayerShadow::distance
        QPushButton *color;        ///< LayerShadow::color
        QDoubleSpinBox *smoothness;///< LayerShadow::smoothness
        QSpinBox *opacity;         ///< LayerShadow::opacity, in %
    };

    /**
     * @brief Builds one shadow's group box.
     * @param title box title. @return the controls, owned by this dialog.
     */
    ShadowControls MakeShadow(const QString &title);
    /// @brief A button showing a colour, which opens QColorDialog. @return the button.
    QPushButton *MakeColorButton();
    /// @brief A 0..100 % spin box. @return the spin box.
    QSpinBox *MakePercent();
    /// @brief A scene-unit size spin box. @return the spin box.
    QDoubleSpinBox *MakeSize();
    /// @brief Paints @p color on @p button and stores it there. @param button target. @param color value.
    void SetButtonColor(QPushButton *button, const QColor &color);
    /// @brief The colour stored by SetButtonColor(). @param button source. @return the colour.
    QColor ButtonColor(const QPushButton *button) const;
    /// @brief Emits Changed() unless Seed() is filling the controls.
    void Notify();

    bool seeding = false;          ///< raised by Seed(): control signals are not user changes

    QSpinBox *opacity_spin;        ///< LayerOptions::opacity, in %
    ShadowControls outer;          ///< LayerOptions::outer_shadow
    ShadowControls inner;          ///< LayerOptions::inner_shadow
    QGroupBox *glow_box;           ///< LayerGlow::enabled
    QComboBox *glow_type;          ///< LayerGlow::inside (0 = outside, 1 = inside)
    QPushButton *glow_color;       ///< LayerGlow::color (alpha = glow opacity)
    QDoubleSpinBox *glow_size;     ///< LayerGlow::size
    QDoubleSpinBox *glow_smoothness; ///< LayerGlow::smoothness
    QGroupBox *outline_box;        ///< LayerOutline::enabled
    QPushButton *outline_color;    ///< LayerOutline::color
    QDoubleSpinBox *outline_size;  ///< LayerOutline::size
    QSpinBox *outline_opacity;     ///< LayerOutline::opacity, in %
    QGroupBox *colorize_box;       ///< LayerColorize::enabled
    QPushButton *colorize_color;   ///< LayerColorize::color
    QSpinBox *colorize_opacity;    ///< LayerColorize::opacity, in %
    QGroupBox *blur_box;           ///< LayerBlur::enabled
    QDoubleSpinBox *blur_intensity; ///< LayerBlur::intensity
    QGroupBox *pixelate_box;       ///< LayerPixelate::enabled
    QDoubleSpinBox *pixelate_size; ///< LayerPixelate::size
};

#endif // LAYEROPTIONSDIALOG_H
