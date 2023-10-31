// https://leetcode.cn/problems/lfu-cache/description/

#include <unordered_map>
#include <list>
#include <iostream>
using namespace std;

class LFUCache {
private:
    // key 到 val 的映射，我们后文称为 KV 表
    unordered_map<int, int> keyToVal;
    // key 到 freq 的映射，我们后文称为 KF 表
    unordered_map<int, int> keyToFreq;
    // freq 到 key 列表的映射，我们后文称为 FK 表
    unordered_map<int, list<int>> freqToKeys;
    // 记录最小的频次
    int minFreq;
    // 记录 LFU 缓存的最大容量
    int cap;

public:
    LFUCache(int capacity) : cap(capacity), minFreq(0) {}

    int get(int key) {
        if (!keyToVal.count(key)) {
            return -1;
        }
        // 增加 key 对应的 freq
        increaseFreq(key);
        return keyToVal[key];
    }

    void put(int key, int val) {
        if (cap <= 0) return;

        /* 若 key 已存在，修改对应的 val 即可 */
        if (keyToVal.count(key)) {
            keyToVal[key] = val;
            // key 对应的 freq 加一
            increaseFreq(key);
            return;
        }

        /* key 不存在，需要插入 */
        /* 容量已满的话需要淘汰一个 freq 最小的 key */
        if (cap <= keyToVal.size()) {
            removeMinFreqKey();
        }

        /* 插入 key 和 val，对应的 freq 为 1 */
        // 插入 KV 表
        keyToVal[key] = val;
        // 插入 KF 表
        keyToFreq[key] = 1;
        // 插入 FK 表
        freqToKeys[1].push_back(key);
        // 插入新 key 后最小的 freq 肯定是 1
        minFreq = 1;
    }

private:
    void increaseFreq(int key) {
        int freq = keyToFreq[key];
        /* 更新 KF 表 */
        keyToFreq[key] = freq + 1;
        /* 更新 FK 表 */
        // 将 key 从 freq 对应的列表中删除
        freqToKeys[freq].remove(key);
        // 将 key 加入 freq + 1 对应的列表中
        freqToKeys[freq + 1].push_back(key);
        // 如果 freq 对应的列表空了，移除这个 freq
        if (freqToKeys[freq].empty()) {
            freqToKeys.erase(freq);
            // 如果这个 freq 恰好是 minFreq，更新 minFreq
            if (freq == minFreq) {
                minFreq++;
            }
        }
    }

    void removeMinFreqKey() {
        // 其中最先被插入的那个 key 就是该被淘汰的 key
        int deletedKey = freqToKeys[minFreq].front();
        /* 更新 FK 表 */
        freqToKeys[minFreq].pop_front();
        if (freqToKeys[minFreq].empty()) {
            freqToKeys.erase(minFreq);
            // 问：这里需要更新 minFreq 的值吗？
        }
        /* 更新 KV 表 */
        keyToVal.erase(deletedKey);
        /* 更新 KF 表 */
        keyToFreq.erase(deletedKey);
    }
};

int main(int argc, char *argv[])
{
    // LFUCache lfu(2);
    // lfu.put(1, 1);   // cache=[1,_], cnt(1)=1
    // lfu.put(2, 2);   // cache=[2,1], cnt(2)=1, cnt(1)=1
    // cout << lfu.get(1) << endl;      // 返回 1
    //                 // cache=[1,2], cnt(2)=1, cnt(1)=2
    // lfu.put(3, 3);   // 去除键 2 ，因为 cnt(2)=1 ，使用计数最小
    //                 // cache=[3,1], cnt(3)=1, cnt(1)=2
    // cout << lfu.get(2) << endl;      // 返回 -1（未找到）
    // cout << lfu.get(3) << endl;      // 返回 3
    //                 // cache=[3,1], cnt(3)=2, cnt(1)=2
    // lfu.put(4, 4);   // 去除键 1 ，1 和 3 的 cnt 相同，但 1 最久未使用
    //                 // cache=[4,3], cnt(4)=1, cnt(3)=2
    // cout << lfu.get(1) << endl;      // 返回 -1（未找到）
    // cout << lfu.get(3) << endl;      // 返回 3
    //                 // cache=[3,4], cnt(4)=1, cnt(3)=3
    // cout << lfu.get(4) << endl;      // 返回 4
    //                 // cache=[3,4], cnt(4)=2, cnt(3)=3
    
    // ["LFUCache","put","put","get","get","put","get","get","get"]
    // [[2],[2,1],[3,2],[3],[2],[4,3],[2],[3],[4]]
    LFUCache lfu(2);
    lfu.put(2, 1);
    lfu.put(3, 2);
    cout << lfu.get(3) << endl;
    cout << lfu.get(2) << endl;
    lfu.put(4, 3);
    cout << lfu.get(2) << endl;
    cout << lfu.get(3) << endl;
    cout << lfu.get(4) << endl;
    return 0;
}