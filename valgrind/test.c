#include<stdlib.h>
void k(void)
{
int *x = malloc(8 * sizeof(int));
x[9] = 0; //数组下标越界
} //内存未释放

int main(void)
{
k();
return 0;
}