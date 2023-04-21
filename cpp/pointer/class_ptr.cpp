//指向类成员函数的函数指针
#include <iostream>
#include <cstdio>
using namespace std;

class A
{
public:
    A(int aa = 0) : a(aa) {}

    ~A() {}

    void setA(int aa = 1)
    {
        a = aa;
    }

    virtual void print()
    {
        cout << "A: " << a << endl;
    }

    virtual void printa()
    {
        cout << "A1: " << a << endl;
    }

private:
    int a;
};

class B : public A
{
public:
    B() : A(), b(0) {}

    B(int aa, int bb) : A(aa), b(bb) {}

    ~B() {}

    virtual void print()
    {
        A::print();
        cout << "B: " << b << endl;
    }

    virtual void printa()
    {
        A::printa();
        cout << "B: " << b << endl;
    }

private:
    int b;
};

int main(void)
{
    A a;
    B b;
    void (A::*ptr)(int) = &A::setA;
    A *pa = &a;

    //对于非虚函数，返回其在内存的真实地址
    printf("A::set(): %p\n", &A::setA);
    //对于虚函数， 返回其在虚函数表的偏移位置
    printf("B::print(): %p\n", &A::print);
    printf("B::print(): %p\n", &A::printa);

    a.print();

    a.setA(10);

    a.print();

    a.setA(100);

    a.print();
    //对于指向类成员函数的函数指针，引用时必须传入一个类对象的this指针，所以必须由类实体调用
    (pa->*ptr)(1000);

    a.print();

    (a.*ptr)(10000);

    a.print();
    return 0;
}