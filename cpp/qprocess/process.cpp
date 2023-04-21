#include <QCoreApplication>
#include <QProcess>
#include <QDebug>


int main(int argc, char *argv[])
{
    QCoreApplication a(argc, argv);
    QProcess process;
    QStringList args = {"--repo=/deepin/linglong/repo/repo", "pull", "--mirror", "repo:org.libreoffice.libreoffice/7.0.0.3/x86_64"};
    process.start("ostree", args);
    process.waitForStarted();
    process.waitForFinished();
    qInfo() << process.readAllStandardOutput();
    return a.exec();
}
