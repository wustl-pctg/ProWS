include ../bench/bench.mk

RTSLIB=../cilkrtssuspend/.libs/libcilkrts.a
LIBNAME=../build/libporr.a
CILKFLAGS=-fcilkplus -fcilk-no-inline

.PHONY = default
default: simple_future_cilkr.cpp
	$(CXX) $(CILKFLAGS) $(INC) -std=c++11 simple_future_cilkr.cpp $(LIBNAME) $(LDFLAGS) -o simple_future_cilkr

old: main.cpp
	$(CXX) -fcilkplus -std=c++11 main.cpp

.PHONY = clean
clean:
	rm -f simple_future_cilkr a.out
