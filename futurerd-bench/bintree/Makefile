include ../bench.mk

.PHONY: all clean reallyclean

TARGETS = merge

all: $(TARGETS)

INC += -I../util
CXXFLAGS += -g -O0 -fcilkplus -fcilk-no-inline $(INC) -std=c++11
LDFLAGS += -flto $(RTS_LIB) -lnuma

merge-all:
	#$(MAKE) mode=$(mode) rdalg=$(rdalg) ftype=$(ftype) btype=base
	#$(MAKE) mode=$(mode) rdalg=$(rdalg) ftype=$(ftype) btype=reach
	#$(MAKE) mode=$(mode) rdalg=$(rdalg) ftype=$(ftype) btype=inst
	$(MAKE) mode=$(mode) rdalg=$(rdalg) ftype=$(ftype) btype=rd

merge.o: merge.cpp
bintree.o: bintree.cpp

merge: merge.o bintree.o ../util/getoptions.o
	$(CXX) $^ $(LDFLAGS) $(LIB) -o $@

merge-serial:
	$(CXX) $(CXXFLAGS) -DSERIAL_ELISION=1 -c merge.cpp -o merge-se.o
	$(CXX) $(CXXFLAGS) -DSERIAL_ELISION=1 -c bintree.cpp -o bintree-se.o
	$(CXX) merge-se.o bintree-se.o ../util/getoptions.o $(LDFLAGS) $(LIB) -o $@

# delete all object files
clean:
	rm -f *.o $(TARGETS) core.* merge-*

reallyclean:
	rm -f *.o merge-base merge-reach merge-inst merge-rd ../util/*.o core.*
