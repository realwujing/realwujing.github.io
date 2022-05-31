#include "service.h"

#include <unistd.h>

Service::Service()
{
    QDBusConnection::sessionBus().unregisterService("com.citos.service");
    QDBusConnection::sessionBus().registerService("com.citos.service");
    QDBusConnection::sessionBus().registerObject("/citos/path", this, QDBusConnection ::ExportAllSlots | QDBusConnection ::ExportAllSignals);
    QDBusConnection::sessionBus().connect(QString(), QString("/citos/path"), "com.citos.test", "send_to_service", this, SLOT(service_get(QString)));

    QDBusConnection p("com.citos.service2");
    QDBusConnection q = QDBusConnection::connectToBus(QDBusConnection::SessionBus, "com.citos.service2");
    q.unregisterService("com.citos.service2");
    q.registerService("com.citos.service2");
    q.registerObject("/citos/path", this, QDBusConnection ::ExportAllSlots | QDBusConnection ::ExportAllSignals);
    q.connect(QString(), QString("/citos/path"), "com.citos.test", "send_to_service", this, SLOT(service_get2(QString)));

}


void Service::service_get(QString st)
{
    qInfo() << "service_get Message get from client: " << st;
    
    std::thread th1(sleep, 100);
    th1.detach();
    qInfo() << "service_get thread detach" << st;
    
}

void Service::service_get2(QString st)
{
    qInfo() << "service_get2 Message get from client: " << st;
    std::thread th1(sleep, 30);
    th1.detach();
    qInfo() << "service_get2 thread detach" << st;
    
}

void service_listen()
{
    while (true)
    {
        getchar();
        qInfo() << "Message send!";
        QDBusMessage message = QDBusMessage::createSignal("/citos/path", "com.citos.test", "send_to_client");
        QDBusConnection::sessionBus().send(message);
    }
}

int main(int argc, char *argv[])
{
    QCoreApplication a(argc, argv);

    Service service;

    std::thread th1(service_listen);
    th1.detach();

    return a.exec();
}