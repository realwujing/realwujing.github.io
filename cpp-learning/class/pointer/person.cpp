#include "person.h"

#include <iostream>

// Person::Person()
// {
//     book = new Book();
// }

// Person::~Person()
// {
//     delete book;
// }

void Person::say()
{
    ::std::cout << "this->book.id:" << book->id << "|this->book.name:" << book->name << std::endl;
}