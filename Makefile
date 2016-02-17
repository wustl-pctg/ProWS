CC=gcc
CXX=g++
CFLAGS=-std=c++11 -fcilkplus -g

default: fib

fib: fib.cpp cilkrr_mutex.cpp
	$(CXX) $(CFLAGS) fib.cpp cilkrr_mutex.cpp -o fib -lcilkrts
