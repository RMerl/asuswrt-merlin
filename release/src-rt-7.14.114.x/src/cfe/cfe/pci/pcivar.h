/*
 * Copyright (c) 1994 Charles Hannum.  All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. All advertising materials mentioning features or use of this software
 *    must display the following acknowledgement:
 *	This product includes software developed by Charles Hannum.
 * 4. The name of the author may not be used to endorse or promote products
 *    derived from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 * NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef _PCIVAR_H_
#define _PCIVAR_H_

/*
 * Definitions for PCI autoconfiguration.
 *
 * The initial segment of this file describes types and functions
 * that are exported for PCI configuration and address mapping.  */

#include "pci_machdep.h"

/* From <cpu>_pci_machdep.c */

pcitag_t  pci_make_tag(int, int, int, int);
pcireg_t  pci_conf_read(pcitag_t, int);
void	  pci_conf_write(pcitag_t, int, pcireg_t);

typedef enum {
    PCI_MATCH_BYTES = 0,
    PCI_MATCH_BITS  = 1
} pci_endian_t;

int       pci_map_io(pcitag_t, int, pci_endian_t, phys_addr_t *);
int       pci_map_mem(pcitag_t, int, pci_endian_t, phys_addr_t *);

uint8_t   inb(unsigned int port);
uint16_t  inw(unsigned int port);
uint32_t  inl(unsigned int port);
void      outb(unsigned int port, uint8_t val);
void      outw(unsigned int port, uint16_t val);
void      outl(unsigned int port, uint32_t val);

int       pci_map_window(phys_addr_t pa,
			 unsigned int offset, unsigned int len,
			 int l2ca, int endian);
int       pci_unmap_window(unsigned int offset, unsigned int len);

/* From pciconf.c */

/* Flags controlling PCI/LDT configuration options */

typedef unsigned int pci_flags_t;

#define PCI_FLG_NORMAL	      0x00000001
#define PCI_FLG_VERBOSE       0x00000003
#define PCI_FLG_LDT_PREFETCH  0x00000004
#define PCI_FLG_LDT_REV_017   0x00000008

void	  pci_configure(pci_flags_t flags);
void      pci_show_configuration(void);
int	  pci_foreachdev(int (*fn)(pcitag_t tag));
int	  pci_cacheline_log2 (void);
int	  pci_maxburst_log2 (void);

int pci_find_device(uint32_t vid, uint32_t did, int enumidx, pcitag_t *tag);
int pci_find_class(uint32_t class, int enumidx, pcitag_t *tag);

/* From pci_subr.c */

void	  pci_devinfo(pcireg_t, pcireg_t, int, char *);
void	  pci_conf_print(pcitag_t tag);

void	  pci_tagprintf (pcitag_t tag, const char *fmt, ...)
#ifdef __long64
	    __attribute__((__format__(__printf__,2,3)))
#endif
            ;



/* Build options. Debug levels >1 are for helping to bring up new
   LDT hardware and will be annoyingly verbose otherwise. */

/* PCI_DEBUG enables general checking and tracing. */
#ifndef PCI_DEBUG
#define PCI_DEBUG 0
#endif

/* LDT_DEBUG enables progress/error reports for LDT fabric initialization. */
#ifndef LDT_DEBUG
#define LDT_DEBUG 0
#endif


struct pci_attach_args {
    pcitag_t pa_tag;
    pcireg_t pa_id;
    pcireg_t pa_class;
};

struct pci_match {
    pcireg_t	class, classmask;
    pcireg_t	id, idmask;
};

/* From <cpu>_pci_machdep.c */

int	  pci_hwinit(int port, pci_flags_t flags);
void	  pci_hwreinit(int port, pci_flags_t flags);
int	  pci_device_preset (pcitag_t tag);
void	  pci_device_setup(pcitag_t tag);
void      pci_bridge_setup(pcitag_t tag, pci_flags_t flags);
void      pci_flush(void);

void	  pci_break_tag(pcitag_t, int *, int *, int *, int *);

int	  pci_canscan(pcitag_t);
int	  pci_probe_tag(pcitag_t tag);

pcireg_t  pci_conf_read8(pcitag_t, int);
void	  pci_conf_write8(pcitag_t, int, pcireg_t);
pcireg_t  pci_conf_read16(pcitag_t, int);
void	  pci_conf_write16(pcitag_t, int, pcireg_t);
pcireg_t  pci_conf_read(pcitag_t, int);
void	  pci_conf_write(pcitag_t, int, pcireg_t);
#define   pci_conf_read32 	pci_conf_read
#define   pci_conf_write32	pci_conf_write
int       pci_conf_write_acked(pcitag_t, int, pcireg_t);

uint8_t   pci_int_line(uint8_t);


/* From <board>_pci.c */

uint8_t   pci_int_shift_0(pcitag_t);
uint8_t   pci_int_map_0(pcitag_t);


/* From ldtinit.c */

#define LDT_PRIMARY    0
#define LDT_SECONDARY  1
unsigned pci_find_ldt_cap (pcitag_t tag, int secondary);

void ldt_link_reset (pcitag_t tag, int delay);
int  ldt_chain_init (pcitag_t tag, int port, int bus, pci_flags_t flags);


/* PCI bus parameters */
struct pci_bus {
    unsigned char	min_gnt;	/* largest min grant */
    unsigned char	max_lat;	/* smallest max latency */
    unsigned char	devsel;		/* slowest devsel */
    char		fast_b2b;	/* support fast b2b */
    char		prefetch;	/* support prefetch */
    char		freq66;		/* support 66MHz */
    char		width64;	/* 64 bit bus */
    int			bandwidth;	/* # of .25us ticks/sec @ 33MHz */
    unsigned char	ndev;		/* # devices (functions) on bus */
    unsigned char	def_ltim;	/* default ltim counter */
    unsigned char	max_ltim;	/* maximum ltim counter */
    uint8_t		port;		/* index of host bridge */
    uint8_t		primary;	/* primary bus number */
    pcitag_t		tag;		/* tag for the bridge to this bus */
    uint32_t		min_io_addr;	/* min I/O address allocated to bus */
    uint32_t		max_io_addr;	/* max I/O address allocated to bus */
    uint32_t		min_mem_addr;	/* min mem address allocated to bus */
    uint32_t		max_mem_addr;	/* max mem address allocated to bus */
    uint8_t		inta_shift;	/* base rotation of interrupt pins */
    char                no_probe;       /* skip businit and query probes */ 
};

/* From <cpu>_pci_machdep.c */

int pci_nextbus(int port);
int pci_maxbus(int port);
struct pci_bus *pci_businfo(int port, int bus);

pcireg_t pci_minmemaddr(int port);
pcireg_t pci_maxmemaddr(int port);
pcireg_t pci_minioaddr(int port);
pcireg_t pci_maxioaddr(int port);

#endif /* _PCIVAR_H_ */
