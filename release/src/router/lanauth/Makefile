#CROSS ?= mipsel-uclibc-
CC ?= $(CROSS)gcc
STRIP ?= $(CROSS)strip

CFLAGS += -Os -Wall -I. -ffunction-sections -fdata-sections $(EXTRACFLAGS)
LDFLAGS += -Wl,--gc-sections

lanauth: lanauth.o md5.o rmd160.o
	$(CC) $(CFLAGS) $(LDFLAGS) -o $@ $^

%.o: %.c
	$(CC) $(CFLAGS) -c $<

clean:
	rm *.o lanauth

install: lanauth
	install -D lanauth $(INSTALLDIR)/usr/sbin/lanauth
	$(STRIP) $(INSTALLDIR)/usr/sbin/lanauth
