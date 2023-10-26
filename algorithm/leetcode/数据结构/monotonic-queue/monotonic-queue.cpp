#include <deque>
#include <vector>
#include <iostream>

class Solution
{
public:
    std::vector<int> maxSlidingWindow(std::vector<int> &nums, int k)
    {
        int n = nums.size();
        std::deque<int> q;
        for (int i = 0; i < k; ++i)
        {
            while (!q.empty() && nums[i] >= nums[q.back()])
            {
                q.pop_back();
            }
            q.push_back(i);
        }

        std::vector<int> ans = {nums[q.front()]};
        for (int i = k; i < n; ++i)
        {
            while (!q.empty() && nums[i] >= nums[q.back()])
            {
                q.pop_back();
            }
            q.push_back(i);
            while (q.front() <= i - k)
            {
                q.pop_front();
            }
            ans.push_back(nums[q.front()]);
        }
        return ans;
    }
};

int main(int argc, char *argv[])
{
    std::vector<int> nums = {1, 3, -1, -3, 5, 3, 6, 7};
    int k = 3;
    Solution sol;
    std::vector<int> result = sol.maxSlidingWindow(nums, 3);
    for (int num : result)
    {
        std::cout << num << " ";
    }
    std::cout << std::endl;
    return 0;
}