---
date: 2024/06/24 13:51:49
updated: 2024/06/24 13:51:49
---

# extern

在 C 语言中，`extern` 关键字用于声明一个变量或函数在其他文件中定义。这样可以在不同的源文件之间共享变量或函数。

以下是一个简单的示例，演示如何使用 `extern` 关键字在多个源文件之间共享变量：

## 文件结构

假设有两个文件：`main.c` 和 `other.c`，以及一个头文件 `other.h`。

### other.h

```c
// other.h
#ifndef OTHER_H
#define OTHER_H

extern int shared_variable; // 声明一个外部变量
void print_shared_variable(void); // 声明一个外部函数

#endif // OTHER_H
```

### other.c

```c
// other.c
#include <stdio.h>
#include "other.h"

int shared_variable = 42; // 定义并初始化外部变量

void print_shared_variable(void) {
    printf("Shared variable: %d\n", shared_variable); // 打印外部变量的值
}
```

### main.c

```c
// main.c
#include <stdio.h>
#include "other.h"

int main(void) {
    printf("Initial shared variable: %d\n", shared_variable); // 打印初始值
    shared_variable = 100; // 修改外部变量的值
    print_shared_variable(); // 调用外部函数，打印修改后的值

    return 0;
}
```

## 编译和运行

1. 编译两个源文件：

   ```sh
   gcc -o my_program main.c other.c -g
   ```

2. 运行生成的可执行文件：

   ```sh
   ./my_program
   ```

   输出：

   ```text
   Initial shared variable: 42
   Shared variable: 100
   ```

## 解释

1. **other.h**: 声明了一个外部变量 `shared_variable` 和一个外部函数 `print_shared_variable`。
2. **other.c**: 定义并初始化了 `shared_variable`，并实现了 `print_shared_variable` 函数。
3. **main.c**: 通过包含 `other.h`，使用 `extern` 声明的变量和函数。在 `main` 函数中，先打印 `shared_variable` 的初始值，然后修改它的值并调用 `print_shared_variable` 函数来打印修改后的值。

这样，`extern` 关键字允许在不同的源文件之间共享变量和函数。
