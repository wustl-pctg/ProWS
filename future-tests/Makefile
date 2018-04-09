include ../common.mk

OBJ = simple_future_cilkr.o
MY_LDFLAGS=-R=../cilkrtssuspend/.libs -lpthread -lcilkrts
MY_CXXFLAGS=-g -std=c++11 -Wfatal-errors -I/home/kdsinger/porridge/cilkrtssuspend/include -fcilkplus -fcilk-no-inline
APP = simple-future
.SECONDARY: $(OBJ)

.PHONY = default
#default: $(APP)
default:
	$(CXX) -O0 -g -fcilkplus -fno-omit-frame-pointer -fcilk-no-inline -I/home/kdsinger/porridge/llvm-cilk/include -std=c++11 -Wall -c simple_future_cilkr.cpp -o simple_future_cilkr.o
	$(CXX) simple_future_cilkr.o -o simple-future -Wl,-rpath=../cilkrtssuspend/.libs/cilkrts -lcilkrts -lrt -ldl -lpthread -ltcmalloc -lpthread -lm -lrt

#.PHONY = $(APP)
#$(APP): $(OBJ)
#	$(CXX) $^ -o $@  -fcilkplus $(MY_LDFLAGS)

#%.o: %.cpp ../src/future.h
	#$(CXX) -E $(CFLAGS) $(CILKFLAGS) -c $(*:=.cpp) > test.cpp
#	$(CXX) $(MY_CXXFLAGS) -c $(*:=.cpp)

.PHONY = clean
clean:
	rm -f *.o a.out simple-future
