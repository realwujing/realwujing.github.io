#include<iostream>

// 类
class A
{
    private:
        const int a;                    // 常对象成员，可以使用初始化列表或者类内初始化

    public:
        // 构造函数
        A() : a(0){};
        A(int x) : a(x){};              // 初始化列表

        // const可用于对重载函数的区分
        int getValue();                 // 普通成员函数
        int getValue() const;           // 常成员函数，不得修改类中的任何数据成员的值
};

void function()
{
    // 对象
    A b;                                // 普通对象，可以调用全部成员函数
    const A a;                          // 常对象，只能调用常成员函数
    const A *p = &a;                    // 指针变量，指向常对象
    const A &q = a;                     // 指向常对象的的引用

    // 指针
    char greeting[] = "Hello";
    char *p1 = greeting;                // 指针变量，指向字符数组变量
    const char *p2 = greeting;          // 指针变量，指向字符数组常量（const 后面是 char，说明只想的字符（char）不可改变）
    char *const p3 = greeting;          // 自身是常量的指针，指向字符数组变量（const 后面是 p3，说明 p3 指针自身不可改变）
    const char *const p4 = greeting;    // 自身是常量的指针，指向字符数组常量
}

// 函数
void function1(const int Var);          // 传递过来的参数在函数内不可变
void function2(const char* Var);        // 参数指针所指内容为常量
void function3(char* const Var);        // 参数指针为常量
void function4(const int& Var);         // 引用参数在函数内为常量

// 函数返回值
const int function5();                  // 返回一个常数
const int *function6();                 // 返回一个指向常量的指针，使用：const int* p = function6()
int *const function7();                 // 返回一个指向变量的常指针，使用：int* const p = function7()

