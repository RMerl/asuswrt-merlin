include ../.config

CC = gcc
CFLAGS = -O3 -Wall
ifeq ($(BCMWL6A),y)
export CFLAGS += -DBCMWL6A
endif

all: trx_asus

trx_asus: trx_asus.c
	$(CC) $(CFLAGS) -o $@ $^

clean:
	rm -f *.o trx_asus piggy target.squashfs vmlinuz-lzma

%.o: %.c
	$(CC) $(CFLAGS) -c $<
