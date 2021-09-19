#include "CThread.h"

class CJob
{
private:
    int      m_JobNo;        //The num was assigned to the job
    char*    m_JobName;      //The job name
    CThread  *m_pWorkThread;     //The thread associated with the job
public:
    CJob( void );
    virtual ~CJob();
       
    int      GetJobNo(void) const { return m_JobNo; }
    void     SetJobNo(int jobno){ m_JobNo = jobno;}
    char*    GetJobName(void) const { return m_JobName; }
    void     SetJobName(char* jobname);
    CThread *GetWorkThread(void){ return m_pWorkThread; }
    void     SetWorkThread ( CThread *pWorkThread ){
        m_pWorkThread = pWorkThread;
    }
    virtual void Run ( void *ptr ) = 0;
};