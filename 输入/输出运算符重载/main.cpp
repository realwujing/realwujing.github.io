#include <iostream>
#include <sstream>


class logstream
{
public:
    template <typename T>
    inline logstream &operator<<(const T &log)
    {
        m_oss << log;
        return *this;
    }

    inline logstream &operator<<(std::ostream &(*)(std::ostream &))
    {
        // 接收std::endl
        std::cout << m_oss.str() << std::endl;
        return *this;
    }

    std::ostringstream m_oss;
};

class logger : public logstream
{
public:
};

int main()
{
    // logger log;
    logger() << 123 << std::endl;

    return 0;
}