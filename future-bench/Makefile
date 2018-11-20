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

COMMON_CXXFLAGS = -DTIMES_TO_RUN=10 -DTIMING_COUNT=10 -O3 -flto -g -fcilkplus -fno-omit-frame-pointer -fcilk-no-inline -I../src -I../llvm-cilk/include -std=c++11 -Wall
COMMON_LDFLAGS  = -L$(mkfile_dir)/../SuperMalloc/release/lib -lsupermalloc -lrt -ldl -lpthread -lnuma -lpthread -lm -lrt

FUTURE_CXXFLAGS = $(COMMON_CXXFLAGS) -I../cilkrtssuspend/runtime/config/x86 -I../cilkrtssuspend/runtime/sslib -I../cilkrtssuspend/include
FUTURE_LDFLAGS 	= ../cilkrtssuspend/.libs/libcilkrts.a $(COMMON_LDFLAGS) 

VANILLA_CXXFLAGS = $(COMMON_CXXFLAGS) -I../cilkplus-rts/runtime/config/x86 -I../cilkplus-rts/runtime/sslib -I../cilkplus-rts/include
VANILLA_LDFLAGS  = ../cilkplus-rts/.libs/libcilkrts.a $(COMMON_LDFLAGS) 

.PHONY: default all clean
default: all

getoptions.o: getoptions.cpp
	$(CXX) $(FUTURE_CXXFLAGS) -c getoptions.cpp -o getoptions.o

ktiming.o: ktiming.c
	$(CXX) -O3 -c ktiming.c -o ktiming.o

TARGETS = fib-se
APPS = fib-se

fib-se: ktiming.o
	$(CXX) $(FUTURE_CXXFLAGS) -c fib_cilkplus_se.cpp -o fib_se.o
	$(CXX) -flto fib_se.o ktiming.o -o fib-se $(FUTURE_LDFLAGS)

TARGETS += fib
APPS += fib

fib: ktiming.o
	$(CXX) $(VANILLA_CXXFLAGS) -c fib_cilkplus.cpp -o fib.o
	$(CXX) -flto fib.o ktiming.o -o fib $(VANILLA_LDFLAGS)

TARGETS += fib-fj
APPS += fib-fj

fib-fj: ktiming.o
	$(CXX) $(VANILLA_CXXFLAGS) -c fib_cilkplus-handcomp.cpp -o fib-fj.o
	$(CXX) -flto fib-fj.o ktiming.o -o fib-fj $(VANILLA_LDFLAGS)

TARGETS += fib-future-macro
APPS += fib-future-macro

fib-future-macro: ktiming.o
	$(CXX) $(FUTURE_CXXFLAGS) -c fib_cilkfut.cpp -o fib-future-macro.o
	$(CXX) -flto fib-future-macro.o ktiming.o -o fib-future-macro $(FUTURE_LDFLAGS)

TARGETS += fib-sf
APPS += fib-sf

fib-sf: ktiming.o
	$(CXX) $(FUTURE_CXXFLAGS) -c handcomp_fib_cilkfut.cpp -o fib-sf.o
	$(CXX) -flto fib-sf.o ktiming.o -o fib-sf $(FUTURE_LDFLAGS)

APPS += fib-sf-stack
TARGETS += fib-sf-stack

fib-sf-stack: ktiming.o
	$(CXX) $(FUTURE_CXXFLAGS) -c handcomp_fib_cilkfut_nofibers.cpp -o fib-sf-stack.o
	$(CXX) -flto fib-sf-stack.o ktiming.o -o fib-sf-stack $(FUTURE_LDFLAGS)

TARGETS += fib-interop-tests
APPS += handcomp_fib_cilkfut_spawn-after-sync-after handcomp_fib_cilkfut_spawn-after-sync-before handcomp_fib_cilkfut_spawn-before-sync-after handcomp_fib_cilkfut_spawn-before-sync-before handcomp_fib_cilkfut_future-interop  handcomp_fib_cilkfut_nofibers

fib-interop-tests: ktiming.o fib fib-future
	# Test spawns before futures, sync after touching the future
	$(CXX) -DTEST_INTEROP_PRE_FUTURE_CREATE $(FUTURE_CXXFLAGS) -c handcomp_fib_cilkfut.cpp -o handcomp_fib_cilkfut.o
	$(CXX) -flto handcomp_fib_cilkfut.o ktiming.o -o handcomp_fib_cilkfut_spawn-before-sync-after $(FUTURE_LDFLAGS)
	# Test spawns before futures, sync before touching the future
	$(CXX) -DTEST_INTEROP_PRE_FUTURE_CREATE -DFUTURE_AFTER_SYNC $(FUTURE_CXXFLAGS) -c handcomp_fib_cilkfut.cpp -o handcomp_fib_cilkfut.o
	$(CXX) -flto handcomp_fib_cilkfut.o ktiming.o -o handcomp_fib_cilkfut_spawn-before-sync-before $(FUTURE_LDFLAGS)
	# Test spawns after futures, sync after touching the future
	$(CXX) -DTEST_INTEROP_POST_FUTURE_CREATE $(FUTURE_CXXFLAGS) -c handcomp_fib_cilkfut.cpp -o handcomp_fib_cilkfut.o
	$(CXX) -flto handcomp_fib_cilkfut.o ktiming.o -o handcomp_fib_cilkfut_spawn-after-sync-after $(FUTURE_LDFLAGS)
	# Test spawns after futures, sync before touching the future
	$(CXX) -DTEST_INTEROP_POST_FUTURE_CREATE -DFUTURE_AFTER_SYNC $(FUTURE_CXXFLAGS) -c handcomp_fib_cilkfut.cpp -o handcomp_fib_cilkfut.o
	$(CXX) -flto handcomp_fib_cilkfut.o ktiming.o -o handcomp_fib_cilkfut_spawn-after-sync-before $(FUTURE_LDFLAGS)
	# Test futures working with futures
	$(CXX) -DTEST_INTEROP_MULTI_FUTURE $(FUTURE_CXXFLAGS) -c handcomp_fib_cilkfut.cpp -o handcomp_fib_cilkfut.o
	$(CXX) -flto handcomp_fib_cilkfut.o ktiming.o -o handcomp_fib_cilkfut_future-interop $(FUTURE_LDFLAGS)
	#make run-tests

run-tests:
	LD_LIBRARY_PATH=$(mkfile_dir)/../SuperMalloc/release/lib ./handcomp_fib_cilkfut_spawn-before-sync-after 40
	LD_LIBRARY_PATH=$(mkfile_dir)/../SuperMalloc/release/lib ./handcomp_fib_cilkfut_spawn-before-sync-before 40
	LD_LIBRARY_PATH=$(mkfile_dir)/../SuperMalloc/release/lib ./handcomp_fib_cilkfut_spawn-after-sync-after 40
	LD_LIBRARY_PATH=$(mkfile_dir)/../SuperMalloc/release/lib ./handcomp_fib_cilkfut_spawn-after-sync-before 40
	LD_LIBRARY_PATH=$(mkfile_dir)/../SuperMalloc/release/lib ./handcomp_fib_cilkfut_future-interop 40
	LD_LIBRARY_PATH=$(mkfile_dir)/../SuperMalloc/release/lib ./fib-future 40
	LD_LIBRARY_PATH=$(mkfile_dir)/../SuperMalloc/release/lib taskset -c 8 ./fib-future 40

TARGETS += smm-se
APPS += smm-se

smm-se: ktiming.o getoptions.o strassen.cpp
	$(CXX) $(FUTURE_CXXFLAGS) -c strassen-se.cpp -o smm-se.o
	$(CXX) -flto smm-se.o getoptions.o ktiming.o -o smm-se $(FUTURE_LDFLAGS)

TARGETS += strass
APPS += strassen

strass: ktiming.o getoptions.o strassen.cpp
	$(CXX) $(VANILLA_CXXFLAGS) -c strassen.cpp -o strassen.o
	$(CXX) -flto strassen.o getoptions.o ktiming.o -o strassen $(FUTURE_LDFLAGS)
	#make run-strass

run-strass:
	LD_LIBRARY_PATH=$(mkfile_dir)/../SuperMalloc/release/lib ./strassen -n 1024 -c

TARGETS += smm-fj
APPS += smm-fj

smm-fj: ktiming.o getoptions.o strassen.cpp
	$(CXX) $(VANILLA_CXXFLAGS) -c strassen-handcomp.cpp -o smm-fj.o
	$(CXX) -flto smm-fj.o getoptions.o ktiming.o -o smm-fj $(VANILLA_LDFLAGS)

TARGETS += smm-sf
APPS += smm-sf
    
smm-sf: ktiming.o getoptions.o strassen-future.cpp
	$(CXX) $(FUTURE_CXXFLAGS) -c strassen-future.cpp -o smm-sf.o
	$(CXX) -flto smm-sf.o getoptions.o ktiming.o -o smm-sf $(FUTURE_LDFLAGS)
	#make run-strass-fut

run-strass-fut:
	LD_LIBRARY_PATH=$(mkfile_dir)/../SuperMalloc/release/lib ./strassen-future -n 1024 -c

TARGETS += strass-fut-nofibers
APPS += strassen-future-nofibers
    
strass-fut-nofibers: ktiming.o getoptions.o strassen-future-nofibers.cpp
	$(CXX) $(FUTURE_CXXFLAGS) -c strassen-future-nofibers.cpp -o strassen-future-nofibers.o
	$(CXX) -flto strassen-future-nofibers.o getoptions.o ktiming.o -o strassen-future-nofibers $(FUTURE_LDFLAGS)
	#make run-strass-fut-nofibers

run-strass-fut-nofibers:
	LD_LIBRARY_PATH=$(mkfile_dir)/../SuperMalloc/release/lib ./strassen-future-nofibers -n 1024 -c

TARGETS += mm-se
APPS += mm-se

mm-se: matmul-se.cpp ktiming.o getoptions.o
	$(CXX) $(FUTURE_CXXFLAGS) -c matmul-se.cpp -o mm-se.o
	$(CXX) -flto mm-se.o getoptions.o ktiming.o -o mm-se $(FUTURE_LDFLAGS)

TARGETS += matmul
APPS += matmul

matmul: matmul.cpp ktiming.o getoptions.o
	$(CXX) $(VANILLA_CXXFLAGS) -c matmul.cpp -o matmul.o
	$(CXX) -flto matmul.o getoptions.o ktiming.o -o matmul $(VANILLA_LDFLAGS)
	##make run-matmul

run-matmul:
	LD_LIBRARY_PATH=$(mkfile_dir)/../SuperMalloc/release/lib ./matmul -n 1024 -c

TARGETS += mm-fj
APPS += mm-fj

mm-fj: matmul-handcomp.cpp ktiming.o getoptions.o
	$(CXX) $(VANILLA_CXXFLAGS) -c matmul-handcomp.cpp -o mm-fj.o
	$(CXX) -flto mm-fj.o getoptions.o ktiming.o -o mm-fj $(VANILLA_LDFLAGS)
	##make run-matmul-handcomp

run-matmul-handcomp:
	LD_LIBRARY_PATH=$(mkfile_dir)/../SuperMalloc/release/lib ./matmul-handcomp -n 1024 -c

TARGETS += mm-sf
APPS += mm-sf

mm-sf: matmul-future.cpp ktiming.o getoptions.o
	$(CXX) $(FUTURE_CXXFLAGS) -c matmul-future.cpp -o mm-sf.o
	$(CXX) -flto mm-sf.o getoptions.o ktiming.o -o mm-sf $(FUTURE_LDFLAGS)
	##make run-matmul-future

run-matmul-future:
	LD_LIBRARY_PATH=$(mkfile_dir)/../SuperMalloc/release/lib ./matmul-future -n 1024 -c

TARGETS += sort-se
APPS += sort-se

sort-se: cilksort-se.cpp ktiming.o getoptions.o
	$(CXX) $(FUTURE_CXXFLAGS) -c cilksort-se.cpp -o sort-se.o
	$(CXX) -flto sort-se.o getoptions.o ktiming.o -o sort-se $(FUTURE_LDFLAGS)

TARGETS += cilksort
APPS += cilksort

cilksort: cilksort.cpp ktiming.o getoptions.o
	$(CXX) $(VANILLA_CXXFLAGS) -c cilksort.cpp -o cilksort.o
	$(CXX) -flto cilksort.o getoptions.o ktiming.o -o cilksort $(VANILLA_LDFLAGS)

TARGETS += sort-fj
APPS += sort-fj

sort-fj: cilksort-handcomp.cpp ktiming.o getoptions.o
	$(CXX) $(VANILLA_CXXFLAGS) -c cilksort-handcomp.cpp -o sort-fj.o
	$(CXX) -flto sort-fj.o getoptions.o ktiming.o -o sort-fj $(VANILLA_LDFLAGS)

TARGETS += sort-sf
APPS += sort-sf

sort-sf: cilksort-future.cpp ktiming.o getoptions.o
	$(CXX) $(FUTURE_CXXFLAGS) -c cilksort-future.cpp -o sort-sf.o
	$(CXX) -flto sort-sf.o getoptions.o ktiming.o -o sort-sf $(FUTURE_LDFLAGS)

###########################################################################
# Though shalt not cross this line lest thou knowest what thou art doing! #
###########################################################################

all: $(TARGETS)

clean:
	rm -f *.o $(APPS)
