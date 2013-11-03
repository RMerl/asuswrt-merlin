KERNEL_INCLUDE=/usr/src/linux/include


CC=gcc
CCOPT=-O2 -Wstrict-prototypes -Wall -Werror -fno-strict-aliasing -fno-common
CFLAGS=-DMODULE -D__KERNEL__ -I$(KERNEL_INCLUDE) $(CCOPT)


all: pg3.o

clean:
	@rm -f *.o
