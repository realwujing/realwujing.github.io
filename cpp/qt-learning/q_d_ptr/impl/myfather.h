// myfather.h
 
#ifndef MYFATHER_H
#define MYFATHER_H
 
#include <QObject>
#include <QString>
#include <QDebug>
 
class MyFatherPrivate;
class MyFather : public QObject
{
    Q_OBJECT
    Q_PROPERTY(QString name READ name WRITE setName NOTIFY nameChanged)
    Q_PROPERTY(int age READ age WRITE setAge NOTIFY ageChanged)
public:
    explicit MyFather(QObject *parent = nullptr);
 
    QString name();
    void setName(QString name);
 
    int age();
    void setAge(int age);
 
    void do_smoke();
    bool canSmoke(){ return smoke_enable;}
    void setSmokeEnable(bool enable){ smoke_enable = enable;}
 
    void do_drink();
    bool canDrink(){ return drink_enable;}
    void setDrinkEnable(bool enable){ drink_enable = enable;}
 
Q_SIGNALS:
    void nameChanged(QString name);
    void ageChanged(int age);
private:
    MyFatherPrivate * const d_ptr;
    Q_DISABLE_COPY_MOVE(MyFather)
    Q_DECLARE_PRIVATE(MyFather)
 
    Q_PRIVATE_SLOT(d_func(), void _q_nameChanged(QString name)) // 把槽函数实现在MyClassPrivate 类中，用做MyClass内部使用的槽函数
    Q_PRIVATE_SLOT(d_func(), void _q_ageChanged(int age))
 
    bool smoke_enable : 1;
    bool drink_enable : 1;
};
 
 
class MyFatherPrivate:public QObject{
    Q_OBJECT
 
public:
    MyFatherPrivate(MyFather *parent):q_ptr(parent){ }
 
public Q_SLOTS:
    void _q_nameChanged(QString name){
        Q_Q(MyFather);
        _name = name;
        qDebug() <<  __FUNCTION__ << "  can drink =" << q->canDrink() ;
    }
 
    void _q_ageChanged(int age){
        Q_Q(MyFather);
        _age = age;
        qDebug() << __FUNCTION__  << "  can smoke:" << q->canSmoke() ;
    }
public:
    MyFather * const q_ptr;
    Q_DECLARE_PUBLIC(MyFather)
 
    QString _name;
    uint _age;
};
 
 
 
#endif // MYFATHER_H