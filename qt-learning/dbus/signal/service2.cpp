#include "service2.h"

Service::Service()
{
    QDBusConnection::sessionBus().unregisterService("com.citos.service2");
    QDBusConnection::sessionBus().registerService("com.citos.service2");
    QDBusConnection::sessionBus().registerObject("/citos/path", this,QDBusConnection :: ExportAllSlots | QDBusConnection :: ExportAllSignals);
    QDBusConnection::sessionBus().connect(QString(), QString("/citos/path"), "com.citos.test", "send_to_service", this, SLOT(service_get(QString)));
}
void Service::service_get(QString st)
{
    qInfo() << "Message get from client: "<< st;
}

void service_listen()
{
    while(true)
    {
        getchar();
        qInfo() << "Message send!";
        QDBusMessage message =QDBusMessage::createSignal("/citos/path", "com.citos.test", "send_to_client");
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