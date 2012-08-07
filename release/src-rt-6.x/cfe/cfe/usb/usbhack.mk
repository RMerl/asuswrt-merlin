
OBJS = usbhack.o lib_malloc.o lib_queue.o ohci.o usbd.o \
	usbdevs.o usbhub.o usbdebug.o usbhid.o usbmass.o usbserial.o

CFLAGS = -I../include -g -Wall
VPATH = ../lib

%.o : %.c
	gcc $(CFLAGS) -c -o $@ $<

all : usbhack
	echo done

usbhack : $(OBJS)
	gcc -o usbhack $(OBJS)

lib_malloc.o : lib_malloc.c

usbhack.o : usbhack.c


clean :
	rm -f *.o usbhack
