#include "singals_slots.h"

Counter::Counter()
{
    m_value = 0;
}

void Counter::setValue(int value)
{
    if (value != m_value)
    {
        m_value = value;
        emit valueChanged(value);
    }
}

int Counter::value() const
{
    return m_value;
}

int main()
{
    Counter a, b;
    QObject::connect(&a, SIGNAL(valueChanged(int)), &b, SLOT(setValue(int)));
    a.setValue(12);     //  a.value() == 12, b.value() == 12
    qInfo() << "a.value():" << a.value() << "b.value():" << b.value() << Qt::endl;
    b.setValue(48);        //   a.value() == 12, b.value() == 48
    qInfo() << "a.value():" << a.value() << "b.value():" << b.value() << Qt::endl;
}
