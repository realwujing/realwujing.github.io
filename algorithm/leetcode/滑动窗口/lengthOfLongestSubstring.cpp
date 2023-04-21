// 这是 LeetCode 第 3 题，Longest Substring Without Repeating Characters，难度 Medium：

#include <iostream>
#include <string>
#include <unordered_map>
#include <vector>
using namespace std;

int lengthOfLongestSubstring(string s)
{
    unordered_map<char, int> window;

    int left = 0, right = 0;
    int res = 0; // 记录结果
    while (right < s.size())
    {
        char c = s[right];
        right++;
        // 进行窗口内数据的一系列更新
        window[c]++;

        // 判断左侧窗口是否要收缩
        while (window[c] > 1)
        {
            char d = s[left];
            left++;
            // 进行窗口内数据的一系列更新
            window[d]--;
        }

        // 在这里更新答案
        res = max(res, right - left);
    }

    return res;
}

int main()
{
    cout << lengthOfLongestSubstring("abcabcbb") << endl;
}