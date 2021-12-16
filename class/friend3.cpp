#include <iostream>
#include <string>
#include <vector>
 
int a;
bool b;
char c;
double d;
 
std::string str;
std::vector<int> vec;


 
int main()
{
	int a2;
	bool b2;
	char c2;
	double d2;
    struct Book {
    int a;
    int b;
    };
    Book book;
	// std::string str2;
	// std::vector<int> vec2;
 
	//Microsoft C++ 异常: std::out_of_range
	//vec.at(0) = 1;
	//vec2.push_back(1);
	// /*
	// std::cout << "a:"<<a << std::endl;
	// std::cout << "b:"<<b << std::endl;
	// std::cout << "c:"<<c << std::endl;
	// std::cout << "d:"<<d << std::endl;
	std::cout << "a2:"<< a2 << std::endl;
	std::cout << "b2:"<< b2 << std::endl;
	std::cout << "c2:"<< c2 << std::endl;
	std::cout << "d2:"<< d2 << std::endl;
    std::cout << "book.a:"<< book.a << std::endl;
	// */
	// system("pause");
	return 0;
}