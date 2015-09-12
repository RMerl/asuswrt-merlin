/*  *********************************************************************
    *  SB1250 Board Support Package
    *  
    *  JTAG Constants and Macros		File: sb1250_jtag.h
    *  
    *  This module contains constants and macros useful for
    *  manipulating the System Control and Debug module on the 1250.
    *  
    *  SB1250 specification level:  User's manual 1/02/02
    *  
    *  Author:  Kip Walker (kwalker@broadcom.com)
    *  
    *********************************************************************  
    *
    *  Copyright 2000,2001,2002,2003
    *  Broadcom Corporation. All rights reserved.
    *  
    *  This software is furnished under license and may be used and 
    *  copied only in accordance with the following terms and 
    *  conditions.  Subject to these conditions, you may download, 
    *  copy, install, use, modify and distribute modified or unmodified 
    *  copies of this software in source and/or binary form.  No title 
    *  or ownership is transferred hereby.
    *  
    *  1) Any source code used, modified or distributed must reproduce 
    *     and retain this copyright notice and list of conditions 
    *     as they appear in the source file.
    *  
    *  2) No right is granted to use any trade name, trademark, or 
    *     logo of Broadcom Corporation.  The "Broadcom Corporation" 
    *     name may not be used to endorse or promote products derived 
    *     from this software without the prior written permission of 
    *     Broadcom Corporation.
    *  
    *  3) THIS SOFTWARE IS PROVIDED "AS-IS" AND ANY EXPRESS OR
    *     IMPLIED WARRANTIES, INCLUDING BUT NOT LIMITED TO, ANY IMPLIED
    *     WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR 
    *     PURPOSE, OR NON-INFRINGEMENT ARE DISCLAIMED. IN NO EVENT 
    *     SHALL BROADCOM BE LIABLE FOR ANY DAMAGES WHATSOEVER, AND IN 
    *     PARTICULAR, BROADCOM SHALL NOT BE LIABLE FOR DIRECT, INDIRECT,
    *     INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES 
    *     (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE
    *     GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR
    *     BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY 
    *     OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR 
    *     TORT (INCLUDING NEGLIGENCE OR OTHERWISE), EVEN IF ADVISED OF 
    *     THE POSSIBILITY OF SUCH DAMAGE.
    ********************************************************************* */

#ifndef _SB1250_JTAG_H
#define _SB1250_JTAG_H

#include "sb1250_defs.h"

#define SB1250_IDCODE_VAL  0x112502a1
#define SB1250_IMPCODE_VAL 0x20814001

/*
 * JTAG Memory region
 */

#define K_SCD_JTAG_MEMBASE        0x0010000000
#define K_SCD_JTAG_MEMSIZE        0x0000020000
#define K_SCD_JTAG_MEMTOP         (K_SCD_JTAG_MEMBASE+K_SCD_JTAG_MEMSIZE)


/*
 * JTAG Instruction Register values
 */

#define SB1250_EXTEST     0x00
#define SB1250_IDCODE     0x01
#define SB1250_IMPCODE    0x03
#define SB1250_ADDRESS    0x08
#define SB1250_DATA       0x09
#define SB1250_CONTROL    0x0A
#define SB1250_EJTAGALL   0x0B
#define SB1250_EJTAGBOOT  0x0C
#define SB1250_NORMALBOOT 0x0D
#define SB1250_SYSCTRL    0x20
#define SB1250_TRACE      0x21
#define SB1250_PERF       0x22
#define SB1250_TRCTRL     0x23
#define SB1250_WAFERID    0x24
#define SB1250_PMON       0x25
#define SB1250_CPU0OSC    0x26
#define SB1250_CPU0DSC    0x27
#define SB1250_CPU0TSC    0x28
#define SB1250_CPU1OSC    0x2A
#define SB1250_CPU1DSC    0x2B
#define SB1250_CPU1TSC    0x2C
#define SB1250_SCANIOB0   0x2E
#define SB1250_SCANIOB1   0x30
#define SB1250_SCANL2C    0x32
#define SB1250_SCANMC     0x34
#define SB1250_SCANSCD    0x36
#define SB1250_SCANALL    0x38
#define SB1250_BSRMODE    0x3A
#define SB1250_SCANTRCCNT 0x3B
#define SB1250_CLAMP      0x3C
#define SB1250_SAMPLE     0x3D
#define SB1250_INTEST     0x3E
#define SB1250_BYPASS     0x3F

/* 
 * IDCODE
 */

#define S_JTAG_REVISION              _SB_MAKE32(28)
#define M_JTAG_REVISION              _SB_MAKEMASK(4,S_JTAG_REVISION)
#define V_JTAG_REVISION(x)           _SB_MAKEVALUE(x,S_JTAG_REVISION)
#define G_JTAG_REVISION(x)           _SB_GETVALUE(x,S_JTAG_REVISION,M_JTAG_REVISION)

#define S_JTAG_PARTNUM               _SB_MAKE32(12)
#define M_JTAG_PARTNUM               _SB_MAKEMASK(16,S_JTAG_PARTNUM)
#define V_JTAG_PARTNUM(x)            _SB_MAKEVALUE(x,S_JTAG_PARTNUM)
#define G_JTAG_PARTNUM(x)            _SB_GETVALUE(x,S_JTAG_PARTNUM,M_JTAG_PARTNUM)

/* This field corresponds to system revision SOC_TYPE field */
#define S_JTAG_PART_TYPE             _SB_MAKE32(12)
#define M_JTAG_PART_TYPE             _SB_MAKEMASK(4,S_JTAG_PART_TYPE)
#define V_JTAG_PART_TYPE(x)          _SB_MAKEVALUE(x,S_JTAG_PART_TYPE)
#define G_JTAG_PART_TYPE(x)          _SB_GETVALUE(x,S_JTAG_PART_TYPE,M_JTAG_PART_TYPE)

#define S_JTAG_PART_L2               _SB_MAKE32(16)
#define M_JTAG_PART_L2               _SB_MAKEMASK(4,S_JTAG_PART_L2)
#define V_JTAG_PART_L2(x)            _SB_MAKEVALUE(x,S_JTAG_PART_L2)
#define G_JTAG_PART_L2(x)            _SB_GETVALUE(x,S_JTAG_PART_L2,M_JTAG_PART_L2)

#define K_JTAG_PART_L2_512           5
#define K_JTAG_PART_L2_256           2
#define K_JTAG_PART_L2_128           1

#define S_JTAG_PART_PERIPH           _SB_MAKE32(12)
#define M_JTAG_PART_PERIPH           _SB_MAKEMASK(4,S_JTAG_PART_PERIPH)
#define V_JTAG_PART_PERIPH(x)        _SB_MAKEVALUE(x,S_JTAG_PART_PERIPH)
#define G_JTAG_PART_PERIPH(x)        _SB_GETVALUE(x,S_JTAG_PART_PERIPH,M_JTAG_PART_PERIPH)

#define K_JTAG_PART_PERIPH_PCIHT     4
#define K_JTAG_PART_PERIPH_PCI       3
#define K_JTAG_PART_PERIPH_NONE      1


/*
 * EJTAG Control Register (Table 15-14)
 */

#define M_JTAG_CR_DM0             _SB_MAKEMASK1(0)
#define M_JTAG_CR_DM1             _SB_MAKEMASK1(1)
#define M_JTAG_CR_EJTAGBreak0     _SB_MAKEMASK1(2)
#define M_JTAG_CR_EJTAGBreak1     _SB_MAKEMASK1(3)
#define M_JTAG_CR_PrTrap0         _SB_MAKEMASK1(4)
#define M_JTAG_CR_PrTrap1         _SB_MAKEMASK1(5)
#define M_JTAG_CR_ProbEn          _SB_MAKEMASK1(6)
#define M_JTAG_CR_PrAcc           _SB_MAKEMASK1(7)
#define M_JTAG_CR_PW              _SB_MAKEMASK1(8)
#define M_JTAG_CR_PbAcc           _SB_MAKEMASK1(9)
#define M_JTAG_CR_MaSl            _SB_MAKEMASK1(10)
#define M_JTAG_CR_ClkStopped      _SB_MAKEMASK1(11)

#define M_JTAG_CR_READ_ONLY       (M_JTAG_CR_DM0 | M_JTAG_CR_DM1 | M_JTAG_CR_ClkStopped)

#define G_JTAG_CR_EJTAGBreak(cpu) (M_JTAG_CR_EJTAGBreak0 << (cpu))
#define G_JTAG_CR_PrTrap(cpu)     (M_JTAG_CR_PrTrap0 << (cpu))

/*
 * System Config "extension" bits 104:64 (Table 15-5)
 */

#define S_SYS_PLLPHASE              0
#define M_SYS_PLLPHASE              _SB_MAKEMASK(2,S_SYS_PLLPHASE)
#define V_SYS_PLLPHASE(x)           _SB_MAKEVALUE(x,S_SYS_PLLPHASE)
#define G_SYS_PLLPHASE(x)           _SB_GETVALUE(x,S_SYS_PLLPHASE,M_SYS_PLLPHASE)

#define S_SYS_PLLCOUNT              2
#define M_SYS_PLLCOUNT              _SB_MAKEMASK(30,S_SYS_PLLCOUNT)
#define V_SYS_PLLCOUNT(x)           _SB_MAKEVALUE(x,S_SYS_PLLCOUNT)
#define G_SYS_PLLCOUNT(x)           _SB_GETVALUE(x,S_SYS_PLLCOUNT,M_SYS_PLLCOUNT)

#define M_SYS_PLLSTOP               _SB_MAKEMASK1(32)
#define M_SYS_STOPSTRETCH           _SB_MAKEMASK1(33)
#define M_SYS_STARTCOND             _SB_MAKEMASK1(34)
#define M_SYS_STOPPING              _SB_MAKEMASK1(35)
#define M_SYS_STOPSTRDONE           _SB_MAKEMASK1(36)
#define M_SYS_SERZB_ARD             _SB_MAKEMASK1(37)
#define M_SYS_SERZB_AR              _SB_MAKEMASK1(38)

#define S_SYS_STRETCHMODE           39
#define M_SYS_STRETCHMODE           _SB_MAKEMASK(2,S_SYS_STRETCHMODE)
#define V_SYS_STRETCHMODE(x)        _SB_MAKEVALUE(x,S_SYS_STRETCHMODE)
#define G_SYS_STRETCHMODE(x)        _SB_GETVALUE(x,S_SYS_STRETCHMODE,M_SYS_STRETCHMODE)

/*
 * ZBbus definitions
 */

#define K_ZB_CMD_READ_SHD	0
#define K_ZB_CMD_READ_EXC	1
#define K_ZB_CMD_WRITE		2
#define K_ZB_CMD_WRITEINV	3
#define K_ZB_CMD_INV		4
#define K_ZB_CMD_NOP		7

#define K_ZB_L1CA_CNCOH		0
#define K_ZB_L1CA_CCOH		1
#define K_ZB_L1CA_UNC		2
#define K_ZB_L1CA_UNC1		3

#define K_ZB_L2CA_NOALLOC	0
#define K_ZB_L2CA_ALLOC		1

#define K_ZB_DMOD_CLEAN		0
#define K_ZB_DMOD_DIRTY		1

#define K_ZB_DCODE_NOP		0
#define K_ZB_DCODE_VLD		1
#define K_ZB_DCODE_VLD_TCORR	2
#define K_ZB_DCODE_VLD_DCORR	3
#define K_ZB_DCODE_BUSERR	4
#define K_ZB_DCODE_FATAL_BUSERR	5
#define K_ZB_DCODE_TAG_UNCORR	6
#define K_ZB_DCODE_DATA_UNCORR	7

#endif
