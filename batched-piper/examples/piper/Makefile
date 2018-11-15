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

# The include directory where the Piper Cilk Plus header files live.
PIPER_INCLUDE_DIR := ../../include

# This Makefile assumes that the directory containing the Piper Cilk
# Plus runtime library has been added to the appropriate paths for
# linking and dynamic loading.

ifeq ($(OS),Windows_NT)
  # Windows build.
  CC := icl
  CXX := icl
  CFLAGS := /Wall /Qstd=c++0x

  ifeq ($(DBG),1)
    CFLAGS += /Zi /Od
  else
    CFLAGS += /O3 /Zi
  endif

  EXE_OUTPUT_FLAG=/Fe
  PREPROC_OUTPUT_FLAGS= /EP /P /Fi
  INDENT_PROG=echo   # No indent on Windows (or at least one that seems to work well...)
  INCLUDE_PATH := /I $(PIPER_INCLUDE_DIR)
else
  # Unix build.
  ifeq ($(COMPILER),gcc)
      CXX := g++
      CFLAGS := -fcilkplus -lcilkrts -ldl -D__CILKRTS_ABI_VERSION=1
  else ifeq ($(COMPILER),clang)
      CXX := clang
      CFLAGS := -fcilkplus -lcilkrts -ldl -D__CILKRTS_ABI_VERSION=1
  else
      CXX := icc
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
  INCLUDE_PATH := -I $(PIPER_INCLUDE_DIR)
endif 

# Flag for overriding debug level for user code.
# Note that setting this flag won't change the runtime debug level,
# because that is set when the runtime is compiled.
PIPER_DEBUG:=0
CFLAGS += -DPIPER_DEBUG=$(PIPER_DEBUG)

H_FILES = cilktest_timer.h

# Sample programs should named sample_<testname>.cpp
SAMPLE_TARGETS := spsps grid

TARGETS = $(addprefix sample_, $(SAMPLE_TARGETS))
DBG_TARGETS= $(addprefix dbg_sample_, $(SAMPLE_TARGETS))
RUN_TARGETS = $(addprefix run_, $(TARGETS))

.PHONY: all run clean

# Build all the binaries, then test them all.
all: binaries 
	make run

binaries: $(TARGETS)

# Runs all tests
run: $(RUN_TARGETS)

# Builds a sample application with name "sample_<testname>.cpp"
sample_%: sample_%.cpp $(H_FILES)
	$(CXX) $(CFLAGS) $(INCLUDE_PATH) $< $(EXE_OUTPUT_FLAG)$@

# Builds a sample application, but prepropcessing step to generate
# better line number information.
dbg_sample_%: sample_%.cpp $(H_FILES)
	$(CXX) $(CFLAGS) $(INCLUDE_PATH) $< $(PREPROC_OUTPUT_FLAGS)RAW_SOURCE_sample_$*.cpp
	$(INDENT_PROG) RAW_SOURCE_sample_$*.cpp
	$(CXX) $(CFLAGS) $(INCLUDE_PATH) RAW_SOURCE_sample_$*.cpp $ $(EXE_OUTPUT_FLAG)$@

# Run a sample test application.
run_sample_%: sample_%
	./$<

# Clean up temporary files.
clean: 
	rm -rf $(TARGETS) $(DBG_TARGETS) *.o *.pdb *.obj RAW_SOURCE_*.cpp *.ilk
