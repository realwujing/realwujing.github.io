/*** 
 * @Author: wujing
 * @Date: 2021-02-24 04:11:46
 * @LastEditTime: 2021-02-24 04:11:59
 * @LastEditors: wujing
 * @Description: 
 * @FilePath: /code/CPlusPlusProject/pthread/sync/sem_t .cpp
 * @可以输入预定的版权声明、个性签名、空行等
 */

#include <iostream>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <semaphore.h>
using namespace std;

int num = 10;
sem_t sem;

void *get_service(void *cid)
{
    int id = *((int *)cid);
    if (sem_wait(&sem) == 0)
    {
        sleep(5);
        cout << "customer " << id << " get the service" << endl;
        cout << "customer " << id << " done " << endl;
        sem_post(&sem);
    }
    return 0;
}

int main()
{
    sem_init(&sem, 0, 2);
    pthread_t customer[num];
    int flag;

    for (int i = 0; i < num; i++)
    {
        int id = i;
        flag = pthread_create(&customer[i], NULL, get_service, &id);
        if (flag)
        {
            cout << "pthread create error" << endl;
            return flag;
        }
        else
        {
            cout << "customer " << i << " arrived " << endl;
        }
        sleep(1);
    }

    //wait all thread done
    for (int j = 0; j < num; j++)
    {
        pthread_join(customer[j], NULL);
    }
    sem_destroy(&sem);
    return 0;
}