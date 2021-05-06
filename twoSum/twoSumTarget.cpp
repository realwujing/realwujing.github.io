#include <vector>
#include <algorithm>
#include <iostream>

using namespace std;

vector<vector<int>> twoSumTarget(vector<int>& nums, int target) {
    // 先对数组排序
    sort(nums.begin(), nums.end());
    vector<vector<int>> res;
    int lo = 0, hi = nums.size() - 1;
    while (lo < hi)
    {
        int sum = nums[lo] + nums[hi];
        // 根据 sum 和 target 的比较，移动左右指针
        if (sum < target)
            lo++;
        else if (sum > target)
            hi--;
        else
        {
            res.push_back({lo, hi});
            lo++;
            hi--;
        }
    }
    return res;
}

vector<vector<int>> twoSumTarget2(vector<int> &nums, int target)
{
    // 先对数组排序
    sort(nums.begin(), nums.end());
    vector<vector<int>> res;
    int lo = 0, hi = nums.size() - 1;
    while (lo < hi)
    {
        int sum = nums[lo] + nums[hi];
        // 记录索引 lo 和 hi 最初对应的值
        int left = nums[lo], right = nums[hi];
        if (sum < target)
            lo++;
        else if (sum > target)
            hi--;
        else
        {
            res.push_back({left, right});
            // 跳过所有重复的元素
            while (lo < hi && nums[lo] == left)
                lo++;
            while (lo < hi && nums[hi] == right)
                hi--;
        }
    }
    return res;
}

vector<vector<int>> twoSumTarget3(vector<int> &nums, int target)
{
    // nums 数组必须有序
    sort(nums.begin(), nums.end());
    int lo = 0, hi = nums.size() - 1;
    vector<vector<int>> res;
    while (lo < hi)
    {
        int sum = nums[lo] + nums[hi];
        int left = nums[lo], right = nums[hi];
        if (sum < target)
        {
            while (lo < hi && nums[lo] == left)
                lo++;
        }
        else if (sum > target)
        {
            while (lo < hi && nums[hi] == right)
                hi--;
        }
        else
        {
            res.push_back({left, right});
            while (lo < hi && nums[lo] == left)
                lo++;
            while (lo < hi && nums[hi] == right)
                hi--;
        }
    }
    return res;
}