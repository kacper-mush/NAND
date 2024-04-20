all:
	gcc -c -Wall -Wextra -Wno-implicit-fallthrough -std=gnu17 -fPIC -O2 nand.c -o nand.o
	gcc -c -Wall -Wextra -Wno-implicit-fallthrough -std=gnu17 -fPIC -O2 memory_tests.c -o memory_tests.o
	gcc -c -Wall -Wextra -Wno-implicit-fallthrough -std=gnu17 -fPIC -O2 nand_example.c -o nand_example.o
	gcc -c -Wall -Wextra -Wno-implicit-fallthrough -std=gnu17 -fPIC -O2 testy.c -o testy.o
	gcc nand.o memory_tests.o -shared -Wl,--wrap=malloc -Wl,--wrap=calloc -Wl,--wrap=realloc -Wl,--wrap=reallocarray -Wl,--wrap=free -Wl,--wrap=strdup -Wl,--wrap=strndup -o libnand.so
	gcc -L. nand_example.o libnand.so -lnand -o example
	gcc -L. testy.o libnand.so -lnand -o test