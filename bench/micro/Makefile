include ../../common.mk

ifeq ($(STATS),1)
	LDFLAGS += -lpapi
endif
CFLAGS += -I$(BUILD_DIR)/../src
LDFLAGS += $(RTS_LIB)

BENCH = fib cilkfor cbt
default: $(BENCH)
.PHONY: $(BENCH) default

%.o: %.cpp
	$(CXX) $(CFLAGS) $(CILKFLAGS) -c $(@:.o=.cpp)

# For reason, Make doesn't believe the targets exist (e.g. fib) even
# when they do, so it chooses to always relink...?
.SECONDEXPANSION:
$(BENCH): $$(@).o $(LIBNAME)
	$(CXX) $< $(LIBNAME) $(LDFLAGS) -o $@

clean:
	rm -f *.o *.a fib cilkfor cbt
