#include <stdio.h>
#include <stdlib.h>

int main(void)
{
int *p = malloc(1);
*p = 'x';
char c = *p;
printf("%c\n",c); //申请后未释放
return 0;
}