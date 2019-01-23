all: tests

CFLAGS=-O3 -g -Wall

pthreadpool.o: pthreadpool.c pthreadpool.h
	gcc -c -O3 -o pthreadpool.o pthreadpool.c -I../../..

tests: tests.o pthreadpool.o
	gcc -o tests tests.o pthreadpool.o -lpthread