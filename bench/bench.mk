# This file defins BASIC_CFLAGS, BASIC_CXXFLAGS, LDFLAGS, LDLIBS, TOOL_FLAGS, TOOL_LDFLAGS
# Makefile including this file should use these flags 

COMPILER_DIR = $(HOME)/sandbox/llvm-cilk
CILK_RTS_INSTALL = $(HOME)/sandbox/cilkrecord/cilkrtssuspend/build/
BENCH_DIR = $(HOME)/sandbox/cilkrecord/bench

CC = $(COMPILER_DIR)/bin/clang
CXX = $(COMPILER_DIR)/bin/clang++

INC += -I$(COMPILER_DIR)/include -I$(CILK_RTS_INSTALL)/include
INC += -I$(BENCH_DIR)
BASIC_CFLAGS += -O3 -g -fcilkplus $(INC) # -Werror 
BASIC_CXXFLAGS = $(BASIC_CFLAGS)
