include ../../bench.mk

OUTPUT = -DOUTPUT

CC=gcc
CXX=g++

OPT_FLAGS = -g -O3 -Werror -Wall $(OUTPUT) -std=c99

# link objects(binaries) together
heartwall: main.o ./AVI/avilib.o ./AVI/avimod.o
	$(CC) main.o ./AVI/avilib.o ./AVI/avimod.o -lm -fopenmp -o heartwall

# compile main function file into object (binary)
main.o: main.c define.c kernel.c
	$(CC) main.c -I./AVI -c $(OPT_FLAGS) -fopenmp

./AVI/avilib.o ./AVI/avimod.o:
	cd AVI; make;

# delete all object files
clean:
	rm -f *.o AVI/*.o heartwall result.txt
