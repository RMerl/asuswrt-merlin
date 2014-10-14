/*
 * Copyright (c) 2006, 2007 QLogic Corporation. All rights reserved.
 * Copyright (c) 2003, 2004, 2005, 2006 PathScale, Inc. All rights reserved.
 *
 * This software is available to you under a choice of one of two
 * licenses.  You may choose to be licensed under the terms of the GNU
 * General Public License (GPL) Version 2, available from the file
 * COPYING in the main directory of this source tree, or the
 * OpenIB.org BSD license below:
 *
 *     Redistribution and use in source and binary forms, with or
 *     without modification, are permitted provided that the following
 *     conditions are met:
 *
 *      - Redistributions of source code must retain the above
 *        copyright notice, this list of conditions and the following
 *        disclaimer.
 *
 *      - Redistributions in binary form must reproduce the above
 *        copyright notice, this list of conditions and the following
 *        disclaimer in the documentation and/or other materials
 *        provided with the distribution.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
 * EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
 * MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
 * NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS
 * BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN
 * ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN
 * CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 */

#ifndef _IPATH_REGISTERS_H
#define _IPATH_REGISTERS_H

/*
 * This file should only be included by kernel source, and by the diags.  It
 * defines the registers, and their contents, for InfiniPath chips.
 */

/*
 * These are the InfiniPath register and buffer bit definitions,
 * that are visible to software, and needed only by the kernel
 * and diag code.  A few, that are visible to protocol and user
 * code are in ipath_common.h.  Some bits are specific
 * to a given chip implementation, and have been moved to the
 * chip-specific source file
 */

/* kr_revision bits */
#define INFINIPATH_R_CHIPREVMINOR_MASK 0xFF
#define INFINIPATH_R_CHIPREVMINOR_SHIFT 0
#define INFINIPATH_R_CHIPREVMAJOR_MASK 0xFF
#define INFINIPATH_R_CHIPREVMAJOR_SHIFT 8
#define INFINIPATH_R_ARCH_MASK 0xFF
#define INFINIPATH_R_ARCH_SHIFT 16
#define INFINIPATH_R_SOFTWARE_MASK 0xFF
#define INFINIPATH_R_SOFTWARE_SHIFT 24
#define INFINIPATH_R_BOARDID_MASK 0xFF
#define INFINIPATH_R_BOARDID_SHIFT 32

/* kr_control bits */
#define INFINIPATH_C_FREEZEMODE 0x00000002
#define INFINIPATH_C_LINKENABLE 0x00000004

/* kr_sendctrl bits */
#define INFINIPATH_S_DISARMPIOBUF_SHIFT 16
#define INFINIPATH_S_UPDTHRESH_SHIFT 24
#define INFINIPATH_S_UPDTHRESH_MASK 0x1f

#define IPATH_S_ABORT		0
#define IPATH_S_PIOINTBUFAVAIL	1
#define IPATH_S_PIOBUFAVAILUPD	2
#define IPATH_S_PIOENABLE	3
#define IPATH_S_SDMAINTENABLE	9
#define IPATH_S_SDMASINGLEDESCRIPTOR	10
#define IPATH_S_SDMAENABLE	11
#define IPATH_S_SDMAHALT	12
#define IPATH_S_DISARM		31

#define INFINIPATH_S_ABORT		(1U << IPATH_S_ABORT)
#define INFINIPATH_S_PIOINTBUFAVAIL	(1U << IPATH_S_PIOINTBUFAVAIL)
#define INFINIPATH_S_PIOBUFAVAILUPD	(1U << IPATH_S_PIOBUFAVAILUPD)
#define INFINIPATH_S_PIOENABLE		(1U << IPATH_S_PIOENABLE)
#define INFINIPATH_S_SDMAINTENABLE	(1U << IPATH_S_SDMAINTENABLE)
#define INFINIPATH_S_SDMASINGLEDESCRIPTOR \
					(1U << IPATH_S_SDMASINGLEDESCRIPTOR)
#define INFINIPATH_S_SDMAENABLE		(1U << IPATH_S_SDMAENABLE)
#define INFINIPATH_S_SDMAHALT		(1U << IPATH_S_SDMAHALT)
#define INFINIPATH_S_DISARM		(1U << IPATH_S_DISARM)

/* kr_rcvctrl bits that are the same on multiple chips */
#define INFINIPATH_R_PORTENABLE_SHIFT 0
#define INFINIPATH_R_QPMAP_ENABLE (1ULL << 38)

/* kr_intstatus, kr_intclear, kr_intmask bits */
#define INFINIPATH_I_SDMAINT		0x8000000000000000ULL
#define INFINIPATH_I_SDMADISABLED	0x4000000000000000ULL
#define INFINIPATH_I_ERROR		0x0000000080000000ULL
#define INFINIPATH_I_SPIOSENT		0x0000000040000000ULL
#define INFINIPATH_I_SPIOBUFAVAIL	0x0000000020000000ULL
#define INFINIPATH_I_GPIO		0x0000000010000000ULL
#define INFINIPATH_I_JINT		0x0000000004000000ULL

/* kr_errorstatus, kr_errorclear, kr_errormask bits */
#define INFINIPATH_E_RFORMATERR			0x0000000000000001ULL
#define INFINIPATH_E_RVCRC			0x0000000000000002ULL
#define INFINIPATH_E_RICRC			0x0000000000000004ULL
#define INFINIPATH_E_RMINPKTLEN			0x0000000000000008ULL
#define INFINIPATH_E_RMAXPKTLEN			0x0000000000000010ULL
#define INFINIPATH_E_RLONGPKTLEN		0x0000000000000020ULL
#define INFINIPATH_E_RSHORTPKTLEN		0x0000000000000040ULL
#define INFINIPATH_E_RUNEXPCHAR			0x0000000000000080ULL
#define INFINIPATH_E_RUNSUPVL			0x0000000000000100ULL
#define INFINIPATH_E_REBP			0x0000000000000200ULL
#define INFINIPATH_E_RIBFLOW			0x0000000000000400ULL
#define INFINIPATH_E_RBADVERSION		0x0000000000000800ULL
#define INFINIPATH_E_RRCVEGRFULL		0x0000000000001000ULL
#define INFINIPATH_E_RRCVHDRFULL		0x0000000000002000ULL
#define INFINIPATH_E_RBADTID			0x0000000000004000ULL
#define INFINIPATH_E_RHDRLEN			0x0000000000008000ULL
#define INFINIPATH_E_RHDR			0x0000000000010000ULL
#define INFINIPATH_E_RIBLOSTLINK		0x0000000000020000ULL
#define INFINIPATH_E_SENDSPECIALTRIGGER		0x0000000008000000ULL
#define INFINIPATH_E_SDMADISABLED		0x0000000010000000ULL
#define INFINIPATH_E_SMINPKTLEN			0x0000000020000000ULL
#define INFINIPATH_E_SMAXPKTLEN			0x0000000040000000ULL
#define INFINIPATH_E_SUNDERRUN			0x0000000080000000ULL
#define INFINIPATH_E_SPKTLEN			0x0000000100000000ULL
#define INFINIPATH_E_SDROPPEDSMPPKT		0x0000000200000000ULL
#define INFINIPATH_E_SDROPPEDDATAPKT		0x0000000400000000ULL
#define INFINIPATH_E_SPIOARMLAUNCH		0x0000000800000000ULL
#define INFINIPATH_E_SUNEXPERRPKTNUM		0x0000001000000000ULL
#define INFINIPATH_E_SUNSUPVL			0x0000002000000000ULL
#define INFINIPATH_E_SENDBUFMISUSE		0x0000004000000000ULL
#define INFINIPATH_E_SDMAGENMISMATCH		0x0000008000000000ULL
#define INFINIPATH_E_SDMAOUTOFBOUND		0x0000010000000000ULL
#define INFINIPATH_E_SDMATAILOUTOFBOUND		0x0000020000000000ULL
#define INFINIPATH_E_SDMABASE			0x0000040000000000ULL
#define INFINIPATH_E_SDMA1STDESC		0x0000080000000000ULL
#define INFINIPATH_E_SDMARPYTAG			0x0000100000000000ULL
#define INFINIPATH_E_SDMADWEN			0x0000200000000000ULL
#define INFINIPATH_E_SDMAMISSINGDW		0x0000400000000000ULL
#define INFINIPATH_E_SDMAUNEXPDATA		0x0000800000000000ULL
#define INFINIPATH_E_IBSTATUSCHANGED		0x0001000000000000ULL
#define INFINIPATH_E_INVALIDADDR		0x0002000000000000ULL
#define INFINIPATH_E_RESET			0x0004000000000000ULL
#define INFINIPATH_E_HARDWARE			0x0008000000000000ULL
#define INFINIPATH_E_SDMADESCADDRMISALIGN	0x0010000000000000ULL
#define INFINIPATH_E_INVALIDEEPCMD		0x0020000000000000ULL

/*
 * this is used to print "common" packet errors only when the
 * __IPATH_ERRPKTDBG bit is set in ipath_debug.
 */
#define INFINIPATH_E_PKTERRS ( INFINIPATH_E_SPKTLEN \
		| INFINIPATH_E_SDROPPEDDATAPKT | INFINIPATH_E_RVCRC \
		| INFINIPATH_E_RICRC | INFINIPATH_E_RSHORTPKTLEN \
		| INFINIPATH_E_REBP )

/* Convenience for decoding Send DMA errors */
#define INFINIPATH_E_SDMAERRS ( \
	INFINIPATH_E_SDMAGENMISMATCH | INFINIPATH_E_SDMAOUTOFBOUND | \
	INFINIPATH_E_SDMATAILOUTOFBOUND | INFINIPATH_E_SDMABASE | \
	INFINIPATH_E_SDMA1STDESC | INFINIPATH_E_SDMARPYTAG | \
	INFINIPATH_E_SDMADWEN | INFINIPATH_E_SDMAMISSINGDW | \
	INFINIPATH_E_SDMAUNEXPDATA | \
	INFINIPATH_E_SDMADESCADDRMISALIGN | \
	INFINIPATH_E_SDMADISABLED | \
	INFINIPATH_E_SENDBUFMISUSE)

/* kr_hwerrclear, kr_hwerrmask, kr_hwerrstatus, bits */
/* TXEMEMPARITYERR bit 0: PIObuf, 1: PIOpbc, 2: launchfifo
 * RXEMEMPARITYERR bit 0: rcvbuf, 1: lookupq, 2:  expTID, 3: eagerTID
 * 		bit 4: flag buffer, 5: datainfo, 6: header info */
#define INFINIPATH_HWE_TXEMEMPARITYERR_MASK 0xFULL
#define INFINIPATH_HWE_TXEMEMPARITYERR_SHIFT 40
#define INFINIPATH_HWE_RXEMEMPARITYERR_MASK 0x7FULL
#define INFINIPATH_HWE_RXEMEMPARITYERR_SHIFT 44
#define INFINIPATH_HWE_IBCBUSTOSPCPARITYERR 0x4000000000000000ULL
#define INFINIPATH_HWE_IBCBUSFRSPCPARITYERR 0x8000000000000000ULL
/* txe mem parity errors (shift by INFINIPATH_HWE_TXEMEMPARITYERR_SHIFT) */
#define INFINIPATH_HWE_TXEMEMPARITYERR_PIOBUF	0x1ULL
#define INFINIPATH_HWE_TXEMEMPARITYERR_PIOPBC	0x2ULL
#define INFINIPATH_HWE_TXEMEMPARITYERR_PIOLAUNCHFIFO 0x4ULL
/* rxe mem parity errors (shift by INFINIPATH_HWE_RXEMEMPARITYERR_SHIFT) */
#define INFINIPATH_HWE_RXEMEMPARITYERR_RCVBUF   0x01ULL
#define INFINIPATH_HWE_RXEMEMPARITYERR_LOOKUPQ  0x02ULL
#define INFINIPATH_HWE_RXEMEMPARITYERR_EXPTID   0x04ULL
#define INFINIPATH_HWE_RXEMEMPARITYERR_EAGERTID 0x08ULL
#define INFINIPATH_HWE_RXEMEMPARITYERR_FLAGBUF  0x10ULL
#define INFINIPATH_HWE_RXEMEMPARITYERR_DATAINFO 0x20ULL
#define INFINIPATH_HWE_RXEMEMPARITYERR_HDRINFO  0x40ULL
/* waldo specific -- find the rest in ipath_6110.c */
#define INFINIPATH_HWE_RXDSYNCMEMPARITYERR  0x0000000400000000ULL
/* 6120/7220 specific -- find the rest in ipath_6120.c and ipath_7220.c */
#define INFINIPATH_HWE_MEMBISTFAILED	0x0040000000000000ULL

/* kr_hwdiagctrl bits */
#define INFINIPATH_DC_FORCETXEMEMPARITYERR_MASK 0xFULL
#define INFINIPATH_DC_FORCETXEMEMPARITYERR_SHIFT 40
#define INFINIPATH_DC_FORCERXEMEMPARITYERR_MASK 0x7FULL
#define INFINIPATH_DC_FORCERXEMEMPARITYERR_SHIFT 44
#define INFINIPATH_DC_FORCERXDSYNCMEMPARITYERR  0x0000000400000000ULL
#define INFINIPATH_DC_COUNTERDISABLE            0x1000000000000000ULL
#define INFINIPATH_DC_COUNTERWREN               0x2000000000000000ULL
#define INFINIPATH_DC_FORCEIBCBUSTOSPCPARITYERR 0x4000000000000000ULL
#define INFINIPATH_DC_FORCEIBCBUSFRSPCPARITYERR 0x8000000000000000ULL

/* kr_ibcctrl bits */
#define INFINIPATH_IBCC_FLOWCTRLPERIOD_MASK 0xFFULL
#define INFINIPATH_IBCC_FLOWCTRLPERIOD_SHIFT 0
#define INFINIPATH_IBCC_FLOWCTRLWATERMARK_MASK 0xFFULL
#define INFINIPATH_IBCC_FLOWCTRLWATERMARK_SHIFT 8
#define INFINIPATH_IBCC_LINKINITCMD_MASK 0x3ULL
#define INFINIPATH_IBCC_LINKINITCMD_DISABLE 1
/* cycle through TS1/TS2 till OK */
#define INFINIPATH_IBCC_LINKINITCMD_POLL 2
/* wait for TS1, then go on */
#define INFINIPATH_IBCC_LINKINITCMD_SLEEP 3
#define INFINIPATH_IBCC_LINKINITCMD_SHIFT 16
#define INFINIPATH_IBCC_LINKCMD_MASK 0x3ULL
#define INFINIPATH_IBCC_LINKCMD_DOWN 1		/* move to 0x11 */
#define INFINIPATH_IBCC_LINKCMD_ARMED 2		/* move to 0x21 */
#define INFINIPATH_IBCC_LINKCMD_ACTIVE 3	/* move to 0x31 */
#define INFINIPATH_IBCC_LINKCMD_SHIFT 18
#define INFINIPATH_IBCC_MAXPKTLEN_MASK 0x7FFULL
#define INFINIPATH_IBCC_MAXPKTLEN_SHIFT 20
#define INFINIPATH_IBCC_PHYERRTHRESHOLD_MASK 0xFULL
#define INFINIPATH_IBCC_PHYERRTHRESHOLD_SHIFT 32
#define INFINIPATH_IBCC_OVERRUNTHRESHOLD_MASK 0xFULL
#define INFINIPATH_IBCC_OVERRUNTHRESHOLD_SHIFT 36
#define INFINIPATH_IBCC_CREDITSCALE_MASK 0x7ULL
#define INFINIPATH_IBCC_CREDITSCALE_SHIFT 40
#define INFINIPATH_IBCC_LOOPBACK             0x8000000000000000ULL
#define INFINIPATH_IBCC_LINKDOWNDEFAULTSTATE 0x4000000000000000ULL

/* kr_ibcstatus bits */
#define INFINIPATH_IBCS_LINKTRAININGSTATE_SHIFT 0
#define INFINIPATH_IBCS_LINKSTATE_MASK 0x7

#define INFINIPATH_IBCS_TXREADY       0x40000000
#define INFINIPATH_IBCS_TXCREDITOK    0x80000000
/* link training states (shift by
   INFINIPATH_IBCS_LINKTRAININGSTATE_SHIFT) */
#define INFINIPATH_IBCS_LT_STATE_DISABLED	0x00
#define INFINIPATH_IBCS_LT_STATE_LINKUP		0x01
#define INFINIPATH_IBCS_LT_STATE_POLLACTIVE	0x02
#define INFINIPATH_IBCS_LT_STATE_POLLQUIET	0x03
#define INFINIPATH_IBCS_LT_STATE_SLEEPDELAY	0x04
#define INFINIPATH_IBCS_LT_STATE_SLEEPQUIET	0x05
#define INFINIPATH_IBCS_LT_STATE_CFGDEBOUNCE	0x08
#define INFINIPATH_IBCS_LT_STATE_CFGRCVFCFG	0x09
#define INFINIPATH_IBCS_LT_STATE_CFGWAITRMT	0x0a
#define INFINIPATH_IBCS_LT_STATE_CFGIDLE	0x0b
#define INFINIPATH_IBCS_LT_STATE_RECOVERRETRAIN	0x0c
#define INFINIPATH_IBCS_LT_STATE_RECOVERWAITRMT	0x0e
#define INFINIPATH_IBCS_LT_STATE_RECOVERIDLE	0x0f
/* link state machine states (shift by ibcs_ls_shift) */
#define INFINIPATH_IBCS_L_STATE_DOWN		0x0
#define INFINIPATH_IBCS_L_STATE_INIT		0x1
#define INFINIPATH_IBCS_L_STATE_ARM		0x2
#define INFINIPATH_IBCS_L_STATE_ACTIVE		0x3
#define INFINIPATH_IBCS_L_STATE_ACT_DEFER	0x4


/* kr_extstatus bits */
#define INFINIPATH_EXTS_SERDESPLLLOCK 0x1
#define INFINIPATH_EXTS_GPIOIN_MASK 0xFFFFULL
#define INFINIPATH_EXTS_GPIOIN_SHIFT 48

/* kr_extctrl bits */
#define INFINIPATH_EXTC_GPIOINVERT_MASK 0xFFFFULL
#define INFINIPATH_EXTC_GPIOINVERT_SHIFT 32
#define INFINIPATH_EXTC_GPIOOE_MASK 0xFFFFULL
#define INFINIPATH_EXTC_GPIOOE_SHIFT 48
#define INFINIPATH_EXTC_SERDESENABLE         0x80000000ULL
#define INFINIPATH_EXTC_SERDESCONNECT        0x40000000ULL
#define INFINIPATH_EXTC_SERDESENTRUNKING     0x20000000ULL
#define INFINIPATH_EXTC_SERDESDISRXFIFO      0x10000000ULL
#define INFINIPATH_EXTC_SERDESENPLPBK1       0x08000000ULL
#define INFINIPATH_EXTC_SERDESENPLPBK2       0x04000000ULL
#define INFINIPATH_EXTC_SERDESENENCDEC       0x02000000ULL
#define INFINIPATH_EXTC_LED1SECPORT_ON       0x00000020ULL
#define INFINIPATH_EXTC_LED2SECPORT_ON       0x00000010ULL
#define INFINIPATH_EXTC_LED1PRIPORT_ON       0x00000008ULL
#define INFINIPATH_EXTC_LED2PRIPORT_ON       0x00000004ULL
#define INFINIPATH_EXTC_LEDGBLOK_ON          0x00000002ULL
#define INFINIPATH_EXTC_LEDGBLERR_OFF        0x00000001ULL

/* kr_partitionkey bits */
#define INFINIPATH_PKEY_SIZE 16
#define INFINIPATH_PKEY_MASK 0xFFFF
#define INFINIPATH_PKEY_DEFAULT_PKEY 0xFFFF

/* kr_serdesconfig0 bits */
#define INFINIPATH_SERDC0_RESET_MASK  0xfULL	/* overal reset bits */
#define INFINIPATH_SERDC0_RESET_PLL   0x10000000ULL	/* pll reset */
/* tx idle enables (per lane) */
#define INFINIPATH_SERDC0_TXIDLE      0xF000ULL
/* rx detect enables (per lane) */
#define INFINIPATH_SERDC0_RXDETECT_EN 0xF0000ULL
/* L1 Power down; use with RXDETECT, Otherwise not used on IB side */
#define INFINIPATH_SERDC0_L1PWR_DN	 0xF0ULL

/* common kr_xgxsconfig bits (or safe in all, even if not implemented) */
#define INFINIPATH_XGXS_RX_POL_SHIFT 19
#define INFINIPATH_XGXS_RX_POL_MASK 0xfULL


/*
 * IPATH_PIO_MAXIBHDR is the max IB header size allowed for in our
 * PIO send buffers.  This is well beyond anything currently
 * defined in the InfiniBand spec.
 */
#define IPATH_PIO_MAXIBHDR 128

typedef u64 ipath_err_t;

/* The following change with the type of device, so
 * need to be part of the ipath_devdata struct, or
 * we could have problems plugging in devices of
 * different types (e.g. one HT, one PCIE)
 * in one system, to be managed by one driver.
 * On the other hand, this file is may also be included
 * by other code, so leave the declarations here
 * temporarily. Minor footprint issue if common-model
 * linker used, none if C89+ linker used.
 */

/* mask of defined bits for various registers */
extern u64 infinipath_i_bitsextant;
extern ipath_err_t infinipath_e_bitsextant, infinipath_hwe_bitsextant;

/* masks that are different in various chips, or only exist in some chips */
extern u32 infinipath_i_rcvavail_mask, infinipath_i_rcvurg_mask;

/*
 * These are the infinipath general register numbers (not offsets).
 * The kernel registers are used directly, those beyond the kernel
 * registers are calculated from one of the base registers.  The use of
 * an integer type doesn't allow type-checking as thorough as, say,
 * an enum but allows for better hiding of chip differences.
 */
typedef const u16 ipath_kreg,	/* infinipath general registers */
 ipath_creg,			/* infinipath counter registers */
 ipath_sreg;			/* kernel-only, infinipath send registers */

/*
 * These are the chip registers common to all infinipath chips, and
 * used both by the kernel and the diagnostics or other user code.
 * They are all implemented such that 64 bit accesses work.
 * Some implement no more than 32 bits.  Because 64 bit reads
 * require 2 HT cmds on opteron, we access those with 32 bit
 * reads for efficiency (they are written as 64 bits, since
 * the extra 32 bits are nearly free on writes, and it slightly reduces
 * complexity).  The rest are all accessed as 64 bits.
 */
struct ipath_kregs {
	/* These are the 32 bit group */
	ipath_kreg kr_control;
	ipath_kreg kr_counterregbase;
	ipath_kreg kr_intmask;
	ipath_kreg kr_intstatus;
	ipath_kreg kr_pagealign;
	ipath_kreg kr_portcnt;
	ipath_kreg kr_rcvtidbase;
	ipath_kreg kr_rcvtidcnt;
	ipath_kreg kr_rcvegrbase;
	ipath_kreg kr_rcvegrcnt;
	ipath_kreg kr_scratch;
	ipath_kreg kr_sendctrl;
	ipath_kreg kr_sendpiobufbase;
	ipath_kreg kr_sendpiobufcnt;
	ipath_kreg kr_sendpiosize;
	ipath_kreg kr_sendregbase;
	ipath_kreg kr_userregbase;
	/* These are the 64 bit group */
	ipath_kreg kr_debugport;
	ipath_kreg kr_debugportselect;
	ipath_kreg kr_errorclear;
	ipath_kreg kr_errormask;
	ipath_kreg kr_errorstatus;
	ipath_kreg kr_extctrl;
	ipath_kreg kr_extstatus;
	ipath_kreg kr_gpio_clear;
	ipath_kreg kr_gpio_mask;
	ipath_kreg kr_gpio_out;
	ipath_kreg kr_gpio_status;
	ipath_kreg kr_hwdiagctrl;
	ipath_kreg kr_hwerrclear;
	ipath_kreg kr_hwerrmask;
	ipath_kreg kr_hwerrstatus;
	ipath_kreg kr_ibcctrl;
	ipath_kreg kr_ibcstatus;
	ipath_kreg kr_intblocked;
	ipath_kreg kr_intclear;
	ipath_kreg kr_interruptconfig;
	ipath_kreg kr_mdio;
	ipath_kreg kr_partitionkey;
	ipath_kreg kr_rcvbthqp;
	ipath_kreg kr_rcvbufbase;
	ipath_kreg kr_rcvbufsize;
	ipath_kreg kr_rcvctrl;
	ipath_kreg kr_rcvhdrcnt;
	ipath_kreg kr_rcvhdrentsize;
	ipath_kreg kr_rcvhdrsize;
	ipath_kreg kr_rcvintmembase;
	ipath_kreg kr_rcvintmemsize;
	ipath_kreg kr_revision;
	ipath_kreg kr_sendbuffererror;
	ipath_kreg kr_sendpioavailaddr;
	ipath_kreg kr_serdesconfig0;
	ipath_kreg kr_serdesconfig1;
	ipath_kreg kr_serdesstatus;
	ipath_kreg kr_txintmembase;
	ipath_kreg kr_txintmemsize;
	ipath_kreg kr_xgxsconfig;
	ipath_kreg kr_ibpllcfg;
	/* use these two (and the following N ports) only with
	 * ipath_k*_kreg64_port(); not *kreg64() */
	ipath_kreg kr_rcvhdraddr;
	ipath_kreg kr_rcvhdrtailaddr;

	/* remaining registers are not present on all types of infinipath
	   chips  */
	ipath_kreg kr_rcvpktledcnt;
	ipath_kreg kr_pcierbuftestreg0;
	ipath_kreg kr_pcierbuftestreg1;
	ipath_kreg kr_pcieq0serdesconfig0;
	ipath_kreg kr_pcieq0serdesconfig1;
	ipath_kreg kr_pcieq0serdesstatus;
	ipath_kreg kr_pcieq1serdesconfig0;
	ipath_kreg kr_pcieq1serdesconfig1;
	ipath_kreg kr_pcieq1serdesstatus;
	ipath_kreg kr_hrtbt_guid;
	ipath_kreg kr_ibcddrctrl;
	ipath_kreg kr_ibcddrstatus;
	ipath_kreg kr_jintreload;

	/* send dma related regs */
	ipath_kreg kr_senddmabase;
	ipath_kreg kr_senddmalengen;
	ipath_kreg kr_senddmatail;
	ipath_kreg kr_senddmahead;
	ipath_kreg kr_senddmaheadaddr;
	ipath_kreg kr_senddmabufmask0;
	ipath_kreg kr_senddmabufmask1;
	ipath_kreg kr_senddmabufmask2;
	ipath_kreg kr_senddmastatus;

	/* SerDes related regs (IBA7220-only) */
	ipath_kreg kr_ibserdesctrl;
	ipath_kreg kr_ib_epbacc;
	ipath_kreg kr_ib_epbtrans;
	ipath_kreg kr_pcie_epbacc;
	ipath_kreg kr_pcie_epbtrans;
	ipath_kreg kr_ib_ddsrxeq;
};

struct ipath_cregs {
	ipath_creg cr_badformatcnt;
	ipath_creg cr_erricrccnt;
	ipath_creg cr_errlinkcnt;
	ipath_creg cr_errlpcrccnt;
	ipath_creg cr_errpkey;
	ipath_creg cr_errrcvflowctrlcnt;
	ipath_creg cr_err_rlencnt;
	ipath_creg cr_errslencnt;
	ipath_creg cr_errtidfull;
	ipath_creg cr_errtidvalid;
	ipath_creg cr_errvcrccnt;
	ipath_creg cr_ibstatuschange;
	ipath_creg cr_intcnt;
	ipath_creg cr_invalidrlencnt;
	ipath_creg cr_invalidslencnt;
	ipath_creg cr_lbflowstallcnt;
	ipath_creg cr_iblinkdowncnt;
	ipath_creg cr_iblinkerrrecovcnt;
	ipath_creg cr_ibsymbolerrcnt;
	ipath_creg cr_pktrcvcnt;
	ipath_creg cr_pktrcvflowctrlcnt;
	ipath_creg cr_pktsendcnt;
	ipath_creg cr_pktsendflowcnt;
	ipath_creg cr_portovflcnt;
	ipath_creg cr_rcvebpcnt;
	ipath_creg cr_rcvovflcnt;
	ipath_creg cr_rxdroppktcnt;
	ipath_creg cr_senddropped;
	ipath_creg cr_sendstallcnt;
	ipath_creg cr_sendunderruncnt;
	ipath_creg cr_unsupvlcnt;
	ipath_creg cr_wordrcvcnt;
	ipath_creg cr_wordsendcnt;
	ipath_creg cr_vl15droppedpktcnt;
	ipath_creg cr_rxotherlocalphyerrcnt;
	ipath_creg cr_excessbufferovflcnt;
	ipath_creg cr_locallinkintegrityerrcnt;
	ipath_creg cr_rxvlerrcnt;
	ipath_creg cr_rxdlidfltrcnt;
	ipath_creg cr_psstat;
	ipath_creg cr_psstart;
	ipath_creg cr_psinterval;
	ipath_creg cr_psrcvdatacount;
	ipath_creg cr_psrcvpktscount;
	ipath_creg cr_psxmitdatacount;
	ipath_creg cr_psxmitpktscount;
	ipath_creg cr_psxmitwaitcount;
};

#endif				/* _IPATH_REGISTERS_H */
