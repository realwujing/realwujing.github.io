#include "shape.h"
#include <assert.h>

// Shape 的虚函数
static uint32_t Shape_area_(Shape const * const me);
static void Shape_draw_(Shape const * const me);

// 构造函数
void Shape_ctor(Shape * const me, int16_t x, int16_t y) 
{
    // Shape 类的虚表
    static struct ShapeVtbl const vtbl = 
    { 
       &Shape_area_,
       &Shape_draw_
    };
    me->vptr = &vtbl; 
    me->x = x;
    me->y = y;
}


void Shape_moveBy(Shape * const me, int16_t dx, int16_t dy)
{
    me->x += dx;
    me->y += dy;
}


int16_t Shape_getX(Shape const * const me) 
{
    return me->x;
}
int16_t Shape_getY(Shape const * const me) 
{
    return me->y;
}

// Shape 类的虚函数实现
static uint32_t Shape_area_(Shape const * const me) 
{
    assert(0); // 类似纯虚函数
    return 0U; // 避免警告
}

static void Shape_draw_(Shape const * const me) 
{
    assert(0); // 纯虚函数不能被调用
}


Shape const *largestShape(Shape const *shapes[], uint32_t nShapes) 
{
    Shape const *s = (Shape *)0;
    uint32_t max = 0U;
    uint32_t i;
    for (i = 0U; i < nShapes; ++i) 
    {
        uint32_t area = Shape_area(shapes[i]);// 虚函数调用
        if (area > max) 
        {
            max = area;
            s = shapes[i];
        }
    }
    return s;
}


void drawAllShapes(Shape const *shapes[], uint32_t nShapes) 
{
    uint32_t i;
    for (i = 0U; i < nShapes; ++i) 
    {
        Shape_draw(shapes[i]); // 虚函数调用
    }
}