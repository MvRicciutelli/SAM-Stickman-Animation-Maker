#ifndef IMAGEEXPORT_H
#define IMAGEEXPORT_H

#include "myview.h"

#include <QObject>
#include <QThread>
#define FFMPEG 1
#define AVI 2
#define PREVIEW 3
#define NONE 0
extern MyView *V;
class ImageExport : public QThread
{
    Q_OBJECT
public:
    explicit ImageExport(QObject *parent = nullptr, QString path = "", int option = NONE);
    void Render(int option);
signals:
    void ImageExportFinished(int option);
public slots:


    // QThread interface
protected:
    virtual void run() override;
};

#endif // IMAGEEXPORT_H
