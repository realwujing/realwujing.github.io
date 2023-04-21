#ifndef TEST_H
#define TEST_H

#include <QObject>

#include "book.h"

class test: public QObject
{
    Q_OBJECT
    //定义Interface名称为com.scorpio.test.value
    Q_CLASSINFO("D-Bus Interface", "com.scorpio.test.value")
public:
    test(int value);

public slots:
    int maxValue();
    int minValue();
    int value();
    Book book(qlonglong &out1);
    Book book();
private:
    int m_value;
};

#endif // TEST_H	