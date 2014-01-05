/*
** Copyright 2000, 2001  Broadcom Corporation
** All Rights Reserved
**
** No portions of this material may be reproduced in any form 
** without the written permission of:
**
**   Broadcom Corporation
**   5300 California Avenue
**   Irvine, California 92617
**
** All information contained in this document is Broadcom 
** Corporation company private proprietary, and trade secret.
**
** ----------------------------------------------------------
**
** 
**
** $Id::                                                          $:
** $Rev::file =  : Global SVN Revision = 1780                     $:
**
*/

#ifndef PLATFORM_H__
#define PLATFORM_H__

/* Define some default miscellaneous macros (to remove compiler's warning) */ 
#ifndef NOT_DEFINED
#define NOT_DEFINED  1
#endif

/* Public declaration of the shmoo algorithm implementation
 */
#include <shmoo_public.h>

/* Declaration of the low-level interface used by the shmoo algorithm.
 * This interface may be implemented or redefined differently on different platforms,
 * see platform specific platform_xxx.h and platform_xxx.c files
 */
/* #include "platform_shmoo_priv.h" */

/*
 * Include API file with low-leval external API that need to be implemented for particular platform
 */
/* DEFINE TYPES */
#define uint32_t unsigned int
#define int32_t int
#define true  1

#ifndef ARRAY_SIZE
#define  ARRAY_SIZE(A) (sizeof(A) / sizeof(A[0]))
#endif

/*
 * Redefine TIMEOUT_VAL const
 */
#undef TIMEOUT_VAL
#define TIMEOUT_VAL	(9000)


/*
 * PHY_REG_OFFSET_PER_WL is the offset in address for the two identical registers in difference WL
 * For BLAST, this difference is 0x200
 * This is for support a loop to do the same procedures for different WL
 */
#define PHY_REG_OFFSET_PER_WL 0x200
#define PHY_REG_OFFSET_PER_BL 0x0A0


/*
 * Define for "read eye" test
 *
 * 1 - build shmoo procedure application
 * 0 - build "read eye" test application without shmoo.
 * It is artifact of sharing code with another platform where there was not enough memory
 * to build both shmoo and "read eye" applications.
 * for the S2_A platform SHMOO_BUILD will always be 1
 */
#define SHMOO_BUILD				1


/*
 * Redefine function style cast that is used in the tinymt.* files
 */
#undef UINT32_C
#define UINT32_C(V) ((uint32_t)(V))

#undef UINT64_C
#define UINT64_C(V) ((uint64_t)(V))

#ifndef FALSE
#define FALSE		0
#endif
#ifndef TRUE
#define TRUE		(!(FALSE))
#endif

/*
 * Include original RDB file
 */

/*
 * PHY platform independent registers description
 */
#include "ddr40_phy_registers.h"

/*
 * Output functions for 2-dimensional printing of the logs using Linux printk() function
 */

#define PHY_VER_D	0

/*
 * DRAM read/write functions
 * Undefine existing macros
 */
#undef SHMOO_DRAM_READ_32
#undef SHMOO_DRAM_WRITE_32

unsigned int SHMOO_DRAM_READ_32(ddr40_addr_t Address);
void SHMOO_DRAM_WRITE_32(ddr40_addr_t Addres, unsigned int Data);


/*
 * Those defines are not used in S2_A platform because start address of the DRAM range
 * is dynamic
 */
#undef SHMOO_DRAM_START_ADDR
#define SHMOO_DRAM_START_ADDR	(0x80000000u)

#undef SHMOO_DRAM_END_ADDR
#define SHMOO_DRAM_END_ADDR	(SHMOO_DRAM_START_ADDR + 0x00008000u)


/*
 * Registers read/write functions
 * Undefine existing macros and replace them with the functions
 */
#undef tb_r
#undef tb_w

unsigned int tb_r(ddr40_addr_t Address);
void tb_w(ddr40_addr_t Addres, unsigned int Data);

/*
 * Undefine NL macro and replace it with the function
 */
#undef NL
void NL(void);

/*
 * Redefine macros from the PHY init code to use functions that will print
 * C-style messages
 */
#ifdef BCMDBG
void PrintfLog(char * const ptFormatStr, ...);
void PrintfErr(char * const ptFormatStr, ...);
void PrintfFatal(char * const ptFormatStr, ...);

#define  print_log   PrintfLog
#define  error_log   PrintfErr
#define  fatal_log   PrintfFatal
#else
#define  print_log(args...)
#define  error_log(args...)
#define  fatal_log(args...)
#endif


/*
 * It is 0 because DDR PHY RDB will have physical base address (not relative)
 */
#define GLOBAL_REG_RBUS_START	(0)

/*
 * This define is not necessary for the S2_A platform because it has other 
 * memory controller
 */
#define MEMC_BASE_ADDR		(0)

/*
 * This is the base address of the PHY registers.
 * First block of the registers is control registers
 * two following are WL 0 and WL 1 registers
 */
#define PHY_CONTROL_BASE_ADDRESS	(DDR_R_DDR40_PHY_ADDR_CTL_REVISION_MEMADDR)


/*
 * Verify some defines
 */
#if defined(SHMOO_DRAM_START_ADDR)
#if (SHMOO_DRAM_START_ADDR == NOT_DEFINED)
#error DRAM start address is not redefined
#endif
#else
#error DRAM start address is not defined
#endif /* defined(SHMOO_DRAM_START_ADDR) */

#if defined(SHMOO_DRAM_END_ADDR)
#if (SHMOO_DRAM_END_ADDR == NOT_DEFINED)
#error DRAM end address is not redefined
#endif
#else
#error DRAM end address is not defined
#endif /* defined(SHMOO_DRAM_END_ADDR) */

#ifndef GLOBAL_REG_RBUS_START
#error Registers address is not defined
#endif

#ifndef PHY_CONTROL_BASE_ADDRESS
#error PHY Control registers base address is not defined
#endif

#ifndef MEMC_BASE_ADDR
#error MEMC base address is not defined
#endif

void timeout_ns(unsigned int nsec);
#endif /* PLATFORM_H__ */
