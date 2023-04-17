#include <stdio.h>
#include <stdlib.h>
int main(void)
{
char *p;
p=(char *)malloc(100);
if(p)
printf("Memory Allocated at: %s/n",p);
else
printf("Not Enough Memory!/n");
free(p); //重复释放
free(p);
free(p);
return 0;
}