#include <iostream>
using namespace std;

int fib(int N)
{
    if (N == 1 || N == 2)
        return 1;
    return fib(N - 1) + fib(N - 2);
}

int main()
{
    cout << fib(20) << endl;
    return 0;
}