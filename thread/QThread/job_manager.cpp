#include "job_manager.h"

JobManager::JobManager(QObject *parent)
{

}

JobManager::~JobManager()
{
}

int JobManager::removeJob()
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