#include <iostream>
#include "library.h"
using std::cout;
using std::endl;

int main()
{

    hello();
    cout << "1 + 2 = " << sum(1, 2) << endl;
    cout << "1 + 2 + 3 = " << sum(1, 2, 3) << endl;

    return 0;
}