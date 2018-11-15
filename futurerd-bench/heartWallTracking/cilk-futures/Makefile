# This MUST be before any includes
mkfile_path := $(abspath $(lastword $(MAKEFILE_LIST)))
mkfile_dir := $(patsubst %/,%,$(dir $(mkfile_path)))

include ../../bench.mk

DEFINES = -DOUTPUT
#CFLAGS += $(APPFLAGS) $(DEFINES) $(INC) -I./AVI
#CXXFLAGS += $(APPFLAGS) $(DEFINES) $(INC) -I./AVI -std=c++11
LDFLAGS += -flto -lnuma -L$(mkfile_dir)/../../../SuperMalloc/release/lib -lsupermalloc

CXXFLAGS = -g -fno-omit-frame-pointer -mno-omit-leaf-frame-pointer -O3 -flto -DOUTPUT -I./AVI -std=c++11 -Wall
#CFLAGS = -g -fno-omit-frame-pointer -mno-omit-leaf-frame-pointer -O3 -flto

VANILLA_CXXFLAGS = $(CXXFLAGS) -fcilkplus -I../../../cilkplus-rts/include
VANILLA_LDFLAGS = $(LDFLAGS) ../../../cilkplus-rts/.libs/libcilkrts.a

FUTURE_CXXFLAGS = $(CXXFLAGS) -fcilkplus -I../../../cilkrtssuspend/include
FUTURE_LDFLAGS = $(LDFLAGS) ../../../cilkrtssuspend/.libs/libcilkrts.a


TARGETS = hw-fj hw-sf hw-gf hw-se

OBJ = $(addprefix ./AVI/,avilib avimod)
INST = no
ifeq ($(btype),inst)
  INST = yes
endif
ifeq ($(btype),rd)
  INST = yes
endif
#ifeq ($(INST),yes)
#  OBJECTS = $(OBJ:=-inst.o)
#else
  OBJECTS = $(OBJ:=-noinst.o)
#endif

.PHONY: all clean hw-all
all: $(TARGETS)

#hw: main.o compute-steps.o $(OBJECTS)
#	$(CXX) $^ $(LDFLAGS) -o $@

hw-fj: main.cpp compute-steps.cpp $(OBJECTS)
	$(CXX) $(VANILLA_CXXFLAGS) -DNO_FUTURES -c main.cpp -o main-fj.o
	$(CXX) $(VANILLA_CXXFLAGS) -DNO_FUTURES -c compute-steps.cpp -o compute-steps-fj.o
	$(CXX) main-fj.o compute-steps-fj.o $(OBJECTS) $(VANILLA_LDFLAGS) -o $@

hw-sf: main.cpp compute-steps.cpp $(OBJECTS)
	$(CXX) $(FUTURE_CXXFLAGS) -DSTRUCTURED_FUTURES -c main.cpp -o main-sf.o
	$(CXX) $(FUTURE_CXXFLAGS) -DSTRUCTURED_FUTURES -c compute-steps.cpp -o compute-steps-sf.o
	$(CXX) main-sf.o compute-steps-sf.o $(OBJECTS) $(FUTURE_LDFLAGS) -o $@

hw-gf: main.cpp compute-steps.cpp $(OBJECTS)
	$(CXX) $(FUTURE_CXXFLAGS) -DNONBLOCKING_FUTURES -c main.cpp -o main-gf.o
	$(CXX) $(FUTURE_CXXFLAGS) -DNONBLOCKING_FUTURES -c compute-steps.cpp -o compute-steps-gf.o
	$(CXX) main-gf.o compute-steps-gf.o $(OBJECTS) $(FUTURE_LDFLAGS) -o $@

hw-se: main.cpp compute-steps.cpp $(OBJECTS)
	$(CXX) $(CXXFLAGS) -DSERIAL_ELISION -c main.cpp -o main-se.o
	$(CXX) $(CXXFLAGS) -DSERIAL_ELISION -c compute-steps.cpp -o compute-steps-se.o
	$(CXX) main-se.o compute-steps-se.o $(OBJECTS) $(LDFLAGS) -o $@

.SECONDARY: main-$(btype).o compute-steps-$(btype).o

#main.o: main.cpp define.hpp
#	$(CXX) $(CXXFLAGS) -I./AVI $(INC) -o $@ -c $<

#compute-steps.o: compute-steps.cpp
#	$(CXX) $(CXXFLAGS) -I./AVI $(INC) -o $@ -c $<

./AVI/avilib-%.o ./AVI/avimod-%.o:
	$(MAKE) mode=$(mode) btype=$(btype) -C AVI
#	cd AVI; make;

# delete all object files
clean:
	rm -f *.o AVI/*.o result.txt
	rm -f hw-*
