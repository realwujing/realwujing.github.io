#include <iostream>

using namespace std;
void add(int &a, int &b, int &c);


int main ()
{
    int  *ptr = NULL;
    int c, a = 2, b = 4;
    add(a, b, c);
    cout << "ptr 的值是 " << ptr << endl;
    cout << "c 的值是 " << c << endl;

    return 0;
}

void add(int &a, int &b, int &c)
{
    
    c  = a + b;
}