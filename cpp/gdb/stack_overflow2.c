#include <stdio.h>
#include <unistd.h>

#define TEST_DATA_LEN 1000

void test() {
    long stack_array[TEST_DATA_LEN]; // 消耗栈空间
    for (int i = 0; i < TEST_DATA_LEN; i++)
        stack_array[i] = i; // 防止编译器优化
    sleep(1);
}

int main() {
    test();
    return 0;
}