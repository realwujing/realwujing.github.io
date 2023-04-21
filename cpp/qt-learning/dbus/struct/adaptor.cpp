#include <QCoreApplication>
#include <QDBusConnection>
#include <QDebug>
#include <QDBusError>

#include "test.h"
#include "valueAdaptor.h"

int main(int argc, char *argv[])
{
    QCoreApplication a(argc, argv);

    qRegisterMetaType<Book>("Book");
    qDBusRegisterMetaType<Book>();

    QDBusConnection connection = QDBusConnection::sessionBus();
    test object(60);
    //ValueAdaptor是qdbusxml2cpp生成的Adaptor类
    ValueAdaptor valueAdaptor(&object);
    if (!connection.registerService("com.scorpio.test"))
    {
        qDebug() << connection.lastError().message();
        exit(1);
    }
    connection.registerObject("/test/objects", &object);
    return a.exec();
}