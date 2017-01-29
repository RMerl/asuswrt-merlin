
/* Copyright (C) 2003  Advanced Micro Devices Inc. */
#include <stdio.h>
#include "internal.h"

typedef enum {
	/* VAL2 */
	RDMD0			= (1 << 16),
	/* VAL1 */
	TDMD3			= (1 << 11),
	TDMD2			= (1 << 10),
	TDMD1			= (1 << 9),
	TDMD0			= (1 << 8),
	/* VAL0 */
	UINTCMD			= (1 << 6),
	RX_FAST_SPND		= (1 << 5),
	TX_FAST_SPND		= (1 << 4),
	RX_SPND			= (1 << 3),
	TX_SPND			= (1 << 2),
	INTREN			= (1 << 1),
	RUN			= (1 << 0),

	CMD0_CLEAR 		= 0x000F0F7F,   /* Command style register */

}CMD0_BITS;
typedef enum {

	/* VAL3 */
	CONDUIT_MODE		= (1 << 29),
	/* VAL2 */
	RPA			= (1 << 19),
	DRCVPA			= (1 << 18),
	DRCVBC			= (1 << 17),
	PROM			= (1 << 16),
	/* VAL1 */
	ASTRP_RCV		= (1 << 13),
	RCV_DROP0	  	= (1 << 12),
	EMBA			= (1 << 11),
	DXMT2PD			= (1 << 10),
	LTINTEN			= (1 << 9),
	DXMTFCS			= (1 << 8),
	/* VAL0 */
	APAD_XMT		= (1 << 6),
	DRTY			= (1 << 5),
	INLOOP			= (1 << 4),
	EXLOOP			= (1 << 3),
	REX_RTRY		= (1 << 2),
	REX_UFLO		= (1 << 1),
	REX_LCOL		= (1 << 0),

	CMD2_CLEAR 		= 0x3F7F3F7F,   /* Command style register */

}CMD2_BITS;
typedef enum {

	/* VAL3 */
	ASF_INIT_DONE_ALIAS	= (1 << 29),
	/* VAL2 */
	JUMBO			= (1 << 21),
	VSIZE			= (1 << 20),
	VLONLY			= (1 << 19),
	VL_TAG_DEL		= (1 << 18),
	/* VAL1 */
	EN_PMGR			= (1 << 14),
	INTLEVEL		= (1 << 13),
	FORCE_FULL_DUPLEX	= (1 << 12),
	FORCE_LINK_STATUS	= (1 << 11),
	APEP			= (1 << 10),
	MPPLBA			= (1 << 9),
	/* VAL0 */
	RESET_PHY_PULSE		= (1 << 2),
	RESET_PHY		= (1 << 1),
	PHY_RST_POL		= (1 << 0),

}CMD3_BITS;
typedef enum {

	INTR			= (1 << 31),
	PCSINT			= (1 << 28),
	LCINT			= (1 << 27),
	APINT5			= (1 << 26),
	APINT4			= (1 << 25),
	APINT3			= (1 << 24),
	TINT_SUM		= (1 << 23),
	APINT2			= (1 << 22),
	APINT1			= (1 << 21),
	APINT0			= (1 << 20),
	MIIPDTINT		= (1 << 19),
	MCCINT			= (1 << 17),
	MREINT			= (1 << 16),
	RINT_SUM		= (1 << 15),
	SPNDINT			= (1 << 14),
	MPINT			= (1 << 13),
	SINT			= (1 << 12),
	TINT3			= (1 << 11),
	TINT2			= (1 << 10),
	TINT1			= (1 << 9),
	TINT0			= (1 << 8),
	UINT			= (1 << 7),
	STINT			= (1 << 4),
	RINT0			= (1 << 0),

}INT0_BITS;
typedef enum {

	/* VAL3 */
	LCINTEN			= (1 << 27),
	APINT5EN		= (1 << 26),
	APINT4EN		= (1 << 25),
	APINT3EN		= (1 << 24),
	/* VAL2 */
	APINT2EN		= (1 << 22),
	APINT1EN		= (1 << 21),
	APINT0EN		= (1 << 20),
	MIIPDTINTEN		= (1 << 19),
	MCCIINTEN		= (1 << 18),
	MCCINTEN		= (1 << 17),
	MREINTEN		= (1 << 16),
	/* VAL1 */
	SPNDINTEN		= (1 << 14),
	MPINTEN			= (1 << 13),
	TINTEN3			= (1 << 11),
	SINTEN			= (1 << 12),
	TINTEN2			= (1 << 10),
	TINTEN1			= (1 << 9),
	TINTEN0			= (1 << 8),
	/* VAL0 */
	STINTEN			= (1 << 4),
	RINTEN0			= (1 << 0),

	INTEN0_CLEAR 		= 0x1F7F7F1F, /* Command style register */

}INTEN0_BITS;

typedef enum {

	PMAT_DET		= (1 << 12),
	MP_DET		        = (1 << 11),
	LC_DET			= (1 << 10),
	SPEED_MASK		= (1 << 9)|(1 << 8)|(1 << 7),
	FULL_DPLX		= (1 << 6),
	LINK_STATS		= (1 << 5),
	AUTONEG_COMPLETE	= (1 << 4),
	MIIPD			= (1 << 3),
	RX_SUSPENDED		= (1 << 2),
	TX_SUSPENDED		= (1 << 1),
	RUNNING			= (1 << 0),

}STAT0_BITS;

#define PHY_SPEED_10		0x2
#define PHY_SPEED_100		0x3


int amd8111e_dump_regs(struct ethtool_drvinfo *info, struct ethtool_regs *regs)
{

	u32 *reg_buff = (u32 *)regs->data;
	u32 reg;

	fprintf(stdout, "Descriptor Registers\n");
	fprintf(stdout, "---------------------\n");

	/* Transmit descriptor base address register */
	reg = reg_buff[0];
	fprintf(stdout,
		"0x00100: Transmit descriptor base address register %08X\n",reg);

	/* Transmit descriptor length register */
	reg = reg_buff[1];
	fprintf(stdout,
		"0x00140: Transmit descriptor length register 0x%08X\n",reg);

	/* Receive descriptor base address register */
	reg = reg_buff[2];
	fprintf(stdout,
		"0x00120: Receive descriptor base address register %08X\n",reg);

	/* Receive descriptor length register */
	reg = reg_buff[3];
	fprintf(stdout,
		"0x00150: Receive descriptor length register 0x%08X\n",reg);

	fprintf(stdout, "\n");


	fprintf(stdout, "Command Registers\n");
	fprintf(stdout, "-------------------\n");

	/* Command 0 Register */
	reg = reg_buff[4];
	fprintf(stdout,
		"0x00048: Command 0 register  0x%08X\n"
		"	Interrupts:				%s\n"
		"	Device:					%s\n",
		reg,
		reg & INTREN   		? "Enabled"     : "Disabled",
		reg & RUN    		? "Running"     : "Stopped");

	/* Command 2 Register */
	reg = reg_buff[5];
	fprintf(stdout,
		"0x00050: Command 2 register  0x%08X\n"
		"	Promiscuous mode:			%s\n"
		"	Retransmit on underflow:		%s\n",
		reg,
		reg & PROM   		? "Enabled"     : "Disabled",
		reg & REX_UFLO   	? "Enabled"     : "Disabled");
	/* Command 3 Register */
	reg = reg_buff[6];
	fprintf(stdout,
		"0x00054: Command 3 register  0x%08X\n"
		"	Jumbo frame:				%s\n"
		"	Admit only VLAN frame:	 		%s\n"
		"	Delete VLAN tag:			%s\n",
		reg,
		reg & JUMBO  		? "Enabled"     : "Disabled",
		reg &  VLONLY 		? "Yes"     : "No",
		reg &  VL_TAG_DEL 		? "Yes"     : "No");

	/* Command 7 Register */
	reg = reg_buff[7];
	fprintf(stdout,
		"0x00064: Command 7 register  0x%08X\n",
		 reg);

	fprintf(stdout, "\n");
	fprintf(stdout, "Interrupt Registers\n");
	fprintf(stdout, "-------------------\n");

	/* Interrupt 0 Register */
	reg = reg_buff[8];
	fprintf(stdout,
		"0x00038: Interrupt register  0x%08X\n"
		"	Any interrupt is set: 			%s\n"
		"	Link change interrupt:	  		%s\n"
		"	Register 0 auto-poll interrupt:		%s\n"
		"	Transmit interrupt:			%s\n"
		"	Software timer interrupt:		%s\n"
		"	Receive interrupt:			%s\n",
		 reg,
		 reg &   INTR		? "Yes"     : "No",
		 reg &   LCINT		? "Yes"     : "No",
		 reg &   APINT0		? "Yes"     : "No",
		 reg &   TINT0		? "Yes"     : "No",
		 reg &   STINT		? "Yes"     : "No",
		 reg &   RINT0		? "Yes"     : "No"
		 );
	/* Interrupt 0 enable Register */
	reg = reg_buff[9];
	fprintf(stdout,
		"0x00040: Interrupt enable register  0x%08X\n"
		"	Link change interrupt:	  		%s\n"
		"	Register 0 auto-poll interrupt:		%s\n"
		"	Transmit interrupt:			%s\n"
		"	Software timer interrupt:		%s\n"
		"	Receive interrupt:			%s\n",
		 reg,
		 reg &   LCINTEN		? "Enabled"     : "Disabled",
		 reg &   APINT0EN		? "Enabled"     : "Disabled",
		 reg &   TINTEN0		? "Enabled"     : "Disabled",
		 reg &   STINTEN		? "Enabled"     : "Disabled",
		 reg &   RINTEN0		? "Enabled"     : "Disabled"
		);

	fprintf(stdout, "\n");
	fprintf(stdout, "Logical Address Filter Register\n");
	fprintf(stdout, "-------------------\n");

	/* Logical Address Filter Register */
	fprintf(stdout,
		"0x00168: Logical address filter register  0x%08X%08X\n",
		 reg_buff[11],reg_buff[10]);

	fprintf(stdout, "\n");
	fprintf(stdout, "Link status Register\n");
	fprintf(stdout, "-------------------\n");

	/* Status 0  Register */
	reg = reg_buff[12];
	if(reg & LINK_STATS){
	fprintf(stdout,
		"0x00030: Link status register  0x%08X\n"
		"	Link status:	  		%s\n"
		"	Auto negotiation complete	%s\n"
		"	Duplex				%s\n"
		"	Speed				%s\n",
		reg,
		reg &  LINK_STATS 		? "Valid"     : "Invalid",
		reg &  AUTONEG_COMPLETE		? "Yes"	      : "No",
		reg &  FULL_DPLX 		? "Full"      : "Half",
		((reg & SPEED_MASK) >> 7 == PHY_SPEED_10) ? "10Mbits/ Sec":
							"100Mbits/Sec");

	}
	else{
	fprintf(stdout,
		"0x00030: Link status register  0x%08X\n"
		"	Link status:	  		%s\n",
		reg,
		reg &  LINK_STATS 		? "Valid"     : "Invalid");
	}
	return 0;

}
