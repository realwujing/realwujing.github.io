#include "Test3.h"

int Test3::var3 = 3333333; //静态成员的正确的初始化方法

// int Test3::var1 = 11111;; 错误静态成员才能初始化
// int Test3::var2 = 22222; 错误
// int Test3::var44 = 44444; // 错误的方法，提示重定义
Test3::Test3(void) : var1(11111), var2(22222) // 正确的初始化方法 //var3(33333)不能在这里初始化
{
    var1 = 11111; //正确, 普通变量也可以在这里初始化
    //var2 = 222222; 错误，因为常量不能赋值，只能在 “constructor initializer （构造函数的初始化列表）” 那里初始化

    var3 = 44444; //这个赋值是正确的，不过因为所有对象一个静态成员，所以会影响到其他的，这不能叫做初始化了吧
}
Test3::~Test3(void) {}

int main()
{
    Test3 test3(void);
    return 0;
}