/**
 *@desc: myreactor头文件, myreactor.h
 *@author: zhangyl
 *@date: 2016.12.03
 */
#ifndef __MYREACTOR_H__
#define __MYREACTOR_H__

#include <list>
#include <memory>
#include <thread>
#include <mutex>
#include <condition_variable>

#define WORKER_THREAD_NUM   5

class CMyReactor
{
public:
    CMyReactor();
    ~CMyReactor();

    bool init(const char* ip, short nport);
    bool uninit();

    bool close_client(int clientfd);

    static void* main_loop(void* p);

private:
    //no copyable
    CMyReactor(const CMyReactor& rhs);
    CMyReactor& operator = (const CMyReactor& rhs);

    bool create_server_listener(const char* ip, short port);

    static void accept_thread_proc(CMyReactor* pReatcor);
    static void worker_thread_proc(CMyReactor* pReatcor);

private:
    //C11语法可以在这里初始化
    int                             m_listenfd = 0;
    int                             m_epollfd  = 0;
    bool                         m_bStop    = false;

    std::shared_ptr<std::thread> m_acceptthread;
    std::shared_ptr<std::thread> m_workerthreads[WORKER_THREAD_NUM];

    std::condition_variable         m_acceptcond;
    std::mutex                     m_acceptmutex;

    std::condition_variable         m_workercond ;
    std::mutex                     m_workermutex;

    std::list<int>                 m_listClients;
};

#endif //!__MYREACTOR_H__