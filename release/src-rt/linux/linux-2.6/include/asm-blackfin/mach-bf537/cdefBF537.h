/*
 * File:         include/asm-blackfin/mach-bf537/cdefBF537.h
 * Based on:
 * Author:
 *
 * Created:
 * Description:
 *	System MMR Register Map
 * Rev:
 *
 * Modified:
 *
 *
 * Bugs:         Enter bugs at http://blackfin.uclinux.org/
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2, or (at your option)
 * any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; see the file COPYING.
 * If not, write to the Free Software Foundation,
 * 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
 */

#ifndef _CDEF_BF537_H
#define _CDEF_BF537_H

/* Include MMRs Common to BF534 								*/
#include "cdefBF534.h"

/* Include all Core registers and bit definitions 									*/
#include "defBF537.h"

/* Include Macro "Defines" For EMAC (Unique to BF536/BF537		*/
/* 10/100 Ethernet Controller	(0xFFC03000 - 0xFFC031FF) 						*/
#define bfin_read_EMAC_OPMODE()              bfin_read32(EMAC_OPMODE)
#define bfin_write_EMAC_OPMODE(val)          bfin_write32(EMAC_OPMODE,val)
#define bfin_read_EMAC_ADDRLO()              bfin_read32(EMAC_ADDRLO)
#define bfin_write_EMAC_ADDRLO(val)          bfin_write32(EMAC_ADDRLO,val)
#define bfin_read_EMAC_ADDRHI()              bfin_read32(EMAC_ADDRHI)
#define bfin_write_EMAC_ADDRHI(val)          bfin_write32(EMAC_ADDRHI,val)
#define bfin_read_EMAC_HASHLO()              bfin_read32(EMAC_HASHLO)
#define bfin_write_EMAC_HASHLO(val)          bfin_write32(EMAC_HASHLO,val)
#define bfin_read_EMAC_HASHHI()              bfin_read32(EMAC_HASHHI)
#define bfin_write_EMAC_HASHHI(val)          bfin_write32(EMAC_HASHHI,val)
#define bfin_read_EMAC_STAADD()              bfin_read32(EMAC_STAADD)
#define bfin_write_EMAC_STAADD(val)          bfin_write32(EMAC_STAADD,val)
#define bfin_read_EMAC_STADAT()              bfin_read32(EMAC_STADAT)
#define bfin_write_EMAC_STADAT(val)          bfin_write32(EMAC_STADAT,val)
#define bfin_read_EMAC_FLC()                 bfin_read32(EMAC_FLC)
#define bfin_write_EMAC_FLC(val)             bfin_write32(EMAC_FLC,val)
#define bfin_read_EMAC_VLAN1()               bfin_read32(EMAC_VLAN1)
#define bfin_write_EMAC_VLAN1(val)           bfin_write32(EMAC_VLAN1,val)
#define bfin_read_EMAC_VLAN2()               bfin_read32(EMAC_VLAN2)
#define bfin_write_EMAC_VLAN2(val)           bfin_write32(EMAC_VLAN2,val)
#define bfin_read_EMAC_WKUP_CTL()            bfin_read32(EMAC_WKUP_CTL)
#define bfin_write_EMAC_WKUP_CTL(val)        bfin_write32(EMAC_WKUP_CTL,val)
#define bfin_read_EMAC_WKUP_FFMSK0()         bfin_read32(EMAC_WKUP_FFMSK0)
#define bfin_write_EMAC_WKUP_FFMSK0(val)     bfin_write32(EMAC_WKUP_FFMSK0,val)
#define bfin_read_EMAC_WKUP_FFMSK1()         bfin_read32(EMAC_WKUP_FFMSK1)
#define bfin_write_EMAC_WKUP_FFMSK1(val)     bfin_write32(EMAC_WKUP_FFMSK1,val)
#define bfin_read_EMAC_WKUP_FFMSK2()         bfin_read32(EMAC_WKUP_FFMSK2)
#define bfin_write_EMAC_WKUP_FFMSK2(val)     bfin_write32(EMAC_WKUP_FFMSK2,val)
#define bfin_read_EMAC_WKUP_FFMSK3()         bfin_read32(EMAC_WKUP_FFMSK3)
#define bfin_write_EMAC_WKUP_FFMSK3(val)     bfin_write32(EMAC_WKUP_FFMSK3,val)
#define bfin_read_EMAC_WKUP_FFCMD()          bfin_read32(EMAC_WKUP_FFCMD)
#define bfin_write_EMAC_WKUP_FFCMD(val)      bfin_write32(EMAC_WKUP_FFCMD,val)
#define bfin_read_EMAC_WKUP_FFOFF()          bfin_read32(EMAC_WKUP_FFOFF)
#define bfin_write_EMAC_WKUP_FFOFF(val)      bfin_write32(EMAC_WKUP_FFOFF,val)
#define bfin_read_EMAC_WKUP_FFCRC0()         bfin_read32(EMAC_WKUP_FFCRC0)
#define bfin_write_EMAC_WKUP_FFCRC0(val)     bfin_write32(EMAC_WKUP_FFCRC0,val)
#define bfin_read_EMAC_WKUP_FFCRC1()         bfin_read32(EMAC_WKUP_FFCRC1)
#define bfin_write_EMAC_WKUP_FFCRC1(val)     bfin_write32(EMAC_WKUP_FFCRC1,val)

#define bfin_read_EMAC_SYSCTL()              bfin_read32(EMAC_SYSCTL)
#define bfin_write_EMAC_SYSCTL(val)          bfin_write32(EMAC_SYSCTL,val)
#define bfin_read_EMAC_SYSTAT()              bfin_read32(EMAC_SYSTAT)
#define bfin_write_EMAC_SYSTAT(val)          bfin_write32(EMAC_SYSTAT,val)
#define bfin_read_EMAC_RX_STAT()             bfin_read32(EMAC_RX_STAT)
#define bfin_write_EMAC_RX_STAT(val)         bfin_write32(EMAC_RX_STAT,val)
#define bfin_read_EMAC_RX_STKY()             bfin_read32(EMAC_RX_STKY)
#define bfin_write_EMAC_RX_STKY(val)         bfin_write32(EMAC_RX_STKY,val)
#define bfin_read_EMAC_RX_IRQE()             bfin_read32(EMAC_RX_IRQE)
#define bfin_write_EMAC_RX_IRQE(val)         bfin_write32(EMAC_RX_IRQE,val)
#define bfin_read_EMAC_TX_STAT()             bfin_read32(EMAC_TX_STAT)
#define bfin_write_EMAC_TX_STAT(val)         bfin_write32(EMAC_TX_STAT,val)
#define bfin_read_EMAC_TX_STKY()             bfin_read32(EMAC_TX_STKY)
#define bfin_write_EMAC_TX_STKY(val)         bfin_write32(EMAC_TX_STKY,val)
#define bfin_read_EMAC_TX_IRQE()             bfin_read32(EMAC_TX_IRQE)
#define bfin_write_EMAC_TX_IRQE(val)         bfin_write32(EMAC_TX_IRQE,val)

#define bfin_read_EMAC_MMC_CTL()             bfin_read32(EMAC_MMC_CTL)
#define bfin_write_EMAC_MMC_CTL(val)         bfin_write32(EMAC_MMC_CTL,val)
#define bfin_read_EMAC_MMC_RIRQS()           bfin_read32(EMAC_MMC_RIRQS)
#define bfin_write_EMAC_MMC_RIRQS(val)       bfin_write32(EMAC_MMC_RIRQS,val)
#define bfin_read_EMAC_MMC_RIRQE()           bfin_read32(EMAC_MMC_RIRQE)
#define bfin_write_EMAC_MMC_RIRQE(val)       bfin_write32(EMAC_MMC_RIRQE,val)
#define bfin_read_EMAC_MMC_TIRQS()           bfin_read32(EMAC_MMC_TIRQS)
#define bfin_write_EMAC_MMC_TIRQS(val)       bfin_write32(EMAC_MMC_TIRQS,val)
#define bfin_read_EMAC_MMC_TIRQE()           bfin_read32(EMAC_MMC_TIRQE)
#define bfin_write_EMAC_MMC_TIRQE(val)       bfin_write32(EMAC_MMC_TIRQE,val)

#define bfin_read_EMAC_RXC_OK()              bfin_read32(EMAC_RXC_OK)
#define bfin_write_EMAC_RXC_OK(val)          bfin_write32(EMAC_RXC_OK,val)
#define bfin_read_EMAC_RXC_FCS()             bfin_read32(EMAC_RXC_FCS)
#define bfin_write_EMAC_RXC_FCS(val)         bfin_write32(EMAC_RXC_FCS,val)
#define bfin_read_EMAC_RXC_ALIGN()           bfin_read32(EMAC_RXC_ALIGN)
#define bfin_write_EMAC_RXC_ALIGN(val)       bfin_write32(EMAC_RXC_ALIGN,val)
#define bfin_read_EMAC_RXC_OCTET()           bfin_read32(EMAC_RXC_OCTET)
#define bfin_write_EMAC_RXC_OCTET(val)       bfin_write32(EMAC_RXC_OCTET,val)
#define bfin_read_EMAC_RXC_DMAOVF()          bfin_read32(EMAC_RXC_DMAOVF)
#define bfin_write_EMAC_RXC_DMAOVF(val)      bfin_write32(EMAC_RXC_DMAOVF,val)
#define bfin_read_EMAC_RXC_UNICST()          bfin_read32(EMAC_RXC_UNICST)
#define bfin_write_EMAC_RXC_UNICST(val)      bfin_write32(EMAC_RXC_UNICST,val)
#define bfin_read_EMAC_RXC_MULTI()           bfin_read32(EMAC_RXC_MULTI)
#define bfin_write_EMAC_RXC_MULTI(val)       bfin_write32(EMAC_RXC_MULTI,val)
#define bfin_read_EMAC_RXC_BROAD()           bfin_read32(EMAC_RXC_BROAD)
#define bfin_write_EMAC_RXC_BROAD(val)       bfin_write32(EMAC_RXC_BROAD,val)
#define bfin_read_EMAC_RXC_LNERRI()          bfin_read32(EMAC_RXC_LNERRI)
#define bfin_write_EMAC_RXC_LNERRI(val)      bfin_write32(EMAC_RXC_LNERRI,val)
#define bfin_read_EMAC_RXC_LNERRO()          bfin_read32(EMAC_RXC_LNERRO)
#define bfin_write_EMAC_RXC_LNERRO(val)      bfin_write32(EMAC_RXC_LNERRO,val)
#define bfin_read_EMAC_RXC_LONG()            bfin_read32(EMAC_RXC_LONG)
#define bfin_write_EMAC_RXC_LONG(val)        bfin_write32(EMAC_RXC_LONG,val)
#define bfin_read_EMAC_RXC_MACCTL()          bfin_read32(EMAC_RXC_MACCTL)
#define bfin_write_EMAC_RXC_MACCTL(val)      bfin_write32(EMAC_RXC_MACCTL,val)
#define bfin_read_EMAC_RXC_OPCODE()          bfin_read32(EMAC_RXC_OPCODE)
#define bfin_write_EMAC_RXC_OPCODE(val)      bfin_write32(EMAC_RXC_OPCODE,val)
#define bfin_read_EMAC_RXC_PAUSE()           bfin_read32(EMAC_RXC_PAUSE)
#define bfin_write_EMAC_RXC_PAUSE(val)       bfin_write32(EMAC_RXC_PAUSE,val)
#define bfin_read_EMAC_RXC_ALLFRM()          bfin_read32(EMAC_RXC_ALLFRM)
#define bfin_write_EMAC_RXC_ALLFRM(val)      bfin_write32(EMAC_RXC_ALLFRM,val)
#define bfin_read_EMAC_RXC_ALLOCT()          bfin_read32(EMAC_RXC_ALLOCT)
#define bfin_write_EMAC_RXC_ALLOCT(val)      bfin_write32(EMAC_RXC_ALLOCT,val)
#define bfin_read_EMAC_RXC_TYPED()           bfin_read32(EMAC_RXC_TYPED)
#define bfin_write_EMAC_RXC_TYPED(val)       bfin_write32(EMAC_RXC_TYPED,val)
#define bfin_read_EMAC_RXC_SHORT()           bfin_read32(EMAC_RXC_SHORT)
#define bfin_write_EMAC_RXC_SHORT(val)       bfin_write32(EMAC_RXC_SHORT,val)
#define bfin_read_EMAC_RXC_EQ64()            bfin_read32(EMAC_RXC_EQ64)
#define bfin_write_EMAC_RXC_EQ64(val)        bfin_write32(EMAC_RXC_EQ64,val)
#define bfin_read_EMAC_RXC_LT128()           bfin_read32(EMAC_RXC_LT128)
#define bfin_write_EMAC_RXC_LT128(val)       bfin_write32(EMAC_RXC_LT128,val)
#define bfin_read_EMAC_RXC_LT256()           bfin_read32(EMAC_RXC_LT256)
#define bfin_write_EMAC_RXC_LT256(val)       bfin_write32(EMAC_RXC_LT256,val)
#define bfin_read_EMAC_RXC_LT512()           bfin_read32(EMAC_RXC_LT512)
#define bfin_write_EMAC_RXC_LT512(val)       bfin_write32(EMAC_RXC_LT512,val)
#define bfin_read_EMAC_RXC_LT1024()          bfin_read32(EMAC_RXC_LT1024)
#define bfin_write_EMAC_RXC_LT1024(val)      bfin_write32(EMAC_RXC_LT1024,val)
#define bfin_read_EMAC_RXC_GE1024()          bfin_read32(EMAC_RXC_GE1024)
#define bfin_write_EMAC_RXC_GE1024(val)      bfin_write32(EMAC_RXC_GE1024,val)

#define bfin_read_EMAC_TXC_OK()              bfin_read32(EMAC_TXC_OK)
#define bfin_write_EMAC_TXC_OK(val)          bfin_write32(EMAC_TXC_OK,val)
#define bfin_read_EMAC_TXC_1COL()            bfin_read32(EMAC_TXC_1COL)
#define bfin_write_EMAC_TXC_1COL(val)        bfin_write32(EMAC_TXC_1COL,val)
#define bfin_read_EMAC_TXC_GT1COL()          bfin_read32(EMAC_TXC_GT1COL)
#define bfin_write_EMAC_TXC_GT1COL(val)      bfin_write32(EMAC_TXC_GT1COL,val)
#define bfin_read_EMAC_TXC_OCTET()           bfin_read32(EMAC_TXC_OCTET)
#define bfin_write_EMAC_TXC_OCTET(val)       bfin_write32(EMAC_TXC_OCTET,val)
#define bfin_read_EMAC_TXC_DEFER()           bfin_read32(EMAC_TXC_DEFER)
#define bfin_write_EMAC_TXC_DEFER(val)       bfin_write32(EMAC_TXC_DEFER,val)
#define bfin_read_EMAC_TXC_LATECL()          bfin_read32(EMAC_TXC_LATECL)
#define bfin_write_EMAC_TXC_LATECL(val)      bfin_write32(EMAC_TXC_LATECL,val)
#define bfin_read_EMAC_TXC_XS_COL()          bfin_read32(EMAC_TXC_XS_COL)
#define bfin_write_EMAC_TXC_XS_COL(val)      bfin_write32(EMAC_TXC_XS_COL,val)
#define bfin_read_EMAC_TXC_DMAUND()          bfin_read32(EMAC_TXC_DMAUND)
#define bfin_write_EMAC_TXC_DMAUND(val)      bfin_write32(EMAC_TXC_DMAUND,val)
#define bfin_read_EMAC_TXC_CRSERR()          bfin_read32(EMAC_TXC_CRSERR)
#define bfin_write_EMAC_TXC_CRSERR(val)      bfin_write32(EMAC_TXC_CRSERR,val)
#define bfin_read_EMAC_TXC_UNICST()          bfin_read32(EMAC_TXC_UNICST)
#define bfin_write_EMAC_TXC_UNICST(val)      bfin_write32(EMAC_TXC_UNICST,val)
#define bfin_read_EMAC_TXC_MULTI()           bfin_read32(EMAC_TXC_MULTI)
#define bfin_write_EMAC_TXC_MULTI(val)       bfin_write32(EMAC_TXC_MULTI,val)
#define bfin_read_EMAC_TXC_BROAD()           bfin_read32(EMAC_TXC_BROAD)
#define bfin_write_EMAC_TXC_BROAD(val)       bfin_write32(EMAC_TXC_BROAD,val)
#define bfin_read_EMAC_TXC_XS_DFR()          bfin_read32(EMAC_TXC_XS_DFR)
#define bfin_write_EMAC_TXC_XS_DFR(val)      bfin_write32(EMAC_TXC_XS_DFR,val)
#define bfin_read_EMAC_TXC_MACCTL()          bfin_read32(EMAC_TXC_MACCTL)
#define bfin_write_EMAC_TXC_MACCTL(val)      bfin_write32(EMAC_TXC_MACCTL,val)
#define bfin_read_EMAC_TXC_ALLFRM()          bfin_read32(EMAC_TXC_ALLFRM)
#define bfin_write_EMAC_TXC_ALLFRM(val)      bfin_write32(EMAC_TXC_ALLFRM,val)
#define bfin_read_EMAC_TXC_ALLOCT()          bfin_read32(EMAC_TXC_ALLOCT)
#define bfin_write_EMAC_TXC_ALLOCT(val)      bfin_write32(EMAC_TXC_ALLOCT,val)
#define bfin_read_EMAC_TXC_EQ64()            bfin_read32(EMAC_TXC_EQ64)
#define bfin_write_EMAC_TXC_EQ64(val)        bfin_write32(EMAC_TXC_EQ64,val)
#define bfin_read_EMAC_TXC_LT128()           bfin_read32(EMAC_TXC_LT128)
#define bfin_write_EMAC_TXC_LT128(val)       bfin_write32(EMAC_TXC_LT128,val)
#define bfin_read_EMAC_TXC_LT256()           bfin_read32(EMAC_TXC_LT256)
#define bfin_write_EMAC_TXC_LT256(val)       bfin_write32(EMAC_TXC_LT256,val)
#define bfin_read_EMAC_TXC_LT512()           bfin_read32(EMAC_TXC_LT512)
#define bfin_write_EMAC_TXC_LT512(val)       bfin_write32(EMAC_TXC_LT512,val)
#define bfin_read_EMAC_TXC_LT1024()          bfin_read32(EMAC_TXC_LT1024)
#define bfin_write_EMAC_TXC_LT1024(val)      bfin_write32(EMAC_TXC_LT1024,val)
#define bfin_read_EMAC_TXC_GE1024()          bfin_read32(EMAC_TXC_GE1024)
#define bfin_write_EMAC_TXC_GE1024(val)      bfin_write32(EMAC_TXC_GE1024,val)
#define bfin_read_EMAC_TXC_ABORT()           bfin_read32(EMAC_TXC_ABORT)
#define bfin_write_EMAC_TXC_ABORT(val)       bfin_write32(EMAC_TXC_ABORT,val)

#endif				/* _CDEF_BF537_H */
