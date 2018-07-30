THIS_FILE := $(lastword $(MAKEFILE_LIST))
include ../bench.mk

BENCH = lcs sw matmul_z
ALLTARGETS = $(BENCH:=-rd) $(BENCH:=-inst) $(BENCH:=-reach) $(BENCH:=-base)
TARGETS = $(BENCH:=-$(btype))

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
$(BENCH:=-serial): $$@.o ../util/getoptions.o
	$(CXX) $^ $(LDFLAGS) -o $@

$(BENCH:=-all):
	$(MAKE) mode=$(mode) rdalg=$(rdalg) ftype=$(ftype) btype=rd $(@:-all=)
	$(MAKE) mode=$(mode) rdalg=$(rdalg) ftype=$(ftype) btype=inst $(@:-all=)
	$(MAKE) mode=$(mode) rdalg=$(rdalg) ftype=$(ftype) btype=reach $(@:-all=)
	$(MAKE) mode=$(mode) rdalg=$(rdalg) ftype=$(ftype) btype=base $(@:-all=)

INC += -I../util
CFLAGS += $(APPFLAGS)
CXXFLAGS += $(APPFLAGS)

.SECONDEXPANSION:
$(BENCH): $$@-$(btype)
$(ALLTARGETS): $$@.o $(GETOPT) | $(RDLIB)
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
