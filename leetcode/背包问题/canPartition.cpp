#include <vector>
#include "Log.h"

using namespace std;

bool canPartition(vector<int> &nums)
{
    int sum = 0;
    for (int num : nums)
        sum += num;
    // 和为奇数时，不可能划分成两个和相等的集合
    if (sum % 2 != 0)
        return false;
    int n = nums.size();
    sum = sum / 2;
    vector<vector<bool>> dp(n + 1, vector<bool>(sum + 1, false));

    // base case
    for (int i = 0; i <= n; i++)
        dp[i][0] = true;

    for (int i = 1; i <= n; i++)
    {
        for (int j = 1; j <= sum; j++)
        {
            FLOG("canPartition") << dp[i][j] << endl;

            if (j - nums[i - 1] < 0)
            {
                // 背包容量不足，不能装入第 i 个物品
                dp[i][j] = dp[i - 1][j];
                FLOG("canPartition") << dp[i][j] << endl;
            }
            else
            {
                // 装入或不装入背包
                dp[i][j] = dp[i - 1][j] || dp[i - 1][j - nums[i - 1]];
                FLOG("canPartition") << dp[i][j] << endl;
            }
        }
    }
    return dp[n][sum];
}

bool canPartition2(vector<int> &nums)
{
    int sum = 0, n = nums.size();
    for (int num : nums)
        sum += num;
    if (sum % 2 != 0)
        return false;
    sum = sum / 2;
    vector<bool> dp(sum + 1, false);
    // base case
    dp[0] = true;

    for (int i = 0; i < n; i++)
        for (int j = sum; j >= 0; j--)
            if (j - nums[i] >= 0)
                dp[j] = dp[j] || dp[j - nums[i]];

    return dp[sum];
}

int main()
{
    vector<int> nums = {1, 5, 11, 5};
    bool res = canPartition(nums);
    bool res2 = canPartition2(nums);

    FLOG("canPartition") << res << endl;

    FLOG("canPartition2") << res2 << endl;
}