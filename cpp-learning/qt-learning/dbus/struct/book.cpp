#include "book.h"

QDBusArgument &operator<<(QDBusArgument &argument, const Book &book)
{
    argument.beginStructure();

    argument << book.name;
    argument << book.page;
    argument << book.author;

    argument.endStructure();

    return argument;
}

const QDBusArgument &operator>>(const QDBusArgument &argument, Book &book)
{
    argument.beginStructure();

    argument >> book.name;
    argument >> book.page;
    argument >> book.author;

    argument.endStructure();

    return argument;
}