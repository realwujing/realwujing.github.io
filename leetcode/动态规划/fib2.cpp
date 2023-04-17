/*** 
 * @Author: wujing
 * @Date: 2021-04-09 21:28:38
 * @LastEditTime: 2021-04-09 21:30:42
 * @LastEditors: wujing
 * @Description: 
 * @FilePath: /LeetCode/动态规划/fib2.cpp
 * @可以输入预定的版权声明、个性签名、空行等
 */
#include <iostream>
#include <vector>
using namespace std;

int helper(vector<int> &memo, int n);
int fib(int N);

int fib(int N)
{
    if (N < 1)
        return 0;
    // 备忘录全初始化为 0
    vector<int> memo(N + 1, 0);
    // 进行带备忘录的递归
    return helper(memo, N);
}

int helper(vector<int> &memo, int n)
{
    // base case
    if (n == 1 || n == 2)
        return 1;
    // 已经计算过
    if (memo[n] != 0)
        return memo[n];
    memo[n] = helper(memo, n - 1) + helper(memo, n - 2);
    return memo[n];
}

int main()
{
    cout << fib(20) << endl;
    return 0;
}