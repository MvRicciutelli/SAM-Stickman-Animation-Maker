#include "brushpicker.h"
#include <myview.h>          // MakeRoundBrush(): il generatore della punta tonda integrata
#include <QColorDialog>
#include <QComboBox>
#include <QDialogButtonBox>
#include <QDir>
#include <QDirIterator>
#include <QFileDialog>
#include <QFileInfo>
#include <QFormLayout>
#include <QLabel>
#include <QListWidget>
#include <QPainter>
#include <QPainterPath>
#include <QPushButton>
#include <QSpinBox>
#include <QSplitter>
#include <QVBoxLayout>

/**
 * @details Lifted out of Widget's constructor when the brush border needed the same library.
 * Nothing about the behaviour changed in the move: same folder, same "each .abr is a
 * collection" rule, same seven built-in presets, same in-place thumbnail regeneration.
 */

namespace {

/// I preset della punta integrata: solo parametri, nessuna texture — le matite si distinguono
/// per durezza/opacità/spessore, con la punta un po' ovale e ruotata a caso dab per dab (angle
/// jitter) che dà il bordo irregolare della grafite.
const BrushPreset brush_presets[] = {
    { QStringLiteral("Round"),     20, 100, 25,  75, 100,  0,   0 },
    { QStringLiteral("Pencil 3H"),  4,  35, 10, 100,  80, 20, 100 },
    { QStringLiteral("Pencil H/B"), 6,  60, 12,  95,  80, 25, 100 },
    { QStringLiteral("Pencil B2"),  9,  80, 14,  85,  75, 35, 100 },
    { QStringLiteral("Pencil B6"), 14, 100, 18,  70,  70, 45, 100 },
    { QStringLiteral("Aerograph"), 60,  12,  5,   0, 100,  0,   0 },
    { QStringLiteral("Ink pen"),    6, 100,  6, 100, 100,  0,   0 } };

/// Anteprima di una punta: lo stamp reso allo spessore e all'opacità con cui verrà usato,
/// altrimenti le sette punte integrate sono sette cerchi neri identici.
QImage TipPreview(const QImage &stamp, int size, int opacity)
{
    QImage preview(128, 128, QImage::Format_ARGB32_Premultiplied);
    preview.fill(Qt::transparent);
    QPainter pp(&preview);
    pp.setOpacity(qBound(0, opacity, 100)/100.0);
    double d = qBound(24.0, size*4.0, 128.0); // relativo, ma leggibile anche per la 3H
    pp.drawImage(QRectF((128-d)/2.0, (128-d)/2.0, d, d), stamp);
    pp.end();
    return preview;
}

} // namespace

BrushPicker::BrushPicker(const QString &brushes_path, QWidget *parent)
    : QWidget(parent), brushes_path(brushes_path)
{
    QVBoxLayout *col = new QVBoxLayout(this);
    col->setContentsMargins(0, 0, 0, 0);

    // selettore collezione: ogni file .abr è una "collezione" trattata come una sottocartella a
    // sé (le sottocartelle reali compaiono come prefisso nel percorso relativo dell'etichetta)
    QHBoxLayout *folder_row = new QHBoxLayout();
    folder_row->addWidget(new QLabel(tr("Collection"), this));
    folder_combo = new QComboBox(this);
    folder_combo->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    folder_row->addWidget(folder_combo, 1);
    col->addLayout(folder_row);

    // griglia anteprime: icone in griglia che rifluisce al ridimensionamento. Ogni item porta il
    // proprio stamp (QImage ARGB, alpha = copertura) nel UserRole; i built-in portano anche il
    // loro indice di preset in UserRole+1, che ne applica tutti i parametri.
    brush_list = new QListWidget(this);
    brush_list->setViewMode(QListView::IconMode);
    brush_list->setIconSize(QSize(64, 64));
    brush_list->setGridSize(QSize(78, 86));
    brush_list->setResizeMode(QListView::Adjust);
    brush_list->setMovement(QListView::Static);
    brush_list->setUniformItemSizes(true);
    brush_list->setSpacing(4);
    brush_list->setMinimumHeight(80);
    connect(brush_list, &QListWidget::currentItemChanged, this,
            [this](QListWidgetItem *cur, QListWidgetItem *){
        if(!cur) return;
        QImage stamp = cur->data(Qt::UserRole).value<QImage>();
        if(!stamp.isNull())
            emit brushChosen(stamp);
        QVariant preset_idx = cur->data(Qt::UserRole + 1);
        if(!preset_idx.isValid())
            return; // pennello .abr: solo lo stamp, i parametri restano quelli impostati a mano
        emit presetChosen(brush_presets[preset_idx.toInt()]);
    });

    // import (.abr copiati nella cartella) + refresh, nel pannello inferiore dello splitter
    QHBoxLayout *btn_row = new QHBoxLayout();
    QPushButton *import_btn = new QPushButton(tr("Import brush"), this);
    QPushButton *refresh_btn = new QPushButton(tr("Refresh"), this);
    btn_row->addWidget(import_btn);
    btn_row->addWidget(refresh_btn);
    connect(import_btn, &QPushButton::clicked, this, &BrushPicker::Import);
    connect(refresh_btn, &QPushButton::clicked, this, [this]{
        Rescan();
        Populate(folder_combo->currentData().toString());
    });
    connect(folder_combo, QOverload<int>::of(&QComboBox::currentIndexChanged), this,
            [this](int){ Populate(folder_combo->currentData().toString()); });

    // griglia ridimensionabile: trascina la maniglia dello splitter per allargarla
    QSplitter *split = new QSplitter(Qt::Vertical, this);
    split->setChildrenCollapsible(false);
    split->addWidget(brush_list);
    QWidget *below = new QWidget(this);
    QVBoxLayout *below_col = new QVBoxLayout(below);
    below_col->setContentsMargins(0, 0, 0, 0);
    below_col->addLayout(btn_row);
    split->addWidget(below);
    split->setStretchFactor(0, 1);
    split->setStretchFactor(1, 0);
    col->addWidget(split, 1);

    Rescan();
    Populate(folder_combo->currentData().toString());
}

/// @details Empty collection = the built-in presets, one item each, thumbnail rendered at the
/// preset's own thickness and opacity. Otherwise every sampled tip LoadAbr() could decode.
void BrushPicker::Populate(const QString &collection)
{
    brush_list->clear();
    if(collection.isEmpty()){
        for(int i = 0; i < int(std::size(brush_presets)); i++){
            const BrushPreset &p = brush_presets[i];
            QImage stamp = MyView::MakeRoundBrush(128, p.hardness, p.roundness);
            QListWidgetItem *it = new QListWidgetItem(
                        QIcon(QPixmap::fromImage(TipPreview(stamp, p.size, p.opacity))), p.name);
            it->setData(Qt::UserRole, stamp);
            it->setData(Qt::UserRole + 1, i);
            brush_list->addItem(it);
        }
        brush_list->setCurrentRow(0);
        return;
    }
    const QList<AbrBrush> brushes = LoadAbr(collection);
    for(const AbrBrush &b : brushes){
        QListWidgetItem *it = new QListWidgetItem(QIcon(QPixmap::fromImage(b.stamp)), b.name);
        it->setData(Qt::UserRole, b.stamp);
        brush_list->addItem(it);
    }
    if(brush_list->count())
        brush_list->setCurrentRow(0);
}

/// @details Entry 0 is the built-in tip; every .abr under the folder (subdirectories included)
/// follows, labelled by its path relative to the folder. Preserves the current selection when
/// it is still there, so a Refresh does not jump the user back to the built-in tip.
void BrushPicker::Rescan()
{
    const QString prev = folder_combo->currentData().toString();
    folder_combo->blockSignals(true);
    folder_combo->clear();
    folder_combo->addItem(tr("Built-in"), QString());
    QDir root(brushes_path);
    QDirIterator it(brushes_path, QStringList() << "*.abr", QDir::Files,
                    QDirIterator::Subdirectories);
    QStringList files;
    while(it.hasNext()){ it.next(); files << it.filePath(); }
    files.sort(Qt::CaseInsensitive);
    for(const QString &f : files)
        folder_combo->addItem(root.relativeFilePath(f), f);
    int idx = folder_combo->findData(prev);
    folder_combo->setCurrentIndex(idx >= 0 ? idx : 0);
    folder_combo->blockSignals(false);
}

/// @details Importing means copying into the folder, the same rule the pattern library follows.
void BrushPicker::Import()
{
    const QStringList files = QFileDialog::getOpenFileNames(this, tr("Import brushes"),
                                  QString(), tr("Photoshop brushes (*.abr)"));
    QString last;
    for(const QString &src : files){
        QString dest = brushes_path + "/" + QFileInfo(src).fileName();
        if(QFileInfo(src).absoluteFilePath() != QFileInfo(dest).absoluteFilePath()){
            QFile::remove(dest);      // sovrascrivi un omonimo esistente
            QFile::copy(src, dest);
        }
        last = dest;
    }
    Rescan();
    int idx = last.isEmpty() ? -1 : folder_combo->findData(last);
    if(idx >= 0 && idx != folder_combo->currentIndex())
        folder_combo->setCurrentIndex(idx);  // la collezione appena importata (fa Populate)
    else
        Populate(folder_combo->currentData().toString());
}

/// @details See the header for why this regenerates in place instead of repopulating.
void BrushPicker::RebuildBuiltIn(int hardness, int roundness, int size, int opacity)
{
    if(!folder_combo->currentData().toString().isEmpty())
        return;
    QListWidgetItem *cur = brush_list->currentItem();
    if(!cur)
        return;
    QImage stamp = MyView::MakeRoundBrush(128, hardness, roundness);
    cur->setData(Qt::UserRole, stamp);
    cur->setIcon(QIcon(QPixmap::fromImage(TipPreview(stamp, size, opacity))));
    emit brushChosen(stamp);
}

QImage BrushPicker::CurrentStamp() const
{
    QListWidgetItem *cur = brush_list->currentItem();
    return cur ? cur->data(Qt::UserRole).value<QImage>() : QImage();
}

// ---------------------------------------------------------------------------------------------

/**
 * @details The picker plus the five numbers a border brush stores. There is no colour, size or
 * opacity control on purpose: for a border those three are the pen's, so adding them here would
 * be a second control for each, free to disagree with the Style column's own.
 */
BrushStrokeDialog::BrushStrokeDialog(const QString &brushes_path, QWidget *parent)
    : QDialog(parent)
{
    setWindowTitle(tr("Brush border"));
    QVBoxLayout *col = new QVBoxLayout(this);

    picker = new BrushPicker(brushes_path, this);
    col->addWidget(picker, 1);
    connect(picker, &BrushPicker::brushChosen, this, [this](const QImage &stamp){
        brush.stamp = stamp;
        RefreshPreview();
    });
    // un preset della punta integrata porta con sé anche la spaziatura e i jitter: un contorno
    // "matita B6" deve venire fuori come la matita B6 del disegno a mano libera, non come la sua
    // sola forma. Colore/spessore/opacità del preset invece NON si applicano: sono della penna.
    connect(picker, &BrushPicker::presetChosen, this, [this](const BrushPreset &p){
        spacing_spin->setValue(p.spacing);
        size_jitter_spin->setValue(p.size_jitter);
        angle_jitter_spin->setValue(p.angle_jitter);
    });

    QFormLayout *form = new QFormLayout();
    struct { QSpinBox **box; QString label; int min, max; QString tip; } rows[] = {
        { &spacing_spin, tr("Spacing"), 1, 1000,
          tr("Dab pitch, as a percentage of the border thickness. Over 100% the dabs separate "
             "into a dotted line.") },
        { &angle_spin, tr("Tip angle"), -180, 180, tr("Rotation of the brush tip.") },
        { &size_jitter_spin, tr("Size jitter"), 0, 100,
          tr("Random per-dab size reduction (0% = every dab at the border thickness).") },
        { &angle_jitter_spin, tr("Rotation jitter"), 0, 100,
          tr("Random per-dab rotation (100% = anywhere in the full circle).") },
        { &opacity_jitter_spin, tr("Opacity jitter"), 0, 100,
          tr("Random per-dab opacity reduction, below the border's own opacity.") } };
    for(auto &r : rows){
        QSpinBox *box = new QSpinBox(this);
        box->setRange(r.min, r.max);
        box->setSuffix(r.label == tr("Tip angle") ? QString(QChar(0xB0)) : tr("%"));
        box->setToolTip(r.tip);
        form->addRow(r.label, box);
        *r.box = box;
        connect(box, QOverload<int>::of(&QSpinBox::valueChanged), this, [this]{
            brush.spacing = spacing_spin->value();
            brush.angle = angle_spin->value();
            brush.size_jitter = size_jitter_spin->value();
            brush.angle_jitter = angle_jitter_spin->value();
            brush.opacity_jitter = opacity_jitter_spin->value();
            RefreshPreview();
        });
    }
    col->addLayout(form);

    // il colore È quello della penna, e questo bottone lo scrive: senza di lui non ci sarebbe
    // strada per cambiarlo, perché la voce "Solid" della combo — l'altra che apre un selettore di
    // colore — butta via il pennello per definizione (un contorno alla volta)
    QPushButton *color_btn = new QPushButton(tr("Border colour"), this);
    connect(color_btn, &QPushButton::clicked, this, [this, color_btn]{
        QColor picked = QColorDialog::getColor(brush.color, this, tr("Border colour"),
                                               QColorDialog::ShowAlphaChannel);
        if(!picked.isValid())
            return;
        brush.color = picked;
        color_btn->setStyleSheet(QString("background-color: %1").arg(picked.name()));
        RefreshPreview();
    });
    col->addWidget(color_btn);
    color_button = color_btn;

    preview = new QLabel(this);
    preview->setMinimumHeight(90);
    preview->setToolTip(tr("The border as it will be stamped, at the stick's own thickness, "
                           "colour and opacity."));
    col->addWidget(preview);

    QDialogButtonBox *buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel,
                                                     this);
    connect(buttons, &QDialogButtonBox::accepted, this, &QDialog::accept);
    connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);
    col->addWidget(buttons);
}

void BrushStrokeDialog::Seed(const BrushStroke &seed)
{
    // lo stamp nullo (nessun contorno a pennello ancora) non deve cancellare la punta scelta
    // nella griglia: è proprio quella la punta con cui il contorno sta per nascere
    const QImage keep = brush.stamp.isNull() ? picker->CurrentStamp() : brush.stamp;
    brush = seed;
    if(brush.stamp.isNull())
        brush.stamp = keep;
    const QList<QPair<QSpinBox*, int>> values = {
        { spacing_spin,        brush.spacing },
        { angle_spin,          brush.angle },
        { size_jitter_spin,    brush.size_jitter },
        { angle_jitter_spin,   brush.angle_jitter },
        { opacity_jitter_spin, brush.opacity_jitter } };
    for(const auto &v : values){
        QSignalBlocker mute(v.first);
        v.first->setValue(v.second);
    }
    color_button->setStyleSheet(QString("background-color: %1").arg(brush.color.name()));
    RefreshPreview();
}

/// @details Stamps the real thing along a straight sample: same StampAlongPath() the renderer
/// uses, so spacing and jitter cannot look one way here and another on the canvas. The dab size
/// is the pen width Seed() supplied, capped to the strip's height so a thick border still fits.
void BrushStrokeDialog::RefreshPreview()
{
    const QSize strip(qMax(200, preview->width()), 80);
    QPixmap canvas(strip);
    canvas.fill(Qt::white);
    BrushStroke sample = brush;
    sample.size = qBound(1.0, sample.size, strip.height()/2.0);
    QPainterPath line;
    line.moveTo(10, strip.height()/2.0);
    line.lineTo(strip.width() - 10, strip.height()/2.0);
    QPainter p(&canvas);
    p.setRenderHint(QPainter::Antialiasing, true);
    StampAlongPath(&p, line, sample);
    p.end();
    preview->setPixmap(canvas);
}
