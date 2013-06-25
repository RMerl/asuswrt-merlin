#
# Broadcom BCM57XX Gigabit Ethernet driver make file.
#
# $Id: Makefile,v 1.3 2008-05-02 22:49:54 $
#

B57SRCDIR := ../../../../../bcm57xx
SHAREDSRCDIR := ../../../../../shared

obj-$(CONFIG_BCM57XX) := bcm57xx.o

bcm57xx-objs := $(B57SRCDIR)/linux/b57um.o $(B57SRCDIR)/linux/b57proc.o \
		$(B57SRCDIR)/linux/b57diag.o \
		$(B57SRCDIR)/sys/tigon3.o $(B57SRCDIR)/sys/autoneg.o \
                $(B57SRCDIR)/sys/5701rls.o $(B57SRCDIR)/sys/tcp_seg.o \
		$(SHAREDSRCDIR)/hndgige.o

# If source directory src/bcm57xx exists, build objects from there
ifneq ($(wildcard $(src)/$(B57SRCDIR)/sys),)

EXTRA_CFLAGS += -DDBG=0 -DT3_JUMBO_RCV_RCB_ENTRY_COUNT=256 -DNICE_SUPPORT
EXTRA_CFLAGS += -DPCIX_TARGET_WORKAROUND=1 -DINCLUDE_TBI_SUPPORT -DINCLUDE_5701_AX_FIX=1
EXTRA_CFLAGS += -I$(src)/$(B57SRCDIR)/linux -I$(src)/$(B57SRCDIR)/sys \
		-I$(src)/$(B57SRCDIR)/include

EXTRA_CFLAGS += -DLINUX_HYBRID_POSTPROCESSING_REMOVAL
# Otherwise, use object module(s) in this directory
else

bcm57xx-objs := $(B57SRCDIR)/linux/bcm57xx.o

endif

ifneq ($(KERNELRELEASE),)
# kbuild part of makefile
else
# Normal makefile
KERNELDIR := ../../../../../linux/linux-2.6
all:
	$(MAKE) -C $(KERNELDIR) M=`pwd`

clean:
	$(MAKE) -C $(KERNELDIR) M=`pwd` clean
endif

clean-files += $(B57SRCDIR)/sys/*.o $(B57SRCDIR)/sys/.*.o.cmd \
	       $(B57SRCDIR)/linux/*.o $(B57SRCDIR)/linux/.*.o.cmd \
	       $(SHAREDSRCDIR)/hndgige.o $(SHAREDSRCDIR)/.hndgige.o.cmd
