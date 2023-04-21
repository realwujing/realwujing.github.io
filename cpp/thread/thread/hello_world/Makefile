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