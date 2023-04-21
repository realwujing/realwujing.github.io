#pragma once

#include <string>

struct Book
{
    int id;
    std::string name;
};

class Person
{
public:
    // Person();
    // ~Person();
    void say();

    Book *book;
    int heigh;
};