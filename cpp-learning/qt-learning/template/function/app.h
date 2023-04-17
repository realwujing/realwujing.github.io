#pragma once
#include <QObject>
#include <QString>

class App : public QObject
{
    Q_OBJECT
    Q_PROPERTY(QString appId MEMBER appId)
    Q_PROPERTY(QString appName MEMBER appName)
    Q_PROPERTY(QString appVersion MEMBER appVersion)
public:
    QString appId;
    QString appName;
    QString appVersion;
};

// template <typename T>
// QDBusArgument &operator<<(QDBusArgument &argument, const T &object)
// {
//     const QMetaObject* metaObject = object.metaObject();
//     int propertyIndex;
//     int propertyCount = metaObject->propertyCount();
//     for (propertyIndex = 0; propertyIndex < propertyCount; ++propertyIndex)
//     {

//         QMetaProperty oneProperty = metaObject->property(propertyIndex);
//         qDebug() << "name: " << oneProperty.name() << "type: " << oneProperty.type();
//         if (oneProperty.isReadable())
//         {
//             QVariant variant = oneProperty.read(object);
//             argument.beginStructure();
//             argument << variant.value<oneProperty.type()>();
//             argument.endStructure();
//         }
//     }
// }

// inline QDBusArgument &operator<<(QDBusArgument &argument, const app &app)
// {
//     argument.beginStructure();
//     argument << app.appId << app.appName << app.appVersion;
//     argument.endStructure();
//     return argument;
// }

// inline QDBusArgument &operator>>(QDBusArgument &argument, app &app)
// {
//     argument.beginStructure();
//     argument >> app.appId >> app.appName >> app.appVersion;
//     argument.endStructure();
//     return argument;
// }

// template <typename T>
// QDBusArgument &operator>>(QDBusArgument &argument, T &object)
// {
//     const QMetaObject* metaObject = object.metaObject();
//     int propertyIndex;
//     int propertyCount = metaObject->propertyCount();
//     for (propertyIndex = 0; propertyIndex < propertyCount; ++propertyIndex)
//     {

//         QMetaProperty oneProperty = metaObject->property(propertyIndex);
//         qDebug() << "name: " << oneProperty.name() << "type: " << oneProperty.type();
//         if (oneProperty.isWritable())
//         {
//             QVariant variant = oneProperty.read(object);
//             argument.beginStructure();
//             argument << variant.value<oneProperty.type()>();
//             argument.endStructure();
//         }
//     }
// }