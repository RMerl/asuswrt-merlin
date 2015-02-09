/*
 * SPROM format definitions for the Broadcom 47xx and 43xx chip family.
 *
 * $Id: sbsprom.h 241182 2011-02-17 21:50:03Z $
 * Copyright(c) 2002 Broadcom Corporation
 */

#ifndef	_SBSPROM_H
#define	_SBSPROM_H

#include "typedefs.h"
#include "bcmdevs.h"

/* A word is this many bytes */
#define SRW		2

/* offset into PCI config space for write enable bit */
#define CFG_SROM_WRITABLE_OFFSET	0x88
#define SROM_WRITEABLE			0x10

/* enumeration space consists of N contiguous 4Kbyte core register sets */
#define SBCORES_BASE	0x18000000
#define SBCORES_EACH	0x1000

/* offset from BAR0 for srom space */
#define SROM_BASE	4096

/* number of 2-byte words in srom */
#define SROM_SIZE	64

#define SROM_BYTES	(SROM_SIZE * SRW)

#define MAX_FN		4

/* Word 0, Hardware control */
#define SROM_HWCTL	0
#define HW_FUNMSK	0x000f
#define HW_FCLK		0x0200
#define HW_CBM		0x0400
#define HW_PIMSK	0xf000
#define HW_PISHIFT	12
#define HW_4301PISHIFT 13
#define HW_PI4402	0x2
#define HW_FUN4401	0x0001
#define HW_FCLK4402	0x0000

/* Word 1, common-power/boot-rom */
#define SROM_COMMPW		1
/* boot rom present bit */
#define BR_PRESSHIFT	8
/* 15:9 for n; boot rom size is 2^(14 + n) bytes */
#define BR_SIZESHIFT	9

/* Word 2, SubsystemId */
#define SROM_SSID	2

/* Word 3, VendorId */
#define SROM_VID	3

/* Function 0 info, function info length */
#define SROM_FN0	4
#define SROM_FNSZ	8

/* Within each function: */
/* Word 0, deviceID */
#define SRFN_DID	0

/* Words 1-2, ClassCode */
#define SRFN_CCL	1
/* Word 2, D0 Power */
#define SRFN_CCHD0	2

/* Word 3, PME and D1D2D3 power */
#define SRFN_PMED123	3

#define PME_IL		0
#define PME_ENET0	1
#define PME_ENET1	2
#define PME_CODEC	3

#define PME_4402_ENET	0
#define PME_4402_CODEC	1
#define PME_4301_WL	2
#define PMEREP_4402_ENET	(PMERD3CV | PMERD3CA | PMERD3H | PMERD2 | PMERD1 | PMERD0 | PME)

/* Word 4, Bar1 enable, pme reports */
#define SRFN_B1PMER	4
#define B1E		1
#define B1SZMSK	0xe
#define B1SZSH		1
#define PMERMSK	0x0ff0
#define PME		0x0010
#define PMERD0		0x0020
#define PMERD1		0x0040
#define PMERD2		0x0080
#define PMERD3H	0x0100
#define PMERD3CA	0x0200
#define PMERD3CV	0x0400
#define IGNCLKRR	0x0800
#define B0LMSK		0xf000

/* Words 4-5, Bar0 Sonics value */
#define SRFN_B0H	5
/* Words 6-7, CIS Pointer */
#define SRFN_CISL	6
#define SRFN_CISH	7

/* Words 36-38: iLine MAC address */
#define SROM_I_MACHI	36
#define SROM_I_MACMID	37
#define SROM_I_MACLO	38

/* Words 36-38: wireless0 MAC address on 43xx */
#define SROM_W0_MACHI	36
#define SROM_W0_MACMID	37
#define SROM_W0_MACLO	38

/* Words 39-41: enet0 MAC address */
#define SROM_E0_MACHI	39
#define SROM_E0_MACMID	40
#define SROM_E0_MACLO	41

/* Words 42-44: enet1 MAC address */
#define SROM_E1_MACHI	42
#define SROM_E1_MACMID	43
#define SROM_E1_MACLO	44

/* Words 42-44: wireless1 MAC address on 4309 */
#define SROM_W1_MACHI	42
#define SROM_W1_MACMID	43
#define SROM_W1_MACLO	44

#define SROM_EPHY	45

/* Word 46: BdRev & Antennas0/1 & ccLock for 430x */
#define SROM_REV_AA_LOCK	46

/* Words 47-51 wl0 PA bx */
#define SROM_WL0_PAB0	47
#define SROM_WL0_PAB1	48
#define SROM_WL0_PAB2	49
#define SROM_WL0_PAB3	50
#define SROM_WL0_PAB4	51

/* Word 52: wl0/wl1 MaxPower */
#define SROM_WL_MAXPWR	52

/* Words 53-55 wl1 PA bx */
#define SROM_WL1_PAB0	53
#define SROM_WL1_PAB1	54
#define SROM_WL1_PAB2	55

/* Woprd 56: itt */
#define SROM_ITT        56

/* Words 59-62: OEM Space */
#define SROM_WL_OEM	59
#define SROM_OEM_SIZE	4

/* Contents for the srom */

#define BU4710_SSID	0x0400
#define VSIM4710_SSID	0x0401
#define QT4710_SSID	0x0402

#define BU4610_SSID	0x0403
#define VSIM4610_SSID	0x0404

#define BU4307_SSID	0x0405
#define BCM94301CB_SSID	0x0406
#define BCM94301MP_SSID	0x0407
#define BCM94307MP_SSID	0x0408
#define AP4307_SSID	0x0409

#define BU4309_SSID	0x040a
#define BCM94309CB_SSID	0x040b
#define BCM94309MP_SSID	0x040c
#define AP4309_SSID	0x040d

#define BU4312_SSID	0x048a

#define BU4402_SSID	0x4402

#define CLASS_OTHER	0x8000
#define CLASS_ETHER	0x0000
#define CLASS_NET	0x0002
#define CLASS_COMM	0x0007
#define CLASS_MODEM	0x0300
#define CLASS_MIPS	0x3000
#define CLASS_PROC	0x000b
#define CLASS_FLASH	0x0100
#define CLASS_MEM	0x0005
#define CLASS_SERIALBUS 0x000c
#define CLASS_OHCI	0x0310

/* Broadcom IEEE MAC addresses are 00:90:4c:xx:xx:xx */
#define MACHI			0x90

#define MACMID_BU4710I		0x4c17
#define MACMID_BU4710E0		0x4c18
#define MACMID_BU4710E1		0x4c19

#define MACMID_94710R1I		0x4c1a
#define MACMID_94710R1E0	0x4c1b
#define MACMID_94710R1E1	0x4c1c

#define MACMID_94710R4I		0x4c1d
#define MACMID_94710R4E0	0x4c1e
#define MACMID_94710R4E1	0x4c1f

#define MACMID_94710DEVI	0x4c20
#define MACMID_94710DEVE0	0x4c21
#define MACMID_94710DEVE1	0x4c22

#define MACMID_BU4402		0x4c23

#define MACMID_BU4610I		0x4c24
#define MACMID_BU4610E0		0x4c25
#define MACMID_BU4610E1		0x4c26

#define MACMID_BU4307W		0x4c27
#define MACMID_BU4307E		0x4c28

#define MACMID_94301CB		0x4c29

#define MACMID_94301MP		0x4c2a

#define MACMID_94307MPW		0x4c2b
#define MACMID_94307MPE		0x4c2c

#define MACMID_AP4307W		0x4c2d
#define MACMID_AP4307E		0x4c2e

#define MACMID_BU4309W0		0x4c2f
#define MACMID_BU4309W1		0x4c30
#define MACMID_BU4309E		0x4c31

#define MACMID_94309CBW0	0x4c32
#define MACMID_94309CBW1	0x4c33

#define MACMID_94309MPW0	0x4c34
#define MACMID_94309MPW1	0x4c35
#define MACMID_94309MPE		0x4c36

#define MACMID_BU4401		0x4c37

/* Enet phy settings one or two singles or a dual	*/
/* Bits 4-0 : MII address for enet0 (0x1f for not there */
/* Bits 9-5 : MII address for enet1 (0x1f for not there */
/* Bit 14   : Mdio for enet0  */
/* Bit 15   : Mdio for enet1  */

/* bu4710 with only one phy on enet1 with address 7: */
#define SROM_EPHY_ONE	0x80ff

/* bu4710 with two individual phys, at 6 and 7, */
/* each mdio connected to its own mac: */
#define SROM_EPHY_TWO	0x80e6

/* bu4710 with a dual phy addresses 0 & 1, mdio-connected to enet0 */
#define SROM_EPHY_DUAL	0x0001

/* r1 board with a dual phy at 0, 1 (NOT swapped and mdc0 */
#define SROM_EPHY_R1	0x0010

/* r4 board with a single phy on enet0 at address 5 and a switch */
/* chip on enet1 (speciall case: 0x1e */
#define SROM_EPHY_R4	0x83e5

/* 4402 uses an internal phy at phyaddr 1; want mdcport == coreunit == 0 */
#define SROM_EPHY_INTERNAL 0x0001

/* 4307 uses an external phy at phyaddr 0; want mdcport == coreunit == 0 */
#define SROM_EPHY_ZERO	0x0000

#define SROM_VERS	0x0001


#endif	/* _SBSPROM_H */
