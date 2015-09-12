
CC = gcc
CFLAGS = -I ../include

all : mkbootimage installboot

mkbootimage : mkbootimage.c
	$(CC) $(CFLAGS) -o mkbootimage mkbootimage.c

installboot : installboot.c
	$(CC) $(CFLAGS) -o installboot installboot.c
