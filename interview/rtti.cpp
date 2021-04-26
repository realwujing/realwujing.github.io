#include <iostream>
using namespace std;

class Flyable // 能飞的
{
public:
    virtual void takeoff() = 0; // 起飞
    virtual void land() = 0;    // 降落
};
class Bird : public Flyable // 鸟
{
public:
    void foraging() {  } // 觅食
    virtual void takeoff() {  }
    virtual void land() {  }
    virtual ~Bird() {}
};
class Plane : public Flyable // 飞机
{
public:
    void carry() {  } // 运输
    virtual void takeoff() {  }
    virtual void land() {  }
};

class type_info
{
public:
    const char *name() const;
    bool operator==(const type_info &rhs) const;
    bool operator!=(const type_info &rhs) const;
    int before(const type_info &rhs) const;
    virtual ~type_info();

private:
    
};

void doSomething(Flyable *obj) // 做些事情
{
    obj->takeoff();

    cout << typeid(*obj).name() << endl; // 输出传入对象类型（"class Bird" or "class Plane"）

    if (typeid(*obj) == typeid(Bird)) // 判断对象类型
    {
        Bird *bird = dynamic_cast<Bird *>(obj); // 对象转化
        bird->foraging();
    }

    obj->land();
}

int main()
{
    Bird *b = new Bird();
    doSomething(b);
    delete b;
    b = nullptr;
    return 0;
}