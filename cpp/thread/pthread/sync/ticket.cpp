/*** 
 * @Author: wujing
 * @Date: 2021-02-23 23:25:22
 * @LastEditTime: 2021-02-23 23:25:40
 * @LastEditors: wujing
 * @Description: 
 * @FilePath: /code/CPlusPlusProject/pthread/synchronization/ticket.cpp
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
void *sell_ticket(void *arg)
{
    for (int i = 0; i < 20; i++)
    {
        if (ticket_sum > 0)
        {
            sleep(1);
            cout << "sell the " << 20 - ticket_sum + 1 << "th" << endl;
            ticket_sum--;
        }
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