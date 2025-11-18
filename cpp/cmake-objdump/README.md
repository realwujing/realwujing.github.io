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
