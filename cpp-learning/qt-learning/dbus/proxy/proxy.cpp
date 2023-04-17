#include <QCoreApplication>
#include <QDBusMessage>
#include <QDBusConnection>
#include <QDBusReply>
#include <QDBusInterface>
#include <QDebug>
#include "valueInterface.h"

int main(int argc, char *argv[])
{
    QCoreApplication a(argc, argv);
    // 初始化自动生成的Proxy类com::scorpio::test::value
    com::scorpio::test::value test("com.scorpio.test",
                                   "/test/objects",
                                   QDBusConnection::sessionBus());
    // 调用value方法
    QDBusPendingReply<int> reply = test.value();
    //qdbusxml2cpp生成的Proxy类是采用异步的方式来传递Message，
    //所以需要调用waitForFinished来等到Message执行完成
    reply.waitForFinished();
    if (reply.isValid())
    {
        int value = reply.value();
        qDebug() << QString("value =  %1").arg(value);
    }
    else
    {
        qDebug() << "value method called failed!";
    }

    return a.exec();
}