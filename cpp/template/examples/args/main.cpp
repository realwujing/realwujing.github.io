#include <iostream>
using namespace std;

//递归终止函数
void print()
{
    cout << "empty" << endl;
}

//展开函数
template <class T, class... Args>
void print(T head, Args... rest)
{
    cout << "sizeof...(rest):" << sizeof...(rest) << endl; //打印变参的个数
    cout << "parameter " << head << endl;
    print(rest...);
}

int main(void)
{
    print(1, 2, 3, 4);
    return 0;
}