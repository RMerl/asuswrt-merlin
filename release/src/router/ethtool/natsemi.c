/* Copyright 2001 Sun Microsystems (thockin@sun.com) */
#include <stdio.h>
#include "internal.h"

#define PCI_VENDOR_NATSEMI		0x100b
#define PCI_DEVICE_DP83815		0x0020
#define NATSEMI_MAGIC			(PCI_VENDOR_NATSEMI | \
					 (PCI_DEVICE_DP83815<<16))

/* register indices in the ethtool_regs->data */
#define REG_CR				0
#define   BIT_CR_TXE			(1<<0)
#define   BIT_CR_RXE			(1<<2)
#define   BIT_CR_RST			(1<<8)
#define REG_CFG				1
#define   BIT_CFG_BEM			(1<<0)
#define   BIT_CFG_BROM_DIS		(1<<2)
#define   BIT_CFG_PHY_DIS		(1<<9)
#define   BIT_CFG_PHY_RST		(1<<10)
#define   BIT_CFG_EXT_PHY		(1<<12)
#define   BIT_CFG_ANEG_EN		(1<<13)
#define   BIT_CFG_ANEG_100		(1<<14)
#define   BIT_CFG_ANEG_FDUP		(1<<15)
#define   BIT_CFG_PINT_ACEN		(1<<17)
#define   BIT_CFG_PHY_CFG		(0x3f<<18)
#define   BIT_CFG_ANEG_DN		(1<<27)
#define   BIT_CFG_POL			(1<<28)
#define   BIT_CFG_FDUP			(1<<29)
#define   BIT_CFG_SPEED100		(1<<30)
#define   BIT_CFG_LNKSTS		(1<<31)

#define REG_MEAR			2
#define REG_PTSCR			3
#define   BIT_PTSCR_EEBIST_FAIL		(1<<0)
#define   BIT_PTSCR_EELOAD_EN		(1<<2)
#define   BIT_PTSCR_RBIST_RXFFAIL	(1<<3)
#define   BIT_PTSCR_RBIST_TXFAIL	(1<<4)
#define   BIT_PTSCR_RBIST_RXFAIL	(1<<5)
#define REG_ISR				4
#define REG_IMR				5
#define   BIT_INTR_RXOK			(1<<0)
#define   NAME_INTR_RXOK		"Rx Complete"
#define   BIT_INTR_RXDESC		(1<<1)
#define   NAME_INTR_RXDESC		"Rx Descriptor"
#define   BIT_INTR_RXERR		(1<<2)
#define   NAME_INTR_RXERR		"Rx Packet Error"
#define   BIT_INTR_RXEARLY		(1<<3)
#define   NAME_INTR_RXEARLY		"Rx Early Threshold"
#define   BIT_INTR_RXIDLE		(1<<4)
#define   NAME_INTR_RXIDLE		"Rx Idle"
#define   BIT_INTR_RXORN		(1<<5)
#define   NAME_INTR_RXORN		"Rx Overrun"
#define   BIT_INTR_TXOK			(1<<6)
#define   NAME_INTR_TXOK		"Tx Packet OK"
#define   BIT_INTR_TXDESC		(1<<7)
#define   NAME_INTR_TXDESC		"Tx Descriptor"
#define   BIT_INTR_TXERR		(1<<8)
#define   NAME_INTR_TXERR		"Tx Packet Error"
#define   BIT_INTR_TXIDLE		(1<<9)
#define   NAME_INTR_TXIDLE		"Tx Idle"
#define   BIT_INTR_TXURN		(1<<10)
#define   NAME_INTR_TXURN		"Tx Underrun"
#define   BIT_INTR_MIB			(1<<11)
#define   NAME_INTR_MIB			"MIB Service"
#define   BIT_INTR_SWI			(1<<12)
#define   NAME_INTR_SWI			"Software"
#define   BIT_INTR_PME			(1<<13)
#define   NAME_INTR_PME			"Power Management Event"
#define   BIT_INTR_PHY			(1<<14)
#define   NAME_INTR_PHY			"Phy"
#define   BIT_INTR_HIBERR		(1<<15)
#define   NAME_INTR_HIBERR		"High Bits Error"
#define   BIT_INTR_RXSOVR		(1<<16)
#define   NAME_INTR_RXSOVR		"Rx Status FIFO Overrun"
#define   BIT_INTR_RTABT		(1<<20)
#define   NAME_INTR_RTABT		"Received Target Abort"
#define   BIT_INTR_RMABT		(1<<20)
#define   NAME_INTR_RMABT		"Received Master Abort"
#define   BIT_INTR_SSERR		(1<<20)
#define   NAME_INTR_SSERR		"Signaled System Error"
#define   BIT_INTR_DPERR		(1<<20)
#define   NAME_INTR_DPERR		"Detected Parity Error"
#define   BIT_INTR_RXRCMP		(1<<20)
#define   NAME_INTR_RXRCMP		"Rx Reset Complete"
#define   BIT_INTR_TXRCMP		(1<<20)
#define   NAME_INTR_TXRCMP		"Tx Reset Complete"
#define REG_IER				6
#define   BIT_IER_IE			(1<<0)
#define REG_TXDP			8
#define REG_TXCFG			9
#define   BIT_TXCFG_DRTH		(0x3f<<0)
#define   BIT_TXCFG_FLTH		(0x3f<<8)
#define   BIT_TXCFG_MXDMA		(0x7<<20)
#define   BIT_TXCFG_ATP			(1<<28)
#define   BIT_TXCFG_MLB			(1<<29)
#define   BIT_TXCFG_HBI			(1<<30)
#define   BIT_TXCFG_CSI			(1<<31)
#define REG_RXDP			12
#define REG_RXCFG			13
#define   BIT_RXCFG_DRTH		(0x1f<<1)
#define   BIT_RXCFG_MXDMA		(0x7<<20)
#define   BIT_RXCFG_ALP			(1<<27)
#define   BIT_RXCFG_ATX			(1<<28)
#define   BIT_RXCFG_ARP			(1<<30)
#define   BIT_RXCFG_AEP			(1<<31)
#define REG_CCSR			15
#define   BIT_CCSR_CLKRUN_EN		(1<<0)
#define   BIT_CCSR_PMEEN		(1<<8)
#define   BIT_CCSR_PMESTS		(1<<15)
#define REG_WCSR			16
#define   BIT_WCSR_WKPHY		(1<<0)
#define   BIT_WCSR_WKUCP		(1<<1)
#define   BIT_WCSR_WKMCP		(1<<2)
#define   BIT_WCSR_WKBCP		(1<<3)
#define   BIT_WCSR_WKARP		(1<<4)
#define   BIT_WCSR_WKPAT0		(1<<5)
#define   BIT_WCSR_WKPAT1		(1<<6)
#define   BIT_WCSR_WKPAT2		(1<<7)
#define   BIT_WCSR_WKPAT3		(1<<8)
#define   BIT_WCSR_WKMAG		(1<<9)
#define   BIT_WCSR_MPSOE		(1<<10)
#define   BIT_WCSR_SOHACK		(1<<20)
#define   BIT_WCSR_PHYINT		(1<<22)
#define   BIT_WCSR_UCASTR		(1<<23)
#define   BIT_WCSR_MCASTR		(1<<24)
#define   BIT_WCSR_BCASTR		(1<<25)
#define   BIT_WCSR_ARPR			(1<<26)
#define   BIT_WCSR_PATM0		(1<<27)
#define   BIT_WCSR_PATM1		(1<<28)
#define   BIT_WCSR_PATM2		(1<<29)
#define   BIT_WCSR_PATM3		(1<<30)
#define   BIT_WCSR_MPR			(1<<31)
#define REG_PCR				17
#define   BIT_PCR_PAUSE_CNT		(0xffff<<0)
#define   BIT_PCR_PSNEG			(1<<21)
#define   BIT_PCR_PS_RCVD		(1<<22)
#define   BIT_PCR_PS_DA			(1<<29)
#define   BIT_PCR_PSMCAST		(1<<30)
#define   BIT_PCR_PSEN			(1<<31)
#define REG_RFCR			18
#define   BIT_RFCR_UHEN			(1<<20)
#define   BIT_RFCR_MHEN			(1<<21)
#define   BIT_RFCR_AARP			(1<<22)
#define   BIT_RFCR_APAT0		(1<<23)
#define   BIT_RFCR_APAT1		(1<<24)
#define   BIT_RFCR_APAT2		(1<<25)
#define   BIT_RFCR_APAT3		(1<<26)
#define   BIT_RFCR_APM			(1<<27)
#define   BIT_RFCR_AAU			(1<<28)
#define   BIT_RFCR_AAM			(1<<29)
#define   BIT_RFCR_AAB			(1<<30)
#define   BIT_RFCR_RFEN			(1<<31)
#define REG_RFDR			19
#define REG_BRAR			20
#define   BIT_BRAR_AUTOINC		(1<<31)
#define REG_BRDR			21
#define REG_SRR				22
#define REG_MIBC			23
#define   BIT_MIBC_WRN			(1<<0)
#define   BIT_MIBC_FRZ			(1<<1)
#define REG_MIB0			24
#define REG_MIB1			25
#define REG_MIB2			26
#define REG_MIB3			27
#define REG_MIB4			28
#define REG_MIB5			29
#define REG_MIB6			30
#define REG_BMCR			32
#define   BIT_BMCR_FDUP			(1<<8)
#define   BIT_BMCR_ANRST		(1<<9)
#define   BIT_BMCR_ISOL			(1<<10)
#define   BIT_BMCR_PDOWN		(1<<11)
#define   BIT_BMCR_ANEN			(1<<12)
#define   BIT_BMCR_SPEED		(1<<13)
#define   BIT_BMCR_LOOP			(1<<14)
#define   BIT_BMCR_RST			(1<<15)
#define REG_BMSR			33
#define   BIT_BMSR_JABBER		(1<<1)
#define   BIT_BMSR_LNK			(1<<2)
#define   BIT_BMSR_ANCAP		(1<<3)
#define   BIT_BMSR_RFAULT		(1<<4)
#define   BIT_BMSR_ANDONE		(1<<5)
#define   BIT_BMSR_PREAMBLE		(1<<6)
#define   BIT_BMSR_10HCAP		(1<<11)
#define   BIT_BMSR_10FCAP		(1<<12)
#define   BIT_BMSR_100HCAP		(1<<13)
#define   BIT_BMSR_100FCAP		(1<<14)
#define   BIT_BMSR_100T4CAP		(1<<15)
#define REG_PHYIDR1			34
#define REG_PHYIDR2			35
#define   BIT_PHYIDR2_OUILSB		(0x3f<<10)
#define   BIT_PHYIDR2_MODEL		(0x3f<<4)
#define   BIT_PHYIDR2_REV		(0xf)
#define REG_ANAR			36
#define   BIT_ANAR_PROTO		(0x1f<<0)
#define   BIT_ANAR_10			(1<<5)
#define   BIT_ANAR_10_FD		(1<<6)
#define   BIT_ANAR_TX			(1<<7)
#define   BIT_ANAR_TXFD			(1<<8)
#define   BIT_ANAR_T4			(1<<9)
#define   BIT_ANAR_PAUSE		(1<<10)
#define   BIT_ANAR_RF			(1<<13)
#define   BIT_ANAR_NP			(1<<15)
#define REG_ANLPAR			37
#define   BIT_ANLPAR_PROTO		(0x1f<<0)
#define   BIT_ANLPAR_10			(1<<5)
#define   BIT_ANLPAR_10_FD		(1<<6)
#define   BIT_ANLPAR_TX			(1<<7)
#define   BIT_ANLPAR_TXFD		(1<<8)
#define   BIT_ANLPAR_T4			(1<<9)
#define   BIT_ANLPAR_PAUSE		(1<<10)
#define   BIT_ANLPAR_RF			(1<<13)
#define   BIT_ANLPAR_ACK		(1<<14)
#define   BIT_ANLPAR_NP			(1<<15)
#define REG_ANER			38
#define   BIT_ANER_LP_AN_ENABLE		(1<<0)
#define   BIT_ANER_PAGE_RX		(1<<1)
#define   BIT_ANER_NP_ABLE		(1<<2)
#define   BIT_ANER_LP_NP_ABLE		(1<<3)
#define   BIT_ANER_PDF			(1<<4)
#define REG_ANNPTR			39
#define REG_PHYSTS			48
#define   BIT_PHYSTS_LNK		(1<<0)
#define   BIT_PHYSTS_SPD10		(1<<1)
#define   BIT_PHYSTS_FDUP		(1<<2)
#define   BIT_PHYSTS_LOOP		(1<<3)
#define   BIT_PHYSTS_ANDONE		(1<<4)
#define   BIT_PHYSTS_JABBER		(1<<5)
#define   BIT_PHYSTS_RF			(1<<6)
#define   BIT_PHYSTS_MINT		(1<<7)
#define   BIT_PHYSTS_FC			(1<<11)
#define   BIT_PHYSTS_POL		(1<<12)
#define   BIT_PHYSTS_RXERR		(1<<13)
#define REG_MICR			49
#define   BIT_MICR_INTEN		(1<<1)
#define REG_MISR			50
#define   BIT_MISR_MSK_RHF		(1<<9)
#define   BIT_MISR_MSK_FHF		(1<<10)
#define   BIT_MISR_MSK_ANC		(1<<11)
#define   BIT_MISR_MSK_RF		(1<<12)
#define   BIT_MISR_MSK_JAB		(1<<13)
#define   BIT_MISR_MSK_LNK		(1<<14)
#define   BIT_MISR_MINT			(1<<15)
#define REG_PGSEL			51
#define REG_FCSCR			52
#define REG_RECR			53
#define REG_PCSR			54
#define   BIT_PCSR_NRZI			(1<<2)
#define   BIT_PCSR_FORCE_100		(1<<5)
#define   BIT_PCSR_SDOPT		(1<<8)
#define   BIT_PCSR_SDFORCE		(1<<9)
#define   BIT_PCSR_TQM			(1<<10)
#define   BIT_PCSR_CLK			(1<<11)
#define   BIT_PCSR_4B5B			(1<<12)
#define REG_PHYCR			57
#define   BIT_PHYCR_PHYADDR		(0x1f<<0)
#define   BIT_PHYCR_PAUSE_STS		(1<<7)
#define   BIT_PHYCR_STRETCH		(1<<8)
#define   BIT_PHYCR_BIST		(1<<9)
#define   BIT_PHYCR_BIST_STAT		(1<<10)
#define   BIT_PHYCR_PSR15		(1<<11)
#define REG_TBTSCR			58
#define   BIT_TBTSCR_JAB		(1<<0)
#define   BIT_TBTSCR_BEAT		(1<<1)
#define   BIT_TBTSCR_AUTOPOL		(1<<3)
#define   BIT_TBTSCR_POL		(1<<4)
#define   BIT_TBTSCR_FPOL		(1<<5)
#define   BIT_TBTSCR_FORCE_10		(1<<6)
#define   BIT_TBTSCR_PULSE		(1<<7)
#define   BIT_TBTSCR_LOOP		(1<<8)
#define REG_PMDCSR			64
#define REG_TSTDAT			65
#define REG_DSPCFG			66
#define REG_SDCFG			67
#define REG_PMATCH0			68
#define REG_PMATCH1			69
#define REG_PMATCH2			70
#define REG_PCOUNT0			71
#define REG_PCOUNT1			72
#define REG_SOPASS0			73
#define REG_SOPASS1			74
#define REG_SOPASS2			75

static void __print_intr(int d, int intr, const char *name,
			 const char *s1, const char *s2)
{
	if ((d) & intr)
		fprintf(stdout, "      %s Interrupt: %s\n", name, s1);
	else if (s2)
		fprintf(stdout, "      %s Interrupt: %s\n", name, s2);
}

#define PRINT_INTR(d, i, s1, s2) do { \
	int intr = BIT_INTR_ ## i; \
	const char *name = NAME_INTR_ ## i; \
	__print_intr(d, intr, name, s1, s2); \
} while (0)

#define PRINT_INTRS(d, s1, s2) do { \
	PRINT_INTR((d), RXOK, s1, s2); \
	PRINT_INTR((d), RXDESC, s1, s2); \
	PRINT_INTR((d), RXERR, s1, s2); \
	PRINT_INTR((d), RXEARLY, s1, s2); \
	PRINT_INTR((d), RXIDLE, s1, s2); \
	PRINT_INTR((d), RXORN, s1, s2); \
	PRINT_INTR((d), TXOK, s1, s2); \
	PRINT_INTR((d), TXDESC, s1, s2); \
	PRINT_INTR((d), TXERR, s1, s2); \
	PRINT_INTR((d), TXIDLE, s1, s2); \
	PRINT_INTR((d), TXURN, s1, s2); \
	PRINT_INTR((d), MIB, s1, s2); \
	PRINT_INTR((d), SWI, s1, s2); \
	PRINT_INTR((d), PME, s1, s2); \
	PRINT_INTR((d), PHY, s1, s2); \
	PRINT_INTR((d), HIBERR, s1, s2); \
	PRINT_INTR((d), RXSOVR, s1, s2); \
	PRINT_INTR((d), RTABT, s1, s2); \
	PRINT_INTR((d), RMABT, s1, s2); \
	PRINT_INTR((d), SSERR, s1, s2); \
	PRINT_INTR((d), DPERR, s1, s2); \
	PRINT_INTR((d), RXRCMP, s1, s2); \
	PRINT_INTR((d), TXRCMP, s1, s2); \
} while (0)

int
natsemi_dump_regs(struct ethtool_drvinfo *info, struct ethtool_regs *regs)
{
	u32 *data = (u32 *)regs->data;
	u32 tmp;

	fprintf(stdout, "Mac/BIU Registers\n");
	fprintf(stdout, "-----------------\n");

	/* command register */
	fprintf(stdout,
		"0x00: CR (Command):                      0x%08x\n",
		data[REG_CR]);
	fprintf(stdout,
		"      Transmit %s\n"
		"      Receive %s\n",
		data[REG_CR] & BIT_CR_TXE ? "Active" : "Idle",
		data[REG_CR] & BIT_CR_RXE ? "Active" : "Idle");
	if (data[REG_CR] & BIT_CR_RST) fprintf(stdout,
		"      Reset In Progress\n");

	/* configuration register */
	fprintf(stdout,
		"0x04: CFG (Configuration):               0x%08x\n",
		data[REG_CFG]);
	fprintf(stdout,
		"      %s Endian\n"
		"      Boot ROM %s\n"
		"      Internal Phy %s\n"
		"      Phy Reset %s\n"
		"      External Phy %s\n"
		"      Default Auto-Negotiation %s, %s %s Mb %s Duplex\n"
		"      Phy Interrupt %sAuto-Cleared\n"
		"      Phy Configuration = 0x%02x\n"
		"      Auto-Negotiation %s\n"
		"      %s Polarity\n"
		"      %s Duplex\n"
		"      %d Mb/s\n"
		"      Link %s\n",
		data[REG_CFG] & BIT_CFG_BEM ? "Big" : "Little",
		data[REG_CFG] & BIT_CFG_BROM_DIS ? "Disabled" : "Enabled",
		data[REG_CFG] & BIT_CFG_PHY_DIS ? "Disabled" : "Enabled",
		data[REG_CFG] & BIT_CFG_PHY_RST ? "In Progress" : "Idle",
		data[REG_CFG] & BIT_CFG_EXT_PHY ? "Enabled" : "Disabled",
		data[REG_CFG] & BIT_CFG_ANEG_EN ? "Enabled" : "Disabled",
		data[REG_CFG] & BIT_CFG_ANEG_EN ? "Advertise" : "Force",
		data[REG_CFG] & BIT_CFG_ANEG_100 ?
			(data[REG_CFG] & BIT_CFG_ANEG_EN ? "10/100" : "100")
			: "10",
		data[REG_CFG] & BIT_CFG_ANEG_FDUP ?
			(data[REG_CFG] & BIT_CFG_ANEG_EN ? "Half/Full" : "Full")
			: "Half",
		data[REG_CFG] & BIT_CFG_PINT_ACEN ? "" : "Not ",
		data[REG_CFG] & BIT_CFG_PHY_CFG >> 18,
		data[REG_CFG] & BIT_CFG_ANEG_DN ? "Done" : "Not Done",
		data[REG_CFG] & BIT_CFG_POL ? "Reversed" : "Normal",
		data[REG_CFG] & BIT_CFG_FDUP ? "Full" : "Half",
		data[REG_CFG] & BIT_CFG_SPEED100 ? 100 : 10,
		data[REG_CFG] & BIT_CFG_LNKSTS ? "Up" : "Down");

	/* EEPROM access register */
	fprintf(stdout,
		"0x08: MEAR (EEPROM Access):              0x%08x\n",
		data[REG_MEAR]);

	/* PCI test control register */
	fprintf(stdout,
		"0x0c: PTSCR (PCI Test Control):          0x%08x\n",
		data[REG_PTSCR]);
	fprintf(stdout,
		"      EEPROM Self Test %s\n"
		"      Rx Filter Self Test %s\n"
		"      Tx FIFO Self Test %s\n"
		"      Rx FIFO Self Test %s\n",
		data[REG_PTSCR] & BIT_PTSCR_EEBIST_FAIL ? "Failed" : "Passed",
		data[REG_PTSCR] & BIT_PTSCR_RBIST_RXFFAIL ? "Failed" : "Passed",
		data[REG_PTSCR] & BIT_PTSCR_RBIST_TXFAIL ? "Failed" : "Passed",
		data[REG_PTSCR] & BIT_PTSCR_RBIST_RXFAIL ? "Failed" : "Passed");
	if (data[REG_PTSCR] & BIT_PTSCR_EELOAD_EN) fprintf(stdout,
		"      EEPROM Reload In Progress\n");

	/* Interrupt status register */
	fprintf(stdout,
		"0x10: ISR (Interrupt Status):            0x%08x\n",
		data[REG_ISR]);
	if (data[REG_ISR])
		PRINT_INTRS(data[REG_ISR], "Active", (char *)NULL);
	else
		fprintf(stdout, "      No Interrupts Active\n");

	/* Interrupt mask register */
	fprintf(stdout,
		"0x14: IMR (Interrupt Mask):              0x%08x\n",
		data[REG_IMR]);
	PRINT_INTRS(data[REG_IMR], "Enabled", "Masked");

	/* Interrupt enable register */
	fprintf(stdout,
		"0x18: IER (Interrupt Enable):            0x%08x\n",
		data[REG_IER]);
	fprintf(stdout,
		"      Interrupts %s\n",
		data[REG_IER] & BIT_IER_IE ? "Enabled" : "Disabled");

	/* Tx descriptor pointer register */
	fprintf(stdout,
		"0x20: TXDP (Tx Descriptor Pointer):      0x%08x\n",
		data[REG_TXDP]);

	/* Tx configuration register */
	fprintf(stdout,
		"0x24: TXCFG (Tx Config):                 0x%08x\n",
		data[REG_TXCFG]);
	tmp = (data[REG_TXCFG] & BIT_TXCFG_MXDMA)>>20;
	fprintf(stdout,
		"      Drain Threshold = %d bytes (%d)\n"
		"      Fill Threshold = %d bytes (%d)\n"
		"      Max DMA Burst per Tx = %d bytes\n"
		"      Automatic Tx Padding %s\n"
		"      Mac Loopback %s\n"
		"      Heartbeat Ignore %s\n"
		"      Carrier Sense Ignore %s\n",
		(data[REG_TXCFG] & BIT_TXCFG_DRTH) * 32,
		data[REG_TXCFG] & BIT_TXCFG_DRTH,
		((data[REG_TXCFG] & BIT_TXCFG_FLTH)>>8) * 32,
		data[REG_TXCFG] & BIT_TXCFG_FLTH,
		tmp ? (1<<(tmp-1))*4 : 512,
		data[REG_TXCFG] & BIT_TXCFG_ATP ? "Enabled" : "Disabled",
		data[REG_TXCFG] & BIT_TXCFG_MLB ? "Enabled" : "Disabled",
		data[REG_TXCFG] & BIT_TXCFG_HBI ? "Enabled" : "Disabled",
		data[REG_TXCFG] & BIT_TXCFG_CSI ? "Enabled" : "Disabled");


	/* Rx descriptor pointer register */
	fprintf(stdout,
		"0x30: RXDP (Rx Descriptor Pointer):      0x%08x\n",
		data[REG_RXDP]);

	/* Rx configuration register */
	fprintf(stdout,
		"0x34: RXCFG (Rx Config):                 0x%08x\n",
		data[REG_RXCFG]);
	tmp = (data[REG_RXCFG] & BIT_RXCFG_MXDMA)>>20;
	fprintf(stdout,
		"      Drain Threshold = %d bytes (%d)\n"
		"      Max DMA Burst per Rx = %d bytes\n"
		"      Long Packets %s\n"
		"      Tx Packets %s\n"
		"      Runt Packets %s\n"
		"      Error Packets %s\n",
		((data[REG_RXCFG] & BIT_RXCFG_DRTH) >> 1) * 8,
		(data[REG_RXCFG] & BIT_RXCFG_DRTH) >> 1,
		tmp ? (1<<(tmp-1))*4 : 512,
		data[REG_RXCFG] & BIT_RXCFG_ALP ? "Accepted" : "Rejected",
		data[REG_RXCFG] & BIT_RXCFG_ATX ? "Accepted" : "Rejected",
		data[REG_RXCFG] & BIT_RXCFG_ARP ? "Accepted" : "Rejected",
		data[REG_RXCFG] & BIT_RXCFG_AEP ? "Accepted" : "Rejected");

	/* CLKRUN control/status register */
	fprintf(stdout,
		"0x3c: CCSR (CLKRUN Control/Status):      0x%08x\n",
		data[REG_CCSR]);
	fprintf(stdout,
		"      CLKRUNN %s\n"
		"      Power Management %s\n",
		data[REG_CCSR] & BIT_CCSR_CLKRUN_EN ? "Enabled" : "Disabled",
		data[REG_CCSR] & BIT_CCSR_PMEEN ? "Enabled" : "Disabled");
	if (data[REG_CCSR] & BIT_CCSR_PMESTS) fprintf(stdout,
		"      Power Management Event Pending\n");

	/* WoL control/status register */
	fprintf(stdout,
		"0x40: WCSR (Wake-on-LAN Control/Status): 0x%08x\n",
		data[REG_WCSR]);
	if (data[REG_WCSR] & BIT_WCSR_WKPHY) fprintf(stdout,
		"      Wake on Phy Interrupt Enabled\n");
	if (data[REG_WCSR] & BIT_WCSR_WKUCP) fprintf(stdout,
		"      Wake on Unicast Packet Enabled\n");
	if (data[REG_WCSR] & BIT_WCSR_WKMCP) fprintf(stdout,
		"      Wake on Multicast Packet Enabled\n");
	if (data[REG_WCSR] & BIT_WCSR_WKBCP) fprintf(stdout,
		"      Wake on Broadcast Packet Enabled\n");
	if (data[REG_WCSR] & BIT_WCSR_WKARP) fprintf(stdout,
		"      Wake on Arp Enabled\n");
	if (data[REG_WCSR] & BIT_WCSR_WKPAT0) fprintf(stdout,
		"      Wake on Pattern 0 Match Enabled\n");
	if (data[REG_WCSR] & BIT_WCSR_WKPAT1) fprintf(stdout,
		"      Wake on Pattern 1 Match Enabled\n");
	if (data[REG_WCSR] & BIT_WCSR_WKPAT2) fprintf(stdout,
		"      Wake on Pattern 2 Match Enabled\n");
	if (data[REG_WCSR] & BIT_WCSR_WKPAT3) fprintf(stdout,
		"      Wake on Pattern 3 Match Enabled\n");
	if (data[REG_WCSR] & BIT_WCSR_WKMAG) fprintf(stdout,
		"      Wake on Magic Packet Enabled\n");
	if (data[REG_WCSR] & BIT_WCSR_MPSOE) fprintf(stdout,
		"      Magic Packet SecureOn Enabled\n");
	if (data[REG_WCSR] & BIT_WCSR_SOHACK) fprintf(stdout,
		"      SecureOn Hack Detected\n");
	if (data[REG_WCSR] & BIT_WCSR_PHYINT) fprintf(stdout,
		"      Phy Interrupt Received\n");
	if (data[REG_WCSR] & BIT_WCSR_UCASTR) fprintf(stdout,
		"      Unicast Packet Received\n");
	if (data[REG_WCSR] & BIT_WCSR_MCASTR) fprintf(stdout,
		"      Multicast Packet Received\n");
	if (data[REG_WCSR] & BIT_WCSR_BCASTR) fprintf(stdout,
		"      Broadcast Packet Received\n");
	if (data[REG_WCSR] & BIT_WCSR_ARPR) fprintf(stdout,
		"      Arp Received\n");
	if (data[REG_WCSR] & BIT_WCSR_PATM0) fprintf(stdout,
		"      Pattern 0 Received\n");
	if (data[REG_WCSR] & BIT_WCSR_PATM1) fprintf(stdout,
		"      Pattern 1 Received\n");
	if (data[REG_WCSR] & BIT_WCSR_PATM2) fprintf(stdout,
		"      Pattern 2 Received\n");
	if (data[REG_WCSR] & BIT_WCSR_PATM3) fprintf(stdout,
		"      Pattern 3 Received\n");
	if (data[REG_WCSR] & BIT_WCSR_MPR) fprintf(stdout,
		"      Magic Packet Received\n");

	/* Pause control/status register */
	fprintf(stdout,
		"0x44: PCR (Pause Control/Status):        0x%08x\n",
		data[REG_PCR]);
	fprintf(stdout,
		"      Pause Counter = %d\n"
		"      Pause %sNegotiated\n"
		"      Pause on DA %s\n"
		"      Pause on Mulitcast %s\n"
		"      Pause %s\n",
		data[REG_PCR] & BIT_PCR_PAUSE_CNT,
		data[REG_PCR] & BIT_PCR_PSNEG ? "" : "Not ",
		data[REG_PCR] & BIT_PCR_PS_DA ? "Enabled" : "Disabled",
		data[REG_PCR] & BIT_PCR_PSMCAST ? "Enabled" : "Disabled",
		data[REG_PCR] & BIT_PCR_PSEN ? "Enabled" : "Disabled");
	if (data[REG_PCR] & BIT_PCR_PS_RCVD) fprintf(stdout,
		"      PS_RCVD: Pause Frame Received\n");

	/* Rx Filter Control */
	fprintf(stdout,
		"0x48: RFCR (Rx Filter Control):          0x%08x\n",
		data[REG_RFCR]);
	fprintf(stdout,
		"      Unicast Hash %s\n"
		"      Multicast Hash %s\n"
		"      Arp %s\n"
		"      Pattern 0 Match %s\n"
		"      Pattern 1 Match %s\n"
		"      Pattern 2 Match %s\n"
		"      Pattern 3 Match %s\n"
		"      Perfect Match %s\n"
		"      All Unicast %s\n"
		"      All Multicast %s\n"
		"      All Broadcast %s\n"
		"      Rx Filter %s\n",
		data[REG_RFCR] & BIT_RFCR_UHEN ? "Enabled" : "Disabled",
		data[REG_RFCR] & BIT_RFCR_MHEN ? "Enabled" : "Disabled",
		data[REG_RFCR] & BIT_RFCR_AARP ? "Accepted" : "Rejected",
		data[REG_RFCR] & BIT_RFCR_APAT0 ? "Accepted" : "Rejected",
		data[REG_RFCR] & BIT_RFCR_APAT1 ? "Accepted" : "Rejected",
		data[REG_RFCR] & BIT_RFCR_APAT2 ? "Accepted" : "Rejected",
		data[REG_RFCR] & BIT_RFCR_APAT3 ? "Accepted" : "Rejected",
		data[REG_RFCR] & BIT_RFCR_APM ? "Accepted" : "Rejected",
		data[REG_RFCR] & BIT_RFCR_AAU ? "Accepted" : "Rejected",
		data[REG_RFCR] & BIT_RFCR_AAM ? "Accepted" : "Rejected",
		data[REG_RFCR] & BIT_RFCR_AAB ? "Accepted" : "Rejected",
		data[REG_RFCR] & BIT_RFCR_RFEN ? "Enabled" : "Disabled");

	/* Rx filter data register */
	fprintf(stdout,
		"0x4c: RFDR (Rx Filter Data):             0x%08x\n",
		data[REG_RFDR]);
	if (regs->version >= 1) fprintf(stdout,
		"      PMATCH 1-0 = 0x%08x\n"
		"      PMATCH 3-2 = 0x%08x\n"
		"      PMATCH 5-4 = 0x%08x\n"
		"      PCOUNT 1-0 = 0x%08x\n"
		"      PCOUNT 3-2 = 0x%08x\n"
		"      SOPASS 1-0 = 0x%08x\n"
		"      SOPASS 3-2 = 0x%08x\n"
		"      SOPASS 5-4 = 0x%08x\n",
		data[REG_PMATCH0], data[REG_PMATCH1], data[REG_PMATCH2],
		data[REG_PCOUNT0], data[REG_PCOUNT1],
		data[REG_SOPASS0], data[REG_SOPASS1], data[REG_SOPASS2]);


	/* Boot ROM address register */
	fprintf(stdout,
		"0x50: BRAR (Boot ROM Address):           0x%08x\n",
		data[REG_BRAR]);
	if (data[REG_BRAR] & BIT_BRAR_AUTOINC) fprintf(stdout,
		"      Automatically Increment Address\n");

	/* Boot ROM data register */
	fprintf(stdout,
		"0x54: BRDR (Boot ROM Data):              0x%08x\n",
		data[REG_BRDR]);

	/* Silicon revison register */
	fprintf(stdout,
		"0x58: SRR (Silicon Revision):            0x%08x\n",
		data[REG_SRR]);

	/* Management information base control register */
	fprintf(stdout,
		"0x5c: MIBC (Mgmt Info Base Control):     0x%08x\n",
		data[REG_MIBC]);
	if (data[REG_MIBC] & BIT_MIBC_WRN) fprintf(stdout,
		"      Counter Overflow Warning\n");
	if (data[REG_MIBC] & BIT_MIBC_FRZ) fprintf(stdout,
		"      Counters Frozen\n");

	/* MIB registers */
	fprintf(stdout,
		"0x60: MIB[0] (Rx Errored Packets):       0x%04x\n",
		data[REG_MIB0]);
	fprintf(stdout, "      Value = %d\n", data[REG_MIB0]);
	fprintf(stdout,
		"0x64: MIB[1] (Rx Frame Sequence Errors): 0x%02x\n",
		data[REG_MIB1]);
	fprintf(stdout, "      Value = %d\n", data[REG_MIB1]);
	fprintf(stdout,
		"0x68: MIB[2] (Rx Missed Packets):        0x%02x\n",
		data[REG_MIB2]);
	fprintf(stdout, "      Value = %d\n", data[REG_MIB2]);
	fprintf(stdout,
		"0x6c: MIB[3] (Rx Alignment Errors):      0x%02x\n",
		data[REG_MIB3]);
	fprintf(stdout, "      Value = %d\n", data[REG_MIB3]);
	fprintf(stdout,
		"0x70: MIB[4] (Rx Symbol Errors):         0x%02x\n",
		data[REG_MIB4]);
	fprintf(stdout, "      Value = %d\n", data[REG_MIB4]);
	fprintf(stdout,
		"0x74: MIB[5] (Rx Long Frame Errors):     0x%02x\n",
		data[REG_MIB5]);
	fprintf(stdout, "      Value = %d\n", data[REG_MIB5]);
	fprintf(stdout,
		"0x78: MIB[6] (Tx Heartbeat Errors):      0x%02x\n",
		data[REG_MIB6]);
	fprintf(stdout, "      Value = %d\n", data[REG_MIB6]);

	fprintf(stdout, "\n");
	fprintf(stdout, "Internal Phy Registers\n");
	fprintf(stdout, "----------------------\n");

	/* Basic mode control register */
	fprintf(stdout,
		"0x80: BMCR (Basic Mode Control):         0x%04x\n",
		data[REG_BMCR]);
	fprintf(stdout,
		"      %s Duplex\n"
		"      Port is Powered %s\n"
		"      Auto-Negotiation %s\n"
		"      %d Mb/s\n",
		data[REG_BMCR] & BIT_BMCR_FDUP ? "Full" : "Half",
		data[REG_BMCR] & BIT_BMCR_PDOWN ? "Down" : "Up",
		data[REG_BMCR] & BIT_BMCR_ANEN ? "Enabled" : "Disabled",
		data[REG_BMCR] & BIT_BMCR_SPEED ? 100 : 10);
	if (data[REG_BMCR] & BIT_BMCR_ANRST) fprintf(stdout,
		"      Auto-Negotiation Restarting\n");
	if (data[REG_BMCR] & BIT_BMCR_ISOL) fprintf(stdout,
		"      Port Isolated\n");
	if (data[REG_BMCR] & BIT_BMCR_LOOP) fprintf(stdout,
		"      Loopback Enabled\n");
	if (data[REG_BMCR] & BIT_BMCR_RST) fprintf(stdout,
		"      Reset In Progress\n");

	/* Basic mode status register */
	fprintf(stdout,
		"0x84: BMSR (Basic Mode Status):          0x%04x\n",
		data[REG_BMSR]);
	fprintf(stdout,
		"      Link %s\n"
		"      %sCapable of Auto-Negotiation\n"
		"      Auto-Negotiation %sComplete\n"
		"      %sCapable of Preamble Suppression\n"
		"      %sCapable of 10Base-T Half Duplex\n"
		"      %sCapable of 10Base-T Full Duplex\n"
		"      %sCapable of 100Base-TX Half Duplex\n"
		"      %sCapable of 100Base-TX Full Duplex\n"
		"      %sCapable of 100Base-T4\n",
		data[REG_BMSR] & BIT_BMSR_LNK ? "Up" : "Down",
		data[REG_BMSR] & BIT_BMSR_ANCAP ? "" : "Not ",
		data[REG_BMSR] & BIT_BMSR_ANDONE ? "" : "Not ",
		data[REG_BMSR] & BIT_BMSR_PREAMBLE ? "" : "Not ",
		data[REG_BMSR] & BIT_BMSR_10HCAP ? "" : "Not ",
		data[REG_BMSR] & BIT_BMSR_10FCAP ? "" : "Not ",
		data[REG_BMSR] & BIT_BMSR_100HCAP ? "" : "Not ",
		data[REG_BMSR] & BIT_BMSR_100FCAP ? "" : "Not ",
		data[REG_BMSR] & BIT_BMSR_100T4CAP ? "" : "Not ");
	if (data[REG_BMSR] & BIT_BMSR_JABBER) fprintf(stdout,
		"      Jabber Condition Detected\n");
	if (data[REG_BMSR] & BIT_BMSR_RFAULT) fprintf(stdout,
		"      Remote Fault Detected\n");

	/* PHY identification registers */
	fprintf(stdout,
		"0x88: PHYIDR1 (PHY ID #1):               0x%04x\n",
		data[REG_PHYIDR1]);
	fprintf(stdout,
		"0x8c: PHYIDR2 (PHY ID #2):               0x%04x\n",
		data[REG_PHYIDR2]);
	fprintf(stdout,
		"      OUI = 0x%06x\n"
		"      Model = 0x%02x (%d)\n"
		"      Revision = 0x%01x (%d)\n",
		(data[REG_PHYIDR1] << 6) | (data[REG_PHYIDR2] >> 10),
		(data[REG_PHYIDR2] & BIT_PHYIDR2_MODEL) >> 4 & 0x3f,
		(data[REG_PHYIDR2] & BIT_PHYIDR2_MODEL) >> 4 & 0x3f,
		data[REG_PHYIDR2] & BIT_PHYIDR2_REV,
		data[REG_PHYIDR2] & BIT_PHYIDR2_REV);

	/* autonegotiation advertising register */
	fprintf(stdout,
		"0x90: ANAR (Autoneg Advertising):        0x%04x\n",
		data[REG_ANAR]);
	fprintf(stdout,
		"      Protocol Selector = 0x%02x (%d)\n",
		data[REG_ANAR] & BIT_ANAR_PROTO,
		data[REG_ANAR] & BIT_ANAR_PROTO);
	if (data[REG_ANAR] & BIT_ANAR_10) fprintf(stdout,
		"      Advertising 10Base-T Half Duplex\n");
	if (data[REG_ANAR] & BIT_ANAR_10_FD) fprintf(stdout,
		"      Advertising 10Base-T Full Duplex\n");
	if (data[REG_ANAR] & BIT_ANAR_TX) fprintf(stdout,
		"      Advertising 100Base-TX Half Duplex\n");
	if (data[REG_ANAR] & BIT_ANAR_TXFD) fprintf(stdout,
		"      Advertising 100Base-TX Full Duplex\n");
	if (data[REG_ANAR] & BIT_ANAR_T4) fprintf(stdout,
		"      Advertising 100Base-T4\n");
	if (data[REG_ANAR] & BIT_ANAR_PAUSE) fprintf(stdout,
		"      Advertising Pause\n");
	if (data[REG_ANAR] & BIT_ANAR_RF) fprintf(stdout,
		"      Indicating Remote Fault\n");
	if (data[REG_ANAR] & BIT_ANAR_NP) fprintf(stdout,
		"      Next Page Desired\n");

	/* Autonegotiation link partner ability register */
	fprintf(stdout,
		"0x94: ANLPAR (Autoneg Partner):          0x%04x\n",
		data[REG_ANLPAR]);
	fprintf(stdout,
		"      Protocol Selector = 0x%02x (%d)\n",
		data[REG_ANLPAR] & BIT_ANLPAR_PROTO,
		data[REG_ANLPAR] & BIT_ANLPAR_PROTO);
	if (data[REG_ANLPAR] & BIT_ANLPAR_10) fprintf(stdout,
		"      Supports 10Base-T Half Duplex\n");
	if (data[REG_ANLPAR] & BIT_ANLPAR_10_FD) fprintf(stdout,
		"      Supports 10Base-T Full Duplex\n");
	if (data[REG_ANLPAR] & BIT_ANLPAR_TX) fprintf(stdout,
		"      Supports 100Base-TX Half Duplex\n");
	if (data[REG_ANLPAR] & BIT_ANLPAR_TXFD) fprintf(stdout,
		"      Supports 100Base-TX Full Duplex\n");
	if (data[REG_ANLPAR] & BIT_ANLPAR_T4) fprintf(stdout,
		"      Supports 100Base-T4\n");
	if (data[REG_ANLPAR] & BIT_ANLPAR_PAUSE) fprintf(stdout,
		"      Supports Pause\n");
	if (data[REG_ANLPAR] & BIT_ANLPAR_RF) fprintf(stdout,
		"      Indicates Remote Fault\n");
	if (data[REG_ANLPAR] & BIT_ANLPAR_ACK) fprintf(stdout,
		"      Indicates Acknowledgement\n");
	if (data[REG_ANLPAR] & BIT_ANLPAR_NP) fprintf(stdout,
		"      Next Page Desired\n");

	/* Autonegotiation expansion register */
	fprintf(stdout,
		"0x98: ANER (Autoneg Expansion):          0x%04x\n",
		data[REG_ANER]);
	fprintf(stdout,
		"      Link Partner Can %sAuto-Negotiate\n"
		"      Link Code Word %sReceived\n"
		"      Next Page %sSupported\n"
		"      Link Partner Next Page %sSupported\n",
		data[REG_ANER] & BIT_ANER_LP_AN_ENABLE ? "" : "Not ",
		data[REG_ANER] & BIT_ANER_PAGE_RX ? "" : "Not ",
		data[REG_ANER] & BIT_ANER_NP_ABLE ? "" : "Not ",
		data[REG_ANER] & BIT_ANER_LP_NP_ABLE ? "" : "Not ");
	if (data[REG_ANER] & BIT_ANER_PDF) fprintf(stdout,
		"      Parallel Detection Fault\n");

	/* Autonegotiation next-page tx register */
	fprintf(stdout,
		"0x9c: ANNPTR (Autoneg Next Page Tx):     0x%04x\n",
		data[REG_ANNPTR]);

	/* Phy status register */
	fprintf(stdout,
		"0xc0: PHYSTS (Phy Status):               0x%04x\n",
		data[REG_PHYSTS]);
	fprintf(stdout,
		"      Link %s\n"
		"      %d Mb/s\n"
		"      %s Duplex\n"
		"      Auto-Negotiation %sComplete\n"
		"      %s Polarity\n",
		data[REG_PHYSTS] & BIT_PHYSTS_LNK ? "Up" : "Down",
		data[REG_PHYSTS] & BIT_PHYSTS_SPD10 ? 10 : 100,
		data[REG_PHYSTS] & BIT_PHYSTS_FDUP ? "Full" : "Half",
		data[REG_PHYSTS] & BIT_PHYSTS_ANDONE ? "" : "Not ",
		data[REG_PHYSTS] & BIT_PHYSTS_POL ? "Reverse" : "Normal");
	if (data[REG_PHYSTS] & BIT_PHYSTS_LOOP) fprintf(stdout,
		"      Loopback Enabled\n");
	if (data[REG_PHYSTS] & BIT_PHYSTS_JABBER) fprintf(stdout,
		"      Jabber Condition Detected\n");
	if (data[REG_PHYSTS] & BIT_PHYSTS_RF) fprintf(stdout,
		"      Remote Fault Detected\n");
	if (data[REG_PHYSTS] & BIT_PHYSTS_MINT) fprintf(stdout,
		"      MII Interrupt Detected\n");
	if (data[REG_PHYSTS] & BIT_PHYSTS_FC) fprintf(stdout,
		"      False Carrier Detected\n");
	if (data[REG_PHYSTS] & BIT_PHYSTS_RXERR) fprintf(stdout,
		"      Rx Error Detected\n");

	fprintf(stdout,
		"0xc4: MICR (MII Interrupt Control):      0x%04x\n",
		data[REG_MICR]);
	fprintf(stdout,
		"      MII Interrupts %s\n",
		data[REG_MICR] & BIT_MICR_INTEN ? "Enabled" : "Disabled");

	fprintf(stdout,
		"0xc8: MISR (MII Interrupt Status):       0x%04x\n",
		data[REG_MISR]);
	fprintf(stdout,
		"      Rx Error Counter Half-Full Interrupt %s\n"
		"      False Carrier Counter Half-Full Interrupt %s\n"
		"      Auto-Negotiation Complete Interrupt %s\n"
		"      Remote Fault Interrupt %s\n"
		"      Jabber Interrupt %s\n"
		"      Link Change Interrupt %s\n",
		data[REG_MISR] & BIT_MISR_MSK_RHF ? "Masked" : "Enabled",
		data[REG_MISR] & BIT_MISR_MSK_FHF ? "Masked" : "Enabled",
		data[REG_MISR] & BIT_MISR_MSK_ANC ? "Masked" : "Enabled",
		data[REG_MISR] & BIT_MISR_MSK_RF ? "Masked" : "Enabled",
		data[REG_MISR] & BIT_MISR_MSK_JAB ? "Masked" : "Enabled",
		data[REG_MISR] & BIT_MISR_MSK_LNK ? "Masked" : "Enabled");
	if (data[REG_MISR] & BIT_MISR_MINT) fprintf(stdout,
		"      MII Interrupt Pending\n");

	/* Page select register (from section of spec on 'suggested values') */
	fprintf(stdout,
		"0xcc: PGSEL (Phy Register Page Select):  0x%04x\n",
		data[REG_PGSEL]);

	/* counters */
	fprintf(stdout,
		"0xd0: FCSCR (False Carrier Counter):     0x%04x\n",
		data[REG_FCSCR]);
	fprintf(stdout,
		"      Value = %d\n", data[REG_FCSCR] & 0xff);
	fprintf(stdout,
		"0xd4: RECR (Rx Error Counter):           0x%04x\n",
		data[REG_RECR]);
	fprintf(stdout,
		"      Value = %d\n", data[REG_RECR] & 0xff);

	/* 100 Mbit configuration register */
	fprintf(stdout,
		"0xd8: PCSR (100Mb/s PCS Config/Status):  0x%04x\n",
		data[REG_PCSR]);
	fprintf(stdout,
		"      NRZI Bypass %s\n"
		"      %s Signal Detect Algorithm\n"
		"      %s Signal Detect Operation\n"
		"      True Quiet Mode %s\n"
		"      Rx Clock is %s\n"
		"      4B/5B Operation %s\n",
		data[REG_PCSR] & BIT_PCSR_NRZI ? "Enabled" : "Disabled",
		data[REG_PCSR] & BIT_PCSR_SDOPT ? "Enhanced" : "Reduced",
		data[REG_PCSR] & BIT_PCSR_SDFORCE ? "Forced" : "Normal",
		data[REG_PCSR] & BIT_PCSR_TQM ? "Enabled" : "Disabled",
		data[REG_PCSR] & BIT_PCSR_CLK ?
			"Free-Running" : "Phase-Adjusted",
		data[REG_PCSR] & BIT_PCSR_4B5B ? "Bypassed" : "Normal");
	if (data[REG_PCSR] & BIT_PCSR_FORCE_100) fprintf(stdout,
		"      Forced 100 Mb/s Good Link\n");

	/* Phy control register */
	fprintf(stdout,
		"0xe4: PHYCR (Phy Control):               0x%04x\n",
		data[REG_PHYCR]);
	fprintf(stdout,
		"      Phy Address = 0x%x (%d)\n"
		"      %sPause Compatible with Link Partner\n"
		"      LED Stretching %s\n"
		"      Phy Self Test %s\n"
		"      Self Test Sequence = PSR%d\n",
		data[REG_PHYCR] & BIT_PHYCR_PHYADDR,
		data[REG_PHYCR] & BIT_PHYCR_PHYADDR,
		data[REG_PHYCR] & BIT_PHYCR_PAUSE_STS ? "" : "Not ",
		data[REG_PHYCR] & BIT_PHYCR_STRETCH ? "Bypassed" : "Enabled",
		data[REG_PHYCR] & BIT_PHYCR_BIST ? "In Progress" :
		  data[REG_PHYCR] & BIT_PHYCR_BIST_STAT ?
		    "Passed" : "Failed or Not Run",
		data[REG_PHYCR] & BIT_PHYCR_PSR15 ? 15 : 9);


	/* 10 Mbit control and status register */
	fprintf(stdout,
		"0xe8: TBTSCR (10Base-T Status/Control):  0x%04x\n",
		data[REG_TBTSCR]);
	fprintf(stdout,
		"      Jabber %s\n"
		"      Heartbeat %s\n"
		"      Polarity Auto-Sense/Correct %s\n"
		"      %s Polarity %s\n"
		"      Normal Link Pulse %s\n"
		"      10 Mb/s Loopback %s\n",
		data[REG_TBTSCR] & BIT_TBTSCR_JAB ? "Disabled" : "Enabled",
		data[REG_TBTSCR] & BIT_TBTSCR_BEAT ? "Disabled" : "Enabled",
		data[REG_TBTSCR] & BIT_TBTSCR_AUTOPOL ? "Disabled" : "Enabled",
		data[REG_TBTSCR] & BIT_TBTSCR_AUTOPOL ?
			data[REG_TBTSCR]&BIT_TBTSCR_FPOL ? "Reverse":"Normal" :
			data[REG_TBTSCR]&BIT_TBTSCR_POL ? "Reverse":"Normal",
		data[REG_TBTSCR] & BIT_TBTSCR_AUTOPOL ? "Forced" : "Detected",
		data[REG_TBTSCR] & BIT_TBTSCR_PULSE ? "Disabled" : "Enabled",
		data[REG_TBTSCR] & BIT_TBTSCR_LOOP ? "Enabled" : "Disabled");
	if (data[REG_TBTSCR] & BIT_TBTSCR_FORCE_10) fprintf(stdout,
		"      Forced 10 Mb/s Good Link\n");

	/* the spec says to set these */
	fprintf(stdout, "\n");
	fprintf(stdout, "'Magic' Phy Registers\n");
	fprintf(stdout, "---------------------\n");
	fprintf(stdout,
		"0xe4: PMDCSR:                            0x%04x\n",
		data[REG_PMDCSR]);
	fprintf(stdout,
		"0xf4: DSPCFG:                            0x%04x\n",
		data[REG_DSPCFG]);
	fprintf(stdout,
		"0xf8: SDCFG:                             0x%04x\n",
		data[REG_SDCFG]);
	fprintf(stdout,
		"0xfc: TSTDAT:                            0x%04x\n",
		data[REG_TSTDAT]);

	return 0;
}

int
natsemi_dump_eeprom(struct ethtool_drvinfo *info, struct ethtool_eeprom *ee)
{
	int i;
	u16 *eebuf = (u16 *)ee->data;

	if (ee->magic != NATSEMI_MAGIC) {
		fprintf(stderr, "Magic number 0x%08x does not match 0x%08x\n",
			ee->magic, NATSEMI_MAGIC);
		return -1;
	}

	fprintf(stdout, "Address\tData\n");
	fprintf(stdout, "-------\t------\n");
	for (i = 0; i < ee->len/2; i++) {
		fprintf(stdout, "0x%02x   \t0x%04x\n", i + ee->offset, eebuf[i]);
	}

	return 0;
}

