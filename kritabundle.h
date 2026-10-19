#ifndef KRITABUNDLE_H
#define KRITABUNDLE_H

#include "abrbrush.h"     // PatPattern
#include "brushpicker.h"  // BrushPreset

#include <QImage>
#include <QList>
#include <QString>

/**
 * @file kritabundle.h
 * @brief Reader for Krita resource bundles (.bundle), the import half of "Import brush" and
 *        "Import patterns" for that format.
 *
 * A bundle is a ZIP holding `.kpp` files under `paintoppresets/` (a PNG thumbnail whose `preset` text chunk is
 * the preset's XML), the brush tips they reference under `brushes/` (.gbr, .gih, .png) and fill
 * textures under `patterns/` (.png, GIMP .pat). Only what maps onto this app's stamp model is
 * converted; everything else is counted as skipped, never guessed at.
 */

/** @brief One converted preset: the parameters BrushPicker stores in a `.mbrush`, plus its tip. */
struct KritaBrush {
    BrushPreset params;
    QImage stamp;   ///< Format_Alpha8 coverage mask, at most 512 px on its longer side
};

/** @brief The convertible presets of one bundle. */
struct KritaBundleBrushes {
    QList<KritaBrush> brushes;
    int skipped = 0;   ///< presets left out: another engine, an eraser, a tip type or file we can't read
};

/**
 * @brief Converts a bundle's pixel-engine presets into stamps + parameters.
 * @param path the .bundle file.
 * @return the converted presets in file-name order; empty (with skipped = 0) for a file that is
 *         not a readable ZIP.
 *
 * Converted: `paintbrush` presets whose tip is an auto brush (circle or rectangle) or a .gbr,
 * .gih (first cell) or .png file inside the bundle. Mapped: diameter, spacing, tip angle,
 * opacity x flow, and a random ("fuzzy") rotation, size or opacity sensor as the matching jitter.
 * Skipped: the smudge, clone, deform, particle, curve and experiment engines, eraser presets, and
 * tips the bundle does not carry. Pressure curves, textures and blending modes have no equivalent.
 */
KritaBundleBrushes LoadKritaBundleBrushes(const QString &path);

/**
 * @brief Reads a bundle's fill patterns.
 * @param path the .bundle file.
 * @return every .png/.jpg and GIMP .pat tile under `patterns/`, as ARGB32 tiles; empty if none.
 */
QList<PatPattern> LoadKritaBundlePatterns(const QString &path);

#endif // KRITABUNDLE_H
