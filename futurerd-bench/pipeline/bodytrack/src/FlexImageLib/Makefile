include ../../../../bench.mk

.PHONEY: all clean

# SOURCES is not used; for reference only
SOURCES = BinaryImage.h FlexColor.h FlexDataExchange.h FlexDefs.h \
FlexDraw.h FlexFilter.h FlexImage.h FlexImageStatus.h FlexImageStore.h \
FlexIO.h FlexLib.h FlexOperators.h FlexThreshold.h FlexTransform.h \
FlexImage.cpp FlexIO.cpp

OBJDIR = clang-obj
OBJ = $(OBJDIR)/FlexImage.o $(OBJDIR)/FlexIO.o

INST = no
ifeq ($(btype),inst)
  INST = yes
endif
ifeq ($(btype),rd)
  INST = yes
endif
ifeq ($(INST),yes)
  OBJECTS = $(OBJ:.o=-inst.o)
  TARGETS = $(OBJDIR)/libflex-inst.a
else
  OBJECTS = $(OBJ:.o=-noinst.o)
  TARGETS = $(OBJDIR)/libflex-noinst.a
endif

ifeq ($(btype),all)
all: $(OBJDIR)
	$(MAKE) mode=$(mode) btype=base
	$(MAKE) mode=$(mode) btype=inst
else
all: $(OBJDIR) $(TARGETS)
endif

# -O3 -g
# Originally this had both -fno-exceptions and -fexceptions... 
CXXFLAGS += $(APPFLAGS) #-funroll-loops -fprefetch-loop-arrays -fpermissive -fexceptions
CXXFLAGS += -Wall -Wno-unknown-pragmas # -static-libgcc -Wl,--hash-style=both,--as-needed
AR = ar

$(OBJDIR):
	mkdir -p $(OBJDIR)

$(OBJDIR)/%-inst.o $(OBJDIR)/%-noinst.o: %.cpp
	$(CXX) $(CXXFLAGS) -c -o $@ $<

$(TARGETS): $(OBJECTS)
	$(AR) $(ARFLAGS) $@ $^

clean:
	rm -f $(OBJDIR)/*.o $(OBJDIR)/*.a
