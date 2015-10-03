CFLAGS = -Os -Wall $(EXTRACFLAGS)

all: tftpc tftpd

tftpc: tftpc.c
	$(CC) $(CFLAGS) -o tftpc tftpc.c

tftpd: tftpd.c
	$(CC) $(CFLAGS) -o tftpd tftpd.c

clean:
	rm -f tftpd tftpc

strip:
	strip tftpd tftpc

