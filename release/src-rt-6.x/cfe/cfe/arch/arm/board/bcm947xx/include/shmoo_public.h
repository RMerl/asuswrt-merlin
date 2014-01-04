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
*  $Id:: shmoo_public.h 1758 2012-08-17 15:14:03Z gennady         $:
*  $Rev::file =  : Global SVN Revision = 1780                     $:
**
*/

/* shmoo_public.h */ 

#ifndef SHMOO_PUBLIC_H__
#define SHMOO_PUBLIC_H__

#ifndef DDR40_TYPES__
#define DDR40_TYPES__
typedef unsigned long   ddr40_addr_t;
#endif

#define SHMOO_RD_DLY 0    /* read delay */
#define SHMOO_RD_EN  1    /* read enable */
#define SHMOO_RD_DQS 2    /* read dqs */
#define SHMOO_WR_DQ  3    /* write dq */
#define SHMOO_WR_DM  4    /* write dm */
#define SHMOO_ADDR   5    /* write dm */


#define DO_SHMOO_RD_DLY  (1<<SHMOO_RD_DLY)
#define DO_SHMOO_RD_EN   (1<<SHMOO_RD_EN)
#define DO_SHMOO_RD_DQS  (1<<SHMOO_RD_DQS)
#define DO_SHMOO_WR_DQ   (1<<SHMOO_WR_DQ)
#define DO_SHMOO_WR_DM   (1<<SHMOO_WR_DM)
#define DO_CALIB_ADDR    (1<<SHMOO_ADDR)

#define DO_ALL_SHMOO (DO_SHMOO_RD_EN | DO_SHMOO_RD_DQS | DO_SHMOO_WR_DQ | \
	DO_SHMOO_WR_DM | DO_CALIB_ADDR | DO_SHMOO_RD_DLY)


/* SHMOO ERROR CODE */
/* An error is made up of ERR_HEADER + ERROR_CLASS + ERROR_TYPE + error_id */
#define SHMOO_NO_ERROR	0

#define RD_DLY_ERROR	DO_SHMOO_RD_DLY
#define RD_EN_ERROR	DO_SHMOO_RD_EN
#define RD_DQS_ERROR	DO_SHMOO_RD_DQS
#define WR_DQ_ERROR	DO_SHMOO_WR_DQ
#define WR_DM_ERROR	DO_SHMOO_WR_DM
#define ADDR_ERROR	DO_CALIB_ADDR


#define ERR_HEADER     0xDD000000
#define ERR_CLASS_CORE 0x00100000
#define ERR_CLASS_AUX  0x00200000
#define ERR_CLASS_SW   0x00300000
#define ERR_CLASS_DIS  0x00400000

#define ERR_TYPE_MIPS  0x00010000
#define ERR_TYPE_MSA   0x00020000
#define ERR_TYPE_API   0x00030000

/*
 * shmoo version 1.12
 */
#define ERR_DIS_TO                                 (1 << 16)
#define ERR_MRS_CMD_TO                             (2 << 16)
#define ERR_WR_CHAN_CALIB_LOCK_TO                  (3 << 16)
#define ERR_ADDR_SHMOO_FAILED                      (4 << 16)
#define ERR_AC_VDL_SHIFT_BYTE_MODE_NOT_SUPPORTED   (5 << 16)
#define ERR_AC_VDL_SHIFT_TOO_MUCH                  (6 << 16)
#define ERR_RDEN_BYTE_TO                           (7 << 16)
#define ERR_RDEN_BYTE1_TO                          (8 << 16)
#define ERR_RDEN_BYTE0_TO                          (9 << 16)
#define ERR_BAD_VDL                                (10<< 16)


/*
 * shmoo version 1.12
 *
 * Inputs:
 *   unsigned int phy_reg_base     base offset for PHY
 *   unsigned int memc_reg_base    base offset for MEMC
 *   unsigned int option           shmoo runtime options 
 *                                   [29:28]    : Address Shmoo Type
 *                                               0=normal.  <== DEFAULT 
 *                                               1=low-stress
 *                                               2=med-stress
 *                                               3=high-stress (takes about 40 secs)
 *                                   [27:24] : addx/ctrl initial VDL shift:
 *                                               0 = none   <== DEFAULT
 *                                               90deg * N/16
 *                                   [23:16] : log2( MEM_SIZE )-1.  I.E. 256MB => 27
 *                                   [15:08] : width  32 or 16  
 *                                   [07:00] : run time shmoo options
 *                                               5 = run Addr Calib shmoo
 * 
 *   unsigned int mem_test_base)   memory area for WR/RD testing
 */
int do_shmoo(void *sih, ddr40_addr_t phy_reg_base,
	ddr40_addr_t memc_reg_base, unsigned int option, ddr40_addr_t mem_test_base);

extern int rewrite_mode_registers(void *sih);

void DumpPhyRegisters(ddr40_addr_t phy_reg_base);

#endif /* SHMOO_PUBLIC_H__ */
