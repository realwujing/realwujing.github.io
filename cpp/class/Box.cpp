#include <iostream>
 
using namespace std;
 
// 基类
class Shape 
{
   public:
      void setWidth(int w)
      {
         width = w;
      }
      void setHeight(int h)
      {
         height = h;
      }
//    protected:
    private:
      int width;
      int height;
};
 
// 派生类
class Rectangle: public Shape
{
   public:
      int rect;
};
 
int main(void)
{
   Rectangle Rect;
 
   Rect.rect = 20;
   Rect.width = 100;
 
   return 0;
}