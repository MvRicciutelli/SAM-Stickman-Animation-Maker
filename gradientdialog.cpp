#include "gradientdialog.h"

#include <QColorDialog>
#include <QComboBox>
#include <QDialogButtonBox>
#include <QFormLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QListWidget>
#include <QPainter>
#include <QPushButton>
#include <QSpinBox>
#include <QVBoxLayout>
#include <QtMath>

namespace {

/// Scacchiera di fondo: senza di essa un colore semitrasparente si legge come una tinta
/// piatta più chiara, cioè l'alpha appena impostato sembra non aver fatto niente.
void Checkerboard(QPainter *painter, const QRect &area)
{
    for(int y = area.top(); y <= area.bottom(); y += 8)
        for(int x = area.left(); x <= area.right(); x += 8)
            painter->fillRect(QRect(x, y, 8, 8).intersected(area),
                              ((x/8 + y/8) % 2) ? QColor(235,235,235) : QColor(205,205,205));
}

/// A colour chip for one stop's row in the list.
QIcon ColorSwatch(const QColor &color)
{
    QPixmap chip(24, 16);
    chip.fill(Qt::transparent);
    QPainter painter(&chip);
    Checkerboard(&painter, chip.rect());
    painter.fillRect(chip.rect(), color);
    painter.setPen(Qt::darkGray);          // border: a white stop on a white row is invisible
    painter.drawRect(0, 0, chip.width()-1, chip.height()-1);
    return QIcon(chip);
}

} // namespace

/**
 * @details The three blocks of the layout map one-to-one onto what a gradient is: the stop
 * list (which colours), the geometry form (where they run), and the preview. Radial-only
 * controls are built for both kinds but disabled for LINEAR, the same "grey out what this
 * type cannot act on" convention Animation::UpdateSliders() uses for a TEXT stick's
 * placement panel — an always-present layout is easier to read than one that reflows.
 *
 * ponytail: the controls keep their values because the owner keeps the instance alive, not
 * because the dialog can read an existing gradient back. Opening the editor on a stick
 * loaded from a file therefore starts from the last edited gradient, not from that stick's
 * own one. Upgrade path if that bites: a `Seed(const QGradient&)` that inverts the
 * Gradient() geometry (atan2 for the angle, centre minus 0.5 for the offsets).
 */
GradientDialog::GradientDialog(Kind kind, QWidget *parent)
    : QDialog(parent), kind(kind)
{
    setWindowTitle(kind == LINEAR ? tr("Linear gradient") : tr("Radial gradient"));

    // il gradiente di partenza: due stop, il minimo che si veda come gradiente
    stops.append(QGradientStop(0.0, Qt::white));
    stops.append(QGradientStop(1.0, Qt::black));

    QVBoxLayout *rows = new QVBoxLayout(this);

    // ---- stops: la lista è la vista di `stops`, i bottoni la modificano ----
    stop_list = new QListWidget(this);
    stop_list->setMaximumHeight(110);
    rows->addWidget(new QLabel(tr("Colors"), this));
    rows->addWidget(stop_list);

    QHBoxLayout *stop_buttons = new QHBoxLayout();
    QPushButton *add_button = new QPushButton(tr("Add"), this);
    QPushButton *remove_button = new QPushButton(tr("Remove"), this);
    QPushButton *color_button = new QPushButton(tr("Color..."), this);
    stop_buttons->addWidget(add_button);
    stop_buttons->addWidget(remove_button);
    stop_buttons->addWidget(color_button);
    rows->addLayout(stop_buttons);

    // ---- geometria: tutto in percentuale della forma, mai in pixel di scena ----
    QFormLayout *form = new QFormLayout();
    position_spin = new QSpinBox(this);
    position_spin->setRange(0, 100);
    position_spin->setSuffix(tr("%"));
    position_spin->setToolTip(tr("Where the selected color sits along the gradient"));
    form->addRow(tr("Stop position"), position_spin);

    alpha_spin = new QSpinBox(this);
    alpha_spin->setObjectName("gradient_stop_alpha"); // la presa dei test, che non linkano widget.cpp
    alpha_spin->setRange(0, 100);
    alpha_spin->setSuffix(tr("%"));
    alpha_spin->setToolTip(tr("Opacity of the selected color; 0% makes the gradient fade out "
                              "to fully transparent there"));
    form->addRow(tr("Stop opacity"), alpha_spin);

    spread_combo = new QComboBox(this);
    spread_combo->addItem(tr("Pad"), int(QGradient::PadSpread));
    spread_combo->addItem(tr("Repeat"), int(QGradient::RepeatSpread));
    spread_combo->addItem(tr("Reflect"), int(QGradient::ReflectSpread));
    spread_combo->setToolTip(tr("How the area beyond the last stop is filled"));
    form->addRow(tr("Spread"), spread_combo);

    angle_spin = new QSpinBox(this);
    angle_spin->setRange(-180, 180);
    angle_spin->setSuffix(QString(QChar(0xB0))); // il grado, senza dipendere dall'encoding del sorgente
    angle_spin->setToolTip(kind == LINEAR ? tr("Direction the colors run in")
                                          : tr("Direction the highlight is pushed towards"));
    form->addRow(tr("Angle"), angle_spin);

    offset_x_spin = new QSpinBox(this);
    offset_x_spin->setRange(-100, 100);
    offset_x_spin->setSuffix(tr("%"));
    offset_x_spin->setToolTip(tr("Horizontal shift of the center, in % of the shape"));
    form->addRow(tr("Offset X"), offset_x_spin);

    offset_y_spin = new QSpinBox(this);
    offset_y_spin->setRange(-100, 100);
    offset_y_spin->setSuffix(tr("%"));
    offset_y_spin->setToolTip(tr("Vertical shift of the center, in % of the shape"));
    form->addRow(tr("Offset Y"), offset_y_spin);

    radius_spin = new QSpinBox(this);
    radius_spin->setRange(1, 200);
    radius_spin->setValue(50);
    radius_spin->setSuffix(tr("%"));
    radius_spin->setEnabled(kind == RADIAL);
    radius_spin->setToolTip(tr("Radius of the circle, in % of the shape"));
    form->addRow(tr("Radius"), radius_spin);

    focus_spin = new QSpinBox(this);
    focus_spin->setRange(0, 100);
    focus_spin->setSuffix(tr("%"));
    focus_spin->setEnabled(kind == RADIAL);
    focus_spin->setToolTip(tr("How far the highlight sits from the center (0% = centered, "
                              "which makes Angle irrelevant)"));
    form->addRow(tr("Focus"), focus_spin);
    rows->addLayout(form);

    // ---- anteprima: il rettangolo riempito È quello che si vedrà sulla forma ----
    preview_label = new QLabel(this);
    preview_label->setMinimumHeight(60);
    preview_label->setFrameShape(QFrame::StyledPanel);
    rows->addWidget(preview_label, 1);

    QDialogButtonBox *buttons = new QDialogButtonBox(QDialogButtonBox::Ok|QDialogButtonBox::Cancel, this);
    connect(buttons, &QDialogButtonBox::accepted, this, &QDialog::accept);
    connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);
    rows->addWidget(buttons);

    // aggiungi uno stop a metà strada, colore di quello selezionato: si vede subito dove è
    // finito e resta all'utente solo cambiargli colore
    connect(add_button, &QPushButton::clicked, this, [this]{
        const int row = qMax(0, stop_list->currentRow());
        const qreal position = qMin(1.0, stops[row].first + 0.25);
        stops.append(QGradientStop(position, stops[row].second));
        std::sort(stops.begin(), stops.end(),
                  [](const QGradientStop &a, const QGradientStop &b){ return a.first < b.first; });
        RebuildStopList();
        RefreshPreview();
    });
    // due stop sono il minimo per un gradiente: sotto non si scende
    connect(remove_button, &QPushButton::clicked, this, [this]{
        const int row = stop_list->currentRow();
        if(row < 0 || stops.count() <= 2)
            return;
        stops.removeAt(row);
        RebuildStopList();
        RefreshPreview();
    });
    auto pick_color = [this]{
        const int row = stop_list->currentRow();
        if(row < 0)
            return;
        const QColor color = QColorDialog::getColor(stops[row].second, this, tr("Stop color"),
                                                    QColorDialog::ShowAlphaChannel);
        if(!color.isValid())
            return;
        stops[row].second = color;
        RebuildStopList();
        SyncStopSpins(); // il picker può aver cambiato anche l'alpha: riallinea il suo spinbox
        RefreshPreview();
    };
    connect(color_button, &QPushButton::clicked, this, pick_color);
    connect(stop_list, &QListWidget::itemDoubleClicked, this, pick_color);
    // selezionare uno stop porta i suoi valori negli spinbox senza rilanciare le loro
    // connect, che riscriverebbero lo stop appena selezionato
    connect(stop_list, &QListWidget::currentRowChanged, this, [this](int){ SyncStopSpins(); });
    connect(position_spin, QOverload<int>::of(&QSpinBox::valueChanged), this, [this](int value){
        const int row = stop_list->currentRow();
        if(row < 0 || row >= stops.count())
            return;
        stops[row].first = value / 100.0;
        // riordinare può cambiare quale stop occupa la riga corrente: la selezione resta
        // sull'indice, non sullo stop trascinato (visibile nell'anteprima, innocuo)
        std::sort(stops.begin(), stops.end(),
                  [](const QGradientStop &a, const QGradientStop &b){ return a.first < b.first; });
        RebuildStopList();
        RefreshPreview();
    });
    connect(alpha_spin, QOverload<int>::of(&QSpinBox::valueChanged), this, [this](int value){
        const int row = stop_list->currentRow();
        if(row < 0 || row >= stops.count())
            return;
        stops[row].second.setAlphaF(value / 100.0);
        RebuildStopList();
        RefreshPreview();
    });

    // ogni controllo di geometria ridipinge solo l'anteprima: il gradiente vero lo
    // ricostruisce Gradient() al momento in cui il chiamante lo chiede
    const QList<QSpinBox*> geometry = { angle_spin, offset_x_spin, offset_y_spin,
                                        radius_spin, focus_spin };
    for(QSpinBox *spin : geometry)
        connect(spin, QOverload<int>::of(&QSpinBox::valueChanged), this,
                [this](int){ RefreshPreview(); });
    connect(spread_combo, QOverload<int>::of(&QComboBox::currentIndexChanged), this,
            [this](int){ RefreshPreview(); });

    RebuildStopList();
    SyncStopSpins();
    RefreshPreview();
}

/// @details Kept out of RebuildStopList() on purpose: a position edit re-sorts the stops, so
/// syncing there would snap the position box to whatever stop the current ROW now holds,
/// under the user's own cursor. Only a selection change and the colour picker need the echo.
void GradientDialog::SyncStopSpins()
{
    const int row = stop_list->currentRow();
    if(row < 0 || row >= stops.count())
        return;
    QSignalBlocker mute_position(position_spin);
    QSignalBlocker mute_alpha(alpha_spin);
    position_spin->setValue(qRound(stops[row].first * 100));
    alpha_spin->setValue(qRound(stops[row].second.alphaF() * 100));
}

/// @details See the header: the list is a pure view, so it is cleared and refilled rather
/// than patched, and the previously selected row is restored by index.
void GradientDialog::RebuildStopList()
{
    const int previous = stop_list->currentRow();
    QSignalBlocker mute(stop_list);
    stop_list->clear();
    for(const QGradientStop &stop : stops)
        stop_list->addItem(new QListWidgetItem(ColorSwatch(stop.second),
                                               tr("%1%  %2").arg(qRound(stop.first * 100))
                                               .arg(stop.second.name(QColor::HexArgb))));
    stop_list->setCurrentRow(qBound(0, previous < 0 ? 0 : previous, stops.count()-1));
}

/// @details Painting into a plain rectangle is a faithful preview precisely because the
/// gradient is in ObjectMode: the same 0..1 box maps onto the swatch here and onto the
/// stick's shape on the canvas.
void GradientDialog::RefreshPreview()
{
    const QSize size = preview_label->contentsRect().size().expandedTo(QSize(120, 56));
    QPixmap swatch(size);
    swatch.fill(Qt::transparent);
    QPainter painter(&swatch);
    Checkerboard(&painter, swatch.rect());
    painter.setPen(Qt::NoPen);
    painter.setBrush(QBrush(Gradient()));
    painter.drawRect(swatch.rect());
    painter.end();
    preview_label->setPixmap(swatch);
}

/**
 * @details The geometry is built in the 0..1 object box: (0.5,0.5) is the shape's middle,
 * so the offset spin boxes are a straight displacement from it. The angle is measured in
 * screen convention (x right, y DOWN, so a positive angle turns clockwise on screen),
 * which is the same convention as StickData::img_angle and the rotation spin box — a
 * gradient at the same angle as a stick reads as parallel to it.
 *
 * A LINEAR axis is one box-width long, centred: the stops span the shape exactly at
 * offset 0. A RADIAL gradient puts the focal point (the highlight) `Focus` percent of the
 * radius away from the centre along the angle; at Focus 0 the two coincide and the
 * gradient is concentric.
 */
QGradient GradientDialog::Gradient() const
{
    const QPointF center(0.5 + offset_x_spin->value()/100.0,
                         0.5 + offset_y_spin->value()/100.0);
    const qreal radians = qDegreesToRadians(qreal(angle_spin->value()));
    const QPointF direction(qCos(radians), qSin(radians));

    QGradient gradient;
    if(kind == LINEAR){
        gradient = QLinearGradient(center - direction * 0.5, center + direction * 0.5);
    } else {
        const qreal radius = radius_spin->value()/100.0;
        gradient = QRadialGradient(center, radius,
                                   center + direction * (radius * focus_spin->value()/100.0));
    }
    // ObjectMode: il gradiente vive nel bounding box della forma dipinta, così segue
    // spostamenti, rotazioni e cambi di lunghezza dello stick senza ricalcoli
    gradient.setCoordinateMode(QGradient::ObjectMode);
    gradient.setSpread(QGradient::Spread(spread_combo->currentData().toInt()));
    gradient.setStops(stops);
    return gradient;
}
