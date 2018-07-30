include ../bench.mk

.PHONY: all clean reallyclean

TARGETS = merge-$(btype)

all: $(TARGETS)

INC += -I../util
CFLAGS += $(APPFLAGS) #$(RDFLAGS)
CXXFLAGS += $(APPFLAGS) #$(RDFLAGS)

merge-all:
	$(MAKE) mode=$(mode) rdalg=$(rdalg) ftype=$(ftype) btype=base
	$(MAKE) mode=$(mode) rdalg=$(rdalg) ftype=$(ftype) btype=reach
	$(MAKE) mode=$(mode) rdalg=$(rdalg) ftype=$(ftype) btype=inst
	$(MAKE) mode=$(mode) rdalg=$(rdalg) ftype=$(ftype) btype=rd

merge-%.o: merge.cpp
bintree-%.o: bintree.cpp

merge-%: merge-%.o bintree-%.o ../util/getoptions.o
	$(CXX) $^ $(LDFLAGS) $(LIB) -o $@

# delete all object files
clean:
	rm -f *.o $(TARGETS) core.* merge-*

reallyclean:
	rm -f *.o merge-base merge-reach merge-inst merge-rd ../util/*.o core.*
