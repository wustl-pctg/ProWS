include ../../bench.mk

DEFINES = -DOUTPUT
CFLAGS += $(APPFLAGS) $(DEFINES) $(INC) -I./AVI
CXXFLAGS += $(APPFLAGS) $(DEFINES) $(INC) -I./AVI

TARGETS = hw-$(btype)

OBJ = $(addprefix ./AVI/,avilib avimod)
INST = no
ifeq ($(btype),inst)
  INST = yes
endif
ifeq ($(btype),rd)
  INST = yes
endif
ifeq ($(INST),yes)
  OBJECTS = $(OBJ:=-inst.o)
else
  OBJECTS = $(OBJ:=-noinst.o)
endif

.PHONY: all clean hw-all
all: $(TARGETS)

hw-all:
	$(MAKE) mode=$(mode) rdalg=$(rdalg) ftype=$(ftype) btype=rd
	$(MAKE) mode=$(mode) rdalg=$(rdalg) ftype=$(ftype) btype=inst
	$(MAKE) mode=$(mode) rdalg=$(rdalg) ftype=$(ftype) btype=reach
	$(MAKE) mode=$(mode) rdalg=$(rdalg) ftype=$(ftype) btype=base

hw-%: main-%.o compute-steps-%.o $(OBJECTS)
	$(CXX) $^ $(LDFLAGS) -o $@

.SECONDARY: main-$(btype).o compute-steps-$(btype).o

main-%.o: main.cpp define.hpp
	$(CXX) $(CXXFLAGS) -I./AVI $(INC) -o $@ -c $<

compute-steps-%.o: compute-steps.cpp
	$(CXX) $(CXXFLAGS) -I./AVI $(INC) -o $@ -c $<

./AVI/avilib-%.o ./AVI/avimod-%.o:
	$(MAKE) mode=$(mode) btype=$(btype) -C AVI
#	cd AVI; make;

# delete all object files
clean:
	rm -f *.o AVI/*.o result.txt
	rm -f hw-base hw-reach hw-inst hw-rd
