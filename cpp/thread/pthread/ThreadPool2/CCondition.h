class CCondition
{
private:
    HANDLE m_phEvent; //句柄
public:
    CCondition();
    ~CCondition();
    void Wait();
    void Signal();
};
CCondition::CCondition() {
    m_phEvent = ::CreateEvent(NULL, TRUE, FALSE, NULL);//初始化事件为未触发状态
    //第二个参数决定是否手动调用ResetEvent
}
CCondition::~CCondition()
{
    if (NULL != m_phEvent)
    {
        ::CloseHandle(m_phEvent);//这里只是关闭了线程句柄对象，表示我不再使用该句柄，
        //即不对这个句柄对应的线程做任何干预了，并没有结束线程。
    }
}
void CCondition::Wait()
{
    //等待函数可使线程自愿进入等待状态，直到一个特定的内核对象变为已通知状态为止。
    WaitForSingleObject(m_phEvent, INFINITE);//INFINITE其一直等待，知道被触发
    ResetEvent(m_phEvent);//未激活状态
}
void CCondition::Signal()
{
    //Sets the specified event object to the signaled state
    SetEvent(m_phEvent);//有信号或者激活状态
}
