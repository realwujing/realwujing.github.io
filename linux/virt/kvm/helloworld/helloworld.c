#include <stdio.h>

void main()
{
    printf("Hello World!\n");
    fflush(stdout);
    /* 让程序打印完后继续维持在用户态 */
    while (1);
}