#include "shape.h"

// 构造函数
void Shape_ctor(Shape * const me, int16_t x, int16_t y)
{
    me->x = x;
    me->y = y;
}

void Shape_moveBy(Shape * const me, int16_t dx, int16_t dy) 
{
    me->x += dx;
    me->y += dy;
}

// 获取属性值函数
int16_t Shape_getX(Shape const * const me) 
{
    return me->x;
}
int16_t Shape_getY(Shape const * const me) 
{
    return me->y;
}