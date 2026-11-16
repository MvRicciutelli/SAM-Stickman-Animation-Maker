#include "layeroptionsdialog.h"

#include <QColorDialog>
#include <QComboBox>
#include <QDialogButtonBox>
#include <QDoubleSpinBox>
#include <QFormLayout>
#include <QGridLayout>
#include <QGroupBox>
#include <QPushButton>
#include <QSpinBox>
#include <QVBoxLayout>

/**
 * @details Two columns: opacity, the two shadows and blur on the left, glow, outline, colorize
 * and pixelate on the right, so the dialog stays short enough for a laptop screen. Every control reports to
 * Notify(), which is the single source of Changed().
 */
LayerOptionsDialog::LayerOptionsDialog(QWidget *parent)
    : QDialog(parent)
{
    setWindowTitle(tr("Layer options"));

    opacity_spin = MakePercent();
    QFormLayout *opacity_form = new QFormLayout;
    opacity_form->addRow(tr("Layer opacity"), opacity_spin);

    outer = MakeShadow(tr("Outer shadow"));
    inner = MakeShadow(tr("Inner shadow"));

    glow_box = new QGroupBox(tr("Glow"), this);
    glow_box->setCheckable(true);
    glow_type = new QComboBox(this);
    glow_type->addItems({ tr("Outside"), tr("Inside") });
    glow_color = MakeColorButton();
    glow_color->setToolTip(tr("The colour's alpha is the glow's opacity."));
    glow_size = MakeSize();
    glow_smoothness = MakeSize();
    QFormLayout *glow_form = new QFormLayout(glow_box);
    glow_form->addRow(tr("Type"), glow_type);
    glow_form->addRow(tr("Color"), glow_color);
    glow_form->addRow(tr("Size"), glow_size);
    glow_form->addRow(tr("Smoothness"), glow_smoothness);

    outline_box = new QGroupBox(tr("Outline"), this);
    outline_box->setCheckable(true);
    outline_color = MakeColorButton();
    outline_size = MakeSize();
    outline_opacity = MakePercent();
    QFormLayout *outline_form = new QFormLayout(outline_box);
    outline_form->addRow(tr("Color"), outline_color);
    outline_form->addRow(tr("Stroke size"), outline_size);
    outline_form->addRow(tr("Opacity"), outline_opacity);

    colorize_box = new QGroupBox(tr("Colorize"), this);
    colorize_box->setCheckable(true);
    colorize_color = MakeColorButton();
    colorize_opacity = MakePercent();
    QFormLayout *colorize_form = new QFormLayout(colorize_box);
    colorize_form->addRow(tr("Color"), colorize_color);
    colorize_form->addRow(tr("Color opacity"), colorize_opacity);

    blur_box = new QGroupBox(tr("Blur"), this);
    blur_box->setCheckable(true);
    blur_intensity = MakeSize();
    QFormLayout *blur_form = new QFormLayout(blur_box);
    blur_form->addRow(tr("Intensity"), blur_intensity);

    pixelate_box = new QGroupBox(tr("Pixelate"), this);
    pixelate_box->setCheckable(true);
    pixelate_size = MakeSize();
    QFormLayout *pixelate_form = new QFormLayout(pixelate_box);
    pixelate_form->addRow(tr("Pixel size"), pixelate_size);

    QGridLayout *grid = new QGridLayout;
    grid->addLayout(opacity_form, 0, 0);
    grid->addWidget(outer.box, 1, 0);
    grid->addWidget(inner.box, 2, 0);
    grid->addWidget(glow_box, 0, 1, 2, 1);
    grid->addWidget(outline_box, 2, 1);
    grid->addWidget(colorize_box, 3, 1);
    grid->addWidget(blur_box, 3, 0);
    grid->addWidget(pixelate_box, 4, 1);

    QDialogButtonBox *buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel,
                                                     this);
    QVBoxLayout *root = new QVBoxLayout(this);
    root->addLayout(grid);
    root->addWidget(buttons);
    connect(buttons, &QDialogButtonBox::accepted, this, &QDialog::accept);
    connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);

    for(QGroupBox *box : { outer.box, inner.box, glow_box, outline_box, colorize_box, blur_box, pixelate_box })
        connect(box, &QGroupBox::toggled, this, [this]{ Notify(); });
    connect(glow_type, &QComboBox::currentIndexChanged, this, [this]{ Notify(); });
    Seed(LayerOptions());
}

LayerOptionsDialog::ShadowControls LayerOptionsDialog::MakeShadow(const QString &title)
{
    ShadowControls c;
    c.box = new QGroupBox(title, this);
    c.box->setCheckable(true);
    c.direction = new QSpinBox(this);
    c.direction->setRange(0, 359);
    c.direction->setWrapping(true);
    c.direction->setSuffix(tr(" deg"));
    c.direction->setToolTip(tr("Where the shadow falls: 0 = right, 90 = down."));
    connect(c.direction, &QSpinBox::valueChanged, this, [this]{ Notify(); });
    c.distance = MakeSize();
    c.color = MakeColorButton();
    c.smoothness = MakeSize();
    c.opacity = MakePercent();
    QFormLayout *form = new QFormLayout(c.box);
    form->addRow(tr("Direction"), c.direction);
    form->addRow(tr("Distance"), c.distance);
    form->addRow(tr("Color"), c.color);
    form->addRow(tr("Smoothness"), c.smoothness);
    form->addRow(tr("Opacity"), c.opacity);
    return c;
}

QPushButton *LayerOptionsDialog::MakeColorButton()
{
    QPushButton *button = new QPushButton(this);
    button->setFixedWidth(60);
    connect(button, &QPushButton::clicked, this, [this, button]{
        const QColor chosen = QColorDialog::getColor(ButtonColor(button), this, tr("Choose color"),
                                                     QColorDialog::ShowAlphaChannel);
        if(!chosen.isValid())
            return; // the color dialog was cancelled
        SetButtonColor(button, chosen);
        Notify();
    });
    return button;
}

QSpinBox *LayerOptionsDialog::MakePercent()
{
    QSpinBox *spin = new QSpinBox(this);
    spin->setRange(0, 100);
    spin->setSuffix(" %");
    connect(spin, &QSpinBox::valueChanged, this, [this]{ Notify(); });
    return spin;
}

QDoubleSpinBox *LayerOptionsDialog::MakeSize()
{
    QDoubleSpinBox *spin = new QDoubleSpinBox(this);
    spin->setRange(0, 500);
    spin->setDecimals(1);
    spin->setToolTip(tr("In scene units."));
    connect(spin, &QDoubleSpinBox::valueChanged, this, [this]{ Notify(); });
    return spin;
}

/// @details The colour lives in a dynamic property, so no parallel member per button is needed.
void LayerOptionsDialog::SetButtonColor(QPushButton *button, const QColor &color)
{
    button->setProperty("layer_color", color);
    button->setStyleSheet(QString("background-color: %1").arg(color.name(QColor::HexArgb)));
}

QColor LayerOptionsDialog::ButtonColor(const QPushButton *button) const
{
    return button->property("layer_color").value<QColor>();
}

void LayerOptionsDialog::Notify()
{
    if(!seeding)
        emit Changed();
}

void LayerOptionsDialog::Seed(const LayerOptions &o)
{
    seeding = true;
    opacity_spin->setValue(qRound(o.opacity * 100));
    const QList<QPair<ShadowControls, LayerShadow>> shadows = { { outer, o.outer_shadow }, { inner, o.inner_shadow } };
    for(const auto &pair : shadows){
        const ShadowControls &c = pair.first;
        const LayerShadow &s = pair.second;
        c.box->setChecked(s.enabled);
        c.direction->setValue(qRound(s.angle) % 360);
        c.distance->setValue(s.distance);
        SetButtonColor(c.color, s.color);
        c.smoothness->setValue(s.smoothness);
        c.opacity->setValue(qRound(s.opacity * 100));
    }
    glow_box->setChecked(o.glow.enabled);
    glow_type->setCurrentIndex(o.glow.inside ? 1 : 0);
    SetButtonColor(glow_color, o.glow.color);
    glow_size->setValue(o.glow.size);
    glow_smoothness->setValue(o.glow.smoothness);
    outline_box->setChecked(o.outline.enabled);
    SetButtonColor(outline_color, o.outline.color);
    outline_size->setValue(o.outline.size);
    outline_opacity->setValue(qRound(o.outline.opacity * 100));
    colorize_box->setChecked(o.colorize.enabled);
    SetButtonColor(colorize_color, o.colorize.color);
    colorize_opacity->setValue(qRound(o.colorize.opacity * 100));
    blur_box->setChecked(o.blur.enabled);
    blur_intensity->setValue(o.blur.intensity);
    pixelate_box->setChecked(o.pixelate.enabled);
    pixelate_size->setValue(o.pixelate.size);
    seeding = false;
}

LayerOptions LayerOptionsDialog::Options() const
{
    LayerOptions o;
    o.opacity = opacity_spin->value() / 100.0;
    const QList<QPair<const ShadowControls*, LayerShadow*>> shadows = { { &outer, &o.outer_shadow }, { &inner, &o.inner_shadow } };
    for(const auto &pair : shadows){
        const ShadowControls &c = *pair.first;
        LayerShadow &s = *pair.second;
        s.enabled = c.box->isChecked();
        s.angle = c.direction->value();
        s.distance = c.distance->value();
        s.color = ButtonColor(c.color);
        s.smoothness = c.smoothness->value();
        s.opacity = c.opacity->value() / 100.0;
    }
    o.glow.enabled = glow_box->isChecked();
    o.glow.inside = glow_type->currentIndex() == 1;
    o.glow.color = ButtonColor(glow_color);
    o.glow.size = glow_size->value();
    o.glow.smoothness = glow_smoothness->value();
    o.outline.enabled = outline_box->isChecked();
    o.outline.color = ButtonColor(outline_color);
    o.outline.size = outline_size->value();
    o.outline.opacity = outline_opacity->value() / 100.0;
    o.colorize.enabled = colorize_box->isChecked();
    o.colorize.color = ButtonColor(colorize_color);
    o.colorize.opacity = colorize_opacity->value() / 100.0;
    o.blur.enabled = blur_box->isChecked();
    o.blur.intensity = blur_intensity->value();
    o.pixelate.enabled = pixelate_box->isChecked();
    o.pixelate.size = pixelate_size->value();
    return o;
}
