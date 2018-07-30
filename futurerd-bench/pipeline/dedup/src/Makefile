include ../../../bench.mk

PROG = dedup-serial dedup-cilk
CILK_TARGETS = dedup-base dedup-reach dedup-inst dedup-rd

.PHONY: all clean serial cilk

OBJDIR = clang-obj
UTIL_OBJDIR = $(OBJDIR)/util
BUILDDIR = ../build/

BASIC_FLAGS += -fno-strict-aliasing -DPARALLEL -D_XOPEN_SOURCE=600 $(INCLUDES)
LIB += -lm -lrt

# dedup gets added as a CILK_OBJECT below. It doesn't use any
# parallelism, but need to for calling future_disable_shadowing
# correctly.
OBJ = decoder #dedup
UTIL_OBJ = hashtable util rabin sha ktiming

ifeq ($(btype),inst)
  INST = yes
endif
ifeq ($(btype),rd)
  INST = yes
endif
ifeq ($(INST),yes)
  OBJECTS = $(OBJ:%=$(OBJDIR)/%-inst.o)
  OBJECTS += $(UTIL_OBJ:%=$(UTIL_OBJDIR)/%-inst.o)
else
  OBJECTS = $(OBJ:%=$(OBJDIR)/%.o)
  OBJECTS += $(UTIL_OBJ:%=$(UTIL_OBJDIR)/%.o)
endif

# Uncomment the following to enable gzip compression
BASIC_FLAGS += -DENABLE_GZIP_COMPRESSION
LIB += -ldl -lz

CILK_OBJECTS = $(OBJDIR)/encoder-cilk-$(btype).o
CILK_OBJECTS += $(OBJDIR)/main-$(btype).o
.SECONDARY: $(CILK_OBJECTS)

# Uncomment the following to enable bzip2 compression
# Don't seem to have bzlib.h installed on the system
# BASIC_FLAGS += -DENABLE_BZIP2_COMPRESSION
# LIB += -lbz2

CXXFLAGS += $(BASIC_FLAGS) $(APPFLAGS) $(INC)

TARGETS = dedup-$(btype)
all: $(OBJDIR) $(UTIL_OBJDIR) $(TARGETS)

serial: $(BUILDDIR) $(OBJDIR) $(UTIL_OBJDIR) dedup-serial

dedup-all: $(BUILDDIR) $(OBJDIR) $(UTIL_OBJDIR)
	$(MAKE) mode=$(mode) rdalg=$(rdalg) ftype=$(ftype) btype=rd
	$(MAKE) mode=$(mode) rdalg=$(rdalg) ftype=$(ftype) btype=inst
	$(MAKE) mode=$(mode) rdalg=$(rdalg) ftype=$(ftype) btype=reach
	$(MAKE) mode=$(mode) rdalg=$(rdalg) ftype=$(ftype) btype=base

dedup-%: $(CILK_OBJECTS) $(OBJECTS)
	$(CXX) -o $@ $^ $(LDFLAGS) $(LIB)
	mv $@ $(BUILDDIR)

$(OBJDIR):
	mkdir -p $(OBJDIR)

$(UTIL_OBJDIR):
	mkdir -p $(UTIL_OBJDIR)

$(BUILDDIR):
	mkdir -p $(BUILDDIR)

$(OBJDIR)/%-inst.o $(OBJDIR)/%.o: %.cpp
	$(CXX) -c $(CXXFLAGS) $< -o $@

$(UTIL_OBJDIR)/%-inst.o $(UTIL_OBJDIR)/%.o: util/%.cpp
	$(CXX) -c $(CXXFLAGS) $< -o $@

$(UTIL_OBJDIR)/%-inst.o: util/%.cpp
	$(CXX) -c $(CXXFLAGS) $< -o $@

$(OBJDIR)/encoder-cilk-%.o: encoder-cilk.cpp
	$(CXX) $(CXXFLAGS) -DUSE_CILK_FUTURE=1 -c -o $@ $<

$(OBJDIR)/main-%.o: main.cpp
	$(CXX) $(CXXFLAGS) -DUSE_CILK_FUTURE=1 -c -o $@ $<

dedup-serial: $(OBJDIR)/encoder-serial.o $(OBJECTS)
	$(CXX) -o $@ $^ $(LIB)
	mv $@ $(BUILDDIR)

clean:
	rm -rf *.o *~ $(OBJDIR)/*.o $(UTIL_OBJDIR)/*.o $(PROG)

reallyclean:
	rm -rf *.o *~ $(OBJDIR) $(UTIL_OBJDIR) $(TARGETS) $(PROG) $(BUILDDIR)
