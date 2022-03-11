#include "process_manager.h"

#include <QCoreApplication>
#include <unistd.h>


auto processId = []()
{
    qInfo() << QDateTime::currentDateTime().toString("yyyy.MM.dd hh:mm:ss.zzz ddd");
    // sleep(30);
    QProcess process;
    process.start("/home/wujing/code/cpp-learning/thread/QThread/test", QIODevice::ReadWrite);
    // process.waitForStarted();
    int processId = process.processId();
    // process.waitForFinished(-1);
    qInfo() << "processId:" << processId << QDateTime::currentDateTime().toString("yyyy.MM.dd hh:mm:ss.zzz ddd");
    return processId;
};

int main(int argc, char *argv[])
{
    QCoreApplication a(argc, argv);

    qSetMessagePattern("[File:%{file} Line:%{line} Function:%{function}] %{message}");

    qInfo() << "main thread : " << QThread::currentThreadId();

    // test();
<<<<<<< HEAD
<<<<<<< HEAD
    ProcessManager processManager;
    processManager.jobs["test"] = new Process;
    processManager.jobs["test"]->func = processId;

    // QObject::connect(processManager.jobs["test"], SIGNAL(finished()), &processManager, SLOT(removeJob()));
    // QObject::connect(processManager.jobs["test"], &Process::finished, processManager.jobs["test"], &QObject::deleteLater);
    processManager.jobs["test"]->start();
    processManager.jobs["test"]->getState();
=======
=======
>>>>>>> refactor: rename class
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
    sleep(5);
    downloadManager.jobs["test"]->pause();
    downloadManager.jobs["test"]->getState();
    sleep(5);
    downloadManager.jobs["test"]->resume();
    downloadManager.jobs["test"]->getState();

    sleep(3);
    processManager.jobs["test"]->pause();
    processManager.jobs["test"]->getState();
    // sleep(3);
    // processManager.jobs["test"]->resume();
    // processManager.jobs["test"]->getState();
    // sleep(3);
    // processManager.jobs["test"]->stop();
    // processManager.jobs["test"]->getState();

    qInfo() << "main" << QDateTime::currentDateTime().toString("yyyy.MM.dd hh:mm:ss.zzz ddd");
    qInfo() << "main thread : " << QThread::currentThreadId();

    return a.exec();
}
