/*
 * Macros, definitions, and data structures specific to the IBM PowerPC
 * STB03xxx "Redwood" evaluation board.
 *
 * Author: Armin Kuster <akuster@mvista.com>
 *
 * 2001 (c) MontaVista, Software, Inc.  This file is licensed under
 * the terms of the GNU General Public License version 2.  This program
 * is licensed "as is" without any warranty of any kind, whether express
 * or implied.
 */

#ifdef __KERNEL__
#ifndef __ASM_REDWOOD5_H__
#define __ASM_REDWOOD5_H__

/* Redwood5 has an STB04xxx core */
#include <platforms/4xx/ibmstb4.h>

#ifndef __ASSEMBLY__
typedef struct board_info {
	unsigned char	bi_s_version[4];	/* Version of this structure */
	unsigned char	bi_r_version[30];	/* Version of the IBM ROM */
	unsigned int	bi_memsize;		/* DRAM installed, in bytes */
	unsigned int	bi_dummy;		/* field shouldn't exist */
	unsigned char	bi_enetaddr[6];		/* Ethernet MAC address */
	unsigned int	bi_intfreq;		/* Processor speed, in Hz */
	unsigned int	bi_busfreq;		/* Bus speed, in Hz */
	unsigned int	bi_tbfreq;		/* Software timebase freq */
} bd_t;
#endif /* !__ASSEMBLY__ */


#define SMC91111_BASE_ADDR	0xf2000300
#define SMC91111_REG_SIZE	16
#define SMC91111_IRQ		28

#ifdef MAX_HWIFS
#undef MAX_HWIFS
#endif
#define MAX_HWIFS		1

#define _IO_BASE	0
#define _ISA_MEM_BASE	0
#define PCI_DRAM_OFFSET	0

#define BASE_BAUD		(378000000 / 18 / 16)

#define PPC4xx_MACHINE_NAME	"IBM Redwood5"

#endif /* __ASM_REDWOOD5_H__ */
#endif /* __KERNEL__ */
