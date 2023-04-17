#include <iostream>
using namespace std;

void max(int &num1, int num2)
{
    cout << "(num1 > num2 ? 1 : 0):" << (num1 > num2 ? 1 : 0) << endl;
}

int main()
{   
    int x = 100,y=200,z=300;
    auto ff  = [=,&y,&z](int n) {
        max(y, z);
        cout << x << endl;
        y++; z++;
        return n*n;
    };
    cout << ff(15) << endl;
    cout << y << "," << z << endl;
}