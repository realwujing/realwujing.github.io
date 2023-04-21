#include <iostream>
using namespace std;

//自定义类型的定义
class myType {
public:
    constexpr myType(const char *name,int age):name(name),age(age){};
    constexpr const char * getname(){
        return name;
    }
    constexpr int getage(){
        return age;
    }
private:
    const char* name;
    int age;
    //其它结构体成员
};

int main()
{
    constexpr struct myType mt { "zhangsan", 10 };
    constexpr const char * name = mt.getname();
    constexpr int age = mt.getage();
    cout << name << " " << age << endl;
    return 0;
}