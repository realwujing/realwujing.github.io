#include <vector>
#include <algorithm>
#include <iostream>

using namespace std;

vector<int> twoSum(vector<int> &nums, int target)
{
    // 先对数组排序
    sort(nums.begin(), nums.end());
    // 左右指针
    int lo = 0, hi = nums.size() - 1;
    while (lo < hi)
    {
        int sum = nums[lo] + nums[hi];
        // 根据 sum 和 target 的比较，移动左右指针
        if (sum < target)
        {
            lo++;
        }
        else if (sum > target)
        {
            hi--;
        }
        else if (sum == target)
        {
            return {nums[lo], nums[hi]};
        }
    }
    return {};
}

int main()
{
    vector<int> nums = {5, 3, 1, 6};
    int target = 9;
    vector<int> res = twoSum(nums, target);

    for (size_t i = 0; i < res.size(); i++)
    {
        cout << res[i] << endl;
    }
}