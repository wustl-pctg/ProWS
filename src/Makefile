include ../common.mk

OBJ = mutex.o cilkrr.o acquire.o
.SECONDARY: $(OBJ)

.PHONY: default
default: $(LIBNAME)

# Why do I have to do this?!
%.mk: ;

%.a: $(OBJ)
	$(AR) $(ARFLAGS) $@ $^

%.o: $(%:=.cpp) $(%:=.h) util.h
	$(CXX) $(CFLAGS) $(CILKFLAGS) -c $(*:=.cpp)

clean:
	rm -f *.o
	rm -f ../build/*.a
