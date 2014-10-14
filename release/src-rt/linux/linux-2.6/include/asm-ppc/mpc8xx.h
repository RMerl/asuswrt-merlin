/* This is the single file included by all MPC8xx build options.
 * Since there are many different boards and no standard configuration,
 * we have a unique include file for each.  Rather than change every
 * file that has to include MPC8xx configuration, they all include
 * this one and the configuration switching is done here.
 */
#ifdef __KERNEL__
#ifndef __CONFIG_8xx_DEFS
#define __CONFIG_8xx_DEFS


#ifdef CONFIG_8xx

#ifdef CONFIG_MBX
#include <platforms/mbx.h>
#endif

#ifdef CONFIG_FADS
#include <platforms/fads.h>
#endif

#ifdef CONFIG_RPXLITE
#include <platforms/rpxlite.h>
#endif

#ifdef CONFIG_BSEIP
#include <platforms/bseip.h>
#endif

#ifdef CONFIG_RPXCLASSIC
#include <platforms/rpxclassic.h>
#endif

#if defined(CONFIG_TQM8xxL)
#include <platforms/tqm8xx.h>
#endif

#if defined(CONFIG_IVMS8) || defined(CONFIG_IVML24)
#include <platforms/ivms8.h>
#endif

#if defined(CONFIG_HERMES_PRO)
#include <platforms/hermes.h>
#endif

#if defined(CONFIG_IP860)
#include <platforms/ip860.h>
#endif

#if defined(CONFIG_LWMON)
#include <platforms/lwmon.h>
#endif

#if defined(CONFIG_PCU_E)
#include <platforms/pcu_e.h>
#endif

#if defined(CONFIG_CCM)
#include <platforms/ccm.h>
#endif

#if defined(CONFIG_LANTEC)
#include <platforms/lantec.h>
#endif

#if defined(CONFIG_MPC885ADS)
#include <platforms/mpc885ads.h>
#endif

/* Currently, all 8xx boards that support a processor to PCI/ISA bridge
 * use the same memory map.
 */
#if 0
#if defined(CONFIG_PCI) && defined(PCI_ISA_IO_ADDR)
#define	_IO_BASE PCI_ISA_IO_ADDR
#define	_ISA_MEM_BASE PCI_ISA_MEM_ADDR
#define PCI_DRAM_OFFSET 0x80000000
#else
#define _IO_BASE        0
#define _ISA_MEM_BASE   0
#define PCI_DRAM_OFFSET 0
#endif
#else
#if !defined(_IO_BASE)  /* defined in board specific header */
#define _IO_BASE        0
#endif
#define _ISA_MEM_BASE   0
#define PCI_DRAM_OFFSET 0
#endif

#ifndef __ASSEMBLY__
/* The "residual" data board information structure the boot loader
 * hands to us.
 */
extern unsigned char __res[];

struct pt_regs;

enum ppc_sys_devices {
	MPC8xx_CPM_FEC1,
	MPC8xx_CPM_FEC2,
	MPC8xx_CPM_I2C,
	MPC8xx_CPM_SCC1,
	MPC8xx_CPM_SCC2,
	MPC8xx_CPM_SCC3,
	MPC8xx_CPM_SCC4,
	MPC8xx_CPM_SPI,
	MPC8xx_CPM_MCC1,
	MPC8xx_CPM_MCC2,
	MPC8xx_CPM_SMC1,
	MPC8xx_CPM_SMC2,
	MPC8xx_CPM_USB,
	MPC8xx_MDIO_FEC,
	NUM_PPC_SYS_DEVS,
};

#define PPC_PIN_SIZE	(24 * 1024 * 1024)	/* 24Mbytes of data pinned */

#ifndef BOARD_CHIP_NAME
#define BOARD_CHIP_NAME ""
#endif

#endif /* !__ASSEMBLY__ */
#endif /* CONFIG_8xx */
#endif /* __CONFIG_8xx_DEFS */
#endif /* __KERNEL__ */
