class CThreadMutex
{
private:
    CRITICAL_SECTION m_CritSec;
public:
    CThreadMutex();
    ~CThreadMutex();
    bool lock();
    bool unlock();
    bool trylock();
};
CThreadMutex::CThreadMutex() {
#if (_WIN32_WINNT >= 0x0403)
    //使用 InitializeCriticalSectionAndSpinCount 可以提高性能
    ::InitializeCriticalSectionAndSpinCount(&m_CritSec, 4000);
#else
    ::InitializeCriticalSection(&m_CritSec);
#endif
}
CThreadMutex::~CThreadMutex()
{
    ::DeleteCriticalSection(&m_CritSec);
}
bool CThreadMutex::lock()
{
    //进入临界区
    ::EnterCriticalSection(&m_CritSec);
    return TRUE;
}
bool CThreadMutex::unlock()
{
    //离开临界区
    ::LeaveCriticalSection(&m_CritSec);
    return TRUE;
}
bool CThreadMutex::trylock()
{
    bool bRet = TryEnterCriticalSection(&m_CritSec);
    return bRet;
}

