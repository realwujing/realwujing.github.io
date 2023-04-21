#include <iostream>

class Test2{  
public:  
    static const int mask1;  
    const static int mask2;  
};  
const int Test2::mask1 = 0xffff;  
const int Test2::mask2 = 0xffff;  
//它们的初始化没有区别，虽然一个是静态常量一个是常量静态。静态都将存储在全局变量区域，其实最后结果都一样。可能在不同编译器内，不同处理，但最后结果都一样。 

int main()
{
    std::cout << "Test2::mask1:" << Test2::mask1 << "|Test2::mask2:" << Test2::mask2 << std::endl;
}