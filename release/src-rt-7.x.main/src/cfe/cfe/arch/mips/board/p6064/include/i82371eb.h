/*
 * i82371eb.h: Intel PCI to ISA bridge (PIIX4)
 *
 * Copyright (c) 2000, Algorithmics Ltd.  All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify 
 * it under the terms of the "Free MIPS" License Agreement, a copy of 
 * which is available at:
 *
 *  http://www.algor.co.uk/ftp/pub/doc/freemips-license.txt
 *
 * You may not, however, modify or remove any part of this copyright 
 * message if this program is redistributed or reused in whole or in
 * part.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * "Free MIPS" License for more details.  
 */

#define I82371_IORT		0x4c
#define  I82371_IORT_16BIT(x)	 0x04+(((x)&3)<<0)
#define  I82371_IORT_8BIT(x)	 0x40+(((x)&7)<<3)
#define  I82371_IORT_DMAAC	 0x80

#define I82371_XBCS		0x4e
#define  I82371_XBCS_RTCEN	 0x0001
#define  I82371_XBCS_KBDEN	 0x0002
#define  I82371_XBCS_BIOSWP	 0x0004
#define  I82371_XBCS_ALIAS61	 0x0008
#define  I82371_XBCS_MOUSE	 0x0010
#define  I82371_XBCS_CPERR	 0x0020
#define  I82371_XBCS_LBIOS	 0x0040
#define  I82371_XBCS_XBIOS	 0x0080
#define  I82371_XBCS_APIC	 0x0100
#define  I82371_XBCS_1MXBIOS	 0x0200
#define  I82371_XBCS_MCALE	 0x0400

#define I82371_PIRQRCA		0x60
#define I82371_PIRQRCB		0x61
#define I82371_PIRQRCC		0x62
#define I82371_PIRQRCD		0x63
#define  I82371_PIRQRC(d)	(0x80 | (d & 0xf))

#define I82371_SICR		0x64
#define  I82371_SICR_PW4	 0x00
#define  I82371_SICR_PW6	 0x01
#define  I82371_SICR_PW8	 0x02
#define  I82371_SICR_FS21	 0x10
#define  I82371_SICR_CONT	 0x40
#define  I82371_SICR_IRQEN	 0x80

#define I82371_TOM		0x69
#define  I82371_TOM_FWD_89	 0x02
#define  I82371_TOM_FWD_AB	 0x04
#define  I82371_TOM_FWD_LBIOS	 0x08
#define  I82371_TOM_TOM(mb)	 (((mb)-1) << 4)

#define I82371_MSTAT		0x6a
#define  I82371_MSTAT_NBRE	 0x0080
#define  I82371_MSTAT_SEDT	 0x8000

#define I82371_MBDMA0		0x76
#define I82371_MBDMA1		0x77
#define  I82371_MBDMA_CHNL(x)	((x) & 7)
#define  I82371_MBDMA_FAST	 0x80

#define I82371_APICBASE		0x80

#define I82371_DLC		0x82
#define  I82371_DLC_DT		 0x01 /* delayed transaction enb */
#define  I82371_DLC_PR		 0x02 /* passive release enb */
#define  I82371_DLC_USBPR	 0x04 /* USB passive release enb */
#define  I82371_DLC_DTTE	 0x08 /* SERR on delayed timeout */

#define I82371_PDMACFG		0x90
#define I82371_DDMABP0		0x92
#define I82371_DDMABP1		0x94

#define I82371_GENCFG		0xb0
#define  I82371_GENCFG_CFG	 0xfbfec001 

#define I82371_RTCCFG		0xcb
#define  I82371_RTCPDE		0x20
#define  I82371_RTCLUB		0x10
#define  I82371_RTCLLB		0x08
#define  I82371_RTCURE		0x04
#define  I82371_RTCRTCE		0x01
		
/* PCI function 1 configuration registers */

#define I82371_PCI1BMIBA		0x20
#define I82371_PCI1IDETIM		0x40
#define  I82371_PCI1IDETIM_IDE	0x8000 /* IDE decode enable */
#define  I82371_PCI1IDETIM_SITRE	0x4000 /* IDE decode enable */
#define  I82371_PCI1IDETIM_ISP2	0x3000 /* IDE slave timing */
#define  I82371_PCI1IDETIM_ISP3	0x2000 /* IDE slave timing */
#define  I82371_PCI1IDETIM_ISP4	0x1000 /* IDE slave timing */
#define  I82371_PCI1IDETIM_ISP5	0x0000 /* IDE slave timing */
#define  I82371_PCI1IDETIM_RTC1	0x0300 /* IDE recovery time */
#define  I82371_PCI1IDETIM_RTC2	0x0200 /* IDE recovery time */
#define  I82371_PCI1IDETIM_RTC3	0x0100 /* IDE recovery time */
#define  I82371_PCI1IDETIM_RTC4	0x0000 /* IDE recovery time */
#define  I82371_PCI1IDETIM_DTE1	0x0080 /* IDE DMA Timing enable */
#define  I82371_PCI1IDETIM_PPE1	0x0040 /* IDE prefetch & post enable */
#define  I82371_PCI1IDETIM_IE1	0x0020 /* IDE sample point enable */
#define  I82371_PCI1IDETIM_TIME1	0x0010 /* IDE fast timing enable */
#define  I82371_PCI1IDETIM_DTE0	0x0008 /* IDE DMA Timing enable */
#define  I82371_PCI1IDETIM_PPE0	0x0004 /* IDE prefetch & post enable */
#define  I82371_PCI1IDETIM_IE0	0x0002 /* IDE sample point enable */
#define  I82371_PCI1IDETIM_TIME0	0x0001 /* IDE fast timing enable */


#define	I82371_PCI3_PMBA	0x40		/* Power Management Base Address */
#define	I82371_PCI3_CNTA	0x44		/* Count A */
#define	I82371_PCI3_CNTB	0x48		/* Count B */
#define	I82371_PCI3_GPICTL	0x4C		/* General Purpose Input Control */
#define	I82371_PCI3_DEVRESD	0x50		/* Device Resource D */
#define	I82371_PCI3_DEVACTA	0x54		/* Device Activity A */
#define	I82371_PCI3_DEVACTB	0x58		/* Device Activity B */
#define	I82371_PCI3_DEVRESA	0x5C		/* Device Resource A */
#define	I82371_PCI3_DEVRESB	0x60		/* Device Resource B */
#define	I82371_PCI3_DEVRESC	0x64		/* Device Resource C */
#define	I82371_PCI3_DEVRESE	0x68		/* Device Resource E */
#define	I82371_PCI3_DEVRESF	0x6C		/* Device Resource F */
#define	I82371_PCI3_DEVRESG	0x70		/* Device Resource G */
#define	I82371_PCI3_DEVRESH	0x74		/* Device Resource H */
#define	I82371_PCI3_DEVRESI	0x78		/* Device Resource I */
#define	I82371_PCI3_DEVRESJ	0x7C		/* Device Resource J */
#define	I82371_PCI3_PMREGMISC	0x80		/* Miscellaneous Power Management */
#define	I82371_PCI3_SMBBA	0x90		/* SMBus Base Address */
#define	I82371_PCI3_SMBHSTCFG	0xD2		/* SMBus Host Configuration */
#define	I82371_PCI3_SMBREV	0xD3		/* SMBus Revision ID */
#define	I82371_PCI3_SMBSLVC	0xD4		/* SMBus Slave Command */
#define	I82371_PCI3_SMBSHDW1	0xD5		/* SMBus Slave Shadow Port 1 */
#define	I82371_PCI3_SMBSHDW2	0xD6		/* SMBus Slave Shadow Port 2 */

/* I82371_PCI3_SMBHSTCFG */
#define I82371_PCI3_SMB_HST_EN	0x01		/* enable SMB host interface */

/* Offsets from I82371_PCI3_SMBBA */
#define	I82371_SMB_SMBHSTSTS	0x00		/* SMBus Host Status */
#define	I82371_SMB_SMBSLVSTS	0x01		/* SMBus Slave Status */
#define	I82371_SMB_SMBHSTCNT	0x02		/* SMBus Host Count */
#define	I82371_SMB_SMBHSTCMD	0x03		/* SMBus Host Command */
#define	I82371_SMB_SMBHSTADD	0x04		/* SMBus Host Address */
#define	I82371_SMB_SMBHSTDAT0	0x05		/* SMBus Host Data 0 */
#define	I82371_SMB_SMBHSTDAT1	0x06		/* SMBus Host Data 1 */
#define	I82371_SMB_SMBBLKDAT	0x07		/* SMBus Block Data */
#define	I82371_SMB_SMBSLVCNT	0x08		/* SMBus Slave Count */
#define	I82371_SMB_SMBSHDWCMD	0x09		/* SMBus Shadow Command */
#define	I82371_SMB_SMBSLVEVT	0x0A		/* SMBus Slave Event */
#define	I82371_SMB_SMBSLVDAT	0x0C		/* SMBus Slave Data */

#define I82371_SMB_START	0x40
#define I82371_SMB_QRW		(0<<2)
#define I82371_SMB_BRW		(1<<2)
#define I82371_SMB_BDRW		(2<<2)
#define I82371_SMB_WDRW		(3<<2)
#define I82371_SMB_BKRW		(5<<2)
#define I82371_SMB_KILL		0x02
#define I82371_SMB_INTEREN	0x01

#define	I82371_SMB_FAILED	0x10
#define	I82371_SMB_BUS_ERR	0x08
#define	I82371_SMB_DEV_ERR	0x04
#define	I82371_SMB_INTER	0x02
#define	I82371_SMB_HOST_BUSY	0x01

#define I82371_SMB_ALERT_STS	0x10
#define I82371_SMB_SHDW2_STS	0x08
#define I82371_SMB_SHDW1_STS	0x04
#define I82371_SMB_SLV_STS	0x02
#define I82371_SMB_SLV_BSY	0x01

/* I82371_PCI3_PMREGMISC */
#define I82371_PCI3_PMIOSE	0x01		/* enable SMB host interface */

/* Offsets from I82371_PCI3_PMBA */
#define I82371_PM_PMSTS		0x00
#define I82371_PM_PMEN		0x02
#define I82371_PM_PMCNTRL	0x04
#define I82371_PM_PMTMR		0x08
#define I82371_PM_GPSTS		0x0c
#define I82371_PM_GPEN		0x0e
#define I82371_PM_PCNTRL	0x10
#define I82371_PM_PLVL2		0x14
#define I82371_PM_PLVL3		0x15
#define I82371_PM_GLBSTS	0x18
#define I82371_PM_DEVSTS	0x1c
#define I82371_PM_GLBEN		0x20
#define I82371_PM_GLBCTL	0x28
#define I82371_PM_DEVCTL	0x2c
#define I82371_PM_GPIREG0	0x30
#define I82371_PM_GPIREG1	0x31
#define I82371_PM_GPIREG2	0x32
#define I82371_PM_GPOREG0	0x34
#define I82371_PM_GPOREG1	0x35
#define I82371_PM_GPOREG2	0x36
#define I82371_PM_GPOREG3	0x37
