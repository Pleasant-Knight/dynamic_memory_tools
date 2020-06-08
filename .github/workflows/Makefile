# Michel Hanna: Makefile for libmemh test

CC = gcc
CFLAGS = -g -W -Wall
LIBS = -ldl -lc -lpthread
OBJS = libmemh.o mh_test_memh.o 

all: test_memh libmemh.o libmemh.so runtime_test

%.o: %.c
	$(CC) $(CFLAGS) -c -o $@ $<

libmemh.so: libmemh.c libmemh.h
	$(CC) $(CFLAGS) -fPIC --shared -o $@ libmemh.c

test_memh: $(OBJS)
	$(CC) $(CFLAGS) -o $@ $(OBJS) $(LIBS)

runtime_test: mh_test_memh.c
	$(CC) $(CFLAGS) -o $@ $^ -L. -lmemh -ldl

clean:
	rm -f *.o *.so test* runtime*
