// myfather.cpp
 
 
#include "myfather.h"
 
MyFather::MyFather(QObject *parent): QObject(parent),d_ptr(new MyFatherPrivate(this))
{
    connect(this,SIGNAL(nameChanged(QString)),SLOT(_q_nameChanged(QString)));
    connect(this,SIGNAL(ageChanged(int)),SLOT(_q_ageChanged(int)));
}
 
 
 
QString MyFather::name(){
    Q_D(MyFather);
    return d->_name;
}
 
void MyFather::setName(QString name){
    emit nameChanged(name);
}
 
int MyFather::age(){
    Q_D(MyFather);
    return d->_age;
}
void MyFather::setAge(int age){
    emit ageChanged(age);
}
 
void MyFather::do_smoke(){
    if(false == smoke_enable) return;
    qDebug() << "father can smoke.";
}
 
void MyFather::do_drink(){
    if(false == drink_enable) return;
    qDebug() << "father can drink.";
 
}