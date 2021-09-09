#include <iostream>
using namespace std;

class A
{
public:
    A(int a);
    static void print(); //静态成员函数
private:
    static int aa;          //静态数据成员的声明
    static const int count; //常量静态数据成员（可以在构造函数中初始化）
    const int bb;           //常量数据成员
};

int A::aa = 0;           //静态成员的定义+初始化
const int A::count = 25; //静态常量成员定义+初始化

A::A(int a) : bb(a)
{ //常量成员的初始化
    aa += 1;
}

void A::print()
{
    cout << "count=" << count << endl;
    cout << "aa=" << aa << endl;
}

int main()
{
    A a(10);
    A::print(); //通过类访问静态成员函数
    a.print();  //通过对象访问静态成员函数

    return 0;
}