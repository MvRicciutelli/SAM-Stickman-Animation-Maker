#ifndef UPDATECHECK_H
#define UPDATECHECK_H

#include <QString>
#include <QVersionNumber>

class QWidget;

/**
 * @brief Tells whether a GitHub release tag names a newer version than this build.
 * @param tag the release's tag, "v1.2.0" or "1.2.0".
 * @param current this build's version (APP_VERSION, from VERSION in SAM.pro).
 * @return true only for a strictly newer version; trailing zeros don't count ("v1.0" is not
 *         newer than "1.0.0") and a tag that isn't a version at all is never newer.
 */
inline bool IsNewerRelease(const QString &tag, const QString &current)
{
    const QString number = tag.startsWith('v', Qt::CaseInsensitive) ? tag.mid(1) : tag;
    const QVersionNumber latest = QVersionNumber::fromString(number).normalized();
    return !latest.isNull() && latest > QVersionNumber::fromString(current).normalized();
}

/**
 * @brief Asks GitHub for the latest release in the background and, if it is newer than this
 *        build, offers to open its download page in the browser.
 * @param parent owns the request and the prompt; the prompt appears whenever the reply lands.
 *
 * Silent on every failure (offline, no release yet, private repo, rate limit): an update
 * check must never get in the way of starting the app.
 */
void CheckForUpdate(QWidget *parent);

#endif // UPDATECHECK_H
