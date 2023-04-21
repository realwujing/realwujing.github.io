#include <iostream>
using namespace std;

//普通函数的声明
int noconst_dis(int x);
//常量表达式函数的声明
constexpr int display(int x);

//常量表达式函数的定义
constexpr int display(int x){
    return 1 + 2 + x;
}

int main()
{
    //调用常量表达式函数
    int a[display(3)] = { 1,2,3,4 };
    cout << a[2] << endl;
    //调用普通函数
    cout << noconst_dis(3) << endl;
    return 0;
}

//普通函数的定义
int noconst_dis(int x) {
    return 1 + 2 + x;
}
