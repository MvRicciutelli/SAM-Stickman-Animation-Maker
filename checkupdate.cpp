#include "checkupdate.h"
#include <QCoreApplication>
#include <QDir>
#include <QProcess>
#define VERSION 0
#define SUBVERSION 9
#define R 0
extern QDir program_folder;
CheckUpdate::CheckUpdate()
{

}

void CheckUpdate::run(){
    QProcess *p = new QProcess(this);
    QStringList paramList;
    QString output;
    QString exePath = QCoreApplication::applicationDirPath();
    QFile::copy(":/version.exe",exePath+"/version.exe");
    p->start(exePath+"/version.exe");
    if (!p->waitForStarted()){

    }else{
        p->waitForFinished();
        output = (p->readAllStandardOutput());

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
        int V = release[0].toInt();
        int SubV = release[1].toInt();
        int Rel = release[2].toInt();
        if(release[0].toInt() == -1){
            goto cleanup;
        }else{
            if(V> VERSION){
                goto update;
            }else if(V == VERSION){
                if(SubV > SUBVERSION){
                    goto update;
                }else if(SubV == SUBVERSION){
                    if(Rel> R){
                        goto update;
                    }
                }
            }
        }
        update:
            emit UpdatePresent(output);

    }
    cleanup:
    delete p;
    quit();
    deleteLater();
}
