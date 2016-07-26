# This file defins BASIC_CFLAGS, BASIC_CXXFLAGS, LDFLAGS, LDLIBS, TOOL_FLAGS, TOOL_LDFLAGS
# Makefile including this file should use these flags 

COMPILER_DIR = $(HOME)/src/llvm-cilk
CILKRTS_INSTALL = $(HOME)/src/llvm-cilk
CILKRR_INSTALL = $(HOME)/devel/cilkrecord/
BENCH_DIR = $(HOME)/devel/cilkrecord/bench

CC = $(COMPILER_DIR)/bin/clang 
CXX = $(COMPILER_DIR)/bin/clang++

ifeq ($(CILKRR), 1)
    CILKRR_CFLAGS = -DCILKRR -fcilk-no-inline
    CILKRR_LIBS = $(CILKRR_INSTALL)/libcilkrr.a
else 
    CILKRR_CFLAGS = -fcilk-no-inline
endif

INC += -I$(COMPILER_DIR)/include -I$(CILKRTS_INSTALL)/include
INC += -I$(BENCH_DIR) -I$(CILKRR_INSTALL)
LIBS += -ldl -lpthread $(CILKRR_LIBS) $(CILKRTS_INSTALL)/lib/libcilkrts.a 
# LDFLAGS += -L$(CILKRTS_INSTALL)/lib
BASIC_FLAGS = -O0 -g -fcilkplus $(CILKRR_CFLAGS) $(INC) #-Wfatal-errors
BASIC_CFLAGS += $(BASIC_FLAGS) -std=gnu11
BASIC_CXXFLAGS = $(BASIC_FLAGS) -std=gnu++11
