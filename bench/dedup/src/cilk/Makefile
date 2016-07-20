# Makefile for dedup kernel
TARGET = dedup

CILKSAN=redrace

.default: reducer

.PHONY: all clean reducer check

# define BASIC_CFLAGS, BASIC_CXXFLAGS, LDFLAGS, LDLIBS, TOOL_FLAGS,
# TOOL_LDFLAGS 
include ../../../reducer-bench.mk

# let's use cpp compiler for everything
CC = $(CXX) 

PROG = dedup-reducer
reducer: $(PROG)

ifeq ($(PROF),1)
CFLAGS += -g
endif

export UTIL_OBJ LIBS CFLAGS

CFLAGS += -Wall -D_XOPEN_SOURCE=600 $(INCLUDES) -O3 -DNDEBUG -g 
LIBS += -lm -lrt

DEDUP_OBJ = decoder.o
OTHER_OBJ = hashtable.o util.o rabin.o sha.o ktiming.o
UTIL_OBJ = $(addprefix util/, $(OTHER_OBJ)) 

# Uncomment the following to enable bzip2 compression
# Don't seem to have bzlib.h installed on the system
# CFLAGS += -DENABLE_BZIP2_COMPRESSION
# LIBS += -lbz2

CFLAGS += -DENABLE_GZIP_COMPRESSION
LIBS += -lz

# in comiling dedup. use the TOOL related flags for everything
CFLAGS += -std=c++0x $(BASIC_CFLAGS) $(TOOL_FLAGS)
LIBS += $(LDLIBS)
LDFLAGS += $(TOOL_LDFLAGS)
CPPFLAGS := $(CFLAGS)
# $(warning "CPPFLAGS is $(CPPFLAGS),  CFLAGS is $(CFLAGS)")


all: $(PROG)

dedup-reducer.o: dedup.c
	$(CXX) $(CPPFLAGS) -c $< -o $@

%-reducer.o: %-reducer.cpp
	$(warning "calling rule %-reducer.cpp")
	$(CXX) $(CPPFLAGS) -c $< -o $@ 

%.o: %.c
	$(warning "calling rule %.c")
	$(CC) $(CFLAGS) -c $< -o $@

%.o: %.cpp
	$(warning "calling rule %.cpp")
	$(CXX) $(CPPFLAGS) -c $< -o $@

util/%.o: util/%.c
	$(warning "calling rule util/%.c")
	$(CC) $(CFLAGS) -c $< -o $@

# Specialize rule for Cilk.  Cilk requires extra flags.
$(TARGET)-reducer: encoder-reducer.o $(DEDUP_OBJ) $(UTIL_OBJ) dedup-reducer.o
	$(CXX) $^ -o $@  $(LDFLAGS) $(LIBS)

$(TARGET)-%: encoder-%.o $(DEDUP_OBJ) $(UTIL_OBJ) dedup-serial.o
	$(CC) $^ -o $@ $(LDFLAGS) $(LIBS)  

clean:
	rm -f *~ *.o util/*.o $(PROG) *.tmp

