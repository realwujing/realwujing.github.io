#ifndef CIRCLE_H
#define CIRCLE_H

#include "shape.h" // 基类接口

// 矩形的属性
typedef struct {
    Shape super; // 继承 Shape

    // 自己的属性
    uint16_t radius;
    
} Circle;

// 构造函数
void Circle_ctor(Circle * const me, int16_t x, int16_t y,
                    uint16_t radius);

#endif /* CIRCLE_H */