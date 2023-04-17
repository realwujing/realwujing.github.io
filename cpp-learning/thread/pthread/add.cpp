#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <unistd.h>
#include <sys/syscall.h>
#define NUMBER 4
int g_val = 100;

pthread_mutex_t mutex;//定义互斥锁


void *ThreadWork(void *arg)
{
  int *p = (int*)arg;
  pthread_detach(pthread_self());//自己分离自己，不用主线程回收它的资源了
  while(1)
  {
    pthread_mutex_lock(&mutex);//加锁
    if(g_val > 0)
    {
      printf("i am pid : %d,i get g_val : %d\n",(int)syscall(SYS_gettid),g_val);
      --g_val;
      usleep(2);
    }
    else{
      pthread_mutex_unlock(&mutex);//在所有可能退出的地方，进行解锁
      break;
    }
    pthread_mutex_unlock(&mutex);//解锁

  }
  pthread_exit(NULL);
}

int main()
{
  pthread_t tid[NUMBER];
  pthread_mutex_init(&mutex,NULL);//互斥锁初始化
  int i = 0;
  for(;i < NUMBER;++i)
  {
    int ret = pthread_create(&tid[i],NULL,ThreadWork,(void*)&g_val);//不要传临时变量，这里是示范
    if(ret != 0)
    {
      perror("pthread_create");
      return -1;
     }
  }
  //pthread_join(tid,NULL);//线程等待
  //pthread_detach(tid);//线程分离
  pthread_mutex_destroy(&mutex);//销毁互斥锁
  while(1)
  {
    printf("i am main work thread\n");
    sleep(1);
  }
  return 0;
}

