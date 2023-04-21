//
// Created by wujing on 2020/11/15.
//

#include "Test2.h"
#include "Test1.h"
#include "Test3.h"
#include "Test4.h"

#include <iostream>

void fun2(){
    std::cout << g_str << std::endl;
}

int main(){

    fun2();
    fun1();
    test3.print();
    test4.print();
    return 0;
}