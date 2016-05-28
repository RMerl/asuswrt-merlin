SRCBASE ?= $(shell pwd)/../../../..
CC = gcc
CXX = g++

INCLUDEDIR = .
CFLAGS := -I$(INCLUDEDIR) -D_FILE_OFFSET_BITS=64 -D_LARGEFILE_SOURCE -D_GNU_SOURCE -O2

LZMADIR = $(SRCBASE)/tools/misc/lzma_src/C
LZMAOBJS = LzmaEnc.o LzFind.o LzmaDec.o
CFLAGS += -I$(LZMADIR)

all: mksquashfs-lzma unsquashfs

$(LZMAOBJS): %.o: $(LZMADIR)/%.c
	$(CC) -c $(CFLAGS) $^ -o $@

mksquashfs: mksquashfs.o read_fs.o sort.o sqlzma.o $(LZMAOBJS)
	$(CC) mksquashfs.o read_fs.o sort.o sqlzma.o $(LZMAOBJS) -lpthread -lz -lm -o $@

mksquashfs-lzma: mksquashfs
	ln $< $@

mksquashfs.o: mksquashfs.c squashfs_fs.h mksquashfs.h global.h sort.h

read_fs.o: read_fs.c squashfs_fs.h read_fs.h global.h

sort.o: sort.c squashfs_fs.h global.h sort.h

unsquashfs: unsquashfs.o sqlzma.o $(LZMAOBJS)
	$(CC) unsquashfs.o sqlzma.o $(LZMAOBJS) -lz -o $@

unsquashfs.o: unsquashfs.c squashfs_fs.h read_fs.h global.h

clean:
	-rm -f *.o mksquashfs unsquashfs mksquashfs-lzma $(LZMAOBJS)
