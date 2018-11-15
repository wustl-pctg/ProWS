# This MUST be before any includes
mkfile_path := $(abspath $(lastword $(MAKEFILE_LIST)))
mkfile_dir := $(patsubst %/,%,$(dir $(mkfile_path)))

include ../../common.mk

COMMON_CXXFLAGS = -DTIMES_TO_RUN=10 -DTIMING_COUNT=10 -O3 -flto -g -fcilkplus -fno-omit-frame-pointer -fcilk-no-inline -I../../src -I../../llvm-cilk/include -std=c++11 -Wall
COMMON_LDFLAGS  = -L$(mkfile_dir)/../../SuperMalloc/release/lib -lsupermalloc -lrt -ldl -lpthread -lnuma -lpthread -lm -lrt

FUTURE_CXXFLAGS = $(COMMON_CXXFLAGS) -I../../cilkrtssuspend/runtime/config/x86 -I../../cilkrtssuspend/runtime/sslib -I../../cilkrtssuspend/include
FUTURE_LDFLAGS 	= ../../cilkrtssuspend/.libs/libcilkrts.a $(COMMON_LDFLAGS) 

VANILLA_CXXFLAGS = $(COMMON_CXXFLAGS) -I../../cilkplus-rts/runtime/config/x86 -I../../cilkplus-rts/runtime/sslib -I../../cilkplus-rts/include
VANILLA_LDFLAGS  = ../../cilkplus-rts/.libs/libcilkrts.a $(COMMON_LDFLAGS) 

.PHONY = default
default: all

getoptions.o: ../util/getoptions.cpp
	$(CXX) $(FUTURE_CXXFLAGS) -c ../util/getoptions.cpp -o ../util/getoptions.o

ktiming.o: ../util/ktiming.cpp
	$(CXX) -O3 -c ../util/ktiming.cpp -o ../util/ktiming.o

TARGETS = lcs-se
APPS = lcs-se

lcs-se: lcs-se.cpp ktiming.o getoptions.o
	$(CXX) $(COMMON_CXXFLAGS) -c lcs-se.cpp -o lcs-se.o
	$(CXX) -flto lcs-se.o ../util/ktiming.o ../util/getoptions.o -o lcs-se $(COMMON_LDFLAGS)

TARGETS += lcs-fj
APPS += lcs-fj

lcs-fj: lcs.cpp ktiming.o getoptions.o
	$(CXX) $(VANILLA_CXXFLAGS) -DNO_FUTURES -c lcs.cpp -o lcs-fj.o
	$(CXX) -flto lcs-fj.o ../util/ktiming.o ../util/getoptions.o -o lcs-fj $(VANILLA_LDFLAGS)

TARGETS += lcs-sf
APPS += lcs-sf

lcs-sf: lcs.cpp ktiming.o getoptions.o
	$(CXX) $(FUTURE_CXXFLAGS) -DSTRUCTURED_FUTURES -c lcs.cpp -o lcs-sf.o
	$(CXX) -flto lcs-sf.o ../util/ktiming.o ../util/getoptions.o -o lcs-sf $(FUTURE_LDFLAGS)

TARGETS += lcs-gf
APPS += lcs-gf

lcs-gf: lcs.cpp ktiming.o getoptions.o
	$(CXX) $(FUTURE_CXXFLAGS) -DNONBLOCKING_FUTURES -c lcs.cpp -o lcs-gf.o
	$(CXX) -flto lcs-gf.o ../util/ktiming.o ../util/getoptions.o -o lcs-gf $(FUTURE_LDFLAGS)

TARGETS += sw-se
APPS += sw-se

sw-se: sw-se.cpp ktiming.o getoptions.o
	$(CXX) $(COMMON_CXXFLAGS) -c sw-se.cpp -o sw-se.o
	$(CXX) -flto sw-se.o ../util/ktiming.o ../util/getoptions.o -o sw-se $(COMMON_LDFLAGS)

TARGETS += sw-fj
APPS += sw-fj

sw-fj: sw.cpp ktiming.o getoptions.o
	$(CXX) $(VANILLA_CXXFLAGS) -DNO_FUTURES -c sw.cpp -o sw.o
	$(CXX) -flto sw.o ../util/ktiming.o ../util/getoptions.o -o sw $(VANILLA_LDFLAGS)

TARGETS += sw-sf
APPS += sw-sf

sw-sf: sw.cpp ktiming.o getoptions.o
	$(CXX) $(FUTURE_CXXFLAGS) -DSTRUCTURED_FUTURES -c sw.cpp -o sw-sf.o
	$(CXX) -flto sw-sf.o ../util/ktiming.o ../util/getoptions.o -o sw-sf $(FUTURE_LDFLAGS)

TARGETS += sw-gf
APPS += sw-gf

sw-gf: sw.cpp ktiming.o getoptions.o
	$(CXX) $(FUTURE_CXXFLAGS) -DNONBLOCKING_FUTURES -c sw.cpp -o sw-gf.o
	$(CXX) -flto sw-gf.o ../util/ktiming.o ../util/getoptions.o -o sw-gf $(FUTURE_LDFLAGS)

.PHONY=all
all: $(TARGETS)

.PHONY=clean
clean:
	rm -f *.o $(APPS)
