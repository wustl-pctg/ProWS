# This file defins BASIC_CFLAGS, BASIC_CXXFLAGS, LDFLAGS, LDLIBS, TOOL_FLAGS, TOOL_LDFLAGS
# Makefile including this file should use these flags

CURR := $(shell dirname $(realpath $(lastword $(MAKEFILE_LIST))))
PORR_HOME = $(CURR)/..
BENCH_DIR = $(PORR_HOME)/bench
COMPILER_HOME = $(PORR_HOME)/llvm-cilk
CILKRTS_HOME = $(PORR_HOME)/llvm-cilk

CC = $(COMPILER_HOME)/bin/clang 
CXX = $(COMPILER_HOME)/bin/clang++

ifeq ($(PORR), 1)
    PORR_CFLAGS = -DPORR -fcilk-no-inline
    PORR_LIBS = $(PORR_HOME)/build/libporr.a
else 
    PORR_CFLAGS = -fcilk-no-inline
endif

INC += -I$(COMPILER_HOME)/include -I$(CILKRTS_HOME)/include
INC += -I$(BENCH_DIR) -I$(PORR_HOME)/src
LDFLAGS = -lrt -ldl -lpthread -ltcmalloc
ARFLAGS = rcs
#OPT = -O0 #-O3 -march=native -DNDEBUG
OPT = -O3


LTO ?= 1
ifeq ($(LTO),1)
  OPT += -flto
	LDFLAGS += -flto
	ARFLAGS += --plugin $(COMPILER_HOME)/lib/LLVMgold.so
endif


LIBS += $(PORR_LIBS) $(CILKRTS_HOME)/lib/libcilkrts.a
BASIC_FLAGS = $(OPT) -g -fcilkplus $(PORR_CFLAGS) $(INC) #-Wfatal-errors
BASIC_CFLAGS += $(BASIC_FLAGS) -std=gnu11
BASIC_CXXFLAGS = $(BASIC_FLAGS) -std=gnu++11
