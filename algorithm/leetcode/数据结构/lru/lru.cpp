// https://leetcode.cn/problems/lru-cache/description/

#include <unordered_map>
#include <iostream>
using namespace std;

class Node
{
public:
    int key, val;
    Node *next;
    Node *prev;
    Node(int k, int v) : key(k), val(v), next(nullptr), prev(nullptr) {}
};

class DoubleList
{
private:
    // 头尾虚节点
    Node *head;
    Node *tail;
    // 链表元素数
    int size;

public:
    DoubleList() : head(new Node(0, 0)), tail(new Node(0, 0)), size(0)
    {
        // 初始化双向链表的数据
        head->next = tail;
        tail->prev = head;
    }

    ~DoubleList()
    {
        // 析构函数中释放动态分配的内存
        while (head != nullptr)
        {
            Node *temp = head;
            head = head->next;
            delete temp;
            temp = nullptr;
        }
    }

    // 在链表尾部添加节点 x，时间 O(1)
    void addLast(Node *x)
    {
        x->prev = tail->prev;
        x->next = tail;
        tail->prev->next = x;
        tail->prev = x;
        size++;
    }

    // 删除链表中的 x 节点（x 一定存在）
    // 由于是双链表且给的是目标 Node 节点，时间 O(1)
    void remove(Node *x)
    {
        x->prev->next = x->next;
        x->next->prev = x->prev;
        size--;
    }

    // 删除链表中第一个节点，并返回该节点，时间 O(1)
    Node *removeFirst()
    {
        if (head->next == tail)
            return nullptr;
        Node *first = head->next;
        remove(first);
        return first;
    }

    // 返回链表长度，时间 O(1)
    int getSize() { return size; }
};

class LRUCache
{
private:
    /* 哈希表存储节点指针 key -> Node(key, val) */
    unordered_map<int, Node *> *map;
    // Node(k1, v1) <-> Node(k2, v2)...
    DoubleList *cache;
    // 最大容量
    int cap;

public:
    LRUCache(int capacity) : cap(capacity), map(new unordered_map<int, Node *>()), cache(new DoubleList()) {}

    ~LRUCache()
    {
        delete map; // 释放 unordered_map
        map = nullptr;
        delete cache; // 释放 DoubleList
        cache = nullptr;
    }

    int get(int key)
    {
        if (!map->count(key))
        {
            return -1;
        }
        // 将该数据提升为最近使用的
        makeRecently(key);
        return map->at(key)->val;
    }

    void put(int key, int val)
    {
        if (map->count(key))
        {
            // 删除旧的数据
            deleteKey(key);
            // 新插入的数据为最近使用的数据
            addRecently(key, val);
            return;
        }

        if (cap == cache->getSize())
        {
            // 删除最久未使用的元素
            removeLeastRecently();
        }
        // 添加为最近使用的元素
        addRecently(key, val);

        // for (auto iter = map->begin(); iter != map->end(); ++iter)
        // {
        //     cout << "[" << iter->first << ", " << iter->second->val << "] ";
        // }

        // cout << endl;
    }

private:
    /* 将某个 key 提升为最近使用的 */
    void makeRecently(int key)
    {
        Node *x = map->at(key);
        // 先从链表中删除这个节点
        cache->remove(x);
        // 重新插到队尾
        cache->addLast(x);
    }

    /* 添加最近使用的元素 */
    void addRecently(int key, int val)
    {
        Node *x = new Node(key, val);
        // 链表尾部就是最近使用的元素
        cache->addLast(x);
        // 别忘了在 map 中添加 key 的映射
        map->emplace(key, x);
    }

    /* 删除某一个 key */
    void deleteKey(int key)
    {
        Node *x = map->at(key);
        // 从链表中删除
        cache->remove(x);
        // 从 map 中删除
        map->erase(key);
    }

    /* 删除最久未使用的元素 */
    void removeLeastRecently()
    {
        // 链表头部的第一个元素就是最久未使用的
        Node *deletedNode = cache->removeFirst();
        // 同时别忘了从 map 中删除它的 key
        int deletedKey = deletedNode->key;
        map->erase(deletedKey);
    }
};

int main(int argc, char *argv[])
{
    LRUCache *lRUCache = new LRUCache(2);
    lRUCache->put(1, 1);              // 缓存是 {1=1}
    lRUCache->put(2, 2);              // 缓存是 {1=1, 2=2}
    cout << lRUCache->get(1) << endl; // 返回 1
    lRUCache->put(3, 3);              // 该操作会使得关键字 2 作废，缓存是 {1=1, 3=3}
    cout << lRUCache->get(2) << endl; // 返回 -1 (未找到)
    lRUCache->put(4, 4);              // 该操作会使得关键字 1 作废，缓存是 {4=4, 3=3}
    cout << lRUCache->get(1) << endl; // 返回 -1 (未找到)
    cout << lRUCache->get(3) << endl; // 返回 3
    cout << lRUCache->get(4) << endl; // 返回 4
    delete lRUCache;
    return 0;
}