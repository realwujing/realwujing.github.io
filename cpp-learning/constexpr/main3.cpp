#include <iostream>
using namespace std;

int num = 3;
constexpr int display(int x){
    return num + x;
}
int main()
{
    //调用常量表达式函数
    int a[display(3)] = { 1,2,3,4 };
    return 0;
}