CC=gcc
CXX=g++
CFLAGS=-std=c++11 -g
CILKFLAGS=-fcilkplus

default: fib

fib: fib.cpp mutex.h mutex.cpp cilkrr.h cilkrr.cpp
	$(CXX) $(CFLAGS) $(CILKFLAGS) -c fib.cpp
	$(CXX) $(CFLAGS) $(CILKFLAGS) -c cilkrr.cpp
	$(CXX) $(CFLAGS) $(CILKFLAGS) -c mutex.cpp
	$(CXX) $(CFLAGS) $(CILKFLAGS) fib.o cilkrr.o mutex.o -o fib -lcilkrts
