#include "download_manager.h"

#include <QCoreApplication>
#include <unistd.h>

auto processId = []()
{
    qInfo() << QDateTime::currentDateTime().toString("yyyy.MM.dd hh:mm:ss.zzz ddd");
    // sleep(30);
    QProcess process;
    process.start("/home/wujing/code/cpp-learning/thread/QThread/test.sh", QIODevice::ReadWrite);
    process.waitForFinished(-1);
    int processId = process.processId();

    qInfo() << "processId:" << processId << QDateTime::currentDateTime().toString("yyyy.MM.dd hh:mm:ss.zzz ddd");
    return processId;
};

int main(int argc, char *argv[])
{
    QCoreApplication a(argc, argv);

    qInfo() << "main thread : " << QThread::currentThreadId();

    // test();
    DownloadManager downloadManager;
    downloadManager.jobs["test"] = new Download;
    downloadManager.jobs["test"]->func = processId;

    // downloadManager.jobs["test2"] = new Job2;
    // downloadManager.jobs["test2"]->func = test;
    QObject::connect(downloadManager.jobs["test"], SIGNAL(finished()), &downloadManager, SLOT(removeJob()));
    // QObject::connect(downloadManager.jobs["test2"], SIGNAL(finished()), &jobManager, SLOT(removeJob()));
    QObject::connect(downloadManager.jobs["test"], &Download::finished, downloadManager.jobs["test"], &QObject::deleteLater);
    // connect(downloadManager.jobs["test2"], &Download::finished, downloadManager.jobs["test2"], &QObject::deleteLater);
    downloadManager.jobs["test"]->start();
    downloadManager.jobs["test"]->getState();
    sleep(3);
    downloadManager.jobs["test"]->pause();
    downloadManager.jobs["test"]->getState();
    sleep(3);
    downloadManager.jobs["test"]->resume();
    downloadManager.jobs["test"]->getState();
    sleep(3);
    downloadManager.jobs["test"]->stop();
    downloadManager.jobs["test"]->getState();
  

    qInfo() << "main" << QDateTime::currentDateTime().toString("yyyy.MM.dd hh:mm:ss.zzz ddd");
    qInfo() << "main thread : " << QThread::currentThreadId();

    return a.exec();
}
