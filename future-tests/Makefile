include ../common.mk

OBJ = simple_future_cilkr.o
MY_LDFLAGS=-R=../cilkrtssuspend/.libs -lpthread -lcilkrts
MY_CXXFLAGS=-g -std=c++11 -Wfatal-errors -I/home/kdsinger/porridge/cilkrtssuspend/include -fcilkplus -fcilk-no-inline
APP = simple-future
.SECONDARY: $(OBJ)

.PHONY = default
default:
	$(CXX) -O3 -g -fcilkplus -fno-omit-frame-pointer -fcilk-no-inline -I/home/kdsinger/porridge/llvm-cilk/include -I/home/kdsinger/porridge/cilkrtssuspend/include -std=c++11 -Wall -c simple_future_cilkr.cpp -o simple_future_cilkr.o
	$(CXX) simple_future_cilkr.o -o simple-future /home/kdsinger/porridge/cilkrtssuspend/.libs/libcilkrts.a -lrt -ldl -lpthread -ltcmalloc -lpthread -lm -lrt

qsort: qsort.cpp
	$(CXX) -O3 -g -fcilkplus -fno-omit-frame-pointer -fcilk-no-inline -I/home/kdsinger/porridge/llvm-cilk/include -I/home/kdsinger/porridge/cilkrtssuspend/include -std=c++11 -Wall -c qsort.cpp -o qsort.o
	$(CXX) qsort.o -o qsort /home/kdsinger/porridge/cilkrtssuspend/.libs/libcilkrts.a -lrt -ldl -lpthread -ltcmalloc -lpthread -lm -lrt

.PHONY = clean
clean:
	rm -f *.o a.out simple-future qsort
