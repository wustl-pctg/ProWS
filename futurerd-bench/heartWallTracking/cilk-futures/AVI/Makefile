include ../../../bench.mk

# CC = $(COMPILER_HOME)/bin/clang
# CXX = $(COMPILER_HOME)/bin/clang++

BASE_OBJ = avimod.o avilib.o

INST = no
ifeq ($(btype),inst)
  INST = yes
endif
ifeq ($(btype),rd)
  INST = yes
endif
ifeq ($(INST),yes)
  OBJECTS = $(BASE_OBJ:.o=-inst.o)
else
  OBJECTS = $(BASE_OBJ:.o=-noinst.o)
endif

ifeq ($(btype),all)
all:
	$(MAKE) mode=$(mode) btype=base
	$(MAKE) mode=$(mode) btype=inst
else
all: $(OBJECTS)
endif

# all: avimod.o avilib.o

# OPT = -O3
CFLAGS += $(OUTPUT)
CXXFLAGS += $(APPFLAGS) $(OUTPUT)

%-inst.o %-noinst.o: %.cpp
	$(CXX) $(CXXFLAGS) -c -o $@ $<

# compile supporting function file into object (binary)
# avimod.o: avilib.hpp avimod.cpp
#	$(CXX) -c $(CXXLFAGS) avimod.cpp

# compile supporting function file into object (binary)
# avilib.o: avilib.hpp avilib.cpp
# 	$(CXX) -c $(CXXFLAGS) avilib.cpp

# delete files
clean:
	rm -rf *.o *.out
