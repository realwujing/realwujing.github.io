// 注意：cpp 代码由 chatGPT🤖 根据我的 java 代码翻译，旨在帮助不同背景的读者理解算法逻辑。
// 本代码不保证正确性，仅供参考。如有疑惑，可以参照我写的 java 代码对比查看。
// https://leetcode.cn/problems/design-twitter/description/

#include <unordered_set>
#include <unordered_map>
#include <vector>
#include <unordered_set>
#include <queue>
#include <iostream>

using namespace std;

class Twitter
{
private:
    // Tweet 类
    class Tweet
    {
    private:
        int id;
        // 时间戳用于对信息流按照时间排序
        int timestamp;
        // 指向下一条 tweet，类似单链表结构
        Tweet *next;

    public:
        Tweet(int id, int &globalTime)
        {
            this->id = id;
            // 新建一条 tweet 时记录并更新时间戳
            this->timestamp = globalTime++;
        }

        int getId()
        {
            return id;
        }

        int getTimestamp()
        {
            return timestamp;
        }

        Tweet *getNext()
        {
            return next;
        }

        void setNext(Tweet *next)
        {
            this->next = next;
        }
    };

    // 用户类
    class User
    {
    private:
        // 记录该用户的 id 以及发布的 tweet
        int id;
        Tweet *tweetHead;
        // 记录该用户的关注者
        unordered_set<User *> followedUserSet;

    public:
        User(int id)
        {
            this->id = id;
            this->tweetHead = nullptr;
            this->followedUserSet = unordered_set<User *>();
        }

        int getId()
        {
            return id;
        }

        Tweet *getTweetHead()
        {
            return tweetHead;
        }

        unordered_set<User *> getFollowedUserSet()
        {
            return followedUserSet;
        }

        bool equals(User *other)
        {
            return this->id == other->id;
        }

        // 关注其他人
        void follow(User *other)
        {
            followedUserSet.insert(other);
        }

        // 取关其他人
        void unfollow(User *other)
        {
            followedUserSet.erase(other);
        }

        // 发布一条 tweet
        void post(Tweet *tweet)
        {
            // 把新发布的 tweet 作为链表头节点
            tweet->setNext(tweetHead);
            tweetHead = tweet;
        }
    };

    // 全局时间戳
    int globalTime = 0;
    // 记录用户 ID 到用户示例的映射
    unordered_map<int, User *> idToUser;

public:
    void postTweet(int userId, int tweetId)
    {
        // 如果这个用户还不存在，新建用户
        if (idToUser.find(userId) == idToUser.end())
        {
            idToUser[userId] = new User(userId);
        }
        User *user = idToUser[userId];
        user->post(new Tweet(tweetId, globalTime));
    }

    vector<int> getNewsFeed(int userId)
    {
        vector<int> res = vector<int>();
        if (idToUser.find(userId) == idToUser.end())
        {
            return res;
        }
        // 获取该用户关注的用户列表
        User *user = idToUser[userId];
        unordered_set<User *> followedUserSet = user->getFollowedUserSet();
        // 每个用户的 tweet 是一条按时间排序的链表
        // 现在执行合并多条有序链表的逻辑，找出时间线中的最近 10 条动态
        auto cmp = [](Tweet *a, Tweet *b) -> bool
        {
            // 按照每条 tweet 的发布时间降序排序（最近发布的排在事件流前面）
            return b->getTimestamp() > a->getTimestamp();
        };
        priority_queue<Tweet *, vector<Tweet *>, decltype(cmp)> pq(cmp);
        // 该用户自己的 tweet 也在时间线内
        if (user->getTweetHead() != nullptr)
        {
            pq.push(user->getTweetHead());
        }
        for (User *other : followedUserSet)
        {
            if (other->getTweetHead() != nullptr)
            {
                pq.push(other->getTweetHead());
            }
        }
        // 合并多条有序链表
        int count = 0;
        while (!pq.empty() && count < 10)
        {
            Tweet *tweet = pq.top();
            pq.pop();
            res.push_back(tweet->getId());
            if (tweet->getNext() != nullptr)
            {
                pq.push(tweet->getNext());
            }
            count++;
        }
        return res;
    }

    void follow(int followerId, int followeeId)
    {
        // 如果用户还不存在，则新建用户
        if (idToUser.find(followerId) == idToUser.end())
        {
            idToUser[followerId] = new User(followerId);
        }
        if (idToUser.find(followeeId) == idToUser.end())
        {
            idToUser[followeeId] = new User(followeeId);
        }

        User *follower = idToUser[followerId];
        User *followee = idToUser[followeeId];
        // 关注者关注被关注者
        follower->follow(followee);
    }

    void unfollow(int followerId, int followeeId)
    {
        if (idToUser.find(followerId) == idToUser.end() || idToUser.find(followeeId) == idToUser.end())
        {
            return;
        }
        User *follower = idToUser[followerId];
        User *followee = idToUser[followeeId];
        // 关注者取关被关注者
        follower->unfollow(followee);
    }
};

int main(int argc, char *argv[])
{
    Twitter twitter;

    return 0;
}