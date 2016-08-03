COMPILER_HOME=/home/rob/llvm-cilk
RUNTIME_HOME=./cilkrtssuspend
LIB=$(COMPILER_HOME)/lib/libcilkrts.a

export LIBRARY_PATH=$(COMPILER_HOME)/lib:$LIBRARY_PATH

CC=$(COMPILER_HOME)/bin/clang
CXX=$(COMPILER_HOME)/bin/clang++

STATS ?= 0
PTYPE ?= 1
STAGE ?= 4
DEFS = -DPTYPE=$(PTYPE) -DSTAGE=$(STAGE)
ifeq ($(STATS),1)
  DEFS += -DSTATS
  LIBS = -lpapi
endif

# Stoppard is messed up...
INC = -I/usr/local/gcc5/include/c++/5.3.0 -I/usr/local/gcc5/include/c++/5.3.0/x86_64-unknown-linux-gnu
LIBS += /usr/local/gcc5/lib64/libstdc++.a
ARFLAGS =

# Optimize with lto
LTO = -flto
ARFLAGS = --plugin $(COMPILER_HOME)/lib/LLVMgold.so

OPT = -march=native -O3 -DNDEBUG $(LTO)
CFLAGS = -g -std=c++11 -Wfatal-errors $(OPT) $(INC) $(DEFS)
CILKFLAGS = -fcilkplus -fcilk-no-inline
APPFLAGS = -I$(RUNTIME_HOME)/include
LDFLAGS = $(LTO) -ldl -lpthread -ltcmalloc $(LIBS)

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
