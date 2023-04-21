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

Book test::book(qlonglong &out1)
{
    out1 = 666;
    Book book;
    book.name = "qtdbus";
    book.page = 888;
    book.author = "wujing";
    qInfo() << out1 << book.name << book.page << book.author;
    return book;
}

Book test::book()
{
    Book book;
    book.name = "qtdbus";
    book.page = 888;
    book.author = "wujing";
    qInfo() << book.name << book.page << book.author;
    return book;
}