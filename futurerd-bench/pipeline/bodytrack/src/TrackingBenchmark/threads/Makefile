.PHONEY: all clean

SOURCES = LockTypes.h Thread.h Thread.cpp ThreadGroup.h ThreadGroup.cpp \
WorkerGroup.h WorkerGroup.cpp Mutex.h Mutex.cpp Condition.h Condition.cpp \
Barrier.h Barrier.cpp RWLock.h RWLock.cpp SynchQueue.h TicketDispenser.h

OBJECTS = Thread.o ThreadGroup.o WorkerGroup.o Mutex.o Condition.o Barrier.o RWLock.o

CXXFLAGS = -O3 -g -funroll-loops -fprefetch-loop-arrays -fpermissive -fno-exceptions -static-libgcc -Wl,--hash-style=both,--as-needed -fexceptions -Wall -Wno-unknown-pragmas -pthread

AR = ar
RANLIB = ranlib
LIBRARY = libthreads.a

all: $(LIBRARY)

%.o: %.cpp
	$(CXX) $(CXXFLAGS) -c -o $@ $<

$(LIBRARY): $(OBJECTS)
	$(AR) rcs $@ $^
	$(RANLIB) $@

clean:
	rm -f *.o $(LIBRARY)
