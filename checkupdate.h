#ifndef CHECKUPDATE_H
#define CHECKUPDATE_H
#include <QThread>


class CheckUpdate : public QThread
{
    Q_OBJECT
public:
    explicit CheckUpdate();

signals:
    void UpdatePresent(const QString &s);

    // QThread interface
protected:
    virtual void run() override;
};
#endif // CHECKUPDATE_H


