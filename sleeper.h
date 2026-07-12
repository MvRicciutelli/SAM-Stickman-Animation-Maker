#ifndef SLEEPER_H
#define SLEEPER_H
#include <QThread>
#include <QTimer>
#include "myview.h"
class MySleeper:public QObject
{
    Q_OBJECT
    QTimer timer;
    MyView* v;
    int counter = 0;

public:
    MySleeper(MyView * V, int time);
    void Stop();
public slots:
    void StartPlaying();
    Q_SLOT void Connected();

};
#endif // SLEEPER_H
