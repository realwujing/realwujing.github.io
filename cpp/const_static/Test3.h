#pragma once

class Test3
{
private:
    int var1;
    // int var11= 4; 错误的初始化方法
    const int var2;
    // const int var22 =22222; 错误的初始化方法
    static int var3;
    // static int var3333=33333; 错误，只有静态常量int成员才能直接赋值来初始化
    static const int var4 = 4444; //正确，静态常量成员可以直接初始化
    static const int var44;

public:
    Test3(void);
    ~Test3(void);
};