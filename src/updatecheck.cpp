#include "updatecheck.h"
#include <QDesktopServices>
#include <QJsonDocument>
#include <QJsonObject>
#include <QMessageBox>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QUrl>
#include <QWidget>

// The releases must be PUBLIC: the app has no GitHub login, and shipping a token inside the exe
// is exactly what got the previous update check removed (it carried database credentials).
// While the repository is private GitHub answers 404 here and the check stays silent.
static const char *LATEST_RELEASE_API =
        "https://api.github.com/repos/MvRicciutelli/SAM-Stickman-Animation-Maker/releases/latest";

/**
 * @details Asynchronous on purpose: startup never waits for the network. The manager is
 * parented to `parent` so an app closed before the reply lands takes the request with it,
 * and it is released once the reply is in (the reply is its child). GitHub's
 * `releases/latest` already skips drafts and pre-releases.
 */
void CheckForUpdate(QWidget *parent)
{
    QNetworkAccessManager *net = new QNetworkAccessManager(parent);
    QNetworkRequest request{QUrl(LATEST_RELEASE_API)};
    request.setRawHeader("Accept", "application/vnd.github+json");
    request.setTransferTimeout(10000);
    QNetworkReply *reply = net->get(request);
    QObject::connect(reply, &QNetworkReply::finished, parent, [parent, net, reply]{
        net->deleteLater();
        if(reply->error() != QNetworkReply::NoError)
            return;
        const QJsonObject release = QJsonDocument::fromJson(reply->readAll()).object();
        const QString tag = release.value("tag_name").toString();
        const QUrl page(release.value("html_url").toString());
        // only ever open a GitHub page, whatever the reply says
        if(!IsNewerRelease(tag, APP_VERSION) || page.scheme() != "https" || page.host() != "github.com")
            return;
        if(QMessageBox::question(parent, "Update available",
                QString("SAM %1 is available (this is version %2).\n\nOpen the download page?")
                    .arg(tag, APP_VERSION)) == QMessageBox::Yes)
            QDesktopServices::openUrl(page);
    });
}
