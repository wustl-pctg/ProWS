# This MUST be before any includes
mkfile_path := $(abspath $(lastword $(MAKEFILE_LIST)))
mkfile_dir := $(patsubst %/,%,$(dir $(mkfile_path)))

include ../bench.mk

.PHONY: all clean reallyclean

TARGETS = bst-fj bst-gf bst-se

all: $(TARGETS)

INC += -I../util
CXXFLAGS += -g -O0 -fcilkplus -fcilk-no-inline $(INC) -std=c++11
LDFLAGS += -flto $(RTS_LIB) -lnuma

COMMON_CXXFLAGS = -g -O0 -std=c++11
COMMON_LDFLAGS += -flto -L$(mkfile_dir)/../../SuperMalloc/release/lib -lsupermalloc -lrt -ldl -lpthread -lnuma -lm

FUTURE_CXXFLAGS = $(COMMON_CXXFLAGS) -fcilkplus -fcilk-no-inline -I../../cilkrtssuspend/runtime/config/x86 -I../../cilkrtssuspend/runtime/sslib -I../../cilkrtssuspend/include
FUTURE_LDFLAGS  = ../../cilkrtssuspend/.libs/libcilkrts.a $(COMMON_LDFLAGS)

VANILLA_CXXFLAGS = $(COMMON_CXXFLAGS) -fcilkplus -fcilk-no-inline -I../../cilkplus-rts/runtime/config/x86 -I../../cilkplus-rts/runtime/sslib -I../../cilkplus-rts/include
VANILLA_LDFLAGS  = ../../cilkplus-rts/.libs/libcilkrts.a $(COMMON_LDFLAGS)

merge.o: merge.cpp

getoptions.o: ../util/getoptions.o
	$(CXX) -c ../util/getoptions.cpp -o ../util/getoptions.o

# The definition is STRUCTURED_FUTURES, but this is a misnomer
bst-fj: merge.o bintree.cpp getoptions.o
	$(CXX) $(VANILLA_CXXFLAGS) -DSTRUCTURED_FUTURES -O3 -c optimized_insert.cpp -o optimized-insert-fj.o
	$(CXX) $(VANILLA_CXXFLAGS) -DSTRUCTURED_FUTURES -c bintree.cpp -o bintree-fj.o
	$(CXX) merge.o bintree-fj.o optimized-insert-fj.o ../util/getoptions.o $(VANILLA_LDFLAGS) -o $@

bst-gf: merge.o bintree.cpp getoptions.o
	$(CXX) $(FUTURE_CXXFLAGS) -DNONBLOCKING_FUTURES -O3 -c optimized_insert.cpp -o optimized-insert-gf.o
	$(CXX) $(FUTURE_CXXFLAGS) -DNONBLOCKING_FUTURES -c bintree.cpp -o bintree-gf.o
	$(CXX) merge.o bintree-gf.o optimized-insert-gf.o ../util/getoptions.o $(FUTURE_LDFLAGS) -o $@

bst-se:
	$(CXX) $(COMMON_CXXFLAGS) -DSERIAL_ELISION=1 -O3 -c optimized_insert.cpp -o optimized-insert-se.o
	$(CXX) $(COMMON_CXXFLAGS) -DSERIAL_ELISION=1 -c merge.cpp -o bst-se.o
	$(CXX) $(COMMON_CXXFLAGS) -DSERIAL_ELISION=1 -c bintree.cpp -o bintree-se.o
	$(CXX) bst-se.o bintree-se.o optimized-insert-se.o ../util/getoptions.o $(COMMON_LDFLAGS) -o $@

# delete all object files
clean:
	rm -f *.o $(TARGETS) core.* merge-*

reallyclean:
	rm -f *.o merge-base merge-reach merge-inst merge-rd ../util/*.o core.*
