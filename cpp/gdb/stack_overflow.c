#include <stdio.h>

void test() {
    int stack_array[100000000]; // 消耗栈空间
    stack_array[0] = 0; // 防止编译器优化
}

int main() {
    test();
    return 0;
}