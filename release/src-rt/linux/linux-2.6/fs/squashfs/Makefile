#
# Makefile for the linux squashfs routines.
#
CFLAGS += -I $(SRCBASE)/tools/misc/lzma_src/C -DBCMLZMA

obj-$(CONFIG_SQUASHFS) += squashfs.o
squashfs-y += inode.o
squashfs-y += squashfs2_0.o
squashfs-y += sqlzma.o
