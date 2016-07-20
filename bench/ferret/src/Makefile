include ../../reducer-bench.mk

PROG = reducer

LDLIBS += $(LIBS) -lrt -lm -lcass -lgsl -lgslcblas -ljpeg -lstdc++

.PHONY:	all clean dirs full_clean lib tools reducer
# image server 

VPATH = $(CURDIR)/src:$(CURDIR)/src/lsh:$(CURDIR)/tools:$(CURDIR)/image:$(CURDIR)/benchmark

ROOT = $(CURDIR)
SRCDIR = $(ROOT)/src
INCDIR = $(ROOT)/include

BINDIR = $(ROOT)/build/bin
LIBDIR = $(ROOT)/build/lib
OBJDIR = $(ROOT)/build/obj

CFLAGS += $(BASIC_CFLAGS) -I$(INCDIR)
CXXFLAGS += $(CFLAGS)
LDFLAGS += -L$(LIBDIR) $(TOOL_LDFLAGS)

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

$(OBJDIR)/ferret-reducer.o: ferret-reducer.cpp
	@echo "   '$(CXX) $(CXXFLAGS) $(TOOL_FLAGS) $<' ==> '$@'"
	@$(CC) $(CXXFLAGS) $(TOOL_FLAGS) -c $< -o $@

$(OBJDIR)/%.o: %.cpp
	@echo "   '$(CXX) $(CXXFLAGS) $<' ==> '$@'"
	@$(CXX) $(CXXFLAGS) -c $< -o $@

$(tools_tgt): $(BINDIR)/% : $(OBJDIR)/%.o $(LIBDIR)/libcass.a
	@echo "   '$(CXX) $<' ==> '$@'"
	@$(CXX) $(CXXFLAGS) $(LDFLAGS) $^ -o $@ $(LDLIBS)

$(BINDIR)/ferret-reducer: $(OBJDIR)/ferret-reducer.o $(LIBDIR)/libcass.a $(LIBDIR)/libcassimage.a
	@echo "   '$(CC) $(CXXFLAGS) $(TOOL_LDFLAGS) $(LDLIBS) $<' ==> '$@'"
	@$(CC) $(CXXFLAGS) $(TOOL_LDFLAGS) $(LDFLAGS) $^ -o $@ $(LDLIBS)

$(image_tgt): $(BINDIR)/% : $(OBJDIR)/%.o $(LIBDIR)/libcass.a $(LIBDIR)/libcassimage.a
	@echo "   '$(CXX) $<' ==> '$@'"
	@$(CXX) $(CXXFLAGS) $(LDFLAGS) $^ -o $@ $(LDLIBS)
 
$(BINDIR)/%: $(OBJDIR)/%.o $(LIBDIR)/libcass.a $(LIBDIR)/libcassimage.a
	@echo "   '$(CC) $(CFLAGS) $(LDFLAGS) $(LDLIBS) $<' ==> '$@'"
	@$(CC) $(CFLAGS) $(LDFLAGS) $^ -o $@ $(LDLIBS)
