//
// Created by wujing on 2020/11/15.
//

#include "Test1.h"

#include <iostream>

char g_str[] = "123456"; // 定义全局变量g_str
void fun1() {
    std::cout << g_str << std::endl;
}
