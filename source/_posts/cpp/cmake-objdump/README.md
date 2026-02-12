---
title: 'CMake编译选项与Objdump调试'
date: '2023/12/15 10:30:09'
updated: '2025/11/18 22:04:31'
---

# CMake编译选项与Objdump调试

test cmake .. -DCMAKE_BUILD_TYPE=Release

test1 cmake .. -DCMAKE_BUILD_TYPE=Debug

test2 cmake .. -DCMAKE_BUILD_TYPE=Debug

test3 cmake ..

test test3

test3 24行 -g

test1 test2

strip test1

objdump -d -l -S -C qtdemo > qtdemo.dump
