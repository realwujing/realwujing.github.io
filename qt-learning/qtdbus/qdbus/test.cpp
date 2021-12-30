#include "test.h"
#include <QDebug>

test::test(int value)
{
    m_value = value;
}

int test::maxValue()
{
    qInfo() << "maxValue:" << 100;
    return 100;
}
int test::minValue()
{
    qInfo() << "m_value";
    return 0;
}
int test::value()
{
    qInfo() << "m_value:" << m_value;
    return m_value;
}