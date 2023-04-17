#include <iostream>
using namespace std;

class Student
{
public:
    Student(char *name, int age, float score);

public:
    friend void show(Student *pstu); //将show()声明为友元函数
private:
    char *m_name;
    int m_age;
    float m_score;
};

Student::Student(char *name, int age, float score) : m_name(name), m_age(age), m_score(score) {}

//非成员函数
void show(Student *pstu)
{
    cout << pstu->m_name << "的年龄是 " << pstu->m_age << "，成绩是 " << pstu->m_score << endl;
}

int main()
{
    Student stu("小明", 15, 90.6);
    show(&stu); //调用友元函数
    Student *pstu = new Student("李磊", 16, 80.5);
    show(pstu); //调用友元函数

    return 0;
}