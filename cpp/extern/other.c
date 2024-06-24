// other.c
#include <stdio.h>
#include "other.h"

// int shared_variable; // 定义未初始化的全局变量会默认初始化为0
int shared_variable = 42; // 定义并初始化外部变量

void print_shared_variable(void) {
    printf("Shared variable: %d\n", shared_variable); // 打印外部变量的值
}
