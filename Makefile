COMPILER_HOME=/home/rob/llvm-cilk
RUNTIME_HOME=./cilkrtssuspend
LIB=$(COMPILER_HOME)/lib/libcilkrts.a

export LIBRARY_PATH=$(COMPILER_HOME)/lib:$LIBRARY_PATH

CC=$(COMPILER_HOME)/bin/clang
CXX=$(COMPILER_HOME)/bin/clang++
CFLAGS=-std=c++11 -g -Wfatal-errors
CILKFLAGS=-fcilkplus -I$(COMPILER_HOME)/include -fcilk-no-inline

default: fib

lib: libcilkrr.a

cilkfor: cilkfor.o libcilkrr.a mutex.h $(LIB)
	$(CXX) $(CFLAGS) $(CILKFLAGS) cilkfor.o libcilkrr.a $(LIB) -ldl -lpthread -o cilkfor

fib: fib.o libcilkrr.a mutex.h $(LIB)
	$(CXX) $(CFLAGS) $(CILKFLAGS) fib.o libcilkrr.a $(LIB) -ldl -lpthread -o fib

cilkfor.o: cilkfor.cpp mutex.h cilkrr.h syncstream.h
	$(CXX) $(CFLAGS) $(CILKFLAGS) -c cilkfor.cpp

fib.o: fib.cpp mutex.h cilkrr.h syncstream.h
	$(CXX) $(CFLAGS) $(CILKFLAGS) -c fib.cpp

libcilkrr.a: mutex.o cilkrr.o acquire.o syncstream.o
	ar rcs $@ $^

mutex.o: mutex.h mutex.cpp cilkrr.h syncstream.h
	$(CXX) $(CFLAGS) $(CILKFLAGS) -I$(RUNTIME_HOME)/include -c mutex.cpp

cilkrr.o: cilkrr.h cilkrr.cpp util.h
	$(CXX) $(CFLAGS) $(CILKFLAGS) -I$(RUNTIME_HOME)/include -c cilkrr.cpp

acquire.o: acquire.h syncstream.h util.h acquire.cpp
	$(CXX) $(CFLAGS) $(CILKFLAGS) -I$(RUNTIME_HOME)/include -c acquire.cpp

syncstream.o: syncstream.h syncstream.cpp
	$(CXX) $(CFLAGS) -c syncstream.cpp

clean:
	rm -f *.o *.a fib cilkfor
