#include <vector>
#include <string>
#include <iostream>
using namespace std;

vector<vector<string>> res;
void backtrack(vector<string> &board, int row);
bool isValid(vector<string> &board, int row, int col);
vector<vector<string>> solveNQueens(int n);

/* 输入棋盘边长 n，返回所有合法的放置 */
vector<vector<string>> solveNQueens(int n)
{
    // '.' 表示空，'Q'表示皇后，初始化空棋盘。
    vector<string> board(n, string(n, '.'));
    backtrack(board, 0);
    return res;
}

// 路径：board 中小于 row 的那些行都已经成功放置了皇后
// 选择列表：第 row 行的所有列都是放置皇后的选择
// 结束条件：row 超过 board 的最后一行

void backtrack(vector<string> &board, int row) 
{
    // 触发条件
    if (row == board.size())
    {
        res.push_back(board);
        return;
    }

    int n = board[row].size();
    for (int col = 0; col < n; col++)
    {
        // 排除不合法选择
        if (!isValid(board, row, col))
            continue;

        // 做选择
        board[row][col] = 'Q';
        backtrack(board, row + 1);

        // 撤销选择
        board[row][col] = '.';
    }
}

bool isValid(vector<string> &board, int row, int col)
{
    int n = board.size();

    // 检查列是否有皇后相互冲突
    for (int i = 0; i < n; i++)
    {
        if (board[i][col] == 'Q')
            return false;
    }

    // 检查右是否有皇后相互冲突
    for (int i = row - 1, j = col + 1; i >=0 && j < n; i--, j++)
    {
        if (board[i][j] == 'Q')
            return false;
    }

    // 检查左上方是否有皇后互相冲突
    for (int i = row - 1, j = col - 1;
         i >= 0 && j >= 0; i--, j--)
    {
        if (board[i][j] == 'Q')
            return false;
    }
    return true;
}

int main()
{
    int n;
    cout << "请输入皇后数：";
    cin >> n;
    solveNQueens(n);
    for (size_t i = 0; i < res.size(); i++)
    {
        cout << i << ' ' << string(50, '*') << endl;

        for (size_t j = 0; j < res[i].size(); j++)
        {
            cout << res[i][j] << endl;
        }
    }

    return 0;
}