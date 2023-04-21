#include <stdio.h>
#include <stdlib.h>
int main(int argc,char *argv[])
{
int i;
char* p = (char*)malloc(10);
char* pt=p;
for(i = 0;i < 10;i++)
{
p[i] = 'z';
}
free(p);
pt[1] = 'x';
free(pt);
return 0;
}