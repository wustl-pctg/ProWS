include ../bench.mk

.PHONY: all clean

TARGETS = sum 

all: $(TARGETS) 

INC += -I../util
CFLAGS += $(APPFLAGS) $(RDFLAGS)
CXXFLAGS += $(APPFLAGS) $(RDFLAGS)

sum: sum.o
	$(CXX) $^ $(LDFLAGS) $(LIB) -o $@

# delete all object files
clean:
	rm -f *.o $(TARGETS) core.*
