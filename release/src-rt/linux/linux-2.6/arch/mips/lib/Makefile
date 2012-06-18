#
# Makefile for MIPS-specific library files..
#

lib-y	+= csum_partial.o delay.o memcpy.o memcpy-inatomic.o memset.o \
	   strlen_user.o strncpy_user.o strnlen_user.o uncached.o memcmp.o

obj-y			+= iomap.o
obj-$(CONFIG_PCI)	+= iomap-pci.o

# libgcc-style stuff needed in the kernel
obj-y += ashldi3.o ashrdi3.o lshrdi3.o ucmpdi2.o
