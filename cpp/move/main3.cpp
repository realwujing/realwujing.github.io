#include <iostream>

template <typename T>
void f(T &&t) 
{
    std::cout << t << std::endl;
}

int main(int argc, char const *argv[])
{
    /* code */
    f(10); //t是右值

    int x = 10;
    f(x); //t是左值

    return 0;
}
