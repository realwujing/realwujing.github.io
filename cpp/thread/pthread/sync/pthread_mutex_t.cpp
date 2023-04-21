/*** 
 * @Author: wujing
 * @Date: 2021-02-23 23:32:37
 * @LastEditTime: 2021-02-23 23:32:46
 * @LastEditors: wujing
 * @Description: 
 * @FilePath: /code/CPlusPlusProject/pthread/synchronization/pthread_mutex_t.cpp
 * @可以输入预定的版权声明、个性签名、空行等
 */

#include <iostream>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

using namespace std;

int ticket_sum = 20;
pthread_mutex_t mutex_x = PTHREAD_MUTEX_INITIALIZER; //static init mutex

void *sell_ticket(void *arg)
{
    for (int i = 0; i < 20; i++)
    {
        pthread_mutex_lock(&mutex_x); //atomic opreation through mutex lock
        if (ticket_sum > 0)
        {
            sleep(1);
            cout << "sell the " << 20 - ticket_sum + 1 << "th" << endl;
            ticket_sum--;
        }
        pthread_mutex_unlock(&mutex_x);
    }
    return 0;
}

int main()
{
    int flag;
    pthread_t tids[4];

    for (int i = 0; i < 4; i++)
    {
        flag = pthread_create(&tids[i], NULL, &sell_ticket, NULL);
        if (flag)
        {
            cout << "pthread create error ,flag=" << flag << endl;
            return flag;
        }
    }

    sleep(20);
    void *ans;
    for (int i = 0; i < 4; i++)
    {
        flag = pthread_join(tids[i], &ans);
        if (flag)
        {
            cout << "tid=" << tids[i] << "join erro flag=" << flag << endl;
            return flag;
        }
        cout << "ans=" << ans << endl;
    }
    return 0;
}