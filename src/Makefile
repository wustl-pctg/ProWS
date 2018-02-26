include ../common.mk

OBJ = spinlock.o porr.o acquire.o future.o
.SECONDARY: $(OBJ)

.PHONY: default
default: $(LIBNAME)

%.a: $(OBJ)
	$(AR) $(ARFLAGS) $@ $^

%.o: %.cpp %.h util.h
	$(CXX) $(CFLAGS) $(CILKFLAGS) -c $(*:=.cpp)

clean:
	rm -f *.o
	rm -f ../build/*.a
