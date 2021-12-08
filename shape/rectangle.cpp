#include "rectangle.h"

#include <iostream>

Rectangle::Rectangle()
{
   width = 1;
   height = 1;
   std::cout << "width:" << width << std::endl;
   std::cout << "height:" << height << std::endl;
   std::cout << "Rectangle()" << std::endl;
}

// 派生类
int Rectangle::getArea()
{
   return (width * height); 
}