#ifndef SHAPE_H
#define SHAPE_H

#include <stdint.h>

struct ShapeVtbl;
// Shape 的属性
typedef struct {
    struct ShapeVtbl const *vptr;
    int16_t x; 
    int16_t y; 
} Shape;

// Shape 的虚表
struct ShapeVtbl {
    uint32_t (*area)(Shape const * const me);
    void (*draw)(Shape const * const me);
};

// Shape 的操作函数，接口函数
void Shape_ctor(Shape * const me, int16_t x, int16_t y);
void Shape_moveBy(Shape * const me, int16_t dx, int16_t dy);
int16_t Shape_getX(Shape const * const me);
int16_t Shape_getY(Shape const * const me);

static inline uint32_t Shape_area(Shape const * const me) 
{
    return (*me->vptr->area)(me);
}

static inline void Shape_draw(Shape const * const me)
{
    (*me->vptr->draw)(me);
}


Shape const *largestShape(Shape const *shapes[], uint32_t nShapes);
void drawAllShapes(Shape const *shapes[], uint32_t nShapes);

#endif /* SHAPE_H */