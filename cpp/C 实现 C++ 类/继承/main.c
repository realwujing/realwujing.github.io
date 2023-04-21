#include "rect.h"  
#include <stdio.h> 

int main() 
{
    Rectangle r1, r2;

    // 实例化对象
    Rectangle_ctor(&r1, 0, 2, 10, 15);
    Rectangle_ctor(&r2, -1, 3, 5, 8);

    printf("Rect r1(x=%d,y=%d,width=%d,height=%d)\n",
           Shape_getX(&r1.super), Shape_getY(&r1.super),
           r1.width, r1.height);
    printf("Rect r2(x=%d,y=%d,width=%d,height=%d)\n",
           Shape_getX(&r2.super), Shape_getY(&r2.super),
           r2.width, r2.height);

    // 注意，这里有两种方式，一是强转类型，二是直接使用成员地址
    Shape_moveBy((Shape *)&r1, -2, 3);
    Shape_moveBy(&r2.super, 2, -1);

    printf("Rect r1(x=%d,y=%d,width=%d,height=%d)\n",
           Shape_getX(&r1.super), Shape_getY(&r1.super),
           r1.width, r1.height);
    printf("Rect r2(x=%d,y=%d,width=%d,height=%d)\n",
           Shape_getX(&r2.super), Shape_getY(&r2.super),
           r2.width, r2.height);

    return 0;
}