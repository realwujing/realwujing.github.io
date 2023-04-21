#include <iostream>
#include <vector>
using namespace std;

int coinChange(vector<int> &coins, int amount);

int coinChange(vector<int> &coins, int amount)
{
    // 数组大小为 amount + 1，初始值也为 amount + 1
    vector<int> dp(amount + 1, amount + 1);
    // base case
    dp[0] = 0;
    // 外层 for 循环在遍历所有状态的所有取值
    for (int i = 0; i < dp.size(); i++)
    {
        // 内层 for 循环在求所有选择的最小值
        for (int coin : coins)
        {
            // 子问题无解，跳过
            if (i - coin < 0)
                continue;
            dp[i] = min(dp[i], 1 + dp[i - coin]);
        }
    }
    return (dp[amount] == amount + 1) ? -1 : dp[amount];
}

int main()
{
    vector<int> coins = {1, 2, 5};
    cout << coinChange(coins, 11) << endl;
    return 0;
}