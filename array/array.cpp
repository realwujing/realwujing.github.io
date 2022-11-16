#include <iostream>
using namespace std;
int main()
{
    int a[6] = {1, 2, 3, 4, 5, '\0'};
    cout << a << endl;
    cout << &a << endl;
    cout << *a << endl;
    return 0;
}