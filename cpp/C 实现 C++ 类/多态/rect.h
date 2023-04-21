#ifndef RECT_H
#define RECT_H

#include "shape.h" // 基类接口

// 矩形的属性
typedef struct {
    Shape super; // 继承 Shape

    // 自己的属性
    uint16_t width;
    uint16_t height;
} Rectangle;

// 构造函数
void Rectangle_ctor(Rectangle * const me, int16_t x, int16_t y,
                    uint16_t width, uint16_t height);

#endif /* RECT_H */