include ../common.mk

OBJ = simple_future_cilkr.o
APP = simple-future
.SECONDARY: $(OBJ)

.PHONY = default
default: $(APP)

.PHONY = $(APP)
$(APP): $(OBJ)
	$(CXX) $^ -o $@  $(LDFLAGS) $(LIBS)

%.o: %.cpp
	$(CXX) $(CFLAGS) $(CILKFLAGS) -c $(*:=.cpp)

.PHONY = clean
clean:
	rm -f *.o a.out simple-future
