#include "textureoptionsdialog.h"
#include "stickdata.h"

#include <QCheckBox>
#include <QDialogButtonBox>
#include <QDoubleSpinBox>
#include <QFileDialog>
#include <QFileInfo>
#include <QFormLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QMessageBox>
#include <QPainter>
#include <QPainterPath>
#include <QPushButton>
#include <QSpinBox>
#include <QVBoxLayout>

/**
 * @details The Image button sits at the TOP, above the placement controls, because it is what
 * the rest of the dialog acts on: changing the picture and then adjusting how it sits reads in
 * that order. Everything else is a plain QFormLayout — the controls are the three numbers of a
 * TextureXf plus the tiling flag, and there is nothing to lay out cleverly.
 */
TextureOptionsDialog::TextureOptionsDialog(QWidget *parent)
    : QDialog(parent)
{
    setWindowTitle(tr("Image options"));

    QPushButton *image_button = new QPushButton(tr("Image..."), this);
    name_label = new QLabel(tr("(no image)"), this);
    QHBoxLayout *image_row = new QHBoxLayout;
    image_row->addWidget(image_button);
    image_row->addWidget(name_label, 1);

    preview_label = new QLabel(this);
    preview_label->setFixedSize(180, 120);
    preview_label->setFrameShape(QFrame::StyledPanel);

    scale_spin = new QDoubleSpinBox(this);
    scale_spin->setRange(0.1, 10000.0);
    scale_spin->setDecimals(1);
    scale_spin->setSuffix(" %");
    scale_spin->setToolTip(tr("Painted size of the tile, as a percentage of the image's own "
                              "pixels. 100 % paints it 1:1 with the scene."));

    angle_spin = new QSpinBox(this);
    angle_spin->setRange(-360, 360);
    angle_spin->setSuffix(tr(" deg"));

    offset_x_spin = new QSpinBox(this);
    offset_y_spin = new QSpinBox(this);
    for(QSpinBox *spin : { offset_x_spin, offset_y_spin }){
        spin->setRange(-10000, 10000);
        spin->setToolTip(tr("Displacement of the tile in scene units."));
    }

    tile_check = new QCheckBox(tr("Tile (repeat across the shape)"), this);
    tile_check->setToolTip(tr("On: the image repeats to cover the shape. Off: one single "
                              "instance is painted, wherever the scale, angle and offset "
                              "below put it."));

    QFormLayout *form = new QFormLayout;
    form->addRow(tr("Scale"), scale_spin);
    form->addRow(tr("Angle"), angle_spin);
    form->addRow(tr("Offset X"), offset_x_spin);
    form->addRow(tr("Offset Y"), offset_y_spin);

    QDialogButtonBox *buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel,
                                                     this);
    QVBoxLayout *root = new QVBoxLayout(this);
    root->addLayout(image_row);
    root->addWidget(preview_label, 0, Qt::AlignHCenter);
    root->addLayout(form);
    root->addWidget(tile_check);
    root->addWidget(buttons);

    connect(image_button, &QPushButton::clicked, this, [this]{ ChooseImage(); });
    connect(buttons, &QDialogButtonBox::accepted, this, &QDialog::accept);
    connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);
    connect(scale_spin, &QDoubleSpinBox::valueChanged, this, [this]{ RefreshPreview(); });
    connect(angle_spin, &QSpinBox::valueChanged, this, [this]{ RefreshPreview(); });
    connect(offset_x_spin, &QSpinBox::valueChanged, this, [this]{ RefreshPreview(); });
    connect(offset_y_spin, &QSpinBox::valueChanged, this, [this]{ RefreshPreview(); });
    connect(tile_check, &QCheckBox::toggled, this, [this]{ RefreshPreview(); });
}

/// @details The brush transform's scale is the tile's painted size divided by its pixel size,
/// so the percentage the user reads is that factor times 100 — a number that means the same
/// thing for a 32 px pattern tile and a 4000 px photograph, which a raw factor would not.
void TextureOptionsDialog::Seed(const QBrush &brush, bool tiled, bool tiling_editable)
{
    if(brush.style() == Qt::TexturePattern && !brush.textureImage().isNull()){
        // cacheKey e non un confronto pixel per pixel: serve solo sapere se è UN'ALTRA
        // immagine, per non lasciare accanto il nome del file precedente
        if(tile.cacheKey() != brush.textureImage().cacheKey())
            tile_name = tr("(current image)");
        tile = brush.textureImage();
    }
    const TextureXf xf = ReadTextureXf(brush);
    {
        // segnali bloccati: ogni setValue ridisegnerebbe il preview su uno stato ancora a
        // metà, e lo ridisegniamo una volta sola alla fine

        QSignalBlocker m1(scale_spin), m2(angle_spin), m3(offset_x_spin),
                       m4(offset_y_spin), m5(tile_check);
        scale_spin->setValue(xf.scale * 100.0);
        angle_spin->setValue(qRound(xf.angle));
        offset_x_spin->setValue(qRound(xf.offset.x()));
        offset_y_spin->setValue(qRound(xf.offset.y()));
        tile_check->setChecked(tiling_editable ? tiled : true);
    }
    tile_check->setEnabled(tiling_editable);
    name_label->setText(tile.isNull() ? tr("(no image)")
                                      : tr("%1  (%2x%3)").arg(tile_name)
                                            .arg(tile.width()).arg(tile.height()));
    RefreshPreview();
}

QBrush TextureOptionsDialog::Texture() const
{
    if(tile.isNull())
        return QBrush(Qt::NoBrush);
    QBrush brush(tile);
    TextureXf xf;
    xf.scale = scale_spin->value() / 100.0;
    xf.angle = angle_spin->value();
    xf.offset = QPointF(offset_x_spin->value(), offset_y_spin->value());
    WriteTextureXf(&brush, xf);
    return brush;
}

bool TextureOptionsDialog::Tiled() const
{
    return tile_check->isChecked();
}

/// @details Adopting a new image deliberately leaves the placement controls alone: swapping the
/// picture for another one of a similar size is the common case, and re-dialling the scale and
/// offset every time would be busywork.
bool TextureOptionsDialog::ChooseImage()
{
    const QString path = QFileDialog::getOpenFileName(this, tr("Fill image"), QString(),
                             tr("Images (*.png *.jpg *.jpeg *.bmp *.gif *.tif *.tiff *.webp)"));
    if(path.isEmpty())
        return false;
    const QImage loaded(path);
    if(loaded.isNull()){
        QMessageBox::warning(this, tr("Fill image"), tr("Cannot read %1.").arg(path));
        return false;
    }
    tile = loaded;
    tile_name = QFileInfo(path).fileName();
    name_label->setText(tr("%1  (%2x%3)").arg(tile_name).arg(tile.width()).arg(tile.height()));
    RefreshPreview();
    return true;
}

/// @details Painted with Texture() itself, so the swatch and the drawing cannot disagree. The
/// non-tiling case is faked the same way StickGraphicsItem::SingleInstanceClip() does it for
/// real — fill with the ordinary repeating brush, clipped to the one instance the transform
/// places — because Qt has no non-repeating brush style to preview instead.
void TextureOptionsDialog::RefreshPreview()
{
    QPixmap swatch(preview_label->size());
    swatch.fill(Qt::transparent);
    QPainter painter(&swatch);
    // scacchiera: senza di essa non si distingue un'area vuota (tile singola, niente
    // ripetizione) da una tinta piatta
    for(int y = 0; y < swatch.height(); y += 8)
        for(int x = 0; x < swatch.width(); x += 8)
            painter.fillRect(x, y, 8, 8, ((x/8 + y/8) % 2) ? QColor(235,235,235)
                                                           : QColor(205,205,205));
    const QBrush brush = Texture();
    if(brush.style() == Qt::TexturePattern){
        if(!Tiled()){
            QPainterPath instance;
            instance.addPolygon(brush.transform().map(
                                    QRectF(QPointF(0, 0), brush.textureImage().size())));
            painter.setClipPath(instance, Qt::IntersectClip);
        }
        painter.fillRect(swatch.rect(), brush);
    }
    painter.end();
    preview_label->setPixmap(swatch);
}
