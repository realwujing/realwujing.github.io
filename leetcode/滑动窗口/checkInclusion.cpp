// LeetCode 567 题，Permutation in String，难度 Medium：

#include <iostream>
#include <string>
#include <unordered_map>
#include <climits>
#include <fstream>
using namespace std;

// 判断 s 中是否存在 t 的排列
bool checkInclusion(string t, string s)
{
    unordered_map<char, int> need, window;
    for (char c : t)
        need[c]++;

    int left = 0, right = 0;
    int valid = 0;

    while (right < s.size())
    {
        char c = s[right];
        right++;

        // 进行窗口数据的一系列更新
        if (need.count(c))
        {
            window[c]++;
            if (window[c] == need[c])
                valid++;
        }

        // 判断左侧窗口是否要收缩
        while (right - left >= t.size())
        {
            // 在这里判断是否找到了合法的子串
            if (valid == need.size())
                return true;

            char d = s[left];
            left++;

            // 进行窗口内数据的一系列更新
            if (need.count(d))
            {
                if (window[d] == need[d])
                    valid--;
                window[d]--;
            }
        }
    }

    return false;
}

int main()
{
    cout << checkInclusion("ab", "eidbaooo") << endl;
}