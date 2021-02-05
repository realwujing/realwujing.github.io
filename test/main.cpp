#include <iostream>
#include "name.h"
#include "Box.h"
#include "Rectangle.h"

using namespace std;

int main()
{
    good();
    bad();
    Box Box1;                // 声明 Box1，类型为 Box
    Box Box2;                // 声明 Box2，类型为 Box
    double volume = 0.0;     // 用于存储体积

    // box 1 详述
    Box1.setLength(6.0);
    Box1.setBreadth(7.0);
    Box1.setHeight(5.0);

    // box 2 详述
    Box2.setLength(12.0);
    Box2.setBreadth(13.0);
    Box2.setHeight(10.0);

    // box 1 的体积
    volume = Box1.getVolume();
    cout << "Box1 的体积：" << volume <<endl;

    // box 2 的体积
    volume = Box2.getVolume();
    cout << "Box2 的体积：" << volume <<endl;

    Rectangle Rect;

    Rect.setWidth(5);
    Rect.setHeight(7);

    // 输出对象的面积
    cout << "Total area: " << Rect.getArea() << endl;

    return 0;
}
