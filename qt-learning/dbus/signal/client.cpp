#include "client.h"

Client::Client()
{
    QDBusConnection::sessionBus().unregisterService("com.citos.client");
    QDBusConnection::sessionBus().registerService("com.citos.client");
    QDBusConnection::sessionBus().registerObject("/citos/path", this, QDBusConnection ::ExportAllSlots | QDBusConnection ::ExportAllSignals);
    QDBusConnection::sessionBus().connect(QString(), QString("/citos/path"), "com.citos.test", "send_to_client", this, SLOT(client_get(void)));
}

void Client::client_get()
{
    qInfo() << "Client get!";
}

void client_listen()
{
    while(true)
    {
        getchar();
        qInfo() << "Message send!";
        QDBusMessage message =QDBusMessage::createSignal("/citos/path", "com.citos.test", "send_to_service");
        message << QString("Hello world!");
        QDBusConnection::sessionBus().send(message);
    }
}

int main(int argc, char *argv[])
{
    QCoreApplication a(argc, argv);

    Client client;

    std::thread th1(client_listen);
    th1.detach();

    return a.exec();
}