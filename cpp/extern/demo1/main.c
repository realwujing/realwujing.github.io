// main.c
#include <stdio.h>
#include "shared1.h"

int main() {
    // 打印变量的初始值
    print_shared_variable_from_file1();
    print_shared_variable_from_file2();
    print_shared_variable_from_file3();

    // 再次打印变量的值
    print_shared_variable_from_file1();
    print_shared_variable_from_file2();
    print_shared_variable_from_file3();

    return 0;
}
