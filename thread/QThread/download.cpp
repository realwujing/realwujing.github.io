#include "download.h"

Download::Download(QObject *parent)
    : QThread(parent),
      state(STOPED),
      processId(-1)
{
}

Download::~Download()
{
    qInfo() << "~Thread";
    stop();
}

Download::State Download::getState() const
{
    qInfo() << "state:" << state;
    return state;
}

void Download::start(Priority pri)
{
    QThread::start(pri);
    state = RUNNING;
}

int Download::stop()
{
    if (QThread::isRunning())
    {
        QProcess process;
        process.start("kill", {"-9", QString::number(processId)});
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

int Download::pause()
{
    if (QThread::isRunning())
    {
        QProcess process;
        process.start("kill", {"-STOP", QString::number(processId)});
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

int Download::resume()
{
    if (QThread::isRunning())
    {
        QProcess process;
        process.start("kill", {"-CONT", QString::number(processId)});
        if (QProcess::NormalExit != process.exitCode())
        {
            qCritical() << "kill -CONT pid:" << processId << "failed";
            return -1;
        }
        state = RUNNING;
        return 0;
    }
    return -1;
}

void Download::run()
{
    qInfo() << "enter thread : " << QThread::currentThreadId();

    process(func);

    qInfo() << "exit thread : " << QThread::currentThreadId();
}

void Download::process(std::function<int()> func)
{
    processId = func();
    qInfo() << "processId:" << processId << QDateTime::currentDateTime().toString("yyyy.MM.dd hh:mm:ss.zzz ddd");
}
