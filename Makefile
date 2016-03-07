COMPILER_HOME=/home/rob/src/llvm-cilk
RUNTIME_HOME=./cilkrtssuspend
LIB=$(COMPILER_HOME)/lib/libcilkrts.a

CC=$(COMPILER_HOME)/bin/clang
CXX=$(COMPILER_HOME)/bin/clang++
CFLAGS=-std=c++11 -g
CILKFLAGS=-fcilkplus -I$(COMPILER_HOME)/include -fno-inline-detach

default: fib

fib: fib.o mutex.o cilkrr.o syncstream.o $(LIB)
	$(CXX) $(CFLAGS) $(CILKFLAGS) fib.o cilkrr.o mutex.o syncstream.o $(LIB) -ldl -lpthread -o fib

fib.o: fib.cpp mutex.h cilkrr.h
	$(CXX) $(CFLAGS) $(CILKFLAGS) -c fib.cpp

mutex.o: mutex.h mutex.cpp cilkrr.h
	$(CXX) $(CFLAGS) $(CILKFLAGS) -I$(RUNTIME_HOME)/include -c mutex.cpp

cilkrr.o: cilkrr.h cilkrr.cpp
	$(CXX) $(CFLAGS) $(CILKFLAGS) -I$(RUNTIME_HOME)/include -c cilkrr.cpp

syncstream.o: syncstream.h syncstream.cpp
	$(CXX) $(CFLAGS) -c syncstream.cpp

clean:
	rm *.o fib
