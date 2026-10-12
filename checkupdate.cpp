#include "checkupdate.h"
#include <QCoreApplication>
#include <QDir>
#include <QProcess>
#define VERSION 0     ///< this build's major version, compared against the remote one
#define SUBVERSION 9  ///< this build's minor version
#define R 0           ///< this build's release/patch number
extern QDir program_folder;
CheckUpdate::CheckUpdate()
{

}

/**
 * @details Runs the bundled version.exe (extracted from resources next to the app),
 * reads its stdout, strips the wrapping punctuation and splits it into major,minor,
 * release. If that triple is newer than this build's VERSION/SUBVERSION/R it emits
 * UpdatePresent(). Every early-out routes through the `cleanup` label so this thread is
 * always freed (deleteLater()); a helper that reports
 * -1 or too few fields is treated as "no update".
 */
void CheckUpdate::run(){
    // sullo stack: `new QProcess(this)` qui gira nel thread worker mentre `this` vive in quello
    // della UI, quindi Qt rifiutava comunque il parent
    QProcess p;
    QString output;
    QString exe_path = QCoreApplication::applicationDirPath();
    QFile::copy(":/version.exe",exe_path+"/version.exe");
    p.start(exe_path+"/version.exe");
    if (!p.waitForStarted()){

    }else{
        p.waitForFinished();
        output = (p.readAllStandardOutput());

        output.remove("(",Qt::CaseSensitivity::CaseSensitive);
        output.remove(")",Qt::CaseSensitivity::CaseSensitive);
        output.remove("\r\n",Qt::CaseSensitivity::CaseSensitive);
        if(output.length()<3){
            // salta alla pulizia finale invece di uscire subito: altrimenti p (e questo thread) non vengono mai cancellati
            goto cleanup;
        }
        QStringList release = output.split(QLatin1Char(','), Qt::SkipEmptyParts);
        if(release.count() < 3){
            goto cleanup;
        }
        int major_version = release[0].toInt();
        int sub_version = release[1].toInt();
        int release_num = release[2].toInt();
        if(release[0].toInt() == -1){
            goto cleanup;
        }else{
            if(major_version> VERSION){
                goto update;
            }else if(major_version == VERSION){
                if(sub_version > SUBVERSION){
                    goto update;
                }else if(sub_version == SUBVERSION){
                    if(release_num> R){
                        goto update;
                    }
                }
            }
        }
        // nessuna versione più nuova: senza questo salto il flusso CADEVA dentro l'etichetta
        // `update:` e l'avviso di aggiornamento veniva emesso anche a versioni pari o più vecchie
        goto cleanup;
        update:
            emit UpdatePresent(output);

    }
    cleanup:
    quit();
    deleteLater();
}
