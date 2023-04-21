#include <iostream>
using namespace std;

class M
{
public:
    M(int a)
    {
        A = a;
        B += a;
    }

    static void f1(M m);

private:
    int A;
    static int B;
};

void M::f1(M m)
{
    cout << "A=" << m.A << endl;
    cout << "B=" << M::B << endl;
}

int M::B = 0;

int main()
{
    M P(5), Q(10);
    M::f1(P); //调用时不用对象名
    M::f1(Q);

    return 0;
}