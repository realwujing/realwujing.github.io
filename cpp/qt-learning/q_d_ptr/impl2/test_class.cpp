#include "test_class.h"
#include "test_class_p.h"

#include <QtDebug>

TestClassPrivate::TestClassPrivate(TestClass *q) : q_ptr(q), m_ptr("nihao")
{
}

void TestClassPrivate::testFunc()
{
    Q_Q(TestClass);
    q->showMsg("hello!");
}


TestClass::TestClass(QObject *parent) : d_ptr(new TestClassPrivate(this))
{
}

TestClass::~TestClass()
{
    Q_D(TestClass);
    delete d;
}

void TestClass::doFunc()
{
    Q_D(TestClass);
    d->testFunc();
}

void TestClass::showMsg(QString str)
{
    qInfo() << str;
}
