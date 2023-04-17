#include <iostream>
#include <typeinfo>

using namespace std;
void add(int &a, int &b, int &c);


int main ()
{
    int  *ptr = NULL;
    int c, a = 2, b = 4;
    char e = 'd';

    typedef struct LNode
    {
        int data;
        struct LNode *next;
    } LNode, *LinkList;

    add(a, b, c);

    LinkList linkList;

    ptr = &a;
    cout << "ptr 指向的地址是 " << ptr << endl;
    cout << "ptr 的地址是 " << &ptr << endl;
    cout << "*ptr 的地址是 " << &(*ptr) << endl;
    cout << "ptr 的对象类型是 " << typeid(ptr).name() << endl;
    cout << "c 的值是 " << c << endl;

    cout << "&c 的对象类型是 " << typeid(&c).name() << endl;

    cout << "e 的对象类型是 " << typeid(e).name() << endl;

    cout << "linkList 的对象类型是 " << typeid(linkList).name() << endl;

    return 0;
}

void add(int &a, int &b, int &c)
{
    
    c  = a + b;
}