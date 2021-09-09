#include "rect.h"  
#include "circle.h" 
#include <stdio.h> 

int main() 
{
    Rectangle r1, r2; 
    Circle    c1, c2; 
    Shape const *shapes[] = 
    { 
        &c1.super,
        &r2.super,
        &c2.super,
        &r1.super
    };
    Shape const *s;

    // 实例化矩形对象
    Rectangle_ctor(&r1, 0, 2, 10, 15);
    Rectangle_ctor(&r2, -1, 3, 5, 8);

    // 实例化圆形对象
    Circle_ctor(&c1, 1, -2, 12);
    Circle_ctor(&c2, 1, -3, 6);

    s = largestShape(shapes, sizeof(shapes)/sizeof(shapes[0]));
    printf("largetsShape s(x=%d,y=%d)\n", Shape_getX(s), Shape_getY(s));

    drawAllShapes(shapes, sizeof(shapes)/sizeof(shapes[0]));

    return 0;
}