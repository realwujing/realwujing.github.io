#include <QCoreApplication>

#include <QThread>

#include "worker.h"

void test()
{
    QThread* thread = new QThread();
    Worker* worker = new Worker(0, "a", "b", "c", "d4");
    worker->moveToThread(thread);
    QObject::connect(thread, SIGNAL(started()), worker, SLOT(start()));
    thread->start();
}

int main(int argc, char *argv[])
{
    QCoreApplication a(argc, argv);

    test();

    qInfo() << "main thread......";

    return a.exec();
}