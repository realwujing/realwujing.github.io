// file1.c
#include <stdio.h>
#include "shared.h"
#include "shared1.h"

// 定义全局变量
int shared_variable = 42;

void print_shared_variable_from_file1() {
    printf("Shared variable from file1: %d\n", shared_variable);
}
