/*** 
 * @Author: wujing
 * @Date: 2021-02-23 23:41:50
 * @LastEditTime: 2021-02-23 23:42:00
 * @LastEditors: wujing
 * @Description: 
 * @FilePath: /code/CPlusPlusProject/pthread/sync/pthread_cond_t.cpp
 * @可以输入预定的版权声明、个性签名、空行等
 */

#include <iostream>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
using namespace std;

pthread_cond_t qready = PTHREAD_COND_INITIALIZER;  //cond
pthread_mutex_t qlock = PTHREAD_MUTEX_INITIALIZER; //mutex

int x = 10, y = 20;

void *f1(void *arg)
{
    cout << "f1 start" << endl;
    pthread_mutex_lock(&qlock);
    while (x < y)
    {
        pthread_cond_wait(&qready, &qlock);
    }
    pthread_mutex_unlock(&qlock);
    sleep(3);
    cout << "f1 end" << endl;
    return 0;
}

void *f2(void *arg)
{
    cout << "f2 start" << endl;
    pthread_mutex_lock(&qlock);
    x = 20;
    y = 10;
    cout << "has a change,x=" << x << " y=" << y << endl;
    pthread_mutex_unlock(&qlock);
    if (x > y)
    {
        pthread_cond_signal(&qready);
    }
    cout << "f2 end" << endl;
    return 0;
}

int main()
{
    pthread_t tids[2];
    int flag;

    flag = pthread_create(&tids[0], NULL, f1, NULL);
    if (flag)
    {
        cout << "pthread 1 create error " << endl;
        return flag;
    }

    sleep(2);

    flag = pthread_create(&tids[1], NULL, f2, NULL);
    if (flag)
    {
        cout << "pthread 2 create erro " << endl;
        return flag;
    }

    sleep(5);
    return 0;
}