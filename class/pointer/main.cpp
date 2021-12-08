#include "person.h"

int main()
{
    Person person;
    person.book->id = 1;
    person.book->name = "hello";
    person.say();

    // Person *person2 = new Person();
    // person2->book->id = 2;
    // person2->book->name = "world";
    // person2->say();
    // delete person2;

}