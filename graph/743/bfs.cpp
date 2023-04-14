#include <vector>
#include <queue>
#include <climits>

using namespace std;

class Solution {
public:
    int networkDelayTime(vector<vector<int>>& times, int n, int k) {
        // 节点编号是从 1 开始的，所以要一个大小为 n + 1 的邻接表
        vector<vector<pair<int, int>>> graph(n + 1);
        for (int i = 1; i < n; i++) {
            graph[i] = vector<pair<int, int>>();
        }
        // 构造图
        for (auto &edge : times) {
            int from = edge[0];
            int to = edge[1];
            int weight = edge[2];
            // from -> List<(to, weight)>
            // 邻接表存储图结构，同时存储权重信息
            graph[from].emplace_back(to, weight);
        }
        // 启动 dijkstara 算法计算以节点 k 为起点到其他节点的最短路径
        vector<int> disTo = dijkstara(k, graph, n);

        // 找到最长的那一条最短路径
        int res = 0;
        for (int i = 1; i < disTo.size(); i++) {
            if (disTo[i] == INT_MAX) {
                // 有节点不可达，返回-1
                return -1;
            }
            res = max(res, disTo[i]);
        }
        return res;
    }

private:
    // 输入一个起点 start，计算从 start 到其他节点的最短距离
    vector<int> dijkstara(int start, vector<vector<pair<int, int>>>& graph, int n) {
        // 定义：distTo[i] 的值就是起点 start 到达节点 i 的最短路径权重
        vector<int> disTo(n + 1, INT_MAX);
        // base case，start 到 start 的最短距离就是0
        disTo[start] = 0;

        // 优先级队列，distFromStart 较小的排在前面
        priority_queue<pair<int, int>, vector<pair<int, int>>, greater<pair<int, int>>> pq;
        // 从起点 start 开始进行BFS
        pq.emplace(0 ,start);

        while (!pq.empty()) {
            auto [curDistFromStart, curNodeID] = pq.top();
            pq.pop();

            if (curDistFromStart > disTo[curNodeID]) {
                continue;
            }

            // 将 curNode 的相邻节点装入队列
            for (auto& [nextNodeID, weight] : graph[curNodeID]) {
                int disToNextNode = disTo[curNodeID] + weight;
                // 更新 dp table
                if (disTo[nextNodeID] > disToNextNode) {
                    disTo[nextNodeID] = disToNextNode;
                    pq.emplace(disToNextNode, nextNodeID);
                }
            }
        }
        return disTo;
    }
};

int main(int argc, char const *argv[])
{
    vector<vector<int>> times = {{2, 1, 1}, {2, 3, 1}, {3, 4, 1}};
    int n = 4;
    int k = 2;
    Solution solution;
    int res = solution.networkDelayTime(times, n, k);
    return 0;
}