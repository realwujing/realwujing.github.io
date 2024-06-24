// main.c
#include <stdio.h>
#include "other.h"

int main(void) {
    printf("Initial shared variable: %d\n", shared_variable); // 打印初始值
    shared_variable = 100; // 修改外部变量的值
    print_shared_variable(); // 调用外部函数，打印修改后的值

    return 0;
}
