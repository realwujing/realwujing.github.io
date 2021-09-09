#include <iostream>

class Foo{  
public:  
    Foo();  
// private:  
static int i;  
};  
  
int Foo::i = 20;  

int main()
{
    std::cout << "Foo::i:" << Foo::i << std::endl;
}