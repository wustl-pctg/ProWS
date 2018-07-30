all: avimod.o avilib.o

OPT = -g -O3 -Werror -Wall

# compile supporting function file into object (binary)
avimod.o: avilib.h avimod.c 
	gcc -c $(OPT) avimod.c

# compile supporting function file into object (binary)
avilib.o: avilib.h avilib.c
	gcc -c $(OPT) avilib.c

# delete files
clean:
	rm *.o *.out
