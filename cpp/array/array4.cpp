#include <stdio.h>

int main()
{
    int a[6] = {0, 1, 2, 3, 4, 5};
    int *p = a;
    int len_a = sizeof(a) / sizeof(int);
    int len_p = sizeof(p) / sizeof(int);
    printf("len_a = %d, len_p = %d\n", len_a, len_p);
    return 0;
}