CC       = gcc
CFLAGS   = -Wall -Wextra -Wno-implicit-fallthrough -std=gnu17 -fPIC -O2
LDFLAGS  = -shared -Wl,--wrap=malloc -Wl,--wrap=calloc -Wl,--wrap=realloc \
		   -Wl,--wrap=reallocarray -Wl,--wrap=free -Wl,--wrap=strdup \
		   -Wl,--wrap=strndup

.PHONY: clean auto_test

example_test: nand_example.o libnand.so
	$(CC) -L. -lnand -o $@ $^

better_test: testy.o libnand.so
	$(CC) -L. -lnand -o $@ $^

auto_test:
	./run_tests.sh -v -n 100

libnand.so: nand.o memory_tests.o
	$(CC) $(LDFLAGS) -o $@ $^ 

nand_example.o: nand_example.c
memory_tests.o: memory_tests.c memory_tests.h
nand.o: nand.c nand.h

clean:
	rm -f *.o libnand.so example_test better_test test_gen auto test.in auto.out
	rm -f brute.out