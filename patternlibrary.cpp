#include "patternlibrary.h"
#include "abrbrush.h"

#include <QComboBox>
#include <QDir>
#include <QDirIterator>
#include <QFileDialog>
#include <QFileInfo>
#include <QHBoxLayout>
#include <QLabel>
#include <QListWidget>
#include <QPainter>
#include <QPushButton>
#include <QSpinBox>
#include <QVBoxLayout>

namespace {

/**
 * @details A 64x64 thumbnail that reads correctly for both size extremes a .pat library
 * mixes freely: a tile bigger than the icon is scaled down whole (so the motif is
 * recognizable), a smaller one is tiled to fill it (so the REPEAT is what you see, which
 * is how the tile will actually be used). Scaling a small tile up instead would show a
 * blurry single cell and hide the seam the user is choosing between.
 */
QIcon TilePreview(const QImage &tile)
{
    const int side = 64;
    QPixmap preview(side, side);
    preview.fill(Qt::transparent);
    QPainter painter(&preview);
    if(tile.width() > side || tile.height() > side){
        const QImage scaled = tile.scaled(side, side, Qt::KeepAspectRatio, Qt::SmoothTransformation);
        painter.drawImage(QPointF((side - scaled.width())/2.0, (side - scaled.height())/2.0), scaled);
    } else {
        painter.fillRect(preview.rect(), QBrush(tile));
    }
    painter.end();
    return QIcon(preview);
}

} // namespace

/**
 * @details Layout mirrors the brush grid: collection picker on top, preview grid in the
 * middle, Import/Refresh at the bottom. Qt::Tool plus a non-modal show() is what makes it
 * a palette rather than a step — the user keeps painting sticks with it open, and Esc
 * closes it for free through QDialog.
 */
PatternLibrary::PatternLibrary(const QString &patterns_path, QWidget *parent)
    : QDialog(parent), folder_path(patterns_path)
{
    setWindowTitle(tr("Patterns"));
    setWindowFlags(Qt::Tool); // floats over the main window, no modality, no task-bar entry
    resize(320, 380);

    QVBoxLayout *rows = new QVBoxLayout(this);

    QHBoxLayout *collection_row = new QHBoxLayout();
    collection_row->addWidget(new QLabel(tr("Library"), this));
    collection_combo = new QComboBox(this);
    collection_combo->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    collection_row->addWidget(collection_combo, 1);
    rows->addLayout(collection_row);

    tile_grid = new QListWidget(this);
    tile_grid->setViewMode(QListView::IconMode);
    tile_grid->setIconSize(QSize(64, 64));
    tile_grid->setGridSize(QSize(78, 86));
    tile_grid->setResizeMode(QListView::Adjust);
    tile_grid->setMovement(QListView::Static);
    tile_grid->setUniformItemSizes(true);
    tile_grid->setSpacing(4);
    rows->addWidget(tile_grid, 1);

    QHBoxLayout *size_row = new QHBoxLayout();
    size_row->addWidget(new QLabel(tr("Tile size"), this));
    tile_size_spin = new QSpinBox(this);
    tile_size_spin->setRange(4, 2048);
    tile_size_spin->setValue(48);
    tile_size_spin->setSuffix(tr(" px"));
    tile_size_spin->setToolTip(tr("How big one tile paints on the canvas. Pack tiles are "
                                  "often hundreds of pixels wide, which on a stick-sized "
                                  "shape would show a single flat crop."));
    size_row->addWidget(tile_size_spin, 1);
    rows->addLayout(size_row);

    // "applica" sta da solo e in evidenza: è il gesto che porta il pattern SULLO stick,
    // gemello del doppio click sull'anteprima
    QPushButton *apply_button = new QPushButton(tr("Apply to stick"), this);
    apply_button->setToolTip(tr("Put the selected pattern on the selected stick "
                                "(same as double-clicking it)"));
    rows->addWidget(apply_button);

    QHBoxLayout *button_row = new QHBoxLayout();
    QPushButton *import_button = new QPushButton(tr("Import patterns"), this);
    QPushButton *refresh_button = new QPushButton(tr("Refresh"), this);
    button_row->addWidget(import_button);
    button_row->addWidget(refresh_button);
    rows->addLayout(button_row);

    // clicked, not currentItemChanged: a click is the deliberate "preview this one" gesture,
    // while the current item also moves with the arrow keys and on a repopulate
    connect(tile_grid, &QListWidget::itemClicked, this,
            [this](QListWidgetItem *){ EmitCurrent(); });
    connect(tile_size_spin, QOverload<int>::of(&QSpinBox::valueChanged), this,
            [this](int){ EmitCurrent(); });
    // doppio click e bottone fanno la stessa cosa: EmitCurrent() prima, così il ricevente
    // ha il brush aggiornato anche se il pattern non era ancora stato cliccato una volta
    auto apply = [this]{
        if(!tile_grid->currentItem())
            return;
        EmitCurrent();
        emit patternApplied();
    };
    connect(tile_grid, &QListWidget::itemDoubleClicked, this,
            [apply](QListWidgetItem *){ apply(); });
    connect(apply_button, &QPushButton::clicked, this, apply);
    connect(collection_combo, QOverload<int>::of(&QComboBox::currentIndexChanged), this,
            [this](int){ Populate(collection_combo->currentData().toString()); });
    connect(import_button, &QPushButton::clicked, this, &PatternLibrary::ImportFiles);
    connect(refresh_button, &QPushButton::clicked, this, [this]{
        Rescan();
        Populate(collection_combo->currentData().toString());
    });

    Rescan();
    Populate(collection_combo->currentData().toString());
}

/// @details The current item is the one just clicked, so a size change re-applies the same
/// tile at the new size. A tile-less item (the "no patterns" placeholder) emits nothing.
void PatternLibrary::EmitCurrent()
{
    QListWidgetItem *item = tile_grid->currentItem();
    if(!item)
        return;
    const QImage tile = item->data(Qt::UserRole).value<QImage>();
    if(!tile.isNull())
        emit patternChosen(tile, tile_size_spin->value());
}

/// @details See the header. The recursive walk (and the relative-path label) is what makes
/// a hand-made folder tree under myPatterns usable without any UI for folders.
void PatternLibrary::Rescan()
{
    const QString previous = collection_combo->currentData().toString();
    collection_combo->blockSignals(true);
    collection_combo->clear();
    QDir root(folder_path);
    QDirIterator it(folder_path, QStringList() << "*.pat", QDir::Files,
                    QDirIterator::Subdirectories);
    QStringList files;
    while(it.hasNext()){ it.next(); files << it.filePath(); }
    files.sort(Qt::CaseInsensitive);
    for(const QString &file : files)
        collection_combo->addItem(root.relativeFilePath(file), file);
    const int index = collection_combo->findData(previous);
    collection_combo->setCurrentIndex(index >= 0 ? index : 0);
    collection_combo->blockSignals(false);
}

/// @details LoadPat() reports an unreadable or unsupported file as an empty list, so the
/// empty case covers "no library imported yet" and "this library holds nothing we can
/// decode" with the same placeholder — the distinction is not actionable for the user.
void PatternLibrary::Populate(const QString &collection)
{
    tile_grid->clear();
    const QList<PatPattern> patterns = collection.isEmpty() ? QList<PatPattern>()
                                                            : LoadPat(collection);
    for(const PatPattern &pattern : patterns){
        QListWidgetItem *item = new QListWidgetItem(TilePreview(pattern.tile), pattern.name);
        item->setData(Qt::UserRole, pattern.tile);
        item->setToolTip(tr("%1 (%2x%3)").arg(pattern.name)
                         .arg(pattern.tile.width()).arg(pattern.tile.height()));
        tile_grid->addItem(item);
    }
    if(tile_grid->count())
        return;
    QListWidgetItem *empty = new QListWidgetItem(tr("No patterns —\nuse Import"));
    empty->setFlags(Qt::NoItemFlags); // not selectable: there is nothing to pick
    tile_grid->addItem(empty);
}

/// @details Overwrites a same-named library on purpose (re-importing a file the user has
/// edited outside the app should replace it, not silently keep the old copy) — the same
/// rule the .abr brush import follows.
void PatternLibrary::ImportFiles()
{
    const QStringList sources = QFileDialog::getOpenFileNames(this, tr("Import patterns"),
                                    QString(), tr("Photoshop patterns (*.pat)"));
    QString last;
    for(const QString &source : sources){
        const QString destination = folder_path + "/" + QFileInfo(source).fileName();
        if(QFileInfo(source).absoluteFilePath() != QFileInfo(destination).absoluteFilePath()){
            QFile::remove(destination);
            QFile::copy(source, destination);
        }
        last = destination;
    }
    Rescan();
    const int index = last.isEmpty() ? -1 : collection_combo->findData(last);
    if(index >= 0 && index != collection_combo->currentIndex())
        collection_combo->setCurrentIndex(index); // triggers Populate() through the combo
    else
        Populate(collection_combo->currentData().toString());
}
