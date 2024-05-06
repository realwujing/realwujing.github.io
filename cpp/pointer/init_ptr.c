#include <stdio.h>

struct Data {
    char *string;
    int number;
};

int main() {
    struct Data data;

    // 未显式赋值，string指针的值是不确定的
    if (data.string == NULL) {
        printf("String pointer is NULL\n");
    } else {
        printf("String pointer is not NULL, &data: %px, &(data.string): %px, data.string: %px\n", &data, &(data.string), data.string);
    }

    data.string = NULL;

    printf("&data: %px, &(data.string): %px, data.string: %px\n", &data, &(data.string), data.string);

    return 0;
}
