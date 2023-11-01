#include <iostream>
#include <unordered_map>

class LRUCache {
private:
    int cap;
    std::unordered_map<int, std::pair<int, std::list<int>::iterator>> cache;
    std::list<int> lruList;

public:
    LRUCache(int capacity) {
        cap = capacity;
    }

    int get(int key) {
        if (cache.find(key) == cache.end()) {
            return -1;
        }

        // 将 key 移动到最近使用的位置
        lruList.splice(lruList.end(), lruList, cache[key].second);

        return cache[key].first;
    }

    void put(int key, int value) {
        if (cache.find(key) != cache.end()) {
            // 如果 key 已存在，更新值并移动到最近使用的位置
            cache[key].first = value;
            lruList.splice(lruList.end(), lruList, cache[key].second);
        } else {
            if (cache.size() >= cap) {
                // 如果缓存已满，移除最久未使用的键
                int oldestKey = lruList.front();
                cache.erase(oldestKey);
                lruList.pop_front();
            }

            // 添加新键值对并将其移到最近使用
            lruList.push_back(key);
            cache[key] = {value, std::prev(lruList.end())};
        }
    }
};

int main() {
    LRUCache cache(2);
    cache.put(1, 1);
    cache.put(2, 2);
    std::cout << cache.get(1) << std::endl; // 输出: 1
    cache.put(3, 3); // 2 被移除
    std::cout << cache.get(2) << std::endl; // 输出: -1
    cache.put(4, 4); // 1 被移除
    std::cout << cache.get(1) << std::endl; // 输出: -1
    std::cout << cache.get(3) << std::endl; // 输出: 3
    std::cout << cache.get(4) << std::endl; // 输出: 4

    return 0;
}
