// 注意：cpp 代码由 chatGPT🤖 根据我的 java 代码翻译，旨在帮助不同背景的读者理解算法逻辑。
// 本代码不保证正确性，仅供参考。如有疑惑，可以参照我写的 java 代码对比查看。

#include <stack>
#include <vector>
#include <iostream>

class Solution
{
public:
    std::vector<int> nextGreaterElement(std::vector<int> &nums)
    {
        int n = nums.size();
        // 存放答案的数组
        std::vector<int> res(n);
        std::stack<int> s;
        // 倒着往栈里放
        for (int i = n - 1; i >= 0; i--)
        {
            // 判定个子高矮
            while (!s.empty() && s.top() <= nums[i])
            {
                // 矮个起开，反正也被挡着了。。。
                s.pop();
            }
            // nums[i] 身后的更大元素
            res[i] = s.empty() ? -1 : s.top();
            s.push(nums[i]);
        }
        return res;
    }
};


int main(int argc,char *argv[]){
    std::vector<int> nums = {2,1,2,4,3};
    Solution sol;
    std::vector<int> result = sol.nextGreaterElement(nums);
    for (int num : result) {
        std::cout << num << " ";
    }
    std::cout << std::endl;
    return 0;
}
