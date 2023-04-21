#include <iostream>
#include <typeinfo>
using namespace std;

//自定义类型的定义
struct myType {
    const char* name;
    int age;
    //其它结构体成员
};
//模板函数
template<typename T>
constexpr T dispaly(T t){
    return t;
}

int main()
{
    struct myType stu{"zhangsan",10};
    //普通函数
    struct myType ret = dispaly(stu);
    cout << ret.name << " " << ret.age << endl;
    //常量表达式函数
    constexpr int ret1 = dispaly(10);
    cout << ret1 << " " << typeid(ret1).name() << endl;
    return 0;
}