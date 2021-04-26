#include <iostream>
#include <stdio.h>
#include <string>

typedef int P();                                                                                 // 简单的
typedef void Q(int *p, const std::string &s1, const std::string &s2, size_t size, bool is_true); // 复杂的
class X
{
public:
    P eat_shit; // 等价于声明`int eat_shit();`
    Q bullshit; // 等价于声明`void bullshit(int *p, const string& s1, const string& s2, size_t size, bool is_true);`
};

int main()
{
    X *xx;
    printf("shit ret: %d\n", xx->eat_shit());
    int a[] = {1, 3, 4, 5, 7};
    xx->bullshit(a, "foo", "bar", sizeof(a) / sizeof(int), true);
}

int X::eat_shit()
{
    return 888;
}

void X::bullshit(int *p, const std::string &s1, const std::string &s2, size_t size, bool is_true)
{
    std::cout << "s1: " << s1 << ", s2: " << s2 << ", size: " << size << std::endl;
    printf("elems:\n");
    for (int i = 0; i < size; i++)
    {
        printf("%d %s", *p++, (i == size - 1) ? "" : ",");
    }
    printf("\n");
}