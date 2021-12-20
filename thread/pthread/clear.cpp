#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <pthread.h>
#include <unistd.h>
#include <sys/syscall.h>
#define NUMBER 2
int g_bowl = 0;

pthread_mutex_t mutex;//定义互斥锁

void clean(void *arg)
{
  printf("Clean up:%s\n",(char*)arg);
  pthread_mutex_unlock(&mutex);//释放锁
}

void *WorkCancel(void *arg)
{
  pthread_mutex_lock(&mutex);
  pthread_cleanup_push(clean,"clean up handler");//清除函数的push
  struct timespec t = {3,0};//取消点
  nanosleep(&t,0);
  pthread_cleanup_pop(0);//清除
  pthread_mutex_unlock(&mutex);
}

void *WorkWhile(void *arg)
{
  sleep(5);
  pthread_mutex_lock(&mutex);
  printf("i get the mutex\n");//若能拿到资源，则表示取消清理函数成功！
  pthread_mutex_unlock(&mutex);
  return NULL;
}
int main()
{
  pthread_t cons,prod;
  pthread_mutex_init(&mutex,NULL);//互斥锁初始化
  int ret = pthread_create(&prod,NULL,WorkCancel,(void*)&g_bowl);//该线程拿到锁，然后挂掉
  if(ret != 0)
  {
    perror("pthread_create");
    return -1;
  }
  int ret1 = pthread_create(&cons,NULL,WorkWhile,(void*)&ret);//测试该线程是否可以拿到锁
  if(ret1 != 0)
  {
    perror("pthread_create");
    return -1;
  }

  pthread_cancel(prod);//取消该线程
  pthread_join(prod,NULL);//线程等待
  pthread_join(cons,NULL);//线程等待
  pthread_mutex_destroy(&mutex);//销毁互斥锁
  while(1)
  {
    sleep(1);
  }
  return 0;
}

