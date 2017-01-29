/* Copyright 2004 IBM Corporation (jklewis@us.ibm.com) */

#include <stdio.h>
#include <stdlib.h>
#include "internal.h"

#define BIT0  0x0001
#define BIT1  0x0002
#define BIT2  0x0004
#define BIT3  0x0008
#define BIT4  0x0010
#define BIT5  0x0020
#define BIT6  0x0040
#define BIT7  0x0080
#define BIT8  0x0100
#define BIT9  0x0200
#define BIT10 0x0400
#define BIT11 0x0800
#define BIT12 0x1000
#define BIT13 0x2000
#define BIT14 0x4000
#define BIT15 0x8000

int pcnet32_dump_regs(struct ethtool_drvinfo *info, struct ethtool_regs *regs)
{
	int i, csr;
	u16 *data = (u16 *) regs->data;
	int len = regs->len / 2;
	u16 temp,*ptr;

	printf("Driver:  %s\n",info->driver);
	printf("Version: %s\n",info->version);

	printf("APROM:  ");
	for (i=0; i<8; i++)
		printf(" %04x ", data[i]);
	printf("\n");

	csr = i;
	for (; i<100; i++)
	 {
		if (((i-csr) & 7) == 0) printf("CSR%02d:  ", i-csr);
		printf(" %04x ", data[i]);
		if (((i-csr) & 7) == 7) printf("\n");
	 }
	if (((i-csr) & 7) != 7) printf("\n");

	csr = i;
	for (; i<136; i++)
	 {
		if (((i-csr) & 7) == 0) printf("BCR%02d:  ", i-csr);
		printf(" %04x ", data[i]);
		if (((i-csr) & 7) == 7) printf("\n");
	 }
	if (((i-csr) & 7) != 7) printf("\n");

	csr = i;
	for (; i<len; i++)
	 {
	  if (((i-csr) & 7) == 0) printf("MII%02d:  ", (i-csr) & 0x1f);
		printf(" %04x ", data[i]);
	  if (((i-csr) & 7) == 7) printf("\n");
	 }
	if (((i-csr) & 7) != 7) printf("\n");
	printf("\n");

	ptr=&data[8];            /* start of the CSRs */

	printf("CSR0:   Status and Control         0x%04x\n  ",ptr[0]);
	temp=ptr[0];

	if(temp & BIT15) printf("ERR ");
	if(temp & BIT14) printf("BABL ");
	if(temp & BIT13) printf("CERR ");
	if(temp & BIT12) printf("MISS ");
	if(temp & BIT11) printf("MERR ");
	if(temp & BIT10) printf("RINT ");
	if(temp & BIT9)  printf("TINT ");
	if(temp & BIT8)  printf("IDON ");
	if(temp & BIT7)  printf("INTR ");
	if(temp & BIT6)  printf("INT ");
	if(temp & BIT5)  printf("RXON ");
	if(temp & BIT4)  printf("TXON ");
	if(temp & BIT3)  printf("TDMD ");
	if(temp & BIT2)  printf("STOP ");
	if(temp & BIT1)  printf("STRT ");
	if(temp & BIT0)  printf("INIT ");

	printf("\n");

	printf("CSR3:   Interrupt Mask             0x%04x\n  ",ptr[3]);
	temp=ptr[3];

	if(temp & BIT14) printf("BABLM ");
	if(temp & BIT12) printf("MISSM ");
	if(temp & BIT11) printf("MERRM ");
	if(temp & BIT10) printf("RINTM ");
	if(temp & BIT9)  printf("TINTM ");
	if(temp & BIT8)  printf("IDONM ");
	if(temp & BIT6)  printf("DXSUFLO ");
	if(temp & BIT5)  printf("LAPPEN ");
	if(temp & BIT4)  printf("DXMT2PD ");
	if(temp & BIT3)  printf("EMBA ");
	if(temp & BIT2)  printf("BSWP ");

	printf("\n");

	printf("CSR4:   Test and Features          0x%04x\n  ",ptr[4]);
	temp=ptr[4];

	if(temp & BIT15) printf("EN124 ");
	if(temp & BIT14) printf("DMAPLUS ");
	if(temp & BIT12) printf("TXDPOLL ");
	if(temp & BIT11) printf("APAD_XMT ");
	if(temp & BIT10) printf("ASTRP_RCV ");
	if(temp & BIT9)  printf("MFCO ");
	if(temp & BIT8)  printf("MFCON ");
	if(temp & BIT7)  printf("UINTCMD ");
	if(temp & BIT6)  printf("UINT ");
	if(temp & BIT5)  printf("RCVCCO ");
	if(temp & BIT4)  printf("RCVCCOM ");
	if(temp & BIT3)  printf("TXSTRT ");
	if(temp & BIT2)  printf("TXSTRTM ");
	if(temp & BIT1)  printf("JAB ");
	if(temp & BIT0)  printf("JABM ");

	printf("\n");

	printf("CSR5:   Ext Control and Int 1      0x%04x\n  ",ptr[5]);
	temp=ptr[5];

	if(temp & BIT15) printf("TOKINTD ");
	if(temp & BIT14) printf("LTINTEN ");
	if(temp & BIT11) printf("SINT ");
	if(temp & BIT10) printf("SINTE ");
	if(temp & BIT9)  printf("SLPINT ");
	if(temp & BIT8)  printf("SLPINTE ");
	if(temp & BIT7)  printf("EXDINT ");
	if(temp & BIT6)  printf("EXDINTE ");
	if(temp & BIT5)  printf("MPPLBA ");
	if(temp & BIT4)  printf("MPINT ");
	if(temp & BIT3)  printf("MPINTE ");
	if(temp & BIT2)  printf("MPEN ");
	if(temp & BIT1)  printf("MPMODE ");
	if(temp & BIT0)  printf("SPND ");

	printf("\n");

	printf("CSR7:   Ext Control and Int 2      0x%04x\n  ",ptr[7]);
	temp=ptr[7];

	if(temp & BIT15) printf("FASTSPNDE ");
	if(temp & BIT14) printf("RXFRTG ");
	if(temp & BIT13) printf("RDMD ");
	if(temp & BIT12) printf("RXDPOLL ");
	if(temp & BIT11) printf("STINT ");
	if(temp & BIT10) printf("STINTE ");
	if(temp & BIT9)  printf("MREINT ");
	if(temp & BIT8)  printf("MREINTE ");
	if(temp & BIT7)  printf("MAPINT ");
	if(temp & BIT6)  printf("MAPINTE ");
	if(temp & BIT5)  printf("MCCINT ");
	if(temp & BIT4)  printf("MCCINTE ");
	if(temp & BIT3)  printf("MCCIINT ");
	if(temp & BIT2)  printf("MCCIINTE ");
	if(temp & BIT1)  printf("MIIPDTINT ");
	if(temp & BIT0)  printf("MIIPDTINTE ");

	printf("\n");

	printf("CSR15:  Mode                       0x%04x\n",ptr[15]);
	printf("CSR40:  Current RX Byte Count      0x%04x\n",ptr[40]);
	printf("CSR41:  Current RX Status          0x%04x\n",ptr[41]);
	printf("CSR42:  Current TX Byte Count      0x%04x\n",ptr[42]);
	printf("CSR43:  Current TX Status          0x%04x\n",ptr[43]);
	printf("CSR88:  Chip ID Lower              0x%04x\n",ptr[88]);
	temp = (((ptr[89] << 16) | ptr[88]) >> 12) & 0xffff;
	switch (temp) {
		case 0x2420:
			printf("  PCnet/PCI 79C970\n");
			break;
		case 0x2621:
			printf("  PCnet/PCI II 79C970A\n");
			break;
		case 0x2623:
			printf("  PCnet/FAST 79C971\n");
			break;
		case 0x2624:
			printf("  PCnet/FAST+ 79C972\n");
			break;
		case 0x2625:
			printf("  PCnet/FAST III 79C973\n");
			break;
		case 0x2626:
			printf("  PCnet/Home 79C978\n");
			break;
		case 0x2627:
			printf("  PCnet/FAST III 79C975\n");
			break;
		case 0x2628:
			printf("  PCnet/PRO 79C976\n");
			break;
	}

	printf("CSR89:  Chip ID Upper              0x%04x\n  ",ptr[89]);
	temp=ptr[89];
	printf("VER: %04x  PARTIDU: %04x\n",temp >> 12,temp & 0x00000fff);

	printf("CSR112: Missed Frame Count         0x%04x\n",ptr[90]);    /* 90 is 112 */
	printf("CSR114: RX Collision Count         0x%04x\n",ptr[91]);

	printf("\n");

	ptr=&data[100];        /* point to BCR 0 */

	printf("BCR2:   Misc. Configuration        0x%04x\n  ",ptr[2]);
	temp=ptr[2];

	if(temp & BIT14) printf("TMAULOOP ");
	if(temp & BIT12) printf("LEDPE ");

	if(temp & BIT8)  printf("APROMWE ");
	if(temp & BIT7)  printf("INTLEVEL ");

	if(temp & BIT3)  printf("EADISEL ");
	if(temp & BIT2)  printf("AWAKE ");
	if(temp & BIT1)  printf("ASEL ");
	if(temp & BIT0)  printf("XMAUSEL ");

	printf("\n");

	printf("BCR9:   Full-Duplex Control        0x%04x\n",ptr[9]);
	printf("BCR18:  Burst and Bus Control      0x%04x\n",ptr[18]);

	printf("BCR19:  EEPROM Control and Status  0x%04x\n  ",ptr[19]);
	temp=ptr[19];
	if(temp & BIT15) printf("PVALID ");
	if(temp & BIT13) printf("EEDET ");
	printf("\n");

	printf("BCR23:  PCI Subsystem Vendor ID    0x%04x\n",ptr[23]);
	printf("BCR24:  PCI Subsystem ID           0x%04x\n",ptr[24]);
	printf("BCR31:  Software Timer             0x%04x\n",ptr[31]);
	printf("BCR32:  MII Control and Status     0x%04x\n",ptr[32]);
	printf("BCR35:  PCI Vendor ID              0x%04x\n",ptr[35]);

	return(0);
}

