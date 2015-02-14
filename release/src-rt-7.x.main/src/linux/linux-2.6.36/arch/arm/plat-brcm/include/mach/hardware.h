/*
 * This header file is required by various archiecture common header files.
 */

#ifndef	__MACH_HARDWARE_H
#define	__MACH_HARDWARE_H	__FILE__

/* Minimum size of resource to allocate */
#define PCIBIOS_MIN_IO                  32
#define PCIBIOS_MIN_MEM                 32

/* Needed by drivers/pci/probe.c, tells to re-assign bus numbers if necesary */
#define pcibios_assign_all_busses()     1

#endif	/* __MACH_HARDWARE_H */
