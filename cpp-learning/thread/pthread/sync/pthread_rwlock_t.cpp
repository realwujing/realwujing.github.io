/*** 
 * @Author: wujing
 * @Date: 2021-02-24 04:09:44
 * @LastEditTime: 2021-02-24 04:09:53
 * @LastEditors: wujing
 * @Description: 
 * @FilePath: /code/CPlusPlusProject/pthread/sync/pthread_rwlock_t.cpp
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

int num = 5;
pthread_rwlock_t rwlock;

void *reader(void *arg)
{
    pthread_rwlock_rdlock(&rwlock);
    cout << "reader " << (long)arg << " got the lock" << endl;
    pthread_rwlock_unlock(&rwlock);
    return 0;
}

void *writer(void *arg)
{
    pthread_rwlock_wrlock(&rwlock);
    cout << "writer " << (long)arg << " got the lock" << endl;
    pthread_rwlock_unlock(&rwlock);
    return 0;
}

int main()
{
    int flag;
    long n = 1, m = 1;
    pthread_t wid, rid;
    pthread_attr_t attr;

    flag = pthread_rwlock_init(&rwlock, NULL);
    if (flag)
    {
        cout << "rwlock init error" << endl;
        return flag;
    }

    pthread_attr_init(&attr);
    pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_DETACHED); //thread sepatate

    for (int i = 0; i < num; i++)
    {
        if (i % 3)
        {
            pthread_create(&rid, &attr, reader, (void *)n);
            cout << "create reader " << n << endl;
            n++;
        }
        else
        {
            pthread_create(&wid, &attr, writer, (void *)m);
            cout << "create writer " << m << endl;
            m++;
        }
    }

    sleep(5); //wait other done
    return 0;
}