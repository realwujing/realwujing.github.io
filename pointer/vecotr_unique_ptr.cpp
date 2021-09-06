#include<iostream>
#include<vector>
#include <memory>
using namespace std;

/*
int main()
{
    vector<unique_ptr<int>> vec;
    vec.push_back(1);//错误
    return 0;
}

*/

/*

int main()
{
    vector<unique_ptr<int>> vec;
    unique_ptr<int> sp(new int(126));
    vec.push_back(sp);//尝试引用已删除的函数
    return 0;
}

*/

int main()
{
    vector<unique_ptr<int>> vec;
    unique_ptr<int> sp(new int(126));

    //vec.push_back(1);

    vec.push_back(std::move(sp));//尝试引用已删除的函数
    cout << *vec[0]<< endl;  // 输出126
    //cout << *sp << endl;
    return 0;
}