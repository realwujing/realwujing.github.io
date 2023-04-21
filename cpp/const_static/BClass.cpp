// Initialization of Special Data Member
#include <iostream>
using namespace std;

class BClass
{
public:
    BClass() : i(1), ci(2), ri(i) // 对于常量型成员变量和引用型成员变量，必须通过
    {                             // 参数化列表的方式进行初始化。在构造函数体内进行赋值的方式，是行不通的。
    }

    void print_values()
    {
        cout << "i =\t" << i << endl;
        cout << "ci =\t" << ci << endl;
        cout << "ri =\t" << ri << endl;
        cout << "si =\t" << si << endl;
        cout << "csi =\t" << csi << endl;
        cout << "csi2 =\t" << csi2 << endl;
        cout << "csd =\t" << csd << endl;
    }

private:
    int i;         // 普通成员变量
    const int ci;  // 常量成员变量
    int &ri;      // 引用成员变量
    static int si; // 静态成员变量
    //static int si2 = 100; // error: 只有静态常量成员变量，才可以这样初始化
    static const int csi;        // 静态常量成员变量
    static const int csi2 = 100; // 静态常量成员变量的初始化(Integral type) (1)
    static const double csd;     // 静态常量成员变量(non-Integral type)
    //static const double csd2 = 99.9; // error: 只有静态常量整型数据成员才可以在类中初始化
};
// 静态成员变量的初始化(Integral type)
int BClass::si = 0;
// 静态常量成员变量的初始化(Integral type)
const int BClass::csi = 1;
// 静态常量成员变量的初始化(non-Integral type)
const double BClass::csd = 99.9;

// 在初始化(1)中的csi2时，根据Stanley B. Lippman的说法下面这行是必须的。
// 但在VC2003中如果有下面一行将会产生错误，而在VC2005中，下面这行则可有可无，这个和编译器有关。
const int BClass::csi2;

int main(void)
{
    BClass b_class;
    b_class.print_values();
    return 0;
}