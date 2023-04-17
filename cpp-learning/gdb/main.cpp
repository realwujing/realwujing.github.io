/*** 
 * @Author: wujing
 * @Date: 2021-01-19 13:52:45
 * @LastEditTime: 2021-01-19 18:24:44
 * @LastEditors: wujing
 * @Description: 
 * @FilePath: /wujing/CPlusPlusProject/gdb/main.cpp
 * @可以输入预定的版权声明、个性签名、空行等
 */

#include <stdio.h>
#include <iostream>

int main ()
{
    unsigned long long int n, sum;
    n = 1;
    sum = 0;
    while (n <= 100)
    {
        sum = sum + n;
        n = n + 1;
    }
    
    std:: cout << "sum: " << sum << std::endl;
    
    return 0;
}