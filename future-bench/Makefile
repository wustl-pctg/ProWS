###########################################################################
# NOTE: Thou shalt add thine build targets to the TARGETS designator, and #
#       thou shalt add thine resulting executables to the APPS designator,#
#       lest thou desire to be charged with polluting the systems of      #
#       thine fellows.                                                    #
###########################################################################

# This MUST be before any includes
mkfile_path := $(abspath $(lastword $(MAKEFILE_LIST)))
mkfile_dir := $(patsubst %/,%,$(dir $(mkfile_path)))

include ../common.mk

MY_CXXFLAGS = -O3 -flto -g -fcilkplus -fno-omit-frame-pointer -fcilk-no-inline -DPRECOMPUTE_PEDIGREES=1 -I../src -I../llvm-cilk/include -I../cilkrtssuspend/runtime/config/x86 -I../cilkrtssuspend/runtime/sslib -I../cilkrtssuspend/include -std=c++11 -Wall
MY_CFLAGS = -O3 -g
MY_LDFLAGS = ../cilkrtssuspend/.libs/libcilkrts.a -L$(mkfile_dir)/../SuperMalloc/release/lib -lsupermalloc -lrt -ldl -lpthread -lnuma -lpthread -lm -lrt 

TARGETS = default
APPS = fib_cilkplus

.PHONY = default

default: ktiming.o
	$(CXX) $(MY_CXXFLAGS) -c fib_cilkplus.cpp -o fib_cilkplus.o
	$(CXX) -flto fib_cilkplus.o ktiming.o -o fib_cilkplus $(MY_LDFLAGS)

getoptions.o: getoptions.cpp
	$(CXX) $(MY_CXXFLAGS) -c getoptions.cpp -o getoptions.o

ktiming.o: ktiming.c
	$(CXX) $(MY_CXXFLAGS) -c ktiming.c -o ktiming.o

TARGETS += fib-fut
APPS += fib_cilkfut

fib-fut: ktiming.o
	$(CXX) $(MY_CXXFLAGS) -c fib_cilkfut.cpp -o fib_cilkfut.o
	$(CXX) -flto fib_cilkfut.o ktiming.o -o fib_cilkfut $(MY_LDFLAGS)

TARGETS += fib-handcomp
APPS += handcomp_fib_cilkfut

fib-handcomp: ktiming.o
	$(CXX) $(MY_CXXFLAGS) -c handcomp_fib_cilkfut.cpp -o handcomp_fib_cilkfut.o
	$(CXX) -flto handcomp_fib_cilkfut.o ktiming.o -o handcomp_fib_cilkfut $(MY_LDFLAGS)

APPS += handcomp_fib_cilkfut_nofibers

TARGETS += fib-handcomp-nofibers
fib-handcomp-nofibers: ktiming.o
	$(CXX) $(MY_CXXFLAGS) -c handcomp_fib_cilkfut_nofibers.cpp -o handcomp_fib_cilkfut_nofibers.o
	$(CXX) -flto handcomp_fib_cilkfut_nofibers.o ktiming.o -o handcomp_fib_cilkfut_nofibers $(MY_LDFLAGS)

TARGETS += fib-interop-tests
APPS += handcomp_fib_cilkfut_spawn-after-sync-after handcomp_fib_cilkfut_spawn-after-sync-before handcomp_fib_cilkfut_spawn-before-sync-after handcomp_fib_cilkfut_spawn-before-sync-before handcomp_fib_cilkfut_future-interop  handcomp_fib_cilkfut_nofibers

fib-interop-tests: ktiming.o default fib-handcomp
	# Test spawns before futures, sync after touching the future
	$(CXX) -DTEST_INTEROP_PRE_FUTURE_CREATE $(MY_CXXFLAGS) -c handcomp_fib_cilkfut.cpp -o handcomp_fib_cilkfut.o
	$(CXX) -flto handcomp_fib_cilkfut.o ktiming.o -o handcomp_fib_cilkfut_spawn-before-sync-after $(MY_LDFLAGS)
	# Test spawns before futures, sync before touching the future
	$(CXX) -DTEST_INTEROP_PRE_FUTURE_CREATE -DFUTURE_AFTER_SYNC $(MY_CXXFLAGS) -c handcomp_fib_cilkfut.cpp -o handcomp_fib_cilkfut.o
	$(CXX) -flto handcomp_fib_cilkfut.o ktiming.o -o handcomp_fib_cilkfut_spawn-before-sync-before $(MY_LDFLAGS)
	# Test spawns after futures, sync after touching the future
	$(CXX) -DTEST_INTEROP_POST_FUTURE_CREATE $(MY_CXXFLAGS) -c handcomp_fib_cilkfut.cpp -o handcomp_fib_cilkfut.o
	$(CXX) -flto handcomp_fib_cilkfut.o ktiming.o -o handcomp_fib_cilkfut_spawn-after-sync-after $(MY_LDFLAGS)
	# Test spawns after futures, sync before touching the future
	$(CXX) -DTEST_INTEROP_POST_FUTURE_CREATE -DFUTURE_AFTER_SYNC $(MY_CXXFLAGS) -c handcomp_fib_cilkfut.cpp -o handcomp_fib_cilkfut.o
	$(CXX) -flto handcomp_fib_cilkfut.o ktiming.o -o handcomp_fib_cilkfut_spawn-after-sync-before $(MY_LDFLAGS)
	# Test futures working with futures
	$(CXX) -DTEST_INTEROP_MULTI_FUTURE $(MY_CXXFLAGS) -c handcomp_fib_cilkfut.cpp -o handcomp_fib_cilkfut.o
	$(CXX) -flto handcomp_fib_cilkfut.o ktiming.o -o handcomp_fib_cilkfut_future-interop $(MY_LDFLAGS)
	make run-tests

run-tests:
	LD_LIBRARY_PATH=$(mkfile_dir)/../SuperMalloc/release/lib ./handcomp_fib_cilkfut_spawn-before-sync-after 40
	LD_LIBRARY_PATH=$(mkfile_dir)/../SuperMalloc/release/lib ./handcomp_fib_cilkfut_spawn-before-sync-before 40
	LD_LIBRARY_PATH=$(mkfile_dir)/../SuperMalloc/release/lib ./handcomp_fib_cilkfut_spawn-after-sync-after 40
	LD_LIBRARY_PATH=$(mkfile_dir)/../SuperMalloc/release/lib ./handcomp_fib_cilkfut_spawn-after-sync-before 40
	LD_LIBRARY_PATH=$(mkfile_dir)/../SuperMalloc/release/lib ./handcomp_fib_cilkfut_future-interop 40
	LD_LIBRARY_PATH=$(mkfile_dir)/../SuperMalloc/release/lib ./handcomp_fib_cilkfut 40
	LD_LIBRARY_PATH=$(mkfile_dir)/../SuperMalloc/release/lib taskset -c 8 ./handcomp_fib_cilkfut 40

TARGETS += strass
APPS += strassen

strass: ktiming.o getoptions.o strassen.cpp
	$(CXX) $(MY_CXXFLAGS) -c strassen.cpp -o strassen.o
	$(CXX) -flto strassen.o getoptions.o ktiming.o -o strassen $(MY_LDFLAGS)
	make run-strass

run-strass:
	LD_LIBRARY_PATH=$(mkfile_dir)/../SuperMalloc/release/lib ./strassen -n 1024 -c

TARGETS += strass-fut
APPS += strassen-future
    
strass-fut: ktiming.o getoptions.o strassen-future.cpp
	$(CXX) $(MY_CXXFLAGS) -c strassen-future.cpp -o strassen-future.o
	$(CXX) -flto strassen-future.o getoptions.o ktiming.o -o strassen-future $(MY_LDFLAGS)
	make run-strass-fut

run-strass-fut:
	LD_LIBRARY_PATH=$(mkfile_dir)/../SuperMalloc/release/lib ./strassen-future -n 1024 -c

TARGETS += matmul
APPS += matmul

matmul: ktiming.o getoptions.o
	$(CXX) $(MY_CXXFLAGS) -c matmul.cpp -o matmul.o
	$(CXX) -flto matmul.o getoptions.o ktiming.o -o matmul $(MY_LDFLAGS)

TARGETS += matmul-future
APPS += matmul-future

matmul-future: ktiming.o getoptions.o
	$(CXX) $(MY_CXXFLAGS) -c matmul-future.cpp -o matmul-future.o
	$(CXX) -flto matmul-future.o getoptions.o ktiming.o -o matmul-future $(MY_LDFLAGS)

###########################################################################
# Though shalt not cross this line lest thou knowest what thou art doing! #
###########################################################################

.PHONE = all
all: $(TARGETS)

.PHONY = clean
clean:
	rm -f *.o $(APPS)
