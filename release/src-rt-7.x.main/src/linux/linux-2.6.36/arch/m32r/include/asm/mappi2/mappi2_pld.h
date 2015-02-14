#ifndef _MAPPI2_PLD_H
#define _MAPPI2_PLD_H

/*
 * include/asm-m32r/mappi2/mappi2_pld.h
 *
 * Definitions for Extended IO Logic on MAPPI2 board.
 *  based on m32700ut_pld.h
 *
 * This file is subject to the terms and conditions of the GNU General
 * Public License.  See the file "COPYING" in the main directory of
 * this archive for more details.
 */

#ifndef __ASSEMBLY__
#define PLD_BASE		(0x10c00000 /* + NONCACHE_OFFSET */)
#define __reg8			(volatile unsigned char *)
#define __reg16			(volatile unsigned short *)
#define __reg32			(volatile unsigned int *)
#else
#define PLD_BASE		(0x10c00000 + NONCACHE_OFFSET)
#define __reg8
#define __reg16
#define __reg32
#endif /* __ASSEMBLY__ */

/* CFC */
#define	PLD_CFRSTCR		__reg16(PLD_BASE + 0x0000)
#define PLD_CFSTS		__reg16(PLD_BASE + 0x0002)
#define PLD_CFIMASK		__reg16(PLD_BASE + 0x0004)
#define PLD_CFBUFCR		__reg16(PLD_BASE + 0x0006)
#define PLD_CFCR0		__reg16(PLD_BASE + 0x000a)
#define PLD_CFCR1		__reg16(PLD_BASE + 0x000c)

/* MMC */
#define PLD_MMCCR		__reg16(PLD_BASE + 0x4000)
#define PLD_MMCMOD		__reg16(PLD_BASE + 0x4002)
#define PLD_MMCSTS		__reg16(PLD_BASE + 0x4006)
#define PLD_MMCBAUR		__reg16(PLD_BASE + 0x400a)
#define PLD_MMCCMDBCUT		__reg16(PLD_BASE + 0x400c)
#define PLD_MMCCDTBCUT		__reg16(PLD_BASE + 0x400e)
#define PLD_MMCDET		__reg16(PLD_BASE + 0x4010)
#define PLD_MMCWP		__reg16(PLD_BASE + 0x4012)
#define PLD_MMCWDATA		__reg16(PLD_BASE + 0x5000)
#define PLD_MMCRDATA		__reg16(PLD_BASE + 0x6000)
#define PLD_MMCCMDDATA		__reg16(PLD_BASE + 0x7000)
#define PLD_MMCRSPDATA		__reg16(PLD_BASE + 0x7006)

/* Power Control of MMC and CF */
#define PLD_CPCR		__reg16(PLD_BASE + 0x14000)


/*==== ICU ====*/
#define  M32R_IRQ_PC104        (5)   /* INT4(PC/104) */
#define  M32R_IRQ_I2C          (28)  /* I2C-BUS     */
#define  PLD_IRQ_CFIREQ       (40)  /* CFC Card Interrupt */
#define  PLD_IRQ_CFC_INSERT   (41)  /* CFC Card Insert */
#define  PLD_IRQ_CFC_EJECT    (42)  /* CFC Card Eject */
#define  PLD_IRQ_MMCCARD      (43)  /* MMC Card Insert */
#define  PLD_IRQ_MMCIRQ       (44)  /* MMC Transfer Done */



/* CRC */
#define PLD_CRC7DATA		__reg16(PLD_BASE + 0x18000)
#define PLD_CRC7INDATA		__reg16(PLD_BASE + 0x18002)
#define PLD_CRC16DATA		__reg16(PLD_BASE + 0x18004)
#define PLD_CRC16INDATA		__reg16(PLD_BASE + 0x18006)
#define PLD_CRC16ADATA		__reg16(PLD_BASE + 0x18008)
#define PLD_CRC16AINDATA	__reg16(PLD_BASE + 0x1800a)



#endif /* _MAPPI2_PLD.H */
