#pragma once

#include <QThread>
#include <atomic>
#include <QMutex>
#include <QWaitCondition>
#include <QDebug>
#include <QDateTime>

class Job : public QThread
{
    Q_OBJECT
public:
    Job(QObject *parent = nullptr);
    ~Job() override;

    enum State
    {
        Stoped,     ///<停止状态，包括从未启动过和启动后被停止
        Running,    ///<运行状态
        Paused      ///<暂停状态
    };

    State state() const;

public slots:
    void start(Priority pri = InheritPriority);
    void stop();
    void pause();
    void resume();

protected:
    virtual void run() override final;
    virtual void process(std::function<void()> func);

public:
    std::function<void()> func;

private:
    std::atomic_bool pauseFlag;
    std::atomic_bool stopFlag;
    QMutex mutex;
    QWaitCondition condition;
};
