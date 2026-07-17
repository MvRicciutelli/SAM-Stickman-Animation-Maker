#ifndef CHECKUPDATE_H
#define CHECKUPDATE_H
#include <QThread>


/**
 * @brief Background thread that checks whether a newer build exists and, if so, signals
 *        the main window.
 *
 * Started once from MyMainWindow's constructor. It shells out to the bundled version.exe
 * helper, compares the reported version against this build's VERSION/SUBVERSION/R
 * (checkupdate.cpp) and, when the remote one is newer, emits UpdatePresent() —
 * MyMainWindow::UpdatePresent() then shows the "update available" notice. The thread
 * cleans itself up (deleteLater()) when run() returns.
 */
class CheckUpdate : public QThread
{
    Q_OBJECT
public:
    explicit CheckUpdate();

signals:
    /**
     * @brief Emitted when the remote version is newer than this build.
     * @param s the raw version string reported by the helper.
     */
    void UpdatePresent(const QString &s);

    // QThread interface
protected:
    virtual void run() override;
};
#endif // CHECKUPDATE_H


