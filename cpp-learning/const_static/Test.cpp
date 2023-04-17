#include <iostream>

class Test{  
public:  
    Test():a(0){}  
    enum {size1=100,size2=200};  
// private: 
public: 
    const int a;//只能在构造函数初始化列表中初始化  
    static int b;//在类的实现文件中定义并初始化  
    const static int c;//与 static const int c;相同。  
};  
   
int Test::b = 0;//static成员变量不能在构造函数初始化列表中初始化，因为它不属于某个对象。  
const int Test::c = 0;//注意：给静态成员变量赋值时，不需要加static修饰符，但要加cosnt。  

int main()
{
    Test test;
    std::cout <<"test.size2:" << test.size2 << "|test.a:" << test.a << "|Test::b:" << Test::b << "|Test::c:" << Test::c << std::endl;
    return 0;
}