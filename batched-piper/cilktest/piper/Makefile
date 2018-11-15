#########################################################################
#
#  @copyright
#  Copyright (C) 2013, Intel Corporation
#  All rights reserved.
#  
#  @copyright
#  Redistribution and use in source and binary forms, with or without
#  modification, are permitted provided that the following conditions
#  are met:
#  
#    * Redistributions of source code must retain the above copyright
#      notice, this list of conditions and the following disclaimer.
#    * Redistributions in binary form must reproduce the above copyright
#      notice, this list of conditions and the following disclaimer in
#      the documentation and/or other materials provided with the
#      distribution.
#    * Neither the name of Intel Corporation nor the names of its
#      contributors may be used to endorse or promote products derived
#      from this software without specific prior written permission.
#  
#  @copyright
#  THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
#  "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
#  LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
#  A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
#  HOLDER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT,
#  INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING,
#  BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS
#  OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED
#  AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
#  LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY
#  WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
#  POSSIBILITY OF SUCH DAMAGE.
###########################################################################
#
# Makefile for pipeline test directory.

# The include directory where the Cilk header files live.
CILK_INCLUDE_DIR := ../../include

ifeq ($(OS),Windows_NT)
# Windows build.
  CC := icl
  CFLAGS := /Wall /Qstd=c++0x

  ifeq ($(DBG),1)
    CFLAGS += /Zi /Od
  else
    CFLAGS += /O3 /Zi
  endif

  EXE_OUTPUT_FLAG=/Fe
  PREPROC_OUTPUT_FLAGS= /EP /P /Fi
  INDENT_PROG=echo   # No indent on Windows (or at least one that seems to work well...)
  INCLUDE_PATH := /I $(CILK_INCLUDE_DIR)
else
  # Unix build.
  ifeq ($(COMPILER),gcc)
      CC := g++
      CFLAGS := -fcilkplus -lcilkrts -ldl -D__CILKRTS_ABI_VERSION=1
  else ifeq ($(COMPILER),clang)
      CC := clang
      CFLAGS := -fcilkplus -lcilkrts -ldl -D__CILKRTS_ABI_VERSION=1
  else
      CC := icc
  endif

  CFLAGS += -Wall -g -std=c++0x
  ifeq ($(DBG), 1)
    CFLAGS += -O0
  else
    CFLAGS += -O3
  endif
  EXE_OUTPUT_FLAG=-o 
  PREPROC_OUTPUT_FLAGS=-E -P -o
  INDENT_PROG=indent
  INCLUDE_PATH := -I $(CILK_INCLUDE_DIR)
endif

# Flag for overriding debug level for user code.
# Note that setting this flag won't change the runtime debug level,
# because that is set when the runtime is compiled.
PIPER_DEBUG:=0
CFLAGS += -DPIPER_DEBUG=$(PIPER_DEBUG)

H_FILES = cilktest_harness.h cilktest_timer.h fib.h	\
parallel_test_holder.h sequencer.h serial_test_holder.h	\
pipe_while_loop_counter.h

# These tests run on a specific number of worker threads, because they
# try to force an interleaving.
SEQUENCE_TEST_TARGETS := stage_async stage_wait stage_count		\
 force_reduce nested_chain nested_worker_switch no_inline_iter_helper	\
 stage0_demotion

NORMAL_TEST_TARGETS := pipe_macros simple_pipewhile			\
 pipewhile_throttle pipe_fib pipe_fib_detach_opt pipe_fib_throttle	\
 holder pipewhile_reducers multiple_pipewhiles parallel_iters

OTHER_TEST_TARGETS := pipewhile_pedigrees
## New test cases we aren't including yet.
# pipewhile_lib

TEST_TARGETS := $(NORMAL_TEST_TARGETS)   \
                $(SEQUENCE_TEST_TARGETS) \
                $(OTHER_TEST_TARGETS)


TARGETS = $(addprefix test_, $(TEST_TARGETS))
DBG_TARGETS= $(addprefix dbgtest_, $(TEST_TARGETS))
RUN_TARGETS = $(addprefix run_, $(TARGETS))

.PHONY: all run clean

# Build all the binaries, then test them all.
all: binaries
	make run

binaries: $(TARGETS)

# Runs all tests
run: $(RUN_TARGETS)

test_%: test_%.cpp $(H_FILES)
	$(CC) $(CFLAGS) $(INCLUDE_PATH) $< $(EXE_OUTPUT_FLAG)$@

dbgtest_%: test_%.cpp $(H_FILES)
	$(CC) $(CFLAGS) $(INCLUDE_PATH) $< $(PREPROC_OUTPUT_FLAGS)RAW_SOURCE_test_$*.cpp
	$(INDENT_PROG) RAW_SOURCE_test_$*.cpp
	$(CC) $(CFLAGS) $(INCLUDE_PATH) RAW_SOURCE_test_$*.cpp $(EXE_OUTPUT_FLAG)$@

run_test_%: test_%
	./$<
clean:
	rm -rf $(TARGETS) $(DBG_TARGETS) *.o *.pdb *.obj RAW_SOURCE_*.cpp *.ilk

