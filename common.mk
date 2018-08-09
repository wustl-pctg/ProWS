BASE_DIR=$(dir $(abspath $(lastword $(MAKEFILE_LIST))))
include $(BASE_DIR)/config.mk

CC=$(COMPILER_HOME)/bin/clang
CXX=$(COMPILER_HOME)/bin/clang++
LIBNAME = $(BUILD_DIR)/libporr.a

INC = -I$(RUNTIME_HOME)/include
LDFLAGS = -ldl -lpthread -L$(BASE_DIR)/SuperMalloc/release/lib -lsupermalloc
ARFLAGS = rcs
OPT = -O3 #-march=native -DNDEBUG

STATS ?= 0
PTYPE ?= 1
STAGE ?= 4
DEFS = -DPTYPE=$(PTYPE) -DSTAGE=$(STAGE) -DSTATS=$(STATS)

LTO ?= 1
ifeq ($(LTO),1)
	#OPT += -flto
	#LDFLAGS += -flto
	#ARFLAGS += --plugin $(COMPILER_HOME)/lib/LLVMgold.so
endif

CFLAGS = -g -std=c++11 -Wfatal-errors -DPRECOMPUTE_PEDIGREES=1 $(OPT) $(DEFS) $(INC)
CXXFLAGS = -DPRECOMPUTE_PEDIGREES=1
CILKFLAGS = -fcilkplus -fcilk-no-inline
