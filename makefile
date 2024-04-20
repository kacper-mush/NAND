CC       = gcc
CFLAGS   = -Wall -Wextra -Wno-implicit-fallthrough -std=gnu17 -fPIC -O2
LDFLAGS  = -shared -Wl,--wrap=malloc -Wl,--wrap=calloc \
           -Wl,--wrap=realloc -Wl,--wrap=reallocarray -Wl,--wrap=free \
		   -Wl,--wrap=strdup -Wl,--wrap=strndup

.PHONY: clean

example_test: nand_example.o libnand.so
	$(CC) -L. $^ -lnand -o $@

better_test: testy.o libnand.so
	$(CC) -L. $^ -lnand -o $@

libnand.so: nand.o memory_tests.o
	$(CC) $^ $(LDFLAGS) -o $@

.c.o:
	$(CC) $< $(CFLAGS) -c -o $@

clean:
	rm -f *.o libnand.so example_test better_test