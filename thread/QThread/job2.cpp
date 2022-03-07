#include "job2.h"

Job2::Job2(QObject *parent)
    : QThread(parent),
      pauseFlag(false),
      stopFlag(false)
{

}

Job2::~Job2()
{
    qInfo() << "~Thread";
    stop();
}

Job2::State Job2::state() const
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

void Job2::start(Priority pri)
{
    QThread::start(pri);
}

void Job2::stop()
{
    if (QThread::isRunning())
    {
        stopFlag = true;
        condition.wakeAll();
        QThread::quit();
        QThread::wait();
    }
}

void Job2::pause()
{
    if (QThread::isRunning())
    {
        pauseFlag = true;
    }
}

void Job2::resume()
{
    if (QThread::isRunning())
    {
        pauseFlag = false;
        condition.wakeAll();
    }
}

void Job2::run()
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

void Job2::process(std::function<void()> func)
{
    func();
    stop();
}
