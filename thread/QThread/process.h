#pragma once

#include <QThread>
#include <atomic>
#include <QMutex>
#include <QWaitCondition>
#include <QDebug>
#include <QDateTime>
#include <QProcess>

class Process : public QThread
{
    Q_OBJECT
public:
    Process(QObject *parent = nullptr);
    ~Process() override;

    enum State
    {
        STOPED,  // 停止状态，包括从未启动过和启动后被停止
        RUNNING, // 运行状态
        PAUSED   // 暂停状态
    };

public slots:
    void start(Priority pri = InheritPriority);
    int stop();
    int pause();
    int resume();
    State getState() const;
    void exit(int , QProcess::ExitStatus );

protected:
    virtual void run() override final;
    virtual void process(std::function<int()> func);

public:
    std::function<int()> func;

private:
    State state;
    int processId;
};