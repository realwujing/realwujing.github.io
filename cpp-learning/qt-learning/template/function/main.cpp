#include <QCoreApplication>
#include <QDebug>
#include <QMetaProperty>

#include "app.h"

int main(int argc, char *argv[])
{
    QCoreApplication a(argc, argv);
    App app;
    app.appId = "com.example.app";
    app.appName = "app";
    app.appVersion = "1.0.0";
    const QMetaObject* metaObject = app.metaObject();
    for (int propertyIndex = 1; propertyIndex < metaObject->propertyCount(); ++propertyIndex)
    {
        QMetaProperty oneProperty = metaObject->property(propertyIndex);
        qInfo() << "type:" << oneProperty.type() <<"name:" << oneProperty.name() << "value:" << oneProperty.read(&app);
    }
    return a.exec();
}