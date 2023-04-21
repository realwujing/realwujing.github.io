#include <iostream>

using namespace std;

int test(int a)
{
    return a-1;
}

int test2(int (*fun)(int),int b)
{
    
    int c = fun(10)+b;
    return c;
}
 
int main(int argc, const char * argv[])
{
    
    typedef int (*fp)(int a);
    fp f = test;
    cout<<test2(f, 1)<<endl; // 调用 test2 的时候，把test函数的地址作为参数传递给了 test2
    return 0;
}