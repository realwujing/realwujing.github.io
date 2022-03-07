#include "job_manager2.h"

JobManager2::JobManager2(QObject *parent)
{

}

JobManager2::~JobManager2()
{
}

int JobManager2::removeJob()
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