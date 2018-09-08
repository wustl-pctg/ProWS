THIS_FILE := $(lastword $(MAKEFILE_LIST))
include ../bench.mk

BENCH = lcs sw sw-se lcs-se 
ALLTARGETS = $(BENCH:=-rd) $(BENCH:=-inst) $(BENCH:=-reach) $(BENCH:=-base)
TARGETS = $(BENCH)

ifeq ($(btype),inst)
  INST = yes
endif
ifeq ($(btype),rd)
  INST = yes
endif
ifeq ($(INST),yes)
  GETOPT = ../util/getoptions-inst.o
else
  GETOPT = ../util/getoptions.o
endif

.PHONY: all clean reallyclean $(BENCH) $(BENCH:=-all)
all: $(TARGETS)

.SECONDEXPANSION:
$(BENCH:=-serial): $$@.o ../util/getoptions.o ../util/ktiming.o
	$(CXX) $^ $(LDFLAGS) -o $@

$(BENCH:=-all):
	#$(MAKE) mode=$(mode) rdalg=$(rdalg) ftype=$(ftype) btype=rd $(@:-all=)
	#$(MAKE) mode=$(mode) rdalg=$(rdalg) ftype=$(ftype) btype=inst $(@:-all=)
	#$(MAKE) mode=$(mode) rdalg=$(rdalg) ftype=$(ftype) btype=reach $(@:-all=)
	$(MAKE) mode=$(mode) rdalg=$(rdalg) ftype=$(ftype) btype=base $(@:-all=)

INC += -I../util
CFLAGS += $(APPFLAGS) $(INC)
CXXFLAGS += $(APPFLAGS) $(INC) -std=c++11
LDFLAGS += -flto $(RTS_LIB) -lnuma

.SECONDEXPANSION:
$(BENCH): $$@.o
	$(CXX) $^ ../util/ktiming.o $(GETOPT) $(LDFLAGS) -o $@

$(ALLTARGETS): $$@.o ../util/ktiming.o $(GETOPT) | $(RDLIB)
	$(CXX) $^ $(LDFLAGS) -o $@

ifeq ($(btype),all)
clean:
	rm -rf core.* $(ALLTARGETS) $(ALLTARGETS:=.o)
else
clean:
	rm -f core.* $(TARGETS) $(TARGETS:=.o)
endif

reallyclean:
	rm -f *.o ../util/*.o core.* $(ALLTARGETS)
