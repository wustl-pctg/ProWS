# Makefile for dedup kernel
TARGET = dedup

.default: serial lock reducer

.PHONY: all clean serial lock reducer check

# define BASIC_CFLAGS, BASIC_CXXFLAGS, LDFLAGS, LDLIBS
include ../../../bench.mk

# let's use cpp compiler for everything
# CC = $(CXX) 

PROG = dedup-reducer dedup-serial dedup-lock 
all: $(PROG)
reducer: dedup-reducer
serial: dedup-serial
lock: dedup-lock

ifeq ($(PROF),1)
  FLAGS += -g
endif

export UTIL_OBJ LIBS CFLAGS

FLAGS = $(BASIC_FLAGS) -Wall -D_XOPEN_SOURCE=600 $(INCLUDES) $(TOOL_FLAGS)
LIBS += -lpthread -lm -lrt

DEDUP_OBJ = decoder.o
OTHER_OBJ = hashtable.o util.o rabin.o sha.o ktiming.o
UTIL_OBJ = $(addprefix util/, $(OTHER_OBJ)) 

# Uncomment the following to enable bzip2 compression
# Don't seem to have bzlib.h installed on the system
# FLAGS += -DENABLE_BZIP2_COMPRESSION
# LIBS += -lbz2

FLAGS += -DENABLE_BZIP2_COMPRESSION
LIBS += -lbz2 -lz

# in comiling dedup. use the TOOL related flags for everything
CFLAGS += $(BASIC_CFLAGS) $(FLAGS)
CPPFLAGS += $(BASIC_CXXFLAGS) $(FLAGS)
LIBS += $(LDLIBS)
LDFLAGS += $(TOOL_LDFLAGS)
# $(warning "CPPFLAGS is $(CPPFLAGS),  CFLAGS is $(CFLAGS)")


all: $(PROG)

dedup-%.o: dedup.c
	$(CC) $(CFLAGS) -c $< -o $@

# %-reducer.o: %-reducer.cpp
# 	$(warning "calling rule %-reducer.cpp")
# 	$(CXX) $(CPPFLAGS) -c $< -o $@ 

%.o: %.c
	$(warning "calling rule %.c")
	$(CC) $(CFLAGS) -c $< -o $@

%.o: %.cpp
	$(warning "calling rule %.cpp")
	$(CXX) $(CPPFLAGS) -c $< -o $@

util/%.o: util/%.c
	$(warning "calling rule util/%.c")
	$(CC) $(CFLAGS) -c $< -o $@

dedup-serial: encoder-serial.o $(DEDUP_OBJ) $(UTIL_OBJ) dedup-serial.o
	$(warning "Compiling dedup-serial")
	$(CC) $^ -o $@  $(LDFLAGS) $(LIBS)

dedup-lock: encoder-lock.o $(DEDUP_OBJ) $(UTIL_OBJ) dedup-lock.o
	$(warning "Compiling dedup-lock")
	$(CXX) $^ -o $@  $(LDFLAGS) $(LIBS)

dedup-reducer: encoder-reducer.o $(DEDUP_OBJ) $(UTIL_OBJ) dedup-reducer.o
	$(warning "Compiling dedup-reducer")
	$(CXX) $^ -o $@  $(LDFLAGS) $(LIBS)

clean:
	rm -f *~ *.o util/*.o $(PROG) *.tmp

