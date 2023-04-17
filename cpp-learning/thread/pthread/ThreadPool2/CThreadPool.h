#include <vector>
#include <CThreadMutex>
#include "CWorkerThread.h"
#include "CJob.h"

using namespace std;

class CThreadPool

{

friend class CWorkerThread;

private:

    unsigned int m_MaxNum;   //the max thread num that can create at the same time

    unsigned int m_AvailLow; //The min num of idle thread that shoule kept

    unsigned int m_AvailHigh;    //The max num of idle thread that kept at the same time

    unsigned int m_AvailNum; //the normal thread num of idle num;

    unsigned int m_InitNum;  //Normal thread num;

protected:

    CWorkerThread* GetIdleThread(void);

 

    void    AppendToIdleList(CWorkerThread* jobthread);

    void    MoveToBusyList(CWorkerThread* idlethread);

    void    MoveToIdleList(CWorkerThread* busythread);

 

    void    DeleteIdleThread(int num);

    void    CreateIdleThread(int num);

public:

    CThreadMutex m_BusyMutex;    //when visit busy list,use m_BusyMutex to lock and unlock

    CThreadMutex m_IdleMutex;    //when visit idle list,use m_IdleMutex to lock and unlock

    CThreadMutex m_JobMutex; //when visit job list,use m_JobMutex to lock and unlock

    CThreadMutex m_VarMutex;

 

    CCondition       m_BusyCond; //m_BusyCond is used to sync busy thread list

    CCondition       m_IdleCond; //m_IdleCond is used to sync idle thread list

    CCondition       m_IdleJobCond;  //m_JobCond is used to sync job list

    CCondition       m_MaxNumCond;

 

    vector<CWorkerThread*>   m_ThreadList;

    vector<CWorkerThread*>   m_BusyList;     //Thread List

    vector<CWorkerThread*>   m_IdleList; //Idle List

 

    CThreadPool();

    CThreadPool(int initnum);

    virtual ~CThreadPool();

 

    void    SetMaxNum(int maxnum){m_MaxNum = maxnum;}

    int     GetMaxNum(void){return m_MaxNum;}

    void    SetAvailLowNum(int minnum){m_AvailLow = minnum;}

    int     GetAvailLowNum(void){return m_AvailLow;}

    void    SetAvailHighNum(int highnum){m_AvailHigh = highnum;}

    int     GetAvailHighNum(void){return m_AvailHigh;}

    int     GetActualAvailNum(void){return m_AvailNum;}

    int     GetAllNum(void){return m_ThreadList.size();}

    int     GetBusyNum(void){return m_BusyList.size();}

    void    SetInitNum(int initnum){m_InitNum = initnum;}

    int     GetInitNum(void){return m_InitNum;}

  

    void    TerminateAll(void);

    void    Run(CJob* job,void* jobdata);

};