#include <QCoreApplication>

#include "test_class.h"

int main(int argc, char *argv[])
{
    QCoreApplication a(argc, argv);
 
    TestClass testClass(nullptr);
    testClass.doFunc();
 
    return a.exec();
}