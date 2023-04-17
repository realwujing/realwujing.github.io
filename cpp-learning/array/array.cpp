#include <iostream>
using namespace std;
int main()
{
    int a[6] = {1, 3, 5, 7, 9, '\0'};
    cout << a << endl;
    cout << &a << endl;
    cout << *a << endl;
    return 0;
}