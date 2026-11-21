CC ?= cc
AR ?= ar
CFLAGS ?= -std=c11 -O2 -Wall -Wextra -Wpedantic

.PHONY: all clean test example format

all: libcine.a

cine.o: cine.c cine.h
	$(CC) $(CFLAGS) -c cine.c -o cine.o

libcine.a: cine.o
	$(AR) rcs libcine.a cine.o

tests/test_cine: tests/test_cine.c libcine.a cine.h
	$(CC) $(CFLAGS) -I. tests/test_cine.c libcine.a -o tests/test_cine

test: tests/test_cine
	./tests/test_cine

examples/read_config: examples/read_config.c libcine.a cine.h
	$(CC) $(CFLAGS) -I. examples/read_config.c libcine.a -o examples/read_config

example: examples/read_config
	./examples/read_config

format:
	clang-format -i cine.c cine.h tests/test_cine.c examples/read_config.c

clean:
	rm -f cine.o libcine.a tests/test_cine examples/read_config
