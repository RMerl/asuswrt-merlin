#
# Makefile for the linux YAFFS filesystem routines.
#

YAFFS2_SRCDIR := ../../../../../../src-rt-7.14.114.x/src/yaffs2

obj-$(CONFIG_YAFFS_FS) += yaffs.o

yaffs-y := $(YAFFS2_SRCDIR)/yaffs_ecc.o $(YAFFS2_SRCDIR)/yaffs_vfs.o $(YAFFS2_SRCDIR)/yaffs_guts.o $(YAFFS2_SRCDIR)/yaffs_checkptrw.o
yaffs-y += $(YAFFS2_SRCDIR)/yaffs_packedtags1.o $(YAFFS2_SRCDIR)/yaffs_packedtags2.o $(YAFFS2_SRCDIR)/yaffs_nand.o
yaffs-y += $(YAFFS2_SRCDIR)/yaffs_tagscompat.o $(YAFFS2_SRCDIR)/yaffs_tagsmarshall.o
yaffs-y += $(YAFFS2_SRCDIR)/yaffs_mtdif.o
yaffs-y += $(YAFFS2_SRCDIR)/yaffs_nameval.o $(YAFFS2_SRCDIR)/yaffs_attribs.o
yaffs-y += $(YAFFS2_SRCDIR)/yaffs_allocator.o
yaffs-y += $(YAFFS2_SRCDIR)/yaffs_yaffs1.o
yaffs-y += $(YAFFS2_SRCDIR)/yaffs_yaffs2.o
yaffs-y += $(YAFFS2_SRCDIR)/yaffs_bitmap.o
yaffs-y += $(YAFFS2_SRCDIR)/yaffs_summary.o
yaffs-y += $(YAFFS2_SRCDIR)/yaffs_verify.o

clean-files += $(YAFFS2_SRCDIR)/*.o $(YAFFS2_SRCDIR)/.*.o.cmd
