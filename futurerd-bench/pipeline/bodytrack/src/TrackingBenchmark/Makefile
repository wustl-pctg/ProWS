include ../../../../bench.mk

.PHONY: all clean serial #cilk

# All the *_SOURCES variables are unused; for reference only
SOURCES = DMatrix.h GeneralUtil.h ParticleFilter.h \
	SmallVectors.h Vector3.h system.h AnnealingFactor.h \
	AnnealingFactor.cpp BodyGeometry.h BodyGeometry.cpp BodyPose.h \
	BodyPose.cpp CameraModel.h CameraModel.cpp CovarianceMatrix.h \
	CovarianceMatrix.cpp ImageMeasurements.h ImageMeasurements.cpp \
	ImageProjection.h ImageProjection.cpp RandomGenerator.h \
	RandomGenerator.cpp TrackingModel.h TrackingModel.cpp main.cpp \

OBJDIR = clang-obj

CILK_SOURCE = ParticleFilterCilk.h  TrackingModelCilk.h TrackingModelCilk.cpp

# Need to build differently for instrumentation
# vs. non-instrumentation. The base vs. reach and inst vs. rd
# differences don't matter unless we're using Cilk features (see use
# of $(btype) below.
OBJ = AnnealingFactor BodyGeometry BodyPose CameraModel CovarianceMatrix
OBJ += ImageMeasurements ImageProjection RandomGenerator TrackingModel
INST = no
ifeq ($(btype),inst)
  INST = yes
endif
ifeq ($(btype),rd)
  INST = yes
endif
ifeq ($(INST),yes)
  OBJECTS = $(OBJ:%=$(OBJDIR)/%-inst.o)
  FLEXLIB = ../FlexImageLib/$(OBJDIR)/libflex-inst.a
else
  OBJECTS = $(OBJ:%=$(OBJDIR)/%-noinst.o)
  FLEXLIB = ../FlexImageLib/$(OBJDIR)/libflex-noinst.a
endif

CILK_OBJECTS = $(OBJDIR)/main-$(btype).o $(OBJDIR)/TrackingModelCilk-$(btype).o
.SECONDARY: $(CILK_OBJECTS)

DEPENDENCIES = $(FLEXLIB)

# Need to use += since they are partially defined in bench.mk and
# common.mk
# -O3 -g
CXXFLAGS += $(APPFLAGS) $(INC)
CXXFLAGS += #-funroll-loops -fprefetch-loop-arrays -fpermissive -fexceptions
CXXFLAGS += -Wall -Wno-unknown-pragmas # -static-libgcc -Wl,--hash-style=both,--as-needed -fno-exceptions 
LDFLAGS += $(FLEXLIB)

INCLUDES = -I../FlexImageLib

TARGETS = bt-$(btype)
all: $(OBJDIR) $(TARGETS) #cilk serial

serial: $(OBJDIR) bodytrack-serial
# cilk: CXXFLAGS += $(APPFLAGS) $(RDFLAGS)
# cilk: LDFLAGS += $(LIB)
# cilk: INCLUDES += $(INC)
# cilk: $(OBJDIR) bodytrack-cilk

bt-all: $(OBJDIR)
	$(MAKE) mode=$(mode) ftype=$(ftype) btype=rd
	$(MAKE) mode=$(mode) ftype=$(ftype) btype=inst
	$(MAKE) mode=$(mode) ftype=$(ftype) btype=reach
	$(MAKE) mode=$(mode) ftype=$(ftype) btype=base

bt-%: $(CILK_OBJECTS) $(OBJECTS) $(DEPENDENCIES)
	$(CXX) -o $@ $^ $(LDFLAGS) $(LIBS)

$(OBJDIR):
	mkdir -p $(OBJDIR)

# Only the main file is compiled differently
$(OBJDIR)/main-serial.o: main.cpp
	$(CXX) $(CXXFLAGS) $(INCLUDES) -c -o $@ $<

$(OBJDIR)/main-%.o: main.cpp
	$(CXX) $(CXXFLAGS) -DUSE_CILK_FUTURE=1 $(INCLUDES) -c -o $@ $<

$(OBJDIR)/TrackingModelCilk-%.o: TrackingModelCilk.cpp
	$(CXX) $(CXXFLAGS) -DUSE_CILK_FUTURE=1 $(INCLUDES) -c -o $@ $<

$(OBJDIR)/%-inst.o $(OBJDIR)/%-noinst.o: %.cpp
	$(CXX) $(CXXFLAGS) $(INCLUDES) -c -o $@ $<

bodytrack-serial: $(OBJDIR)/main-serial.o $(OBJECTS) $(DEPENDENCIES)
	$(CXX) -o $@ $^ $(LDFLAGS) $(LIBS)

# bodytrack-cilk: $(OBJECTS) $(CILK_OBJECTS) $(DEPENDENCIES)
# 	$(CXX) -o $@ $^ $(LDFLAGS) $(LIBS)

clean:
	rm -f $(OBJDIR)/*.o bt-*
