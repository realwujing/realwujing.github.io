#include <iostream>
using namespace std;

int main()
{
    int i = 0x12345678;

    if (*((char *)&i) == 0x12)
        cout << "大端" << endl;
    else
        cout << "小端" << endl;

    return 0;
}