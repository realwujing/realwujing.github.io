#include <iostream>
using namespace std;

int main()
{
    constexpr int num = 1 + 2 + 3;
    int url[num] = {1,2,3,4,5,6};
    cout<< num << endl;
    cout<< url[1] << endl;
    return 0;
}