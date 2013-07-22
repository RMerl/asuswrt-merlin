CC = gcc
CFLAGS = -O3 -Wall

all: trx_asus

trx_asus: trx_asus.c
	$(CC) $(CFLAGS) -o $@ $^

clean:
	rm -f *.o trx_asus piggy target.squashfs vmlinuz-lzma

%.o: %.c
	$(CC) $(CFLAGS) -c $<
