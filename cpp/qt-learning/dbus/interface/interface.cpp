#include <QCoreApplication>
#include <QDBusMessage>
#include <QDBusConnection>
#include <QDBusReply>
#include <QDBusInterface>
#include <QDebug>

int main(int argc, char *argv[])
{
    QCoreApplication a(argc, argv);
    // 创建QDBusInterface接口
    QDBusInterface interface("com.scorpio.test", "/test/objects",
                             "com.scorpio.test.value",
                             QDBusConnection::sessionBus());
    if (!interface.isValid())
    {
        qDebug() << qPrintable(QDBusConnection::sessionBus().lastError().message());
        exit(1);
    }
    //调用远程的value方法
    QDBusReply<int> reply = interface.call("value");
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