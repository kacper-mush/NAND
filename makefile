CC       = gcc
CFLAGS   = -Wall -Wextra -Wno-implicit-fallthrough -fPIC -O2
LDFLAGS  = -shared

.PHONY: clean

example_test: nand_example.o libnand.so
	$(CC) -L. -l nand -o $@ $^

libnand.so: nand.o
	$(CC) $(LDFLAGS) -o $@ $^ 

nand_example.o: nand_example.c
nand.o: nand.c nand.h

clean:
	rm -f *.o libnand.so example_test

memcheck:
	valgrind --leak-check=full --show-leak-kinds=all --track-origins=yes ./example_test
