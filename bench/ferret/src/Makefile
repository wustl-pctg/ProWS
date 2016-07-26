include ../../bench.mk

PROG = serial lock reducer

LDLIBS += $(LIBS) -lrt -lm -lcass -lgsl -lgslcblas -ljpeg -lstdc++

.PHONY:	all clean dirs full_clean lib tools serial lock reducer
# image server 

VPATH = $(CURDIR)/src:$(CURDIR)/src/lsh:$(CURDIR)/tools:$(CURDIR)/image:$(CURDIR)/benchmark

ROOT = $(CURDIR)
SRCDIR = $(ROOT)/src
INCDIR = $(ROOT)/include

BINDIR = $(ROOT)/build/bin
LIBDIR = $(ROOT)/build/lib
OBJDIR = $(ROOT)/build/obj

FLAGS = -I$(INCDIR)
#CFLAGS += $(BASIC_CFLAGS) -I$(INCDIR)
CFLAGS += $(FLAGS) $(BASIC_CFLAGS)
#CXXFLAGS += $(CFLAGS)
CXXFLAGS += $(FLAGS) $(BASIC_CXXFLAGS)
LDFLAGS += -L$(LIBDIR)

TARGET_OBJ = $(addprefix $(OBJDIR)/ferret-, $(PROG).o)
TARGET = $(addprefix $(BINDIR)/ferret-, $(PROG))

export LIBDIR BINDIR OBJDIR CC CXX CFLAGS LDFLAGS LDLIBS

all:	$(PROG) 

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

lock: dirs lib tools
	@echo "Building the locking version."
	make $(BINDIR)/ferret-lock

reducer: dirs lib tools
	@echo "Building the reducer version."
	make $(BINDIR)/ferret-reducer 

# image_tgt := cass_img_extract
# image_tgt := $(addprefix $(BINDIR)/, $(image_tgt))

# image: $(image_tgt)

clean:
	rm -rf $(TARGET) $(TARGET_OBJ)

full_clean:
	rm -rf $(OBJDIR)/* $(BINDIR)/* $(LIBDIR)/*

$(OBJDIR)/%.o: %.c
	@echo "   '$(CC) $(CFLAGS) $<' ==> '$@'"
	@$(CC) $(CFLAGS) -c $< -o $@

$(OBJDIR)/ferret-serial.o: ferret-serial.cpp
	@echo "   '$(CXX) $(CXXFLAGS) $<' ==> '$@'"
	@$(CC) $(CXXFLAGS) -c $< -o $@

$(OBJDIR)/ferret-lock.o: ferret-lock.cpp
	@echo "   '$(CXX) $(CXXFLAGS) $<' ==> '$@'"
	@$(CC) $(CXXFLAGS) -c $< -o $@

$(OBJDIR)/ferret-reducer.o: ferret-reducer.cpp
	@echo "   '$(CXX) $(CXXFLAGS) $<' ==> '$@'"
	@$(CC) $(CXXFLAGS) -c $< -o $@

$(OBJDIR)/%.o: %.cpp
	@echo "   '$(CXX) $(CXXFLAGS) $<' ==> '$@'"
	@$(CXX) $(CXXFLAGS) -c $< -o $@

$(tools_tgt): $(BINDIR)/% : $(OBJDIR)/%.o $(LIBDIR)/libcass.a
	@echo "   '$(CXX) $<' ==> '$@'"
	@$(CXX) $(CXXFLAGS) $(LDFLAGS) $^ -o $@ $(LDLIBS)

$(BINDIR)/ferret-reducer: $(OBJDIR)/ferret-reducer.o $(LIBDIR)/libcass.a $(LIBDIR)/libcassimage.a
	@echo "   '$(CC) $(LDFLAGS) $(LDLIBS) $<' ==> '$@'"
	@$(CC) $(LDFLAGS) $^ -o $@ $(LDLIBS)

$(BINDIR)/ferret-serial: $(OBJDIR)/ferret-serial.o $(LIBDIR)/libcass.a $(LIBDIR)/libcassimage.a
	@echo "   '$(CC) $(LDFLAGS) $(LDLIBS) $<' ==> '$@'"
	@$(CC) $(LDFLAGS) $^ -o $@ $(LDLIBS)

$(BINDIR)/ferret-lock: $(OBJDIR)/ferret-lock.o $(LIBDIR)/libcass.a $(LIBDIR)/libcassimage.a
	@echo "   '$(CC) $(LDFLAGS) $(LDLIBS) $<' ==> '$@'"
	@$(CC) $(LDFLAGS) $^ -o $@ $(LDLIBS)

$(image_tgt): $(BINDIR)/% : $(OBJDIR)/%.o $(LIBDIR)/libcass.a $(LIBDIR)/libcassimage.a
	@echo "   '$(CXX) $<' ==> '$@'"
	@$(CXX) $(CXXFLAGS) $(LDFLAGS) $^ -o $@ $(LDLIBS)
 
$(BINDIR)/%: $(OBJDIR)/%.o $(LIBDIR)/libcass.a $(LIBDIR)/libcassimage.a
	@echo "   '$(CC) $(CFLAGS) $(LDFLAGS) $(LDLIBS) $<' ==> '$@'"
	@$(CC) $(CFLAGS) $(LDFLAGS) $^ -o $@ $(LDLIBS)
