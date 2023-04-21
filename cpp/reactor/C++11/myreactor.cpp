/** 
 *@desc: myreactor实现文件, myreactor.cpp
 *@author: zhangyl
 *@date: 2016.12.03
 */
#include "myreactor.h"
#include <iostream>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>  //for htonl() and htons()
#include <fcntl.h>
#include <sys/epoll.h>
#include <list>
#include <errno.h>
#include <time.h>
#include <sstream>
#include <iomanip> //for std::setw()/setfill()
#include <unistd.h>

#define min(a, b) ((a <= b) ? (a) : (b))

CMyReactor::CMyReactor()
{
    //m_listenfd = 0;
    //m_epollfd = 0;
    //m_bStop = false;
}

CMyReactor::~CMyReactor()
{

}

bool CMyReactor::init(const char* ip, short nport)
{
    if (!create_server_listener(ip, nport))
    {
        std::cout << "Unable to bind: " << ip << ":" << nport << "." << std::endl;
        return false;
    }


    std::cout << "main thread id = " << std::this_thread::get_id() << std::endl;

    //启动接收新连接的线程
    m_acceptthread.reset(new std::thread(CMyReactor::accept_thread_proc, this));

    //启动工作线程
    for (auto& t : m_workerthreads)
    {
        t.reset(new std::thread(CMyReactor::worker_thread_proc, this));
    }


    return true;
}

bool CMyReactor::uninit()
{
    m_bStop = true;
    m_acceptcond.notify_one();
    m_workercond.notify_all();

    m_acceptthread->join();
    for (auto& t : m_workerthreads)
    {
        t->join();
    }

    ::epoll_ctl(m_epollfd, EPOLL_CTL_DEL, m_listenfd, NULL);

    //TODO: 是否需要先调用shutdown()一下？
    ::shutdown(m_listenfd, SHUT_RDWR);
    ::close(m_listenfd);
    ::close(m_epollfd);

    return true;
}

bool CMyReactor::close_client(int clientfd)
{
    if (::epoll_ctl(m_epollfd, EPOLL_CTL_DEL, clientfd, NULL) == -1)
    {
        std::cout << "close client socket failed as call epoll_ctl failed" << std::endl;
        //return false;
    }


    ::close(clientfd);

    return true;
}


void* CMyReactor::main_loop(void* p)
{
    std::cout << "main thread id = " << std::this_thread::get_id() << std::endl;

    CMyReactor* pReatcor = static_cast<CMyReactor*>(p);

    while (!pReatcor->m_bStop)
    {
        struct epoll_event ev[1024];
        int n = ::epoll_wait(pReatcor->m_epollfd, ev, 1024, 10);
        if (n == 0)
            continue;
        else if (n < 0)
        {
            std::cout << "epoll_wait error" << std::endl;
            continue;
        }

        int m = min(n, 1024);
        for (int i = 0; i < m; ++i)
        {
            //通知接收连接线程接收新连接
            if (ev[i].data.fd == pReatcor->m_listenfd)
                pReatcor->m_acceptcond.notify_one();
            //通知普通工作线程接收数据
            else
            {
                {
                    std::unique_lock<std::mutex> guard(pReatcor->m_workermutex);
                    pReatcor->m_listClients.push_back(ev[i].data.fd);
                }

                pReatcor->m_workercond.notify_one();
                //std::cout << "signal" << std::endl;
            }// end if

        }// end for-loop
    }// end while

    std::cout << "main loop exit ..." << std::endl;

    return NULL;
}

void CMyReactor::accept_thread_proc(CMyReactor* pReatcor)
{
    std::cout << "accept thread, thread id = " << std::this_thread::get_id() << std::endl;

    while (true)
    {
        int newfd;
        struct sockaddr_in clientaddr;
        socklen_t addrlen;
        {
            std::unique_lock<std::mutex> guard(pReatcor->m_acceptmutex);
            pReatcor->m_acceptcond.wait(guard);
            if (pReatcor->m_bStop)
                break;

            //std::cout << "run loop in accept_thread_proc" << std::endl;

            newfd = ::accept(pReatcor->m_listenfd, (struct sockaddr *)&clientaddr, &addrlen);
        }
        if (newfd == -1)
            continue;

        std::cout << "new client connected: " << ::inet_ntoa(clientaddr.sin_addr) << ":" << ::ntohs(clientaddr.sin_port) << std::endl;

        //将新socket设置为non-blocking
        int oldflag = ::fcntl(newfd, F_GETFL, 0);
        int newflag = oldflag | O_NONBLOCK;
        if (::fcntl(newfd, F_SETFL, newflag) == -1)
        {
            std::cout << "fcntl error, oldflag =" << oldflag << ", newflag = " << newflag << std::endl;
            continue;
        }

        struct epoll_event e;
        memset(&e, 0, sizeof(e));
        e.events = EPOLLIN | EPOLLRDHUP | EPOLLET;
        e.data.fd = newfd;
        if (::epoll_ctl(pReatcor->m_epollfd, EPOLL_CTL_ADD, newfd, &e) == -1)
        {
            std::cout << "epoll_ctl error, fd =" << newfd << std::endl;
        }
    }

    std::cout << "accept thread exit ..." << std::endl;
}

void CMyReactor::worker_thread_proc(CMyReactor* pReatcor)
{
    std::cout << "new worker thread, thread id = " << std::this_thread::get_id() << std::endl;

    while (true)
    {
        int clientfd;
        {
            std::unique_lock<std::mutex> guard(pReatcor->m_workermutex);
            while (pReatcor->m_listClients.empty())
            {
                if (pReatcor->m_bStop)
                {
                    std::cout << "worker thread exit ..." << std::endl;
                    return;
                }

                pReatcor->m_workercond.wait(guard);
            }

            clientfd = pReatcor->m_listClients.front();
            pReatcor->m_listClients.pop_front();
        }

        //gdb调试时不能实时刷新标准输出，用这个函数刷新标准输出，使信息在屏幕上实时显示出来
        std::cout << std::endl;

        std::string strclientmsg;
        char buff[256];
        bool bError = false;
        while (true)
        {
            memset(buff, 0, sizeof(buff));
            int nRecv = ::recv(clientfd, buff, 256, 0);
            if (nRecv == -1)
            {
                if (errno == EWOULDBLOCK)
                    break;
                else
                {
                    std::cout << "recv error, client disconnected, fd = " << clientfd << std::endl;
                    pReatcor->close_client(clientfd);
                    bError = true;
                    break;
                }

            }
            //对端关闭了socket，这端也关闭。
            else if (nRecv == 0)
            {
                std::cout << "peer closed, client disconnected, fd = " << clientfd << std::endl;
                pReatcor->close_client(clientfd);
                bError = true;
                break;
            }

            strclientmsg += buff;
        }

        //出错了，就不要再继续往下执行了
        if (bError)
            continue;

        std::cout << "client msg: " << strclientmsg;

        //将消息加上时间标签后发回
        time_t now = time(NULL);
        struct tm* nowstr = localtime(&now);
        std::ostringstream ostimestr;
        ostimestr << "[" << nowstr->tm_year + 1900 << "-"
            << std::setw(2) << std::setfill('0') << nowstr->tm_mon + 1 << "-"
            << std::setw(2) << std::setfill('0') << nowstr->tm_mday << " "
            << std::setw(2) << std::setfill('0') << nowstr->tm_hour << ":"
            << std::setw(2) << std::setfill('0') << nowstr->tm_min << ":"
            << std::setw(2) << std::setfill('0') << nowstr->tm_sec << "]server reply: ";

        strclientmsg.insert(0, ostimestr.str());

        while (true)
        {
            int nSent = ::send(clientfd, strclientmsg.c_str(), strclientmsg.length(), 0);
            if (nSent == -1)
            {
                if (errno == EWOULDBLOCK)
                {
                    std::this_thread::sleep_for(std::chrono::milliseconds(10));
                    continue;
                }
                else
                {
                    std::cout << "send error, fd = " << clientfd << std::endl;
                    pReatcor->close_client(clientfd);
                    break;
                }

            }

            std::cout << "send: " << strclientmsg;
            strclientmsg.erase(0, nSent);

            if (strclientmsg.empty())
                break;
        }
    }
}

bool CMyReactor::create_server_listener(const char* ip, short port)
{
    m_listenfd = ::socket(AF_INET, SOCK_STREAM | SOCK_NONBLOCK, 0);
    if (m_listenfd == -1)
        return false;

    int on = 1;
    ::setsockopt(m_listenfd, SOL_SOCKET, SO_REUSEADDR, (char *)&on, sizeof(on));
    ::setsockopt(m_listenfd, SOL_SOCKET, SO_REUSEPORT, (char *)&on, sizeof(on));

    struct sockaddr_in servaddr;
    memset(&servaddr, 0, sizeof(servaddr));
    servaddr.sin_family = AF_INET;
    servaddr.sin_addr.s_addr = inet_addr(ip);
    servaddr.sin_port = htons(port);
    if (::bind(m_listenfd, (sockaddr *)&servaddr, sizeof(servaddr)) == -1)
        return false;

    if (::listen(m_listenfd, 50) == -1)
        return false;

    m_epollfd = ::epoll_create(1);
    if (m_epollfd == -1)
        return false;

    struct epoll_event e;
    memset(&e, 0, sizeof(e));
    e.events = EPOLLIN | EPOLLRDHUP;
    e.data.fd = m_listenfd;
    if (::epoll_ctl(m_epollfd, EPOLL_CTL_ADD, m_listenfd, &e) == -1)
        return false;

    return true;
}