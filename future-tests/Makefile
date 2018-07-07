include ../common.mk

OBJ = simple_future_cilkr.o
MY_LDFLAGS= -flto /home/kdsinger/porridge/cilkrtssuspend/.libs/libcilkrts.a -lrt -ldl -lpthread -ltcmalloc -lpthread -lm -lrt
MY_CXXFLAGS=-O3 -g -flto -fcilkplus -fno-omit-frame-pointer -fcilk-no-inline -I/home/kdsinger/porridge/llvm-cilk/include -I/home/kdsinger/porridge/cilkrtssuspend/include -std=c++11 -Wall -c
APP = simple-future
.SECONDARY: $(OBJ)

.PHONY = default
default:
	$(CXX) $(MY_CXXFLAGS) simple_future_cilkr.cpp -o simple_future_cilkr.o
	$(CXX) simple_future_cilkr.o -o simple-future $(MY_LDFLAGS)

qsort: qsort.cpp
	$(CXX) -O3 -g -flto -fcilkplus -fno-omit-frame-pointer -fcilk-no-inline -I/home/kdsinger/porridge/llvm-cilk/include -I/home/kdsinger/porridge/cilkrtssuspend/include -std=c++11 -Wall -c qsort.cpp -o qsort.o
	$(CXX) -flto qsort.o -o qsort /home/kdsinger/porridge/cilkrtssuspend/.libs/libcilkrts.a -lrt -ldl -lpthread -ltcmalloc -lpthread -lm -lrt

.PHONY = clean
clean:
	rm -f *.o a.out simple-future qsort
