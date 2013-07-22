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

#ifndef _PCI_INTERNAL_H_
#define _PCI_INTERNAL_H_

/*
 * Definitions for PCI autoconfiguration.
 *
 * This file describes types and functions which are used only for
 * communication among the PCI modules implementing autoconfiguration.
 */

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

#ifndef CLOCK_GEARING        /* Temporarily here, should be CONFIG option */
#define CLOCK_GEARING 1
#endif

/* From <cpu>_pci_machdep.c */

int	  pci_hwinit(int port, pci_flags_t flags);
void	  pci_hwreinit(int port, pci_flags_t flags);
void	  pci_businit (int port, int bus, int probe_limit, pci_flags_t flags);
void	  pci_businit_hostbridge (pcitag_t tag, pci_flags_t flags);
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

unsigned  pci_msi_index(void);
void      pci_msi_encode(unsigned, uint64_t *, uint16_t *);


/* From <board>_pci.c */

uint8_t   pci_int_shift_0(pcitag_t);
uint8_t   pci_int_map_0(pcitag_t);

/* The following are needed only for boards with chips supporting PCI-X. */

void      pci_clock_reset(void);
void      pci_clock_enable(int);
unsigned int pci_clock_select(unsigned int);


/* From ldtinit.c */

#define LDT_PRIMARY    0
#define LDT_SECONDARY  1
unsigned pci_find_ldt_cap (pcitag_t tag, int secondary);

void ldt_link_reset (pcitag_t tag, int delay);
int  ldt_chain_init (pcitag_t tag, int port, int bus, pci_flags_t flags);

/* From <cpu>_pci_machdep.c */

int pci_maxport(void);
int pci_nextbus(int port);
int pci_maxbus(int port);
struct pci_bus *pci_businfo(int port, int bus);

pcireg_t pci_minmemaddr(int port);
pcireg_t pci_maxmemaddr(int port);
pcireg_t pci_minioaddr(int port);
pcireg_t pci_maxioaddr(int port);

#endif /* _PCI_INTERNAL_H_ */
