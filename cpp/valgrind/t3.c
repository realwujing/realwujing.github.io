#include <stdio.h>
#include <stdlib.h>

int main(void)
{
char *p = malloc(1); //分配1字节
*p = 'a';
char c = *(p+1); //地址加1
printf("\n [%c]\n",c);
free(p);
return 0;
}