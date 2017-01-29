/*
 * qsfp.c: Implements SFF-8636 based QSFP+/QSFP28 Diagnostics Memory map.
 *
 * Copyright 2010 Solarflare Communications Inc.
 * Aurelien Guillaume <aurelien@iwi.me> (C) 2012
 * Copyright (C) 2014 Cumulus networks Inc.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Freeoftware Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 *  Vidya Ravipati <vidya@cumulusnetworks.com>
 *   This implementation is loosely based on current SFP parser
 *   and SFF-8636 spec Rev 2.7 (ftp://ftp.seagate.com/pub/sff/SFF-8636.PDF)
 *   by SFF Committee.
 */

/*
 *	Description:
 *	a) The register/memory layout is up to 5 128 byte pages defined by
 *		a "pages valid" register and switched via a "page select"
 *		register. Memory of 256 bytes can be memory mapped at a time
 *		according to SFF 8636.
 *	b) SFF 8636 based 640 bytes memory layout is presented for parser
 *
 *           SFF 8636 based QSFP Memory Map
 *
 *           2-Wire Serial Address: 1010000x
 *
 *           Lower Page 00h (128 bytes)
 *           ======================
 *           |                     |
 *           |Page Select Byte(127)|
 *           ======================
 *                    |
 *                    V
 *	     ----------------------------------------
 *	    |             |            |             |
 *	    V             V            V             V
 *	 ----------   ----------   ---------    ------------
 *	| Upper    | | Upper    | | Upper    | | Upper      |
 *	| Page 00h | | Page 01h | | Page 02h | | Page 03h   |
 *	|          | |(Optional)| |(Optional)| | (Optional) |
 *	|          | |          | |          | |            |
 *	|          | |          | |          | |            |
 *	|    ID    | |   AST    | |  User    | |  For       |
 *	|  Fields  | |  Table   | | EEPROM   | |  Cable     |
 *	|          | |          | | Data     | | Assemblies |
 *	|          | |          | |          | |            |
 *	|          | |          | |          | |            |
 *	-----------  -----------   ----------  --------------
 *
 *
 **/
#include <stdio.h>
#include <math.h>
#include "internal.h"
#include "sff-common.h"
#include "qsfp.h"

#define MAX_DESC_SIZE	42

static struct sff8636_aw_flags {
	const char *str;        /* Human-readable string, null at the end */
	int offset;             /* A2-relative address offset */
	__u8 value;             /* Alarm is on if (offset & value) != 0. */
} sff8636_aw_flags[] = {
	{ "Laser bias current high alarm   (Chan 1)",
		SFF8636_TX_BIAS_12_AW_OFFSET, (SFF8636_TX_BIAS_1_HALARM) },
	{ "Laser bias current low alarm    (Chan 1)",
		SFF8636_TX_BIAS_12_AW_OFFSET, (SFF8636_TX_BIAS_1_LALARM) },
	{ "Laser bias current high warning (Chan 1)",
		SFF8636_TX_BIAS_12_AW_OFFSET, (SFF8636_TX_BIAS_1_HWARN) },
	{ "Laser bias current low warning  (Chan 1)",
		SFF8636_TX_BIAS_12_AW_OFFSET, (SFF8636_TX_BIAS_1_LWARN) },

	{ "Laser bias current high alarm   (Chan 2)",
		SFF8636_TX_BIAS_12_AW_OFFSET, (SFF8636_TX_BIAS_2_HALARM) },
	{ "Laser bias current low alarm    (Chan 2)",
		SFF8636_TX_BIAS_12_AW_OFFSET, (SFF8636_TX_BIAS_2_LALARM) },
	{ "Laser bias current high warning (Chan 2)",
		SFF8636_TX_BIAS_12_AW_OFFSET, (SFF8636_TX_BIAS_2_HWARN) },
	{ "Laser bias current low warning  (Chan 2)",
		SFF8636_TX_BIAS_12_AW_OFFSET, (SFF8636_TX_BIAS_2_LWARN) },

	{ "Laser bias current high alarm   (Chan 3)",
		SFF8636_TX_BIAS_34_AW_OFFSET, (SFF8636_TX_BIAS_3_HALARM) },
	{ "Laser bias current low alarm    (Chan 3)",
		SFF8636_TX_BIAS_34_AW_OFFSET, (SFF8636_TX_BIAS_3_LALARM) },
	{ "Laser bias current high warning (Chan 3)",
		SFF8636_TX_BIAS_34_AW_OFFSET, (SFF8636_TX_BIAS_3_HWARN) },
	{ "Laser bias current low warning  (Chan 3)",
		SFF8636_TX_BIAS_34_AW_OFFSET, (SFF8636_TX_BIAS_3_LWARN) },

	{ "Laser bias current high alarm   (Chan 4)",
		SFF8636_TX_BIAS_34_AW_OFFSET, (SFF8636_TX_BIAS_4_HALARM) },
	{ "Laser bias current low alarm    (Chan 4)",
		SFF8636_TX_BIAS_34_AW_OFFSET, (SFF8636_TX_BIAS_4_LALARM) },
	{ "Laser bias current high warning (Chan 4)",
		SFF8636_TX_BIAS_34_AW_OFFSET, (SFF8636_TX_BIAS_4_HWARN) },
	{ "Laser bias current low warning  (Chan 4)",
		SFF8636_TX_BIAS_34_AW_OFFSET, (SFF8636_TX_BIAS_4_LWARN) },

	{ "Module temperature high alarm",
		SFF8636_TEMP_AW_OFFSET, (SFF8636_TEMP_HALARM_STATUS) },
	{ "Module temperature low alarm",
		SFF8636_TEMP_AW_OFFSET, (SFF8636_TEMP_LALARM_STATUS) },
	{ "Module temperature high warning",
		SFF8636_TEMP_AW_OFFSET, (SFF8636_TEMP_HWARN_STATUS) },
	{ "Module temperature low warning",
		SFF8636_TEMP_AW_OFFSET, (SFF8636_TEMP_LWARN_STATUS) },

	{ "Module voltage high alarm",
		SFF8636_VCC_AW_OFFSET, (SFF8636_VCC_HALARM_STATUS) },
	{ "Module voltage low alarm",
		SFF8636_VCC_AW_OFFSET, (SFF8636_VCC_LALARM_STATUS) },
	{ "Module voltage high warning",
		SFF8636_VCC_AW_OFFSET, (SFF8636_VCC_HWARN_STATUS) },
	{ "Module voltage low warning",
		SFF8636_VCC_AW_OFFSET, (SFF8636_VCC_LWARN_STATUS) },

	{ "Laser tx power high alarm   (Channel 1)",
		SFF8636_TX_PWR_12_AW_OFFSET, (SFF8636_TX_PWR_1_HALARM) },
	{ "Laser tx power low alarm    (Channel 1)",
		SFF8636_TX_PWR_12_AW_OFFSET, (SFF8636_TX_PWR_1_LALARM) },
	{ "Laser tx power high warning (Channel 1)",
		SFF8636_TX_PWR_12_AW_OFFSET, (SFF8636_TX_PWR_1_HWARN) },
	{ "Laser tx power low warning  (Channel 1)",
		SFF8636_TX_PWR_12_AW_OFFSET, (SFF8636_TX_PWR_1_LWARN) },

	{ "Laser tx power high alarm   (Channel 2)",
		SFF8636_TX_PWR_12_AW_OFFSET, (SFF8636_TX_PWR_2_HALARM) },
	{ "Laser tx power low alarm    (Channel 2)",
		SFF8636_TX_PWR_12_AW_OFFSET, (SFF8636_TX_PWR_2_LALARM) },
	{ "Laser tx power high warning (Channel 2)",
		SFF8636_TX_PWR_12_AW_OFFSET, (SFF8636_TX_PWR_2_HWARN) },
	{ "Laser tx power low warning  (Channel 2)",
		SFF8636_TX_PWR_12_AW_OFFSET, (SFF8636_TX_PWR_2_LWARN) },

	{ "Laser tx power high alarm   (Channel 3)",
		SFF8636_TX_PWR_34_AW_OFFSET, (SFF8636_TX_PWR_3_HALARM) },
	{ "Laser tx power low alarm    (Channel 3)",
		SFF8636_TX_PWR_34_AW_OFFSET, (SFF8636_TX_PWR_3_LALARM) },
	{ "Laser tx power high warning (Channel 3)",
		SFF8636_TX_PWR_34_AW_OFFSET, (SFF8636_TX_PWR_3_HWARN) },
	{ "Laser tx power low warning  (Channel 3)",
		SFF8636_TX_PWR_34_AW_OFFSET, (SFF8636_TX_PWR_3_LWARN) },

	{ "Laser tx power high alarm   (Channel 4)",
		SFF8636_TX_PWR_34_AW_OFFSET, (SFF8636_TX_PWR_4_HALARM) },
	{ "Laser tx power low alarm    (Channel 4)",
		SFF8636_TX_PWR_34_AW_OFFSET, (SFF8636_TX_PWR_4_LALARM) },
	{ "Laser tx power high warning (Channel 4)",
		SFF8636_TX_PWR_34_AW_OFFSET, (SFF8636_TX_PWR_4_HWARN) },
	{ "Laser tx power low warning  (Channel 4)",
		SFF8636_TX_PWR_34_AW_OFFSET, (SFF8636_TX_PWR_4_LWARN) },

	{ "Laser rx power high alarm   (Channel 1)",
		SFF8636_RX_PWR_12_AW_OFFSET, (SFF8636_RX_PWR_1_HALARM) },
	{ "Laser rx power low alarm    (Channel 1)",
		SFF8636_RX_PWR_12_AW_OFFSET, (SFF8636_RX_PWR_1_LALARM) },
	{ "Laser rx power high warning (Channel 1)",
		SFF8636_RX_PWR_12_AW_OFFSET, (SFF8636_RX_PWR_1_HWARN) },
	{ "Laser rx power low warning  (Channel 1)",
		SFF8636_RX_PWR_12_AW_OFFSET, (SFF8636_RX_PWR_1_LWARN) },

	{ "Laser rx power high alarm   (Channel 2)",
		SFF8636_RX_PWR_12_AW_OFFSET, (SFF8636_RX_PWR_2_HALARM) },
	{ "Laser rx power low alarm    (Channel 2)",
		SFF8636_RX_PWR_12_AW_OFFSET, (SFF8636_RX_PWR_2_LALARM) },
	{ "Laser rx power high warning (Channel 2)",
		SFF8636_RX_PWR_12_AW_OFFSET, (SFF8636_RX_PWR_2_HWARN) },
	{ "Laser rx power low warning  (Channel 2)",
		SFF8636_RX_PWR_12_AW_OFFSET, (SFF8636_RX_PWR_2_LWARN) },

	{ "Laser rx power high alarm   (Channel 3)",
		SFF8636_RX_PWR_34_AW_OFFSET, (SFF8636_RX_PWR_3_HALARM) },
	{ "Laser rx power low alarm    (Channel 3)",
		SFF8636_RX_PWR_34_AW_OFFSET, (SFF8636_RX_PWR_3_LALARM) },
	{ "Laser rx power high warning (Channel 3)",
		SFF8636_RX_PWR_34_AW_OFFSET, (SFF8636_RX_PWR_3_HWARN) },
	{ "Laser rx power low warning  (Channel 3)",
		SFF8636_RX_PWR_34_AW_OFFSET, (SFF8636_RX_PWR_3_LWARN) },

	{ "Laser rx power high alarm   (Channel 4)",
		SFF8636_RX_PWR_34_AW_OFFSET, (SFF8636_RX_PWR_4_HALARM) },
	{ "Laser rx power low alarm    (Channel 4)",
		SFF8636_RX_PWR_34_AW_OFFSET, (SFF8636_RX_PWR_4_LALARM) },
	{ "Laser rx power high warning (Channel 4)",
		SFF8636_RX_PWR_34_AW_OFFSET, (SFF8636_RX_PWR_4_HWARN) },
	{ "Laser rx power low warning  (Channel 4)",
		SFF8636_RX_PWR_34_AW_OFFSET, (SFF8636_RX_PWR_4_LWARN) },

	{ NULL, 0, 0 },
};

static void sff8636_show_identifier(const __u8 *id)
{
	sff8024_show_identifier(id, SFF8636_ID_OFFSET);
}

static void sff8636_show_ext_identifier(const __u8 *id)
{
	printf("\t%-41s : 0x%02x\n", "Extended identifier",
			id[SFF8636_EXT_ID_OFFSET]);

	static const char *pfx =
		"\tExtended identifier description           :";

	switch (id[SFF8636_EXT_ID_OFFSET] & SFF8636_EXT_ID_PWR_CLASS_MASK) {
	case SFF8636_EXT_ID_PWR_CLASS_1:
		printf("%s 1.5W max. Power consumption\n", pfx);
		break;
	case SFF8636_EXT_ID_PWR_CLASS_2:
		printf("%s 2.0W max. Power consumption\n", pfx);
		break;
	case SFF8636_EXT_ID_PWR_CLASS_3:
		printf("%s 2.5W max. Power consumption\n", pfx);
		break;
	case SFF8636_EXT_ID_PWR_CLASS_4:
		printf("%s 3.5W max. Power consumption\n", pfx);
		break;
	}

	if (id[SFF8636_EXT_ID_OFFSET] & SFF8636_EXT_ID_CDR_TX_MASK)
		printf("%s CDR present in TX,", pfx);
	else
		printf("%s No CDR in TX,", pfx);

	if (id[SFF8636_EXT_ID_OFFSET] & SFF8636_EXT_ID_CDR_RX_MASK)
		printf(" CDR present in RX\n");
	else
		printf(" No CDR in RX\n");

	switch (id[SFF8636_EXT_ID_OFFSET] & SFF8636_EXT_ID_EPWR_CLASS_MASK) {
	case SFF8636_EXT_ID_PWR_CLASS_LEGACY:
		printf("%s", pfx);
		break;
	case SFF8636_EXT_ID_PWR_CLASS_5:
		printf("%s 4.0W max. Power consumption,", pfx);
		break;
	case SFF8636_EXT_ID_PWR_CLASS_6:
		printf("%s 4.5W max. Power consumption, ", pfx);
		break;
	case SFF8636_EXT_ID_PWR_CLASS_7:
		printf("%s 5.0W max. Power consumption, ", pfx);
		break;
	}
	if (id[SFF8636_PWR_MODE_OFFSET] & SFF8636_HIGH_PWR_ENABLE)
		printf(" High Power Class (> 3.5 W) enabled\n");
	else
		printf(" High Power Class (> 3.5 W) not enabled\n");
}

static void sff8636_show_connector(const __u8 *id)
{
	sff8024_show_connector(id, SFF8636_CTOR_OFFSET);
}

static void sff8636_show_transceiver(const __u8 *id)
{
	static const char *pfx =
		"\tTransceiver type                          :";

	printf("\t%-41s : 0x%02x 0x%02x 0x%02x " \
			"0x%02x 0x%02x 0x%02x 0x%02x 0x%02x\n",
			"Transceiver codes",
			id[SFF8636_ETHERNET_COMP_OFFSET],
			id[SFF8636_SONET_COMP_OFFSET],
			id[SFF8636_SAS_COMP_OFFSET],
			id[SFF8636_GIGE_COMP_OFFSET],
			id[SFF8636_FC_LEN_OFFSET],
			id[SFF8636_FC_TECH_OFFSET],
			id[SFF8636_FC_TRANS_MEDIA_OFFSET],
			id[SFF8636_FC_SPEED_OFFSET]);

	/* 10G/40G Ethernet Compliance Codes */
	if (id[SFF8636_ETHERNET_COMP_OFFSET] & SFF8636_ETHERNET_10G_LRM)
		printf("%s 10G Ethernet: 10G Base-LRM\n", pfx);
	if (id[SFF8636_ETHERNET_COMP_OFFSET] & SFF8636_ETHERNET_10G_LR)
		printf("%s 10G Ethernet: 10G Base-LR\n", pfx);
	if (id[SFF8636_ETHERNET_COMP_OFFSET] & SFF8636_ETHERNET_10G_SR)
		printf("%s 10G Ethernet: 10G Base-SR\n", pfx);
	if (id[SFF8636_ETHERNET_COMP_OFFSET] & SFF8636_ETHERNET_40G_CR4)
		printf("%s 40G Ethernet: 40G Base-CR4\n", pfx);
	if (id[SFF8636_ETHERNET_COMP_OFFSET] & SFF8636_ETHERNET_40G_SR4)
		printf("%s 40G Ethernet: 40G Base-SR4\n", pfx);
	if (id[SFF8636_ETHERNET_COMP_OFFSET] & SFF8636_ETHERNET_40G_LR4)
		printf("%s 40G Ethernet: 40G Base-LR4\n", pfx);
	if (id[SFF8636_ETHERNET_COMP_OFFSET] & SFF8636_ETHERNET_40G_ACTIVE)
		printf("%s 40G Ethernet: 40G Active Cable (XLPPI)\n", pfx);
	/* Extended Specification Compliance Codes from SFF-8024 */
	if (id[SFF8636_ETHERNET_COMP_OFFSET] & SFF8636_ETHERNET_RSRVD) {
		switch (id[SFF8636_OPTION_1_OFFSET]) {
		case SFF8636_ETHERNET_UNSPECIFIED:
			printf("%s (reserved or unknown)\n", pfx);
			break;
		case SFF8636_ETHERNET_100G_AOC:
			printf("%s 100G Ethernet: 100G AOC or 25GAUI C2M AOC with worst BER of 5x10^(-5)\n",
					pfx);
			break;
		case SFF8636_ETHERNET_100G_SR4:
			printf("%s 100G Ethernet: 100G Base-SR4 or 25GBase-SR\n",
					pfx);
			break;
		case SFF8636_ETHERNET_100G_LR4:
			printf("%s 100G Ethernet: 100G Base-LR4\n", pfx);
			break;
		case SFF8636_ETHERNET_100G_ER4:
			printf("%s 100G Ethernet: 100G Base-ER4\n", pfx);
			break;
		case SFF8636_ETHERNET_100G_SR10:
			printf("%s 100G Ethernet: 100G Base-SR10\n", pfx);
			break;
		case SFF8636_ETHERNET_100G_CWDM4_FEC:
			printf("%s 100G Ethernet: 100G CWDM4 MSA with FEC\n", pfx);
			break;
		case SFF8636_ETHERNET_100G_PSM4:
			printf("%s 100G Ethernet: 100G PSM4 Parallel SMF\n", pfx);
			break;
		case SFF8636_ETHERNET_100G_ACC:
			printf("%s 100G Ethernet: 100G ACC or 25GAUI C2M ACC with worst BER of 5x10^(-5)\n",
				pfx);
			break;
		case SFF8636_ETHERNET_100G_CWDM4_NO_FEC:
			printf("%s 100G Ethernet: 100G CWDM4 MSA without FEC\n", pfx);
			break;
		case SFF8636_ETHERNET_100G_RSVD1:
			printf("%s (reserved or unknown)\n", pfx);
			break;
		case SFF8636_ETHERNET_100G_CR4:
			printf("%s 100G Ethernet: 100G Base-CR4 or 25G Base-CR CA-L\n",
				pfx);
			break;
		case SFF8636_ETHERNET_25G_CR_CA_S:
			printf("%s 25G Ethernet: 25G Base-CR CA-S\n", pfx);
			break;
		case SFF8636_ETHERNET_25G_CR_CA_N:
			printf("%s 25G Ethernet: 25G Base-CR CA-N\n", pfx);
			break;
		case SFF8636_ETHERNET_40G_ER4:
			printf("%s 40G Ethernet: 40G Base-ER4\n", pfx);
			break;
		case SFF8636_ETHERNET_4X10_SR:
			printf("%s 4x10G Ethernet: 10G Base-SR\n", pfx);
			break;
		case SFF8636_ETHERNET_40G_PSM4:
			printf("%s 40G Ethernet: 40G PSM4 Parallel SMF\n", pfx);
			break;
		case SFF8636_ETHERNET_G959_P1I1_2D1:
			printf("%s Ethernet: G959.1 profile P1I1-2D1 (10709 MBd, 2km, 1310nm SM)\n",
					pfx);
			break;
		case SFF8636_ETHERNET_G959_P1S1_2D2:
			printf("%s Ethernet: G959.1 profile P1S1-2D2 (10709 MBd, 40km, 1550nm SM)\n",
					pfx);
			break;
		case SFF8636_ETHERNET_G959_P1L1_2D2:
			printf("%s Ethernet: G959.1 profile P1L1-2D2 (10709 MBd, 80km, 1550nm SM)\n",
					pfx);
			break;
		case SFF8636_ETHERNET_10GT_SFI:
			printf("%s 10G Ethernet: 10G Base-T with SFI electrical interface\n",
					pfx);
			break;
		case SFF8636_ETHERNET_100G_CLR4:
			printf("%s 100G Ethernet: 100G CLR4\n", pfx);
			break;
		case SFF8636_ETHERNET_100G_AOC2:
			printf("%s 100G Ethernet: 100G AOC or 25GAUI C2M AOC with worst BER of 10^(-12)\n",
					pfx);
			break;
		case SFF8636_ETHERNET_100G_ACC2:
			printf("%s 100G Ethernet: 100G ACC or 25GAUI C2M ACC with worst BER of 10^(-12)\n",
					pfx);
			break;
		default:
			printf("%s (reserved or unknown)\n", pfx);
			break;
		}
	}

	/* SONET Compliance Codes */
	if (id[SFF8636_SONET_COMP_OFFSET] & (SFF8636_SONET_40G_OTN))
		printf("%s 40G OTN (OTU3B/OTU3C)\n", pfx);
	if (id[SFF8636_SONET_COMP_OFFSET] & (SFF8636_SONET_OC48_LR))
		printf("%s SONET: OC-48, long reach\n", pfx);
	if (id[SFF8636_SONET_COMP_OFFSET] & (SFF8636_SONET_OC48_IR))
		printf("%s SONET: OC-48, intermediate reach\n", pfx);
	if (id[SFF8636_SONET_COMP_OFFSET] & (SFF8636_SONET_OC48_SR))
		printf("%s SONET: OC-48, short reach\n", pfx);

	/* SAS/SATA Compliance Codes */
	if (id[SFF8636_SAS_COMP_OFFSET] & (SFF8636_SAS_6G))
		printf("%s SAS 6.0G\n", pfx);
	if (id[SFF8636_SAS_COMP_OFFSET] & (SFF8636_SAS_3G))
		printf("%s SAS 3.0G\n", pfx);

	/* Ethernet Compliance Codes */
	if (id[SFF8636_GIGE_COMP_OFFSET] & SFF8636_GIGE_1000_BASE_T)
		printf("%s Ethernet: 1000BASE-T\n", pfx);
	if (id[SFF8636_GIGE_COMP_OFFSET] & SFF8636_GIGE_1000_BASE_CX)
		printf("%s Ethernet: 1000BASE-CX\n", pfx);
	if (id[SFF8636_GIGE_COMP_OFFSET] & SFF8636_GIGE_1000_BASE_LX)
		printf("%s Ethernet: 1000BASE-LX\n", pfx);
	if (id[SFF8636_GIGE_COMP_OFFSET] & SFF8636_GIGE_1000_BASE_SX)
		printf("%s Ethernet: 1000BASE-SX\n", pfx);

	/* Fibre Channel link length */
	if (id[SFF8636_FC_LEN_OFFSET] & SFF8636_FC_LEN_VERY_LONG)
		printf("%s FC: very long distance (V)\n", pfx);
	if (id[SFF8636_FC_LEN_OFFSET] & SFF8636_FC_LEN_SHORT)
		printf("%s FC: short distance (S)\n", pfx);
	if (id[SFF8636_FC_LEN_OFFSET] & SFF8636_FC_LEN_INT)
		printf("%s FC: intermediate distance (I)\n", pfx);
	if (id[SFF8636_FC_LEN_OFFSET] & SFF8636_FC_LEN_LONG)
		printf("%s FC: long distance (L)\n", pfx);
	if (id[SFF8636_FC_LEN_OFFSET] & SFF8636_FC_LEN_MED)
		printf("%s FC: medium distance (M)\n", pfx);

	/* Fibre Channel transmitter technology */
	if (id[SFF8636_FC_LEN_OFFSET] & SFF8636_FC_TECH_LONG_LC)
		printf("%s FC: Longwave laser (LC)\n", pfx);
	if (id[SFF8636_FC_LEN_OFFSET] & SFF8636_FC_TECH_ELEC_INTER)
		printf("%s FC: Electrical inter-enclosure (EL)\n", pfx);
	if (id[SFF8636_FC_TECH_OFFSET] & SFF8636_FC_TECH_ELEC_INTRA)
		printf("%s FC: Electrical intra-enclosure (EL)\n", pfx);
	if (id[SFF8636_FC_TECH_OFFSET] & SFF8636_FC_TECH_SHORT_WO_OFC)
		printf("%s FC: Shortwave laser w/o OFC (SN)\n", pfx);
	if (id[SFF8636_FC_TECH_OFFSET] & SFF8636_FC_TECH_SHORT_W_OFC)
		printf("%s FC: Shortwave laser with OFC (SL)\n", pfx);
	if (id[SFF8636_FC_TECH_OFFSET] & SFF8636_FC_TECH_LONG_LL)
		printf("%s FC: Longwave laser (LL)\n", pfx);

	/* Fibre Channel transmission media */
	if (id[SFF8636_FC_TRANS_MEDIA_OFFSET] & SFF8636_FC_TRANS_MEDIA_TW)
		printf("%s FC: Twin Axial Pair (TW)\n", pfx);
	if (id[SFF8636_FC_TRANS_MEDIA_OFFSET] & SFF8636_FC_TRANS_MEDIA_TP)
		printf("%s FC: Twisted Pair (TP)\n", pfx);
	if (id[SFF8636_FC_TRANS_MEDIA_OFFSET] & SFF8636_FC_TRANS_MEDIA_MI)
		printf("%s FC: Miniature Coax (MI)\n", pfx);
	if (id[SFF8636_FC_TRANS_MEDIA_OFFSET] & SFF8636_FC_TRANS_MEDIA_TV)
		printf("%s FC: Video Coax (TV)\n", pfx);
	if (id[SFF8636_FC_TRANS_MEDIA_OFFSET] & SFF8636_FC_TRANS_MEDIA_M6)
		printf("%s FC: Multimode, 62.5m (M6)\n", pfx);
	if (id[SFF8636_FC_TRANS_MEDIA_OFFSET] & SFF8636_FC_TRANS_MEDIA_M5)
		printf("%s FC: Multimode, 50m (M5)\n", pfx);
	if (id[SFF8636_FC_TRANS_MEDIA_OFFSET] & SFF8636_FC_TRANS_MEDIA_OM3)
		printf("%s FC: Multimode, 50um (OM3)\n", pfx);
	if (id[SFF8636_FC_TRANS_MEDIA_OFFSET] & SFF8636_FC_TRANS_MEDIA_SM)
		printf("%s FC: Single Mode (SM)\n", pfx);

	/* Fibre Channel speed */
	if (id[SFF8636_FC_SPEED_OFFSET] & SFF8636_FC_SPEED_1200_MBPS)
		printf("%s FC: 1200 MBytes/sec\n", pfx);
	if (id[SFF8636_FC_SPEED_OFFSET] & SFF8636_FC_SPEED_800_MBPS)
		printf("%s FC: 800 MBytes/sec\n", pfx);
	if (id[SFF8636_FC_SPEED_OFFSET] & SFF8636_FC_SPEED_1600_MBPS)
		printf("%s FC: 1600 MBytes/sec\n", pfx);
	if (id[SFF8636_FC_SPEED_OFFSET] & SFF8636_FC_SPEED_400_MBPS)
		printf("%s FC: 400 MBytes/sec\n", pfx);
	if (id[SFF8636_FC_SPEED_OFFSET] & SFF8636_FC_SPEED_200_MBPS)
		printf("%s FC: 200 MBytes/sec\n", pfx);
	if (id[SFF8636_FC_SPEED_OFFSET] & SFF8636_FC_SPEED_100_MBPS)
		printf("%s FC: 100 MBytes/sec\n", pfx);
}

static void sff8636_show_encoding(const __u8 *id)
{
	sff8024_show_encoding(id, SFF8636_ENCODING_OFFSET, ETH_MODULE_SFF_8636);
}

static void sff8636_show_rate_identifier(const __u8 *id)
{
	/* TODO: Need to fix rate select logic */
	printf("\t%-41s : 0x%02x\n", "Rate identifier",
			id[SFF8636_EXT_RS_OFFSET]);
}

static void sff8636_show_oui(const __u8 *id)
{
	sff8024_show_oui(id, SFF8636_VENDOR_OUI_OFFSET);
}

static void sff8636_show_wavelength_or_copper_compliance(const __u8 *id)
{
	printf("\t%-41s : 0x%02x", "Transmitter technology",
		(id[SFF8636_DEVICE_TECH_OFFSET] & SFF8636_TRANS_TECH_MASK));

	switch (id[SFF8636_DEVICE_TECH_OFFSET] & SFF8636_TRANS_TECH_MASK) {
	case SFF8636_TRANS_850_VCSEL:
		printf(" (850 nm VCSEL)\n");
		break;
	case SFF8636_TRANS_1310_VCSEL:
		printf(" (1310 nm VCSEL)\n");
		break;
	case SFF8636_TRANS_1550_VCSEL:
		printf(" (1550 nm VCSEL)\n");
		break;
	case SFF8636_TRANS_1310_FP:
		printf(" (1310 nm FP)\n");
		break;
	case SFF8636_TRANS_1310_DFB:
		printf(" (1310 nm DFB)\n");
		break;
	case SFF8636_TRANS_1550_DFB:
		printf(" (1550 nm DFB)\n");
		break;
	case SFF8636_TRANS_1310_EML:
		printf(" (1310 nm EML)\n");
		break;
	case SFF8636_TRANS_1550_EML:
		printf(" (1550 nm EML)\n");
		break;
	case SFF8636_TRANS_OTHERS:
		printf(" (Others/Undefined)\n");
		break;
	case SFF8636_TRANS_1490_DFB:
		printf(" (1490 nm DFB)\n");
		break;
	case SFF8636_TRANS_COPPER_PAS_UNEQUAL:
		printf(" (Copper cable unequalized)\n");
		break;
	case SFF8636_TRANS_COPPER_PAS_EQUAL:
		printf(" (Copper cable passive equalized)\n");
		break;
	case SFF8636_TRANS_COPPER_LNR_FAR_EQUAL:
		printf(" (Copper cable, near and far end limiting active equalizers)\n");
		break;
	case SFF8636_TRANS_COPPER_FAR_EQUAL:
		printf(" (Copper cable, far end limiting active equalizers)\n");
		break;
	case SFF8636_TRANS_COPPER_NEAR_EQUAL:
		printf(" (Copper cable, near end limiting active equalizers)\n");
		break;
	case SFF8636_TRANS_COPPER_LNR_EQUAL:
		printf(" (Copper cable, linear active equalizers)\n");
		break;
	}

	if ((id[SFF8636_DEVICE_TECH_OFFSET] & SFF8636_TRANS_TECH_MASK)
			>= SFF8636_TRANS_COPPER_PAS_UNEQUAL) {
		printf("\t%-41s : %udb\n", "Attenuation at 2.5GHz",
			id[SFF8636_WAVELEN_HIGH_BYTE_OFFSET]);
		printf("\t%-41s : %udb\n", "Attenuation at 5.0GHz",
			id[SFF8636_WAVELEN_LOW_BYTE_OFFSET]);
		printf("\t%-41s : %udb\n", "Attenuation at 7.0GHz",
			id[SFF8636_WAVE_TOL_HIGH_BYTE_OFFSET]);
		printf("\t%-41s : %udb\n", "Attenuation at 12.9GHz",
			id[SFF8636_WAVE_TOL_LOW_BYTE_OFFSET]);
	} else {
		printf("\t%-41s : %.3lfnm\n", "Laser wavelength",
			(((id[SFF8636_WAVELEN_HIGH_BYTE_OFFSET] << 8) |
				id[SFF8636_WAVELEN_LOW_BYTE_OFFSET])*0.05));
		printf("\t%-41s : %.3lfnm\n", "Laser wavelength tolerance",
			(((id[SFF8636_WAVE_TOL_HIGH_BYTE_OFFSET] << 8) |
			id[SFF8636_WAVE_TOL_LOW_BYTE_OFFSET])*0.005));
	}
}

static void sff8636_show_revision_compliance(const __u8 *id)
{
	static const char *pfx =
		"\tRevision Compliance                       :";

	switch (id[SFF8636_REV_COMPLIANCE_OFFSET]) {
	case SFF8636_REV_UNSPECIFIED:
		printf("%s Revision not specified\n", pfx);
		break;
	case SFF8636_REV_8436_48:
		printf("%s SFF-8436 Rev 4.8 or earlier\n", pfx);
		break;
	case SFF8636_REV_8436_8636:
		printf("%s SFF-8436 Rev 4.8 or earlier\n", pfx);
		break;
	case SFF8636_REV_8636_13:
		printf("%s SFF-8636 Rev 1.3 or earlier\n", pfx);
		break;
	case SFF8636_REV_8636_14:
		printf("%s SFF-8636 Rev 1.4\n", pfx);
		break;
	case SFF8636_REV_8636_15:
		printf("%s SFF-8636 Rev 1.5\n", pfx);
		break;
	case SFF8636_REV_8636_20:
		printf("%s SFF-8636 Rev 2.0\n", pfx);
		break;
	case SFF8636_REV_8636_27:
		printf("%s SFF-8636 Rev 2.5/2.6/2.7\n", pfx);
		break;
	default:
		printf("%s Unallocated\n", pfx);
		break;
	}
}

/*
 * 2-byte internal temperature conversions:
 * First byte is a signed 8-bit integer, which is the temp decimal part
 * Second byte are 1/256th of degree, which are added to the dec part.
 */
#define SFF8636_OFFSET_TO_TEMP(offset) ((__s16)OFFSET_TO_U16(offset))

static void sff8636_dom_parse(const __u8 *id, struct sff_diags *sd)
{
	int i = 0;

	/* Monitoring Thresholds for Alarms and Warnings */
	sd->sfp_voltage[MCURR] = OFFSET_TO_U16(SFF8636_VCC_CURR);
	sd->sfp_voltage[HALRM] = OFFSET_TO_U16(SFF8636_VCC_HALRM);
	sd->sfp_voltage[LALRM] = OFFSET_TO_U16(SFF8636_VCC_LALRM);
	sd->sfp_voltage[HWARN] = OFFSET_TO_U16(SFF8636_VCC_HWARN);
	sd->sfp_voltage[LWARN] = OFFSET_TO_U16(SFF8636_VCC_LWARN);

	sd->sfp_temp[MCURR] = SFF8636_OFFSET_TO_TEMP(SFF8636_TEMP_CURR);
	sd->sfp_temp[HALRM] = SFF8636_OFFSET_TO_TEMP(SFF8636_TEMP_HALRM);
	sd->sfp_temp[LALRM] = SFF8636_OFFSET_TO_TEMP(SFF8636_TEMP_LALRM);
	sd->sfp_temp[HWARN] = SFF8636_OFFSET_TO_TEMP(SFF8636_TEMP_HWARN);
	sd->sfp_temp[LWARN] = SFF8636_OFFSET_TO_TEMP(SFF8636_TEMP_LWARN);

	sd->bias_cur[HALRM] = OFFSET_TO_U16(SFF8636_TX_BIAS_HALRM);
	sd->bias_cur[LALRM] = OFFSET_TO_U16(SFF8636_TX_BIAS_LALRM);
	sd->bias_cur[HWARN] = OFFSET_TO_U16(SFF8636_TX_BIAS_HWARN);
	sd->bias_cur[LWARN] = OFFSET_TO_U16(SFF8636_TX_BIAS_LWARN);

	sd->tx_power[HALRM] = OFFSET_TO_U16(SFF8636_TX_PWR_HALRM);
	sd->tx_power[LALRM] = OFFSET_TO_U16(SFF8636_TX_PWR_LALRM);
	sd->tx_power[HWARN] = OFFSET_TO_U16(SFF8636_TX_PWR_HWARN);
	sd->tx_power[LWARN] = OFFSET_TO_U16(SFF8636_TX_PWR_LWARN);

	sd->rx_power[HALRM] = OFFSET_TO_U16(SFF8636_RX_PWR_HALRM);
	sd->rx_power[LALRM] = OFFSET_TO_U16(SFF8636_RX_PWR_LALRM);
	sd->rx_power[HWARN] = OFFSET_TO_U16(SFF8636_RX_PWR_HWARN);
	sd->rx_power[LWARN] = OFFSET_TO_U16(SFF8636_RX_PWR_LWARN);


	/* Channel Specific Data */
	for (i = 0; i < MAX_CHANNEL_NUM; i++) {
		u8 rx_power_offset, tx_bias_offset;
		u8 tx_power_offset;

		switch (i) {
		case 0:
			rx_power_offset = SFF8636_RX_PWR_1_OFFSET;
			tx_power_offset = SFF8636_TX_PWR_1_OFFSET;
			tx_bias_offset = SFF8636_TX_BIAS_1_OFFSET;
			break;
		case 1:
			rx_power_offset = SFF8636_RX_PWR_2_OFFSET;
			tx_power_offset = SFF8636_TX_PWR_2_OFFSET;
			tx_bias_offset = SFF8636_TX_BIAS_2_OFFSET;
			break;
		case 2:
			rx_power_offset = SFF8636_RX_PWR_3_OFFSET;
			tx_power_offset = SFF8636_TX_PWR_3_OFFSET;
			tx_bias_offset = SFF8636_TX_BIAS_3_OFFSET;
			break;
		case 3:
			rx_power_offset = SFF8636_RX_PWR_4_OFFSET;
			tx_power_offset = SFF8636_TX_PWR_4_OFFSET;
			tx_bias_offset = SFF8636_TX_BIAS_4_OFFSET;
			break;
		default:
			printf(" Invalid channel: %d\n", i);
			break;
		}
		sd->scd[i].bias_cur = OFFSET_TO_U16(tx_bias_offset);
		sd->scd[i].rx_power = OFFSET_TO_U16(rx_power_offset);
		sd->scd[i].tx_power = OFFSET_TO_U16(tx_power_offset);
	}

}

static void sff8636_show_dom(const __u8 *id, __u32 eeprom_len)
{
	struct sff_diags sd;
	char *rx_power_string = NULL;
	char power_string[MAX_DESC_SIZE];
	int i;

	/*
	 * There is no clear identifier to signify the existence of
	 * optical diagnostics similar to SFF-8472. So checking existence
	 * of page 3, will provide the gurantee for existence of alarms
	 * and thresholds
	 * If pagging support exists, then supports_alarms is marked as 1
	 */

	if (eeprom_len == ETH_MODULE_SFF_8636_MAX_LEN) {
		if (!(id[SFF8636_STATUS_2_OFFSET] &
					SFF8636_STATUS_PAGE_3_PRESENT)) {
			sd.supports_alarms = 1;
		}
	}

	sd.rx_power_type = id[SFF8636_DIAG_TYPE_OFFSET] &
						SFF8636_RX_PWR_TYPE_MASK;
	sd.tx_power_type = id[SFF8636_DIAG_TYPE_OFFSET] &
						SFF8636_RX_PWR_TYPE_MASK;

	sff8636_dom_parse(id, &sd);

	PRINT_TEMP("Module temperature", sd.sfp_temp[MCURR]);
	PRINT_VCC("Module voltage", sd.sfp_voltage[MCURR]);

	/*
	 * SFF-8636/8436 spec is not clear whether RX power/ TX bias
	 * current fields are supported or not. A valid temperature
	 * reading is used as existence for TX/RX power.
	 */
	if ((sd.sfp_temp[MCURR] == 0x0) || (sd.sfp_temp[MCURR] == 0xFFFF))
		return;

	printf("\t%-41s : %s\n", "Alarm/warning flags implemented",
		(sd.supports_alarms ? "Yes" : "No"));

	for (i = 0; i < MAX_CHANNEL_NUM; i++) {
		snprintf(power_string, MAX_DESC_SIZE, "%s (Channel %d)",
					"Laser tx bias current", i+1);
		PRINT_BIAS(power_string, sd.scd[i].bias_cur);
	}

	for (i = 0; i < MAX_CHANNEL_NUM; i++) {
		snprintf(power_string, MAX_DESC_SIZE, "%s (Channel %d)",
					"Transmit avg optical power", i+1);
		PRINT_xX_PWR(power_string, sd.scd[i].tx_power);
	}

	if (!sd.rx_power_type)
		rx_power_string = "Receiver signal OMA";
	else
		rx_power_string = "Rcvr signal avg optical power";

	for (i = 0; i < MAX_CHANNEL_NUM; i++) {
		snprintf(power_string, MAX_DESC_SIZE, "%s(Channel %d)",
					rx_power_string, i+1);
		PRINT_xX_PWR(power_string, sd.scd[i].rx_power);
	}

	if (sd.supports_alarms) {
		for (i = 0; sff8636_aw_flags[i].str; ++i) {
			printf("\t%-41s : %s\n", sff8636_aw_flags[i].str,
			       id[sff8636_aw_flags[i].offset]
			       & sff8636_aw_flags[i].value ? "On" : "Off");
		}

		sff_show_thresholds(sd);
	}

}
void sff8636_show_all(const __u8 *id, __u32 eeprom_len)
{
	sff8636_show_identifier(id);
	if ((id[SFF8636_ID_OFFSET] == SFF8024_ID_QSFP) ||
		(id[SFF8636_ID_OFFSET] == SFF8024_ID_QSFP_PLUS) ||
		(id[SFF8636_ID_OFFSET] == SFF8024_ID_QSFP28)) {
		sff8636_show_ext_identifier(id);
		sff8636_show_connector(id);
		sff8636_show_transceiver(id);
		sff8636_show_encoding(id);
		sff_show_value_with_unit(id, SFF8636_BR_NOMINAL_OFFSET,
				"BR, Nominal", 100, "Mbps");
		sff8636_show_rate_identifier(id);
		sff_show_value_with_unit(id, SFF8636_SM_LEN_OFFSET,
			     "Length (SMF,km)", 1, "km");
		sff_show_value_with_unit(id, SFF8636_OM3_LEN_OFFSET,
				"Length (OM3 50um)", 2, "m");
		sff_show_value_with_unit(id, SFF8636_OM2_LEN_OFFSET,
				"Length (OM2 50um)", 1, "m");
		sff_show_value_with_unit(id, SFF8636_OM1_LEN_OFFSET,
			     "Length (OM1 62.5um)", 1, "m");
		sff_show_value_with_unit(id, SFF8636_CBL_LEN_OFFSET,
			     "Length (Copper or Active cable)", 1, "m");
		sff8636_show_wavelength_or_copper_compliance(id);
		sff_show_ascii(id, SFF8636_VENDOR_NAME_START_OFFSET,
			       SFF8636_VENDOR_NAME_END_OFFSET, "Vendor name");
		sff8636_show_oui(id);
		sff_show_ascii(id, SFF8636_VENDOR_PN_START_OFFSET,
			       SFF8636_VENDOR_PN_END_OFFSET, "Vendor PN");
		sff_show_ascii(id, SFF8636_VENDOR_REV_START_OFFSET,
			       SFF8636_VENDOR_REV_END_OFFSET, "Vendor rev");
		sff_show_ascii(id, SFF8636_VENDOR_SN_START_OFFSET,
			       SFF8636_VENDOR_SN_END_OFFSET, "Vendor SN");
		sff8636_show_revision_compliance(id);
		sff8636_show_dom(id, eeprom_len);
	}
}
