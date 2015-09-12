#
# Makefile for the SystemH specific parts of the kernel
#

obj-y	 := setup.o irq.o io.o

# importantly, with the generic sh7751_pcic_init() code. For now, we'll
# just abuse the hell out of kbuild, because we can..

obj-$(CONFIG_PCI) += pci.o
pci-y := ../../se/7751/pci.o
