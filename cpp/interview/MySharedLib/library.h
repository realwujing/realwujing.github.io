#ifndef MYSHAREDLIB_LIBRARY_H
#define MYSHAREDLIB_LIBRARY_H

// 打印 Hello World!
void hello();

// 使用可变模版参数求和
template <typename T>
T sum(T t)
{
    return t;
}
template <typename T, typename... Types>
T sum(T first, Types... rest)
{
    return first + sum<T>(rest...);
}

#endif