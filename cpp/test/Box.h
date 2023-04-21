//
// Created by wujing on 2020/10/25.
//

#ifndef C__PROJECT_BOX_H
#define C__PROJECT_BOX_H


class Box
{
public:
    double length;         // 长度
    double breadth;        // 宽度
    double height;         // 高度

    // 成员函数声明
    double getVolume(void);
    void setLength( double len );
    void setBreadth( double bre );
    void setHeight( double hei );
};


#endif //C__PROJECT_BOX_H
