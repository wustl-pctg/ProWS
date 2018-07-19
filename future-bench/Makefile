include ../common.mk

OBJ = simple_future_cilkr.o
MY_LDFLAGS=-R=../cilkrtssuspend/.libs -lpthread -lcilkrts
MY_CXXFLAGS=-g -std=c++11 -Wfatal-errors -I/home/kdsinger/workspace/porridge/cilkrtssuspend/include -fcilkplus -fcilk-no-inline
APP = simple-future
.SECONDARY: $(OBJ)

.PHONY = default
default:
	$(CXX) -O3 -flto -g  -fcilkplus -fno-omit-frame-pointer -fcilk-no-inline -I/home/kdsinger/workspace/porridge/llvm-cilk/include -I/home/kdsinger/workspace/porridge/cilkrtssuspend/include -std=c++11 -Wall -c ktiming.c -o ktiming.o
	$(CXX) -O3 -flto -g  -fcilkplus -fno-omit-frame-pointer -fcilk-no-inline -I/home/kdsinger/workspace/porridge/llvm-cilk/include -I/home/kdsinger/workspace/porridge/cilkrtssuspend/include -std=c++11 -Wall -c fib_cilkplus.cpp -o fib_cilkplus.o
	$(CXX) -flto  fib_cilkplus.o ktiming.o -o fib_cilkplus /home/kdsinger/workspace/porridge/cilkrtssuspend/.libs/libcilkrts.a -lrt -ldl -lpthread -ltcmalloc -lpthread -lm -lrt

fib-fut:
	$(CXX) -O3 -flto -g  -fcilkplus -fno-omit-frame-pointer -fcilk-no-inline -I/home/kdsinger/workspace/porridge/llvm-cilk/include -I/home/kdsinger/workspace/porridge/cilkrtssuspend/include -std=c++11 -Wall -c ktiming.c -o ktiming.o
	$(CXX) -O3 -flto -g  -fcilkplus -fno-omit-frame-pointer -fcilk-no-inline -I/home/kdsinger/workspace/porridge/llvm-cilk/include -I/home/kdsinger/workspace/porridge/cilkrtssuspend/runtime/config/x86 -I/home/kdsinger/workspace/porridge/cilkrtssuspend/runtime/sslib -I/home/kdsinger/workspace/porridge/cilkrtssuspend/include -std=c++11 -Wall -c fib_cilkfut.cpp -o fib_cilkfut.o
	$(CXX) -flto fib_cilkfut.o ktiming.o -o fib_cilkfut /home/kdsinger/workspace/porridge/cilkrtssuspend/.libs/libcilkrts.a -lrt -ldl -lpthread -ltcmalloc -lpthread -lm -lrt

fib-handcomp:
	$(CXX) -O3 -flto -g -fcilkplus -fno-omit-frame-pointer -fcilk-no-inline -I/home/kdsinger/workspace/porridge/llvm-cilk/include -I/home/kdsinger/workspace/porridge/cilkrtssuspend/include -std=c++11 -Wall -c ktiming.c -o ktiming.o
	$(CXX) -O3 -flto -g -fcilkplus -fno-omit-frame-pointer -fcilk-no-inline -DPRECOMPUTE_PEDIGREES=1 -I/home/kdsinger/workspace/porridge/llvm-cilk/include -I/home/kdsinger/workspace/porridge/cilkrtssuspend/runtime/config/x86 -I/home/kdsinger/workspace/porridge/cilkrtssuspend/runtime/sslib -I/home/kdsinger/workspace/porridge/cilkrtssuspend/include -std=c++11 -Wall -c handcomp_fib_cilkfut.cpp -o handcomp_fib_cilkfut.o
	$(CXX) -flto handcomp_fib_cilkfut.o ktiming.o -o handcomp_fib_cilkfut /home/kdsinger/workspace/porridge/cilkrtssuspend/.libs/libcilkrts.a -lrt -ldl -lpthread -ltcmalloc -lpthread -lm -lrt

fib-interop-tests:
	$(CXX) -O3 -flto -g -fcilkplus -fno-omit-frame-pointer -fcilk-no-inline -I/home/kdsinger/workspace/porridge/llvm-cilk/include -I/home/kdsinger/workspace/porridge/cilkrtssuspend/include -std=c++11 -Wall -c ktiming.c -o ktiming.o
	# Test just futures
	$(CXX) -O3 -flto -g -fcilkplus -fno-omit-frame-pointer -fcilk-no-inline -DPRECOMPUTE_PEDIGREES=1 -I/home/kdsinger/workspace/porridge/llvm-cilk/include -I/home/kdsinger/workspace/porridge/cilkrtssuspend/runtime/config/x86 -I/home/kdsinger/workspace/porridge/cilkrtssuspend/runtime/sslib -I/home/kdsinger/workspace/porridge/cilkrtssuspend/include -std=c++11 -Wall -c handcomp_fib_cilkfut.cpp -o handcomp_fib_cilkfut.o
	$(CXX) -flto handcomp_fib_cilkfut.o ktiming.o -o handcomp_fib_cilkfut /home/kdsinger/workspace/porridge/cilkrtssuspend/.libs/libcilkrts.a -lrt -ldl -lpthread -ltcmalloc -lpthread -lm -lrt
	# Test spawns before futures, sync after touching the future
	$(CXX) -O3 -flto -g -fcilkplus -fno-omit-frame-pointer -fcilk-no-inline -DTEST_INTEROP_PRE_FUTURE_CREATE -DPRECOMPUTE_PEDIGREES=1 -I/home/kdsinger/workspace/porridge/llvm-cilk/include -I/home/kdsinger/workspace/porridge/cilkrtssuspend/runtime/config/x86 -I/home/kdsinger/workspace/porridge/cilkrtssuspend/runtime/sslib -I/home/kdsinger/workspace/porridge/cilkrtssuspend/include -std=c++11 -Wall -c handcomp_fib_cilkfut.cpp -o handcomp_fib_cilkfut.o
	$(CXX) -flto handcomp_fib_cilkfut.o ktiming.o -o handcomp_fib_cilkfut_spawn-before-sync-after /home/kdsinger/workspace/porridge/cilkrtssuspend/.libs/libcilkrts.a -lrt -ldl -lpthread -ltcmalloc -lpthread -lm -lrt
	# Test spawns before futures, sync before touching the future
	$(CXX) -O3 -flto -g -fcilkplus -fno-omit-frame-pointer -fcilk-no-inline -DTEST_INTEROP_PRE_FUTURE_CREATE -DFUTURE_AFTER_SYNC -DPRECOMPUTE_PEDIGREES=1 -I/home/kdsinger/workspace/porridge/llvm-cilk/include -I/home/kdsinger/workspace/porridge/cilkrtssuspend/runtime/config/x86 -I/home/kdsinger/workspace/porridge/cilkrtssuspend/runtime/sslib -I/home/kdsinger/workspace/porridge/cilkrtssuspend/include -std=c++11 -Wall -c handcomp_fib_cilkfut.cpp -o handcomp_fib_cilkfut.o
	$(CXX) -flto handcomp_fib_cilkfut.o ktiming.o -o handcomp_fib_cilkfut_spawn-before-sync-before /home/kdsinger/workspace/porridge/cilkrtssuspend/.libs/libcilkrts.a -lrt -ldl -lpthread -ltcmalloc -lpthread -lm -lrt
	# Test spawns after futures, sync after touching the future
	$(CXX) -O3 -flto -g -fcilkplus -fno-omit-frame-pointer -fcilk-no-inline -DTEST_INTEROP_POST_FUTURE_CREATE -DPRECOMPUTE_PEDIGREES=1 -I/home/kdsinger/workspace/porridge/llvm-cilk/include -I/home/kdsinger/workspace/porridge/cilkrtssuspend/runtime/config/x86 -I/home/kdsinger/workspace/porridge/cilkrtssuspend/runtime/sslib -I/home/kdsinger/workspace/porridge/cilkrtssuspend/include -std=c++11 -Wall -c handcomp_fib_cilkfut.cpp -o handcomp_fib_cilkfut.o
	$(CXX) -flto handcomp_fib_cilkfut.o ktiming.o -o handcomp_fib_cilkfut_spawn-after-sync-after /home/kdsinger/workspace/porridge/cilkrtssuspend/.libs/libcilkrts.a -lrt -ldl -lpthread -ltcmalloc -lpthread -lm -lrt
	# Test spawns after futures, sync before touching the future
	$(CXX) -O3 -flto -g -fcilkplus -fno-omit-frame-pointer -fcilk-no-inline -DTEST_INTEROP_POST_FUTURE_CREATE -DFUTURE_AFTER_SYNC -DPRECOMPUTE_PEDIGREES=1 -I/home/kdsinger/workspace/porridge/llvm-cilk/include -I/home/kdsinger/workspace/porridge/cilkrtssuspend/runtime/config/x86 -I/home/kdsinger/workspace/porridge/cilkrtssuspend/runtime/sslib -I/home/kdsinger/workspace/porridge/cilkrtssuspend/include -std=c++11 -Wall -c handcomp_fib_cilkfut.cpp -o handcomp_fib_cilkfut.o
	$(CXX) -flto handcomp_fib_cilkfut.o ktiming.o -o handcomp_fib_cilkfut_spawn-after-sync-before /home/kdsinger/workspace/porridge/cilkrtssuspend/.libs/libcilkrts.a -lrt -ldl -lpthread -ltcmalloc -lpthread -lm -lrt
	# Test futures working with futures
	$(CXX) -O3 -flto -g -fcilkplus -fno-omit-frame-pointer -fcilk-no-inline -DTEST_INTEROP_MULTI_FUTURE -DPRECOMPUTE_PEDIGREES=1 -I/home/kdsinger/workspace/porridge/llvm-cilk/include -I/home/kdsinger/workspace/porridge/cilkrtssuspend/runtime/config/x86 -I/home/kdsinger/workspace/porridge/cilkrtssuspend/runtime/sslib -I/home/kdsinger/workspace/porridge/cilkrtssuspend/include -std=c++11 -Wall -c handcomp_fib_cilkfut.cpp -o handcomp_fib_cilkfut.o
	$(CXX) -flto handcomp_fib_cilkfut.o ktiming.o -o handcomp_fib_cilkfut_future-interop /home/kdsinger/workspace/porridge/cilkrtssuspend/.libs/libcilkrts.a -lrt -ldl -lpthread -ltcmalloc -lpthread -lm -lrt
	./handcomp_fib_cilkfut_spawn-before-sync-after 40
	./handcomp_fib_cilkfut_spawn-before-sync-before 40
	./handcomp_fib_cilkfut_spawn-after-sync-after 40
	./handcomp_fib_cilkfut_spawn-after-sync-before 40
	./handcomp_fib_cilkfut_future-interop 40
	./handcomp_fib_cilkfut 40
	./fib_cilkplus 40
	taskset -c 8 ./fib_cilkplus 40
	taskset -c 8 ./handcomp_fib_cilkfut 40


.PHONY = clean
clean:
	rm -f *.o handcomp_fib_cilkfut fib_cilkfut fib_cilkplus
