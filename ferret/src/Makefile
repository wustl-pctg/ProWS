PROG = serial pthreads std-future cilk-future tbb parsec3-tbb cilk piper cilkview

COMPILER_HOME=/home/kdsinger/porridge/llvm-cilk
PIPER_HOME=/home/kdsinger/porridge/cilkrtssuspend
#RD_HOME=/home/kdsinger/pracer/rdtool
#RD_INCLUDE=$(RD_HOME)/include 
RTS_INCLUDE=$(PIPER_HOME)/include/
RTS_STATIC_LIB=$(PIPER_HOME)/.libs/libcilkrts.a
#RTS_DYNAMIC_LIB=-Wl,-rpath -Wl,$(PIPER_HOME)/.libs/libcilkrts.so -lcilkrts
CXXFLAGS=-std=c++11 #-flto

# compilation flags
PIPER_CC=$(COMPILER_HOME)/bin/clang
PIPER_CXX=$(COMPILER_HOME)/bin/clang++

PIPER_LDLIBS=-lpthread -ldl -lrt
LDLIBS += $(LIBS) -lrt -lm -lcass -lgsl -lgslcblas -ljpeg -lstdc++ -lpthread

.PHONY:	all clean dirs full_clean lib tools image server \
        serial tbb parsec3-tbb pthreads std-future cilk-future piper cilk cilkview

#include ../../Makefile.myenv

VPATH = $(CURDIR)/src:$(CURDIR)/src/lsh:$(CURDIR)/tools:$(CURDIR)/image:$(CURDIR)/benchmark:$(RD_HOME)/src:$(RD_HOME)/src/om

ROOT = $(CURDIR)
SRCDIR = $(ROOT)/src
INCDIR = $(ROOT)/include

BINDIR = $(ROOT)/build/bin
LIBDIR = $(ROOT)/build/lib
OBJDIR = $(ROOT)/build/obj

# Jim: If I do "+=", then the same flag gets added multiple times.
# Why? Because in this makefile, we have a target which recursively
# calls Make.  So CFLAGS gets updated twice.
#
# WHY was the code structured this way to begin with??
PIPER_FLAGS=-g -O3 -fcilkplus -fno-omit-frame-pointer -std=c++11 -D__CILKRTS_ABI_VERSION=1 -fcilk-no-inline -I$(INCDIR)
BASIC_CPPFLAGS=-g -O3 -fcilkplus -fno-omit-frame-pointer -std=c++11 -D__CILKRTS_ABI_VERSION=1 -fcilk-no-inline -I$(INCDIR) -I$(RTS_INCLUDE)
BASIC_CFLAGS=-g -O3 -fcilkplus -fno-omit-frame-pointer  -D__CILKRTS_ABI_VERSION=1 -fcilk-no-inline -fno-strict-aliasing -I$(INCDIR)
BASIC_GCCFLAGS=-g -O3 -std=gnu99 -fno-omit-frame-pointer

CFLAGS ?= -O3 -g -I$(INCDIR) -fno-omit-frame-pointer
CFLAGS_S = -O3 -g -I$(INCDIR) -fno-omit-frame-pointer

LDFLAGS += -L/usr/lib64 -L$(LIBDIR)

TARGET = $(addprefix $(BINDIR)/ferret-, $(PROG))

export LIBDIR BINDIR OBJDIR CC CXX CFLAGS LDFLAGS LDLIBS

# Modify the following variables if source code moves location
cilkm_top_dir := $(CILKM_RTS) 
cilkm_rts := $(CILKM_RTS)/runtime
CILK_FLAGS := -DHAVE_CONFIG_H -I$(cilkm_top_dir) -I$(cilkm_rts) -O3 -std=gnu99
CILK_LDFLAGS := -L$(cilkm_rts)/.libs -L$(cilkm_rts) -lcilkrt0 -lcilk -Wl,-rpath=$(cilkm_rts)/.libs 

# Point to the install path for Piper Cilk Plus
PIPER_ROOT := $(CILKPLUS_PIPER_LIB)
PIPER_INCLUDE_DIR := $(PIPER_ROOT)/include
PIPER_LIB_DIR := $(PIPER_ROOT)/lib

all:	$(PROG) 
cilk:   CFLAGS += $(CILK_FLAGS) 
cilk:   LDFLAGS += $(CILK_LDFLAGS)

tbb:    LDLIBS += -ltbb -ltbbmalloc
parsec3-tbb:   LDLIBS += -ltbb -ltbbmalloc
#piper:  CFLAGS += -I$(PIPER_INCLUDE_DIR) -std=c++0x
#piper:  LDLIBS += -Wl,-rpath -Wl,$(PIPER_LIB_DIR) -ltbbmalloc -L$(PIPER_LIB_DIR)

dirs:
	mkdir -p $(BINDIR)
	mkdir -p $(LIBDIR)
	mkdir -p $(OBJDIR)

# build the cass library
lib:	$(LIBDIR)/libcass.a 

lib_src := $(wildcard src/*.c) $(wildcard src/lsh/*.c)
lib_src := $(notdir $(lib_src))
lib_obj := $(addprefix $(OBJDIR)/, $(lib_src:.c=.o))


$(LIBDIR)/libcass.a:	$(lib_obj)
	@echo "   A  '$@'"
	@$(AR) rcs $@ $^

# build the image support library
libimage_src := image.c extract.c edge.c srm.c
libimage_obj := $(addprefix $(OBJDIR)/, $(libimage_src:.c=.o))

$(LIBDIR)/libcassimage.a:	$(libimage_obj)
	@echo "   A  '$@'"
	@$(AR) rcs $@ $^

# build the tool programs
tools_src := $(wildcard tools/*.c)
tools_tgt := $(notdir $(tools_src))
tools_tgt := $(addprefix $(BINDIR)/, $(tools_tgt:.c=))

tools : $(tools_tgt)

serial: dirs lib tools
	@echo "Building the serial version."
	make $(BINDIR)/ferret-serial 

tbb: dirs lib tools
	@echo "Building the TBB version."
	make $(BINDIR)/ferret-tbb

parsec3-tbb: dirs lib tools
	@echo "Building the TBB version from Parsec 3.0."
	make $(BINDIR)/ferret-parsec3-tbb

piper: dirs lib tools
	@echo "Build the Piper Cilk Plus Version."
	make $(BINDIR)/ferret-piper

cilkview: dirs lib tools
	@echo "Build the Cilkview Version."
	make $(BINDIR)/ferret-cilkview

pthreads: dirs lib tools
	@echo "Building the pthreads version."
	make $(BINDIR)/ferret-pthreads

std-future: dirs lib tools
	@echo "Building the std future version."
	make $(BINDIR)/ferret-std-future

cilk-future: dirs lib tools
	@echo "Building the std future version."
	make $(BINDIR)/ferret-cilk-future

cilk: dirs lib tools
	@echo "Building the Cilk version."
	make $(BINDIR)/ferret-cilk

image_tgt := cass_img_extract
image_tgt := $(addprefix $(BINDIR)/, $(image_tgt))

image: $(image_tgt)

clean:
	rm -rf $(TARGET)

full_clean:
	rm -rf $(OBJDIR)/* $(BINDIR)/* $(LIBDIR)/*

$(OBJDIR)/%.o: %.c
	@echo "   '$(CC) $(CFLAGS) $<' ==> '$@'"
#	@$(CC) $(CFLAGS) -c $< -o $@
	$(PIPER_CC)  $(CFLAGS) -I$(RD_HOME) -I$(RD_INCLUDE) -c $< -o $@

/project/adams/home/xuyifan/2D/piper_bench/piper_bench/ferret/src/src/lsh/LSH_query.o: LSH_query.c
	$(PIPER_CC)  $(CFLAGS_S) -I$(RD_HOME) -I$(RD_INCLUDE) -c $< -o $@

$(OBJDIR)/%.o: %.cpp
	@echo "   '$(CXX) $(CFLAGS) $(CXXFLAGS) $<' ==> '$@'"
#	@$(CXX) $(CFLAGS) $(CXXFLAGS) -c $< -o $@
#	$(PIPER_CXX) -fsanitize=thread $(CFLAGS) $(CXXFLAGS) -I$(RD_HOME) -I$(RD_INCLUDE) -c $< -o $@
	$(PIPER_CXX)  $(BASIC_CPPFLAGS) -I$(RD_HOME) -I$(RD_INCLUDE) -c $< -o $@

$(OBJDIR)/ferret-piper.o: ferret-piper.cpp
	@echo "   'icpc $(CFLAGS) $(CXXFLAGS) $<' ==> '$@'"
	#@icpc $(CFLAGS) $(CXXFLAGS) -c $< -o $@
	$(PIPER_CXX)  $(PIPER_FLAGS) -I$(RTS_INCLUDE) -I$(RD_HOME) -I$(RD_INCLUDE) -c $< -o $@


$(OBJDIR)/ferret-cilkview.o: ferret-cilkview.c
	@echo "   'icc $(CFLAGS) $<' ==> '$@'"
	@icc $(CFLAGS) -c $< -o $@

$(tools_tgt): $(BINDIR)/% : $(OBJDIR)/%.o $(LIBDIR)/libcass.a  
	@echo "   '$(CXX) $<' ==> '$@'"
	#@$(CXX) $(CFLAGS) $(LDFLAGS) $(RTS_STATIC_LIB) $(PIPER_LDLIBS)  $^ -o $@ $(LDLIBS) 
	$(PIPER_CXX) $^ $(RTS_STATIC_LIB) $(PIPER_LDLIBS) $(LDFLAGS) $(LDLIBS) -o $@

$(image_tgt): $(BINDIR)/% : $(OBJDIR)/%.o $(LIBDIR)/libcass.a $(LIBDIR)/libcassimage.a 
	@echo "   '$(CXX) $<' ==> '$@'"
	#@$(CXX) $(CFLAGS) $(LDFLAGS)  $^ -o $@ $(LDLIBS)
	$(PIPER_CXX) $^ $(RTS_STATIC_LIB) $(PIPER_LDLIBS) $(LDFLAGS) $(LDLIBS) -o $@

%.o: %.cpp
	$(PIPER_CXX) $(PIPER_FLAGS) -I$(RTS_INCLUDE) -I$(RD_HOME) -I$(RD_INCLUDE) -I/home/kdsinger/pracer/batched-piper/runtime -c $< -o $@

$(BINDIR)/ferret-piper: $(OBJDIR)/ferret-piper.o $(LIBDIR)/libcass.a $(LIBDIR)/libcassimage.a  
#omrd.o instrumentation.o piper_rd.o mem_access.o
	@echo "   'icpc $(CFLAGS) $(LDFLAGS) $(LDLIBS) $<' ==> '$@'"
	#@icpc $(CFLAGS) $(LDFLAGS) $^ -o $@ $(LDLIBS)
	$(PIPER_CXX) $^ $(RTS_STATIC_LIB) $(PIPER_LDLIBS) $(LDFLAGS) $(LDLIBS) -o $@

$(BINDIR)/ferret-cilkview: $(OBJDIR)/ferret-cilkview.o $(LIBDIR)/libcass.a $(LIBDIR)/libcassimage.a
	@echo "   'icc $(CFLAGS) $(LDFLAGS) $(LDLIBS) $<' ==> '$@'"
	@icc $(CFLAGS) $(LDFLAGS) $^ -o $@ $(LDLIBS)

$(BINDIR)/%: $(OBJDIR)/%.o $(LIBDIR)/libcass.a $(LIBDIR)/libcassimage.a
	@echo "   '$(CC) $(CFLAGS) $(LDFLAGS) $(LDLIBS) $<' ==> '$@'"
	#@$(CC) $(CFLAGS) $(LDFLAGS)  $^ -o $@ $(LDLIBS)
	$(PIPER_CXX) $^ $(RTS_STATIC_LIB) $(PIPER_LDLIBS) $(LDFLAGS) $(LDLIBS) -o $@

