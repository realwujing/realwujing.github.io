#pragma once

#include <QObject>
 
class TestClassPrivate;
class TestClass
{
public:
    explicit TestClass(QObject *parent);
    ~TestClass();
    
    void doFunc();
    
private:
    void showMsg(QString str);
    
private:
    TestClassPrivate * const d_ptr;
    Q_DECLARE_PRIVATE(TestClass);
    Q_DISABLE_COPY(TestClass);
};