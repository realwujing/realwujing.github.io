#include <iostream>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
using namespace std;

pthread_cond_t taxi_cond = PTHREAD_COND_INITIALIZER;    //taix arrive cond
pthread_mutex_t taxi_mutex = PTHREAD_MUTEX_INITIALIZER; // sync mutex

void *traveler_arrive(void *name)
{
    cout << "Traveler:" << (char *)name << " needs a taxi now!" << endl;
    pthread_mutex_lock(&taxi_mutex);
    pthread_cond_wait(&taxi_cond, &taxi_mutex);
    pthread_mutex_unlock(&taxi_mutex);
    cout << "Traveler:" << (char *)name << " now got a taxi!" << endl;
    pthread_exit((void *)0);
}

void *taxi_arrive(void *name)
{
    cout << "Taxi:" << (char *)name << " arriver." << endl;
    pthread_cond_signal(&taxi_cond);
    pthread_exit((void *)0);
}

int main()
{
    pthread_t tids[3];
    int flag;

    flag = pthread_create(&tids[0], NULL, taxi_arrive, (void *)("Jack"));
    if (flag)
    {
        cout << "pthread_create error:flag=" << flag << endl;
        return flag;
    }
    cout << "time passing by" << endl;
    sleep(1);

    flag = pthread_create(&tids[1], NULL, traveler_arrive, (void *)("Susan"));
    if (flag)
    {
        cout << "pthread_create error:flag=" << flag << endl;
        return flag;
    }
    cout << "time passing by" << endl;
    sleep(1);

    flag = pthread_create(&tids[2], NULL, taxi_arrive, (void *)("Mike"));
    if (flag)
    {
        cout << "pthread_create error:flag=" << flag << endl;
        return flag;
    }
    cout << "time passing by" << endl;
    sleep(1);

    void *ans;
    for (int i = 0; i < 3; i++)
    {
        flag = pthread_join(tids[i], &ans);
        if (flag)
        {
            cout << "pthread_join error:flag=" << flag << endl;
            return flag;
        }
        cout << "ans=" << ans << endl;
    }
    return 0;
}
