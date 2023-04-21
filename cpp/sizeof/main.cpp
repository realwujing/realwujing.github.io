#include <iostream>

int main()
{
    int arr[100];
    int *p = arr;
    arr[0] = 0;
    arr[1] = 1;
    std::cout << "arr[0]:" << arr[0] << "|arr[1]:" << arr[1] << std::endl;
    std::cout << "*p:" << *p << "|*(p + 1):" << *(p + 1) << std::endl;
    std::cout << "sizeof(arr):" << sizeof(arr) << "|sizeof(p):" << sizeof(p) << std::endl;

    return 0;
}