#include <iostream>
#include<string>
#include<unordered_map>
#include <climits>
using namespace std;

string minWindow(string s, string t)
{
    cout << "s:" << s << " s.size:" << s.size() << " t:" << t << " t.size:" << t.size() << endl;

    unordered_map<char, int> need, window;
    for (char c : t)
    {
        need[c]++;
        cout << "c:" << c << " need[c]:" << need[c] << endl;
    }
        
    int left = 0, right = 0;
    int valid = 0;

    // 记录最小覆盖子串的起始索引及长度
    int start = 0, len = INT_MAX;
    while (right < s.size())
    {
        // c 是将移入窗口的字符
        char c = s[right];
        cout << "27 c:" << c << " right:" << right << " s[right]:" << s[right] << endl;
        // 右移窗口
        right++;
        // 进行窗口内数据的一系列更新
        cout << "31 need.count(c):" << need.count(c) << endl;
        if (need.count(c))
        {
            window[c]++;
            cout << "35 valid:" << valid << " window[c]:" << window[c] << " need[c]:" << need[c] << " window[c] == need[c]:" << (window[c] == need[c]) << endl;
            if (window[c] == need[c])
                valid++;
            cout << "38 valid:" << valid << endl;
        }

        // 判断左侧窗口是否要收缩
        cout << "42 valid:" << valid << " need.size():" << need.size() << " valid == need.size():" << (valid == need.size()) << " " << string(10, '*') << endl;
        while (valid == need.size())
        {
            cout << "right - left < len:" << (right - left < len) << " right - left:" << right - left << " len:" << len << " right:" << right << " left:" << left << " " << string(20, '*') << endl;
            // 在这里更新最小覆盖子串
            if (right - left < len)
            {
                start = left;
                len = right - left;
                cout << "51 start:" << start << " len:" << len << " right:" << right << " left:" << left << " s.substr(start, len):" << s.substr(start, len) << " " << string(30, '*') << endl;
            }

            // d 是将移出窗口的字符
            char d = s[left];
            cout << "56 d:" << c << " left:" << left << " s[left]:" << s[left] << endl;
            // 左移窗口
            left++;
            cout << "59 need.count(d):" << need.count(d) << endl;
            if(need.count(d))
            {
                cout << "62 window[d]:" << window[d] << " need[d]:" << need[d] << endl;
                if(window[d] == need[d])
                    valid--;
                window[d]--;
                cout << "66 valid:" << valid << endl;
            }
        }
    }
    // 返回最小覆盖子串
    return len == INT_MAX ? "" : s.substr(start, len);
}

int main()
{
    cout << minWindow("ADOBECODEBANC", "ABC") << endl;
}
