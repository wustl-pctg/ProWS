include ../common.mk

OBJ = simple_future_cilkr.o
MY_LDFLAGS=-R=../cilkrtssuspend/.libs -lpthread -lcilkrts
MY_CXXFLAGS=-g -std=c++11 -Wfatal-errors -I/home/kdsinger/porridge/cilkrtssuspend/include -fcilkplus -fcilk-no-inline
APP = simple-future
.SECONDARY: $(OBJ)

.PHONY = default
default:
	$(CXX) -O3 -flto -g  -fcilkplus -fno-omit-frame-pointer -fcilk-no-inline -I/home/kdsinger/porridge/llvm-cilk/include -I/home/kdsinger/porridge/cilkrtssuspend/include -std=c++11 -Wall -c ktiming.c -o ktiming.o
	$(CXX) -O3 -flto -g  -fcilkplus -fno-omit-frame-pointer -fcilk-no-inline -I/home/kdsinger/porridge/llvm-cilk/include -I/home/kdsinger/porridge/cilkrtssuspend/include -std=c++11 -Wall -c fib_cilkplus.cpp -o fib_cilkplus.o
	$(CXX) -flto  fib_cilkplus.o ktiming.o -o fib_cilkplus /home/kdsinger/porridge/cilkrtssuspend/.libs/libcilkrts.a -lrt -ldl -lpthread -ltcmalloc -lpthread -lm -lrt

fib-fut:
	$(CXX) -O3 -flto -g  -fcilkplus -fno-omit-frame-pointer -fcilk-no-inline -I/home/kdsinger/porridge/llvm-cilk/include -I/home/kdsinger/porridge/cilkrtssuspend/include -std=c++11 -Wall -c ktiming.c -o ktiming.o
	$(CXX) -O3 -flto -g  -fcilkplus -fno-omit-frame-pointer -fcilk-no-inline -I/home/kdsinger/porridge/llvm-cilk/include -I/home/kdsinger/porridge/cilkrtssuspend/runtime/config/x86 -I/home/kdsinger/porridge/cilkrtssuspend/runtime/sslib -I/home/kdsinger/porridge/cilkrtssuspend/include -std=c++11 -Wall -c fib_cilkfut.cpp -o fib_cilkfut.o
	$(CXX) -flto fib_cilkfut.o ktiming.o -o fib_cilkfut /home/kdsinger/porridge/cilkrtssuspend/.libs/libcilkrts.a -lrt -ldl -lpthread -ltcmalloc -lpthread -lm -lrt

fib-handcomp:
	$(CXX) -O3 -flto -g -fcilkplus -fno-omit-frame-pointer -fcilk-no-inline -I/home/kdsinger/porridge/llvm-cilk/include -I/home/kdsinger/porridge/cilkrtssuspend/include -std=c++11 -Wall -c ktiming.c -o ktiming.o
	$(CXX) -O3 -flto -g -fcilkplus -fno-omit-frame-pointer -fcilk-no-inline -DPRECOMPUTE_PEDIGREES=1 -I/home/kdsinger/porridge/llvm-cilk/include -I/home/kdsinger/porridge/cilkrtssuspend/runtime/config/x86 -I/home/kdsinger/porridge/cilkrtssuspend/runtime/sslib -I/home/kdsinger/porridge/cilkrtssuspend/include -std=c++11 -Wall -c handcomp_fib_cilkfut.cpp -o handcomp_fib_cilkfut.o
	$(CXX) -flto handcomp_fib_cilkfut.o ktiming.o -o handcomp_fib_cilkfut /home/kdsinger/porridge/cilkrtssuspend/.libs/libcilkrts.a -lrt -ldl -lpthread -ltcmalloc -lpthread -lm -lrt

.PHONY = clean
clean:
	rm -f *.o handcomp_fib_cilkfut fib_cilkfut fib_cilkplus
