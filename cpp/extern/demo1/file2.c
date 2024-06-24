// file2.c
#include <stdio.h>
#include "shared.h"
#include "shared1.h"

void print_shared_variable_from_file2() {
    printf("Shared variable from file2: %d\n", shared_variable);
    // 修改全局变量的值
    shared_variable = 100;
}
