#pragma once

// 基类
class Shape 
{
public:
   Shape();
   // 提供接口框架的纯虚函数
   virtual int getArea() = 0;
   void setWidth(int w);
   void setHeight(int h);
   
protected:
   int width;
   int height;
};
