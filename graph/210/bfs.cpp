#include <vector>
#include <queue>
using namespace std;

class Solution {
public:
    vector<int> findOrder(int numCourses, vector<vector<int>>& prerequisites) {
        // 建图，和环检测算法相同
        vector<vector<int>> graph(numCourses);
        for (auto edge : prerequisites) {
            int from = edge[1], to = edge[0];
            graph[from].push_back(to);
        }
        // 计算入度，和环检测算法相同
        vector<int> indegree(numCourses, 0);
        for (auto edge : prerequisites) {
            int from = edge[1], to = edge[0];
            indegree[to]++;
        }

        // 根据入度初始化队列中的节点，和环检测算法相同
        queue<int> q;
        for (int i = 0; i < numCourses; i++) {
            if (indegree[i] == 0) {
                q.push(i);
            }
        }

        // 记录拓扑排序结果
        vector<int> res;
        // 开始执行 BFS 算法
        while (!q.empty()) {
            int cur = q.front();
            q.pop();
            // 弹出节点的顺序即为拓扑排序结果
            res.push_back(cur);
            for (int next : graph[cur]) {
                indegree[next]--;
                if (indegree[next] == 0) {
                    q.push(next);
                }
            }
        }

        if (res.size() != numCourses) {
            // 存在环，拓扑排序不存在
            return vector<int>{};
        }
        
        return res;
    }

    // 建图函数
    vector<vector<int>> buildGraph(int n, vector<vector<int>>& edges) {
        // 图中共有 numCourses 个节点
        vector<vector<int>> graph(n);
        for (auto edge : edges) {
            int from = edge[1];
            int to = edge[0];
            // 修完课程 from 才能修课程 to
            // 在图中添加一条从 from 指向 to 的有向边
            graph[from].push_back(to);
        }
        return graph;
    }
};

int main(int argc, char const *argv[])
{
    int numCourses = 4;
    vector<vector<int>> prerequisites = {{1, 0}, {2, 1}, {2, 3}};
    Solution solution;
    vector<int> res = solution.findOrder(numCourses, prerequisites);
    return 0;
}
