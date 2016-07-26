COMPILER_HOME=/home/rob/llvm-cilk
RUNTIME_HOME=./cilkrtssuspend
LIB=$(COMPILER_HOME)/lib/libcilkrts.a

export LIBRARY_PATH=$(COMPILER_HOME)/lib:$LIBRARY_PATH

CC=$(COMPILER_HOME)/bin/clang
CXX=$(COMPILER_HOME)/bin/clang++

# Optimize with lto
OPT ?= -march=native -flto -O3 -DNDEBUG
ARFLAGS = --plugin $(COMPILER_HOME)/lib/LLVMgold.so

# OPT ?= -march=native -O3 -DNDEBUG
# ARFLAGS = 

# Don't optimize
# OPT ?= -O0 
# ARFLAGS = 

INC = -I/usr/local/gcc5/include/c++/5.3.0 -I/usr/local/gcc5/include/c++/5.3.0/x86_64-unknown-linux-gnu
CFLAGS = -std=c++11 -Wfatal-errors $(OPT) $(PARAMS) $(INC) -g
CILKFLAGS = -fcilkplus -fcilk-no-inline
APPFLAGS = -I$(RUNTIME_HOME)/include
LDFLAGS = -ldl -lpthread -l/usr/local/gcc5/lib64/libstdc++.a -ltcmalloc

default: fib cilkfor cbt

lib: libcilkrr.a

libcilkrr.a: mutex.o cilkrr.o acquire.o
	ar rcs $(ARFLAGS) $@ $^

mutex.o: mutex.h mutex.cpp cilkrr.h
	$(CXX) $(CFLAGS) $(CILKFLAGS) -I$(RUNTIME_HOME)/include -c mutex.cpp

cilkrr.o: cilkrr.h cilkrr.cpp util.h
	$(CXX) $(CFLAGS) $(CILKFLAGS) -I$(RUNTIME_HOME)/include -c cilkrr.cpp

acquire.o: acquire.h util.h acquire.cpp
	$(CXX) $(CFLAGS) $(CILKFLAGS) -I$(RUNTIME_HOME)/include -c acquire.cpp

cilkfor.o: cilkfor.cpp mutex.h cilkrr.h
	$(CXX) $(CFLAGS) $(CILKFLAGS) $(APPFLAGS) -c cilkfor.cpp

fib.o: fib.cpp mutex.h cilkrr.h
	$(CXX) $(CFLAGS) $(CILKFLAGS) $(APPFLAGS) -c fib.cpp

cbt.o: cbt.cpp mutex.h cilkrr.h
	$(CXX) $(CFLAGS) $(CILKFLAGS) $(APPFLAGS) -c cbt.cpp

cbt: cbt.o libcilkrr.a mutex.h $(LIB)
	$(CXX) cbt.o libcilkrr.a $(LIB) $(LDFLAGS) -o cbt

cilkfor: cilkfor.o libcilkrr.a mutex.h $(LIB)
	$(CXX) cilkfor.o libcilkrr.a $(LIB) $(LDFLAGS) -o cilkfor

fib: fib.o libcilkrr.a mutex.h $(LIB)
	$(CXX) fib.o libcilkrr.a $(LIB) $(LDFLAGS) -o fib

clean:
	rm -f *.o *.a fib cilkfor cbt
