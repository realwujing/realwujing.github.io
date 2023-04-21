#include <stdio.h>

//求和函数
int sum(int v1, int v2)
{
    return v1 + v2;
}

//宏定义
/*
  #define 代表要定义一个宏
  SUM  宏的名称
  (v1,v2)  参数，不要写数据类型
  v1+v2 用于替换的内容
 
 宏定义并不会做任何运算，无论是有参数还是无参数，仅仅是在翻译成0和1之前做一个简单的“替换”
 */
#define SUM(v1, v2) v1 + v2

//注意点：一般情况下写带参数的宏的时候，给每个参数加()
#define CF(v1, v2) (v1) * (v2)

int main(int argc, const char *argv[])
{

    //1.求和 用函数的方式
    int a = 10;
    int b = 5;
    int res = sum(a, b);
    printf("res=%i\n", res); //res=15

    //2.使用宏
    int result = SUM(a, b);
    printf("result=%i\n", result); //result=15

    /*
     如果函数内部的功能比较简单，仅仅是做一些简单的运算，那么可以使用宏定义。
     如果函数内部的功能比较复杂，不仅仅是一些简单的运算，那么建议使用函数。
     */

    return 0;
}
