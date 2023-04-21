#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <unistd.h>
#include <sys/syscall.h>
#define NUMBER 2

int g_bowl = 0;
pthread_mutex_t mutex;//定义互斥锁
pthread_cond_t cond1;//条件变量
pthread_cond_t cond2;//条件变量


void *WorkProduct(void *arg)
{
  int *p = (int*)arg;
  while(1)
  {
    pthread_mutex_lock(&mutex);
    while(*p > 0)
    {
      pthread_cond_wait(&cond2,&mutex);//条件等待，条件不满足，陷入阻塞
    }
    ++(*p);
    printf("i am workproduct :%d,i product %d\n",(int)syscall(SYS_gettid),*p);
    pthread_cond_signal(&cond1);//通知消费者
    pthread_mutex_unlock(&mutex);//释放锁
  }
  return NULL;
}

void *WorkConsume(void *arg)
{
  int *p = (int*)arg;
  while(1)
  {
    pthread_mutex_lock(&mutex);
    while(*p <= 0)
    {
      pthread_cond_wait(&cond1,&mutex);//条件等待，条件不满足，陷入阻塞
    }
    printf("i am workconsume :%d,i consume %d\n",(int)syscall(SYS_gettid),*p);
    --(*p);
    pthread_cond_signal(&cond2);//通知生产者
    pthread_mutex_unlock(&mutex);//释放锁
  }
  return NULL;
}
int main()
{
  pthread_t cons[NUMBER],prod[NUMBER];
  pthread_mutex_init(&mutex,NULL);//互斥锁初始化
  pthread_cond_init(&cond1,NULL);//条件变量初始化
  pthread_cond_init(&cond2,NULL);//条件变量初始化
  int i = 0;
  for(;i < NUMBER;++i)
  {
    int ret = pthread_create(&prod[i],NULL,WorkProduct,(void*)&g_bowl);
    if(ret != 0)
    {
      perror("pthread_create");
      return -1;
    }
    ret = pthread_create(&cons[i],NULL,WorkConsume,(void*)&g_bowl);
    if(ret != 0)
    {
      perror("pthread_create");
      return -1;
    }
  }
  for(i = 0;i < NUMBER;++i)
  {
    pthread_join(cons[i],NULL);//线程等待
    pthread_join(prod[i],NULL);
  }
  pthread_mutex_destroy(&mutex);//销毁互斥锁
  pthread_cond_destroy(&cond1);
  pthread_cond_destroy(&cond2);
  while(1)
  {
    printf("i am main work thread\n");
    sleep(1);
  }
  return 0;
}

