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

.PHONY = default
default: all

getoptions.o: getoptions.cpp
	$(CXX) $(MY_CXXFLAGS) -c getoptions.cpp -o getoptions.o

ktiming.o: ktiming.c
	$(CXX) $(MY_CXXFLAGS) -c ktiming.c -o ktiming.o

TARGETS = fib
APPS = fib

fib: ktiming.o
	$(CXX) $(MY_CXXFLAGS) -c fib_cilkplus.cpp -o fib.o
	$(CXX) -flto fib.o ktiming.o -o fib $(MY_LDFLAGS)

TARGETS += fib-handcomp
APPS += fib-handcomp

fib-handcomp: ktiming.o
	$(CXX) $(MY_CXXFLAGS) -c fib_cilkplus-handcomp.cpp -o fib-handcomp.o
	$(CXX) -flto fib-handcomp.o ktiming.o -o fib-handcomp $(MY_LDFLAGS)

TARGETS += fib-future-macro
APPS += fib-future-macro

fib-future-macro: ktiming.o
	$(CXX) $(MY_CXXFLAGS) -c fib_cilkfut.cpp -o fib-future-macro.o
	$(CXX) -flto fib-future-macro.o ktiming.o -o fib-future-macro $(MY_LDFLAGS)

TARGETS += fib-future
APPS += fib-future

fib-future: ktiming.o
	$(CXX) $(MY_CXXFLAGS) -c handcomp_fib_cilkfut.cpp -o fib-future.o
	$(CXX) -flto fib-future.o ktiming.o -o fib-future $(MY_LDFLAGS)

APPS += fib-future-nofibers
TARGETS += fib-future-nofibers

fib-future-nofibers: ktiming.o
	$(CXX) $(MY_CXXFLAGS) -c handcomp_fib_cilkfut_nofibers.cpp -o fib-future-nofibers.o
	$(CXX) -flto fib-future-nofibers.o ktiming.o -o fib-future-nofibers $(MY_LDFLAGS)

TARGETS += fib-interop-tests
APPS += handcomp_fib_cilkfut_spawn-after-sync-after handcomp_fib_cilkfut_spawn-after-sync-before handcomp_fib_cilkfut_spawn-before-sync-after handcomp_fib_cilkfut_spawn-before-sync-before handcomp_fib_cilkfut_future-interop  handcomp_fib_cilkfut_nofibers

fib-interop-tests: ktiming.o fib fib-future
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
	LD_LIBRARY_PATH=$(mkfile_dir)/../SuperMalloc/release/lib ./fib-future 40
	LD_LIBRARY_PATH=$(mkfile_dir)/../SuperMalloc/release/lib taskset -c 8 ./fib-future 40

TARGETS += strass
APPS += strassen

strass: ktiming.o getoptions.o strassen.cpp
	$(CXX) $(MY_CXXFLAGS) -c strassen.cpp -o strassen.o
	$(CXX) -flto strassen.o getoptions.o ktiming.o -o strassen $(MY_LDFLAGS)
	make run-strass

run-strass:
	LD_LIBRARY_PATH=$(mkfile_dir)/../SuperMalloc/release/lib ./strassen -n 1024 -c

TARGETS += strass-handcomp
APPS += strassen-handcomp

strass-handcomp: ktiming.o getoptions.o strassen.cpp
	$(CXX) $(MY_CXXFLAGS) -c strassen-handcomp.cpp -o strassen-handcomp.o
	$(CXX) -flto strassen-handcomp.o getoptions.o ktiming.o -o strassen-handcomp $(MY_LDFLAGS)

TARGETS += strass-fut
APPS += strassen-future
    
strass-fut: ktiming.o getoptions.o strassen-future.cpp
	$(CXX) $(MY_CXXFLAGS) -c strassen-future.cpp -o strassen-future.o
	$(CXX) -flto strassen-future.o getoptions.o ktiming.o -o strassen-future $(MY_LDFLAGS)
	make run-strass-fut

run-strass-fut:
	LD_LIBRARY_PATH=$(mkfile_dir)/../SuperMalloc/release/lib ./strassen-future -n 1024 -c

TARGETS += strass-fut-nofibers
APPS += strassen-future-nofibers
    
strass-fut-nofibers: ktiming.o getoptions.o strassen-future-nofibers.cpp
	$(CXX) $(MY_CXXFLAGS) -c strassen-future-nofibers.cpp -o strassen-future-nofibers.o
	$(CXX) -flto strassen-future-nofibers.o getoptions.o ktiming.o -o strassen-future-nofibers $(MY_LDFLAGS)
	make run-strass-fut-nofibers

run-strass-fut-nofibers:
	LD_LIBRARY_PATH=$(mkfile_dir)/../SuperMalloc/release/lib ./strassen-future-nofibers -n 1024 -c

TARGETS += matmul
APPS += matmul

matmul: matmul.cpp ktiming.o getoptions.o
	$(CXX) $(MY_CXXFLAGS) -c matmul.cpp -o matmul.o
	$(CXX) -flto matmul.o getoptions.o ktiming.o -o matmul $(MY_LDFLAGS)
	#make run-matmul

run-matmul:
	LD_LIBRARY_PATH=$(mkfile_dir)/../SuperMalloc/release/lib ./matmul -n 1024 -c

TARGETS += matmul-handcomp
APPS += matmul-handcomp

matmul-handcomp: matmul-handcomp.cpp ktiming.o getoptions.o
	$(CXX) $(MY_CXXFLAGS) -c matmul-handcomp.cpp -o matmul-handcomp.o
	$(CXX) -flto matmul-handcomp.o getoptions.o ktiming.o -o matmul-handcomp $(MY_LDFLAGS)
	#make run-matmul-handcomp

run-matmul-handcomp:
	LD_LIBRARY_PATH=$(mkfile_dir)/../SuperMalloc/release/lib ./matmul-handcomp -n 1024 -c

TARGETS += matmul-future
APPS += matmul-future

matmul-future: matmul-future.cpp ktiming.o getoptions.o
	$(CXX) $(MY_CXXFLAGS) -c matmul-future.cpp -o matmul-future.o
	$(CXX) -flto matmul-future.o getoptions.o ktiming.o -o matmul-future $(MY_LDFLAGS)
	#make run-matmul-future

run-matmul-future:
	LD_LIBRARY_PATH=$(mkfile_dir)/../SuperMalloc/release/lib ./matmul-future -n 1024 -c

TARGETS += cilksort
APPS += cilksort

cilksort: cilksort.cpp ktiming.o getoptions.o
	$(CXX) $(MY_CXXFLAGS) -c cilksort.cpp -o cilksort.o
	$(CXX) -flto cilksort.o getoptions.o ktiming.o -o cilksort $(MY_LDFLAGS)

TARGETS += cilksort-handcomp
APPS += cilksort-handcomp

cilksort-handcomp: cilksort-handcomp.cpp ktiming.o getoptions.o
	$(CXX) $(MY_CXXFLAGS) -c cilksort-handcomp.cpp -o cilksort-handcomp.o
	$(CXX) -flto cilksort-handcomp.o getoptions.o ktiming.o -o cilksort-handcomp $(MY_LDFLAGS)

TARGETS += cilksort-future
APPS += cilksort-future

cilksort-future: cilksort-future.cpp ktiming.o getoptions.o
	$(CXX) $(MY_CXXFLAGS) -c cilksort-future.cpp -o cilksort-future.o
	$(CXX) -flto cilksort-future.o getoptions.o ktiming.o -o cilksort-future $(MY_LDFLAGS)

###########################################################################
# Though shalt not cross this line lest thou knowest what thou art doing! #
###########################################################################

.PHONE = all
all: $(TARGETS)

.PHONY = clean
clean:
	rm -f *.o $(APPS)
