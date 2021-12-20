#ifndef THREAD_POOL_H
#define THREAD_POOL_H
 
#include <vector>
#include <queue>
#include <memory>
#include <thread>
#include <mutex>
#include <condition_variable>
#include <future>
#include <functional>
#include <stdexcept>
 
class ThreadPool {
 
public:
    ThreadPool(size_t);                          //构造函数
    template<class F, class... Args>             //类模板
    auto enqueue(F&& f, Args&&... args) -> std::future<typename std::result_of<F(Args...)>::type>;//任务入队
    ~ThreadPool();                              //析构函数
 
private:
    std::vector< std::thread > workers;            //线程队列，每个元素为一个Thread对象
    std::queue< std::function<void()> > tasks;     //任务队列，每个元素为一个函数对象    
 
    std::mutex queue_mutex;                        //互斥量
    std::condition_variable condition;             //条件变量
    bool stop;                                     //停止
};
 
// 构造函数，把线程插入线程队列，插入时调用embrace_back()，用匿名函数lambda初始化Thread对象
inline ThreadPool::ThreadPool(size_t threads) : stop(false){
 
    for(size_t i = 0; i<threads; ++i)
        workers.emplace_back(
            [this]
            {
                for(;;)
                {
                    // task是一个函数类型，从任务队列接收任务
                    std::function<void()> task;  
                    {
                        //给互斥量加锁，锁对象生命周期结束后自动解锁
                        std::unique_lock<std::mutex> lock(this->queue_mutex);
                        
                        //（1）当匿名函数返回false时才阻塞线程，阻塞时自动释放锁。
                        //（2）当匿名函数返回true且受到通知时解阻塞，然后加锁。
                        this->condition.wait(lock,[this]{ return this->stop || !this->tasks.empty(); });
                       
                         if(this->stop && this->tasks.empty())
                            return;
                        
                        //从任务队列取出一个任务
                        task = std::move(this->tasks.front());
                        this->tasks.pop();
                    }                            // 自动解锁
                    task();                      // 执行这个任务
                }
            }
        );
}
 
// 添加新的任务到任务队列
template<class F, class... Args>
auto ThreadPool::enqueue(F&& f, Args&&... args) 
    -> std::future<typename std::result_of<F(Args...)>::type>
{
    // 获取函数返回值类型        
    using return_type = typename std::result_of<F(Args...)>::type;
 
    // 创建一个指向任务的只能指针
    auto task = std::make_shared< std::packaged_task<return_type()> >(
            std::bind(std::forward<F>(f), std::forward<Args>(args)...)
        );
        
    std::future<return_type> res = task->get_future();
    {
        std::unique_lock<std::mutex> lock(queue_mutex);  //加锁
        if(stop)
            throw std::runtime_error("enqueue on stopped ThreadPool");
 
        tasks.emplace([task](){ (*task)(); });          //把任务加入队列
    }                                                   //自动解锁
    condition.notify_one();                             //通知条件变量，唤醒一个线程
    return res;
}
 
// 析构函数，删除所有线程
inline ThreadPool::~ThreadPool()
{
    {
        std::unique_lock<std::mutex> lock(queue_mutex);
        stop = true;
    }
    condition.notify_all();
    for(std::thread &worker: workers)
        worker.join();
}
 
#endif