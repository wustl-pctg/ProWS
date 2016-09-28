# This file defins BASIC_CFLAGS, BASIC_CXXFLAGS, LDFLAGS, LDLIBS, TOOL_FLAGS, TOOL_LDFLAGS
# Makefile including this file should use these flags 

COMPILER_DIR = $(HOME)/llvm-cilk
CILKRTS_INSTALL = $(HOME)/llvm-cilk
PORR_INSTALL = $(HOME)/devel/cilkrecord/
BENCH_DIR = $(HOME)/devel/cilkrecord/bench

CC = $(COMPILER_DIR)/bin/clang 
CXX = $(COMPILER_DIR)/bin/clang++

ifeq ($(PORR), 1)
    PORR_CFLAGS = -DPORR -fcilk-no-inline
    PORR_LIBS = $(PORR_INSTALL)/build/libporr.a
else 
    PORR_CFLAGS = -fcilk-no-inline
endif

INC += -I$(COMPILER_DIR)/include -I$(CILKRTS_INSTALL)/include
INC += -I$(BENCH_DIR) -I$(PORR_INSTALL)/src
LDFLAGS = 
ARFLAGS = rcs
OPT = -O3 -march=native -DNDEBUG


LTO ?= 1
ifeq ($(LTO),1)
  OPT += -flto
	LDFLAGS += -flto
	ARFLAGS += --plugin $(COMPILER_HOME)/lib/LLVMgold.so
endif


LIBS += -ldl -lpthread -ltcmalloc $(PORR_LIBS) $(CILKRTS_INSTALL)/lib/libcilkrts.a
BASIC_FLAGS = $(OPT) -g -fcilkplus $(PORR_CFLAGS) $(INC) #-Wfatal-errors
BASIC_CFLAGS += $(BASIC_FLAGS) -std=gnu11
BASIC_CXXFLAGS = $(BASIC_FLAGS) -std=gnu++11
