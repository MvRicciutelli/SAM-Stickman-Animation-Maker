#ifndef SLEEPER_H
#define SLEEPER_H
#include <QThread>
#include <QTimer>
#include "myview.h"

/**
 * @brief Drives frame-by-frame playback preview: renders every frame to an image once,
 *        then flips through them on a timer.
 *
 * Widget::on_PlayButton_clicked() constructs one of these (kept in the global `play`) to
 * start playback and calls Stop() to end it early; MySleeper deletes itself (Connected())
 * once the last frame has been shown, so it has no external owner.
 */
class MySleeper:public QObject
{
    Q_OBJECT
    QTimer timer;
    MyView* v;       ///< canvas whose current animation is being previewed
    int counter = 0; ///< index of the next frame to display

public:
    /**
     * @brief Starts a background render of every frame, then begins playback once it's done.
     * @param V canvas whose current animation will be played.
     * @param time milliseconds between frames (derived from the chosen FPS).
     */
    MySleeper(MyView * V, int time);

    /**
     * @brief Requests playback stop after the frame currently displaying finishes its tick.
     */
    void Stop();
public slots:
    /**
     * @brief Starts the display timer once the preview render thread has finished.
     */
    void StartPlaying();

    /**
     * @brief Timer tick: shows the next rendered frame, or ends playback and self-destructs
     *        once every frame has been shown or Stop() was called.
     */
    Q_SLOT void Connected();

};
#endif // SLEEPER_H
