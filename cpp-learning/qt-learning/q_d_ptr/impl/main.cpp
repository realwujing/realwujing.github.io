// main.cpp
 
 
#include <QCoreApplication>
#include <QDebug>

#include "myfather.h"

int main(int argc, char *argv[])
{
    QCoreApplication a(argc, argv);
 
    MyFather father;
    father.setObjectName("My dear father");
    father.setDrinkEnable(true);
    father.do_drink();
    father.do_smoke();
 
    father.setProperty("name","Lao sun");
    qDebug() << father.name();
 
    father.setAge(56);
    qDebug() << father.age();
 
    return a.exec();
}