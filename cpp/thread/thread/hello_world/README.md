# std::thread "Hello world"

下面是一个最简单的使用 std::thread 类的例子：

```cpp
#include <iostream> // std::cout
#include <thread>   // std::thread

void thread_task() {
    std::cout << "hello thread" << std::endl;
}

int main(int argc, const char *argv[])
{
    std::thread t(thread_task);
    t.join();

    return EXIT_SUCCESS;
}
```

```makefile
all:hello_world

CC=g++
CPPFLAGS=-Wall -std=c++11 -ggdb
LDFLAGS=-pthread

hello_world:hello_world.o
	$(CC) $(LDFLAGS) -o $@ $^

hello_world.o:hello_world.cpp
	$(CC) $(CPPFLAGS) -o $@ -c $^

.PHONY:
	clean

clean:
	rm hello_world.o hello_world
```

[C++11 并发指南一(C++11 多线程初探)](https://www.cnblogs.com/haippy/p/3235560.html)
