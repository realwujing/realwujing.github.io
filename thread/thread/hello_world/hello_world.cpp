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