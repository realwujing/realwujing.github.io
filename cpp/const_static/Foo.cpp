#include <iostream>

class Foo{  
public:  
    Foo(int j):i(j){}  
// private:  
    // const int i=100;//error!!!  
    const int i;
};  
//或者通过这样的方式来进行初始化  
// Foo::Foo():i(100){}  

int main()
{
    Foo foo = Foo(102);
    std::cout << "foo.i:" << foo.i << std::endl;
    return 0;
}