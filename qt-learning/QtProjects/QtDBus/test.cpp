#include "test.h"

test::test(int value)
{
    m_value = value;
}

int test::maxValue()
{
    return 100;
}
int test::minValue()
{
    return 0;
}
int test::value()
{
    return m_value;
}