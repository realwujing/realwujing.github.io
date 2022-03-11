#include "process.h"

Process::Process(QObject *parent)
    : QThread(parent),
      state(STOPED),
      processId(-1)
{
}

Process::~Process()
{
    qInfo() << "~Thread";
    stop();
}

Process::State Process::getState() const
{
    qInfo() << "state:" << state;
    return state;
}

void Process::start(Priority pri)
{
    QThread::start(pri);
    state = RUNNING;
}

int Process::stop()
{
    qInfo() << "hhhhhhhhhhhhhhhhhhhhhhhhhhhh:" << processId << "failed";
    if (QThread::isRunning())
    {
        QProcess process;
        process.start("kill", {"-9", QString::number(processId)});

        process.waitForFinished();
        if (QProcess::NormalExit != process.exitCode())
        {
            qCritical() << "kill -9 pid:" << processId << "failed";
            QThread::terminate();
            return -1;
        }
        state = STOPED;
        return 0;
    }
    return -1;
}

int Process::pause()
{
    if (QThread::isRunning())
    {
        qInfo() << QDateTime::currentDateTime().toString("yyyy.MM.dd hh:mm:ss.zzz ddd") << " pause process:" << processId;

        QProcess process;
        QStringList argStrList = {"-STOP", QString::number(processId)};
        process.start("kill", argStrList);
        if (!process.waitForStarted())
        {
            qCritical() << "kill init failed!";
            return -1;
        }
        if (!process.waitForFinished())
        {
            qCritical() << " run finish failed!";
            return -1;
        }
        if (QProcess::NormalExit != process.exitCode())
        {
            qCritical() << "kill -STOP pid:" << processId << "failed";
            return -1;
        }
        state = PAUSED;
        return 0;
    }
    return -1;
}

int Process::resume()
{
    if (QThread::isRunning())
    {
        QProcess process;
        process.start("kill", {"-CONT", QString::number(processId)});
<<<<<<< HEAD:thread/QThread/process.cpp
        process.waitForStarted(3000);
        process.waitForFinished(3000);
=======
        process.waitForFinished();
>>>>>>> add qthread:thread/QThread/download.cpp
        if (QProcess::NormalExit != process.exitCode())
        {
            qCritical() << "kill -CONT pid:" << processId << "failed";
            return -1;
        }
        state = RUNNING;
        return 0;
    }
    return -1;
<<<<<<< HEAD:thread/QThread/process.cpp
}

void Process::run()
{
    qInfo() << "enter thread : " << QThread::currentThreadId();

    process(func);

    qInfo() << "exit thread : " << QThread::currentThreadId();
}

void Process::process(std::function<int()> func)
{
    processId = func();
    qInfo() << "processId:" << processId << QDateTime::currentDateTime().toString("yyyy.MM.dd hh:mm:ss.zzz ddd");
    while (true)
    {
        /* code */
    }
    
}

void Download::exit(int , QProcess::ExitStatus )
{
    qInfo() << "void Download::exit(int , QProcess::ExitStatus )";
}
=======
>>>>>>> refactor: rename class:thread/QThread/process.cpp
