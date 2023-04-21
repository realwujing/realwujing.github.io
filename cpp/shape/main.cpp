#include <iostream>

#include "rectangle.h"
#include "triangle.h"
 
using namespace std;
 
 
int main(void)
{
   Rectangle Rect;
   Triangle  Tri;
 
   // Rect.setWidth(5);
   // Rect.setHeight(7);
   // 输出对象的面积
   cout << "Total Rectangle area: " << Rect.getArea() << endl;
 
   Tri.setWidth(5);
   Tri.setHeight(7);
   // 输出对象的面积
   cout << "Total Triangle area: " << Tri.getArea() << endl; 
 
   return 0;
}