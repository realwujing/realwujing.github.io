#include "process_manager.h"

ProcessManager::ProcessManager(QObject *parent)
{

}

ProcessManager::~ProcessManager()
{
}

int ProcessManager::removeJob()
{
    for(auto iter=jobs.begin(); iter!=jobs.end(); ++iter)
    {
        if (iter.value()->isFinished())
        {
            qInfo() << "appId:" << iter.key();
            delete iter.value();
            jobs.erase(iter);
            return 0;
        }
    }
    return -1;
}