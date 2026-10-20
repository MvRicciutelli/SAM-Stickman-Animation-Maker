#ifndef MFVIDEO_H
#define MFVIDEO_H

#include <QString>
#include <QStringList>

/**
 * @brief Encodes an already-rendered image sequence into an H.264 .mp4 using the Windows
 *        Media Foundation encoder that ships with the operating system.
 * @param out_file destination .mp4 path (the container is inferred from the extension).
 * @param frame_files the rendered frames, in playback order.
 * @param fps frame rate of the result; values <= 0 fall back to 24.
 * @param error when non-null, receives a human-readable reason on failure.
 * @return true when the file was written and finalized.
 *
 * The alternative to shipping an encoder: no ffmpeg process, no bundled codec, and the
 * AVC patent licence comes with Windows. Called from the ImageExport worker thread
 * (MFVIDEO mode), never from the UI thread — encoding a whole animation blocks.
 * Odd canvas sides are rounded down to even, which H.264 requires.
 */
bool WriteMp4WithMediaFoundation(const QString &out_file, const QStringList &frame_files,
                                 int fps, QString *error);

#endif // MFVIDEO_H
