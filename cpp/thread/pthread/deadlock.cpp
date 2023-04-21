/**

查看多个线程堆栈：thread apply all bt
跳转到线程中：t 线程号
查看具体的调用堆栈：f 堆栈号
直接从pid号用gdb调试：gdb attach pid

*/

#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <unistd.h>
#include <sys/syscall.h>
#define NUMBER 2

pthread_mutex_t mutex1;//定义互斥锁
pthread_mutex_t mutex2;


void *ThreadWork1(void *arg)
{
  int *p = (int*)arg;
  pthread_mutex_lock(&mutex1);
  
  sleep(2);
  
  pthread_mutex_lock(&mutex2);
  pthread_mutex_unlock(&mutex2);
  pthread_mutex_unlock(&mutex1);
  return NULL;
}

void *ThreadWork2(void *arg)
{
  int *p = (int*)arg;
  pthread_mutex_lock(&mutex2);
  
  sleep(2);
  
  pthread_mutex_lock(&mutex1);
  pthread_mutex_unlock(&mutex1);
  pthread_mutex_unlock(&mutex2);
  return NULL;
}
int main()
{
  pthread_t tid[NUMBER];
  pthread_mutex_init(&mutex1,NULL);//互斥锁初始化
  pthread_mutex_init(&mutex2,NULL);//互斥锁初始化
  int i = 0;
  int ret = pthread_create(&tid[0],NULL,ThreadWork1,(void*)&i);
  if(ret != 0)
  {
    perror("pthread_create");
    return -1;
  }
  ret = pthread_create(&tid[1],NULL,ThreadWork2,(void*)&i);
  if(ret != 0)
  {
    perror("pthread_create");
    return -1;
  }
  //pthread_join(tid,NULL);//线程等待
  //pthread_join(tid,NULL);//线程等待
  //pthread_detach(tid);//线程分离
  pthread_join(tid[0],NULL);
  pthread_join(tid[1],NULL);
  pthread_mutex_destroy(&mutex1);//销毁互斥锁
  pthread_mutex_destroy(&mutex2);//销毁互斥锁
  while(1)
  {
    printf("i am main work thread\n");
    sleep(1);
  }
  return 0;
}


