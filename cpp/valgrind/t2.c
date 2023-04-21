#include <stdio.h>
#include <stdlib.h>

int main(void)
{
char *p = malloc(1); //分配
*p = 'a';

char c = *p;

printf("\n [%c]\n",c);

free(p); //释放
c = *p; //取值
printf("\n [%c]\n",c);
return 0;
}