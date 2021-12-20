/*** 
 * @Author: wujing
 * @Date: 2021-02-23 23:39:03
 * @LastEditTime: 2021-02-23 23:39:11
 * @LastEditors: wujing
 * @Description: 
 * @FilePath: /code/CPlusPlusProject/pthread/sync/pthread_mutex_lock.cpp
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

int ticket_sum = 20;
pthread_mutex_t mutex_x = PTHREAD_MUTEX_INITIALIZER; //static init mutex

void *sell_ticket_1(void *arg)
{
    for (int i = 0; i < 20; i++)
    {
        pthread_mutex_lock(&mutex_x);
        if (ticket_sum > 0)
        {
            sleep(1);
            cout << "thread_1 sell the " << 20 - ticket_sum + 1 << "th ticket" << endl;
            ticket_sum--;
        }
        sleep(1);
        pthread_mutex_unlock(&mutex_x);
        sleep(1);
    }
    return 0;
}

void *sell_ticket_2(void *arg)
{
    int flag;
    for (int i = 0; i < 10; i++)
    {
        flag = pthread_mutex_trylock(&mutex_x);
        if (flag == EBUSY)
        {
            cout << "sell_ticket_2:the variable is locked by sell_ticket_1" << endl;
        }
        else if (flag == 0)
        {
            if (ticket_sum > 0)
            {
                sleep(1);
                cout << "thread_2 sell the " << 20 - ticket_sum + 1 << "th tickets" << endl;
                ticket_sum--;
            }
            pthread_mutex_unlock(&mutex_x);
        }
        sleep(1);
    }
    return 0;
}
int main()
{
    int flag;
    pthread_t tids[2];

    flag = pthread_create(&tids[0], NULL, &sell_ticket_1, NULL);
    if (flag)
    {
        cout << "pthread create error ,flag=" << flag << endl;
        return flag;
    }

    flag = pthread_create(&tids[1], NULL, &sell_ticket_2, NULL);
    if (flag)
    {
        cout << "pthread create error ,flag=" << flag << endl;

        return flag;
    }

    void *ans;
    sleep(30);
    flag = pthread_join(tids[0], &ans);
    if (flag)
    {
        cout << "tid=" << tids[0] << "join erro flag=" << flag << endl;
        return flag;
    }
    else
    {
        cout << "ans=" << ans << endl;
    }

    flag = pthread_join(tids[1], &ans);
    if (flag)
    {
        cout << "tid=" << tids[1] << "join erro flag=" << flag << endl;
        return flag;
    }
    else
    {
        cout << "ans=" << ans << endl;
    }

    return 0;
}