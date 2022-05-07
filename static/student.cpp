/*** 
 * @Author: wujing
 * @Date: 2021-04-23 19:52:49
 * @LastEditTime: 2021-04-23 20:01:18
 * @LastEditors: wujing
 * @Description: 
 * @FilePath: /LeetCode/student.cpp
 * @可以输入预定的版权声明、个性签名、空行等
 */

#include <iostream>
using namespace std;

class Student
{
public:
    Student(char *name, int age, float score);
    void show();

public: //声明静态成员函数
    static int getTotal();
    static float getPoints();

private:
    static int m_total;    //总人数
    static float m_points; //总成绩
private:
    char *m_name;
    int m_age;
    float m_score;
};

int Student::m_total = 0;
float Student::m_points = 0.0;

Student::Student(char *name, int age, float score) : m_name(name), m_age(age), m_score(score)
{
    m_total++;
    m_points += score;
}
void Student::show()
{
    cout << m_name << "的年龄是" << m_age << "，成绩是" << m_score << endl;
    cout << "getTotal:" << getTotal() << " m_total:" << m_total << endl;
}
//定义静态成员函数
int Student::getTotal()
{
    // show();
    return m_total;
}
float Student::getPoints()
{
    return m_points;
}

int main()
{
    (new Student("小明", 15, 90.6))->show();
    (new Student("李磊", 16, 80.5))->show();
    (new Student("张华", 16, 99.0))->show();
    (new Student("王康", 14, 60.8))->show();
    29
    int total = Student::getTotal();
    float points = Student::getPoints();
    cout << "当前共有" << total << "名学生，总成绩是" << points << "，平均分是" << points / total << endl;

    return 0;
}
