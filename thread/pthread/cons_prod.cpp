#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <pthread.h>
#include <queue>
#include <sys/syscall.h>

#define PTHREAD_COUNT 2
int data = 0;//全局变量作为插入数据
pthread_mutex_t mutex1;
class ModelOfConProd{
  public:
    ModelOfConProd()//构造
    {
      _capacity = 10;
      pthread_mutex_init(&_mutex,NULL);
      pthread_cond_init(&_cons,NULL);
      pthread_cond_init(&_prod,NULL);
    }
    ~ModelOfConProd()//析构
    {
      _capacity = 0;
      pthread_mutex_destroy(&_mutex);
      pthread_cond_destroy(&_cons);
      pthread_cond_destroy(&_prod);
    }

    void Push(int data)//push数据，生产者线程使用的
    {
      pthread_mutex_lock(&_mutex);
      while((int)_queue.size() >= _capacity)
      {
        pthread_cond_wait(&_prod,&_mutex);
      }
      _queue.push(data);
      pthread_mutex_unlock(&_mutex);
      pthread_cond_signal(&_cons);
    }


    void Pop(int& data)//pop数据，消费者线程使用的
    {
      pthread_mutex_lock(&_mutex);
      while(_queue.empty())
      {
        pthread_cond_wait(&_cons,&_mutex);
      }
      data = _queue.front();
      _queue.pop();
      pthread_mutex_unlock(&_mutex);
      pthread_cond_signal(&_prod);
    }

  private:
    int _capacity;//容量大小，限制容量大小
    std::queue<int> _queue;//队列
    pthread_mutex_t _mutex;//互斥锁
    pthread_cond_t _cons;//消费者条件变量
    pthread_cond_t _prod;//生产者条件变量
};

void *ConsumerStart(void *arg)//消费者入口函数
{
  ModelOfConProd *cp = (ModelOfConProd *)arg;
  while(1)
  {
    cp->Push(data);
    printf("i am pid : %d,i push :%d\n",(int)syscall(SYS_gettid),data);
    pthread_mutex_lock(&mutex1);//++的时候，给该全局变量加锁
    ++data;
    pthread_mutex_unlock(&mutex1);
  }
}


void *ProductsStart(void *arg)//生产者入口函数
{
  ModelOfConProd *cp = (ModelOfConProd *)arg;
  int data = 0;
  while(1)
  {
    cp->Pop(data);
    printf("i am pid : %d,i pop :%d\n",(int)syscall(SYS_gettid),data);
  }
}


int main()
{
  ModelOfConProd *cp = new ModelOfConProd;
  pthread_mutex_init(&mutex1,NULL);
  pthread_t cons[PTHREAD_COUNT],prod[PTHREAD_COUNT];
  for(int i = 0;i < PTHREAD_COUNT; ++i)
  {
    int ret = pthread_create(&cons[i],NULL,ConsumerStart,(void*)cp);
    if(ret < 0)
    {
      perror("pthread_create");
      return -1;
    }
    ret = pthread_create(&prod[i],NULL,ProductsStart,(void*)cp);
    if(ret < 0)
    {
      perror("pthread_create");
      return -1;
    }
  }


  for(int i = 0;i < PTHREAD_COUNT;++i)
  {
    pthread_join(cons[i],NULL);
    pthread_join(prod[i],NULL);
  }


  pthread_mutex_destroy(&mutex1);

  return 0;
}

