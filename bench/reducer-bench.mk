# This file defins BASIC_CFLAGS, BASIC_CXXFLAGS, LDFLAGS, LDLIBS, TOOL_FLAGS, TOOL_LDFLAGS
# Makefile including this file should use these flags 

# where the gcc-supertech is installed
#TOP_DIR = /home/angelee/sandbox/cilk_ok/gcc-supertech
# CC = $(TOP_DIR)/bin/gcc 
# CXX = $(TOP_DIR)/bin/g++
TOP_DIR = $(HOME)/src/llvm-cilk
CC = $(TOP_DIR)/bin/clang
CXX = $(TOP_DIR)/bin/clang++

INC = -I$(TOP_DIR)/include
BASIC_CFLAGS += -O3 -g -fcilkplus $(INC) # -Werror 
BASIC_CXXFLAGS = $(BASIC_CFLAGS)

# where the vanilla Cilk Plus is installed
ORIG_CILK_RTS = /home/angelee/sandbox/cilk_ok/cilkplus_rts

# where the Cilk Plus that implement -fcilktool is installed
TOOL_CILK_RTS = $(TOP_DIR)

# where the various tool directories reside
TOOL_DIR = $(TOP_DIR)/src/cilkscreen-with-threadsanitizer
NULL_TOOL = $(TOOL_DIR)/nullsan

TOOL_FLAGS = 
TOOL_LDFLAGS =
LDFLAGS += -fcilkplus -g
LDLIBS += -ldl

# use this flag to enable / disable -flto
# disable for now; seem to cause memory reference instrumentations to be
# optimized away for unknown reasons
# LTO_FLAGS = -flto

# Unfortunately setting TOOL at the top doesn't seem to do it
# You will have to invoke make using 'make TOOL=viewread' to get this 
# ifeq to take effect.  As if the TOOL defined at top is different from the
# TOOL variable we are checking here ... I don't know why.
ifeq ($(TOOL), $(filter none, $(TOOL)))
# use the runtime that doesn't have reducer strand instrumentations
TOOL_FLAGS += -I$(ORIG_CILK_RTS)/include
TOOL_FLAGS += -I$(NULL_TOOL) $(LTO_FLAGS)
# annoyingly still need to link w/ nullsan since the runtime declare various
# cilktool interface with weak sym link (I don't know why the weak sym didn't
# get used), but nothing in the nulltool is called
# LDFLAGS += -Wl,-rpath -Wl,$(ORIG_CILK_RTS)/lib
# TOOL_LDFLAGS += -Wl,-rpath -Wl,$(NULL_TOOL) -L$(NULL_TOOL) $(LTO_FLAGS)
# LDLIBS += -lcilksan
else
TOOL_FLAGS += -I$(TOOL_DIR)/$(TOOL) -fcilktool
TOOL_FLAGS += -I$(TOOL_CILK_RTS)/include
LDFLAGS += -Wl,-rpath -Wl,$(TOOL_CILK_RTS)/lib64
ifeq ($(TOOL), $(filter viewread null_viewread, $(TOOL)))
    BASIC_CFLAGS += -DVIEWREAD=1
    TOOL_FLAGS += $(LTO_FLAGS)
    ifeq ($(TOOL), $(filter viewread, $(TOOL)))
        LDLIBS += $(TOOL_DIR)/$(TOOL)/lib$(TOOL).a
    else
        LDLIBS += $(NULL_TOOL)/libcilksan.a
    endif
    TOOL_LDFLAGS += $(LTO_FLAGS)
else # everything else use thread-sanitizer
    BASIC_CFLAGS += -DCILKSAN=1
    LDLIBS += $(TOOL_DIR)/$(TOOL)/libcilksan.a
    TOOL_FLAGS += -fsanitize=thread
    ifneq ($(TOOL), $(filter nullsan, $(TOOL)))
        TOOL_FLAGS += $(LTO_FLAGS)
        TOOL_LDFLAGS += $(LTO_FLAGS)
    endif
    # LDLIBS += -lcilksan
endif
endif

