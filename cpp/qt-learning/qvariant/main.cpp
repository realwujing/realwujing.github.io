#include <QVariant>
#include <QPoint>
#include <QDebug>

class MyClass
{
public:
    MyClass():i(0){}
    friend QDebug operator<<(QDebug d, const MyClass &c);
    
private:
    int i;
};
//注意，如果想要自定义的MyStruct也支持QVariant，那么需要Q_DECLARE_METATYPE向Qt元对象系统进行注册
Q_DECLARE_METATYPE(MyClass)
QDebug operator<<(QDebug d, const MyClass &c)
{
    d << "MyStruct(" << c.i << ")";
}

int main(int argc, char *argv[])
{
    QVariant v = 42;
    
    qDebug() << "v.canConvert<int>(): " << v.canConvert<int>() << v.toInt();
    qDebug() << "v.canConvert<QString>()" << v.canConvert<QString>() << v.toString();
    qDebug() << "v.canConvert<QPoint>()" << v.canConvert<QPoint>() << v.toPoint();
    
    qDebug() << "v covert to QString before:" << v;
    v.convert(QVariant::String);
    qDebug() << "v covert to QString after:" << v;
    v.convert(QVariant::Point);//如果无法转换，则QVariant本身会被清空
    qDebug() << "v covert to QPoint after:" << v;
    
    //支持自定义类型
    MyClass c;
    v.setValue(c);
    //...
    MyClass c2 = v.value<MyClass>();
    qDebug() << c2;
}