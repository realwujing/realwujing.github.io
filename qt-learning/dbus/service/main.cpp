#include <QCoreApplication>
#include <QDBusConnection>
#include <QDebug>
#include <QDBusError>
#include "test.h"

int main(int argc, char *argv[])
{
    qSetMessagePattern("[%{type}] DateTime:%{time [yyyy-MM-dd hh:mm:ss ddd]} File:%{file} Line:%{line} Function:%{function} Message:%{message}");

    qInfo() << "start";

    QCoreApplication a(argc, argv);

    //建立到session bus的连接
    QDBusConnection connection = QDBusConnection::sessionBus();
    //在session bus上注册名为com.scorpio.test的服务
    if(!connection.registerService("com.scorpio.test"))
    {
        qDebug() << "error:" << connection.lastError().message();
        exit(-1);
    }
    test object(60);
    //注册名为/test/objects的对象，把类Object所有槽函数导出为object的method
    connection.registerObject("/test/objects", &object,QDBusConnection::ExportAllSlots);

    qInfo() << "here";

    return a.exec();
}