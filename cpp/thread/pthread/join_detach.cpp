#include <iostream>
#include <thread>
#include <unistd.h>
using namespace std;

void func()
{
    for (int i = -10; i > -20; i--)
    {
        cout << "from func():" << i << endl;
        sleep(10);
    }
}

int main()			//主线程
{
    cout << "mian()" << endl;
    cout << "mian()" << endl;
    cout << "mian()" << endl;
    thread t(func);	//子线程
    t.join();		//等待子线程结束后才进入主线程
    // t.detach();		//分离子线程
    return 0;
}
