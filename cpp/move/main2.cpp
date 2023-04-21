#include <iostream>
using namespace std;

int g_constructCount = 0;
int g_copyConstructCount = 0;
int g_destructCount = 0;
struct A
{
    A()
    {
        cout << "construct: " << ++g_constructCount << endl;
    }

    A(const A &a)
    {
        cout << "copy construct: " << ++g_copyConstructCount << endl;
    }
    ~A()
    {
        cout << "destruct: " << ++g_destructCount << endl;
    }
};

A GetA()
{
    return A();
}

int main()
{
    A&& a = GetA();
    return 0;
}