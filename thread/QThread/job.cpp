#include "job.h"

Job::Job(QObject *parent)
    : QThread(parent),
      pauseFlag(false),
      stopFlag(false)
{

}

Job::~Job()
{
    qInfo() << "~Thread";
    stop();
}

Job::State Job::state() const
{
    State s = Stoped;
    if (!QThread::isRunning())
    {
        s = Stoped;
    }
    else if (QThread::isRunning() && pauseFlag)
    {
        s = Paused;
    }
    else if (QThread::isRunning() && (!pauseFlag))
    {
        s = Running;
    }
    return s;
}

void Job::start(Priority pri)
{
    QThread::start(pri);
}

void Job::stop()
{
    if (QThread::isRunning())
    {
        stopFlag = true;
        condition.wakeAll();
        QThread::quit();
        QThread::wait();
    }
}

void Job::pause()
{
    if (QThread::isRunning())
    {
        pauseFlag = true;
    }
}

void Job::resume()
{
    if (QThread::isRunning())
    {
        pauseFlag = false;
        condition.wakeAll();
    }
}

void Job::run()
{
    qInfo() << "enter thread : " << QThread::currentThreadId();
    while (!stopFlag)
    {
        process(func);
        if (pauseFlag)
        {
            mutex.lock();
            condition.wait(&mutex);
            mutex.unlock();
        }
    }
    pauseFlag = false;
    stopFlag = false;

    qInfo() << "exit thread : " << QThread::currentThreadId();
}

void Job::process(std::function<void()> func)
{
    func();
    stop();
}
