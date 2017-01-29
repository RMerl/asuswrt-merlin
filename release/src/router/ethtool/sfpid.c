/****************************************************************************
 * Support for Solarflare Solarstorm network controllers and boards
 * Copyright 2010 Solarflare Communications Inc.
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License version 2 as published
 * by the Free Software Foundation, incorporated herein by reference.
 */

#include <stdio.h>
#include "internal.h"
#include "sff-common.h"

static void sff8079_show_identifier(const __u8 *id)
{
	sff8024_show_identifier(id, 0);
}

static void sff8079_show_ext_identifier(const __u8 *id)
{
	printf("\t%-41s : 0x%02x", "Extended identifier", id[1]);
	if (id[1] == 0x00)
		printf(" (GBIC not specified / not MOD_DEF compliant)\n");
	else if (id[1] == 0x04)
		printf(" (GBIC/SFP defined by 2-wire interface ID)\n");
	else if (id[1] <= 0x07)
		printf(" (GBIC compliant with MOD_DEF %u)\n", id[1]);
	else
		printf(" (unknown)\n");
}

static void sff8079_show_connector(const __u8 *id)
{
	sff8024_show_connector(id, 2);
}

static void sff8079_show_transceiver(const __u8 *id)
{
	static const char *pfx =
		"\tTransceiver type                          :";

	printf("\t%-41s : 0x%02x 0x%02x 0x%02x " \
	       "0x%02x 0x%02x 0x%02x 0x%02x 0x%02x\n",
		   "Transceiver codes",
	       id[3], id[4], id[5], id[6],
	       id[7], id[8], id[9], id[10]);
	/* 10G Ethernet Compliance Codes */
	if (id[3] & (1 << 7))
		printf("%s 10G Ethernet: 10G Base-ER" \
		       " [SFF-8472 rev10.4 only]\n", pfx);
	if (id[3] & (1 << 6))
		printf("%s 10G Ethernet: 10G Base-LRM\n", pfx);
	if (id[3] & (1 << 5))
		printf("%s 10G Ethernet: 10G Base-LR\n", pfx);
	if (id[3] & (1 << 4))
		printf("%s 10G Ethernet: 10G Base-SR\n", pfx);
	/* Infiniband Compliance Codes */
	if (id[3] & (1 << 3))
		printf("%s Infiniband: 1X SX\n", pfx);
	if (id[3] & (1 << 2))
		printf("%s Infiniband: 1X LX\n", pfx);
	if (id[3] & (1 << 1))
		printf("%s Infiniband: 1X Copper Active\n", pfx);
	if (id[3] & (1 << 0))
		printf("%s Infiniband: 1X Copper Passive\n", pfx);
	/* ESCON Compliance Codes */
	if (id[4] & (1 << 7))
		printf("%s ESCON: ESCON MMF, 1310nm LED\n", pfx);
	if (id[4] & (1 << 6))
		printf("%s ESCON: ESCON SMF, 1310nm Laser\n", pfx);
	/* SONET Compliance Codes */
	if (id[4] & (1 << 5))
		printf("%s SONET: OC-192, short reach\n", pfx);
	if (id[4] & (1 << 4))
		printf("%s SONET: SONET reach specifier bit 1\n", pfx);
	if (id[4] & (1 << 3))
		printf("%s SONET: SONET reach specifier bit 2\n", pfx);
	if (id[4] & (1 << 2))
		printf("%s SONET: OC-48, long reach\n", pfx);
	if (id[4] & (1 << 1))
		printf("%s SONET: OC-48, intermediate reach\n", pfx);
	if (id[4] & (1 << 0))
		printf("%s SONET: OC-48, short reach\n", pfx);
	if (id[5] & (1 << 6))
		printf("%s SONET: OC-12, single mode, long reach\n", pfx);
	if (id[5] & (1 << 5))
		printf("%s SONET: OC-12, single mode, inter. reach\n", pfx);
	if (id[5] & (1 << 4))
		printf("%s SONET: OC-12, short reach\n", pfx);
	if (id[5] & (1 << 2))
		printf("%s SONET: OC-3, single mode, long reach\n", pfx);
	if (id[5] & (1 << 1))
		printf("%s SONET: OC-3, single mode, inter. reach\n", pfx);
	if (id[5] & (1 << 0))
		printf("%s SONET: OC-3, short reach\n", pfx);
	/* Ethernet Compliance Codes */
	if (id[6] & (1 << 7))
		printf("%s Ethernet: BASE-PX\n", pfx);
	if (id[6] & (1 << 6))
		printf("%s Ethernet: BASE-BX10\n", pfx);
	if (id[6] & (1 << 5))
		printf("%s Ethernet: 100BASE-FX\n", pfx);
	if (id[6] & (1 << 4))
		printf("%s Ethernet: 100BASE-LX/LX10\n", pfx);
	if (id[6] & (1 << 3))
		printf("%s Ethernet: 1000BASE-T\n", pfx);
	if (id[6] & (1 << 2))
		printf("%s Ethernet: 1000BASE-CX\n", pfx);
	if (id[6] & (1 << 1))
		printf("%s Ethernet: 1000BASE-LX\n", pfx);
	if (id[6] & (1 << 0))
		printf("%s Ethernet: 1000BASE-SX\n", pfx);
	/* Fibre Channel link length */
	if (id[7] & (1 << 7))
		printf("%s FC: very long distance (V)\n", pfx);
	if (id[7] & (1 << 6))
		printf("%s FC: short distance (S)\n", pfx);
	if (id[7] & (1 << 5))
		printf("%s FC: intermediate distance (I)\n", pfx);
	if (id[7] & (1 << 4))
		printf("%s FC: long distance (L)\n", pfx);
	if (id[7] & (1 << 3))
		printf("%s FC: medium distance (M)\n", pfx);
	/* Fibre Channel transmitter technology */
	if (id[7] & (1 << 2))
		printf("%s FC: Shortwave laser, linear Rx (SA)\n", pfx);
	if (id[7] & (1 << 1))
		printf("%s FC: Longwave laser (LC)\n", pfx);
	if (id[7] & (1 << 0))
		printf("%s FC: Electrical inter-enclosure (EL)\n", pfx);
	if (id[8] & (1 << 7))
		printf("%s FC: Electrical intra-enclosure (EL)\n", pfx);
	if (id[8] & (1 << 6))
		printf("%s FC: Shortwave laser w/o OFC (SN)\n", pfx);
	if (id[8] & (1 << 5))
		printf("%s FC: Shortwave laser with OFC (SL)\n", pfx);
	if (id[8] & (1 << 4))
		printf("%s FC: Longwave laser (LL)\n", pfx);
	if (id[8] & (1 << 3))
		printf("%s FC: Copper Active\n", pfx);
	if (id[8] & (1 << 2))
		printf("%s FC: Copper Passive\n", pfx);
	if (id[8] & (1 << 1))
		printf("%s FC: Copper FC-BaseT\n", pfx);
	/* Fibre Channel transmission media */
	if (id[9] & (1 << 7))
		printf("%s FC: Twin Axial Pair (TW)\n", pfx);
	if (id[9] & (1 << 6))
		printf("%s FC: Twisted Pair (TP)\n", pfx);
	if (id[9] & (1 << 5))
		printf("%s FC: Miniature Coax (MI)\n", pfx);
	if (id[9] & (1 << 4))
		printf("%s FC: Video Coax (TV)\n", pfx);
	if (id[9] & (1 << 3))
		printf("%s FC: Multimode, 62.5um (M6)\n", pfx);
	if (id[9] & (1 << 2))
		printf("%s FC: Multimode, 50um (M5)\n", pfx);
	if (id[9] & (1 << 0))
		printf("%s FC: Single Mode (SM)\n", pfx);
	/* Fibre Channel speed */
	if (id[10] & (1 << 7))
		printf("%s FC: 1200 MBytes/sec\n", pfx);
	if (id[10] & (1 << 6))
		printf("%s FC: 800 MBytes/sec\n", pfx);
	if (id[10] & (1 << 4))
		printf("%s FC: 400 MBytes/sec\n", pfx);
	if (id[10] & (1 << 2))
		printf("%s FC: 200 MBytes/sec\n", pfx);
	if (id[10] & (1 << 0))
		printf("%s FC: 100 MBytes/sec\n", pfx);
}

static void sff8079_show_encoding(const __u8 *id)
{
	sff8024_show_encoding(id, 11, ETH_MODULE_SFF_8472);
}

static void sff8079_show_rate_identifier(const __u8 *id)
{
	printf("\t%-41s : 0x%02x", "Rate identifier", id[13]);
	switch (id[13]) {
	case 0x00:
		printf(" (unspecified)\n");
		break;
	case 0x01:
		printf(" (4/2/1G Rate_Select & AS0/AS1)\n");
		break;
	case 0x02:
		printf(" (8/4/2G Rx Rate_Select only)\n");
		break;
	case 0x03:
		printf(" (8/4/2G Independent Rx & Tx Rate_Select)\n");
		break;
	case 0x04:
		printf(" (8/4/2G Tx Rate_Select only)\n");
		break;
	default:
		printf(" (reserved or unknown)\n");
		break;
	}
}

static void sff8079_show_oui(const __u8 *id)
{
	printf("\t%-41s : %02x:%02x:%02x\n", "Vendor OUI",
	       id[37], id[38], id[39]);
}

static void sff8079_show_wavelength_or_copper_compliance(const __u8 *id)
{
	if (id[8] & (1 << 2)) {
		printf("\t%-41s : 0x%02x", "Passive Cu cmplnce.", id[60]);
		switch (id[60]) {
		case 0x00:
			printf(" (unspecified)");
			break;
		case 0x01:
			printf(" (SFF-8431 appendix E)");
			break;
		default:
			printf(" (unknown)");
			break;
		}
		printf(" [SFF-8472 rev10.4 only]\n");
	} else if (id[8] & (1 << 3)) {
		printf("\t%-41s : 0x%02x", "Active Cu cmplnce.", id[60]);
		switch (id[60]) {
		case 0x00:
			printf(" (unspecified)");
			break;
		case 0x01:
			printf(" (SFF-8431 appendix E)");
			break;
		case 0x04:
			printf(" (SFF-8431 limiting)");
			break;
		default:
			printf(" (unknown)");
			break;
		}
		printf(" [SFF-8472 rev10.4 only]\n");
	} else {
		printf("\t%-41s : %unm\n", "Laser wavelength",
		       (id[60] << 8) | id[61]);
	}
}

static void sff8079_show_value_with_unit(const __u8 *id, unsigned int reg,
					 const char *name, unsigned int mult,
					 const char *unit)
{
	unsigned int val = id[reg];

	printf("\t%-41s : %u%s\n", name, val * mult, unit);
}

static void sff8079_show_ascii(const __u8 *id, unsigned int first_reg,
			       unsigned int last_reg, const char *name)
{
	unsigned int reg, val;

	printf("\t%-41s : ", name);
	while (first_reg <= last_reg && id[last_reg] == ' ')
		last_reg--;
	for (reg = first_reg; reg <= last_reg; reg++) {
		val = id[reg];
		putchar(((val >= 32) && (val <= 126)) ? val : '_');
	}
	printf("\n");
}

static void sff8079_show_options(const __u8 *id)
{
	static const char *pfx =
		"\tOption                                    :";

	printf("\t%-41s : 0x%02x 0x%02x\n", "Option values", id[64], id[65]);
	if (id[65] & (1 << 1))
		printf("%s RX_LOS implemented\n", pfx);
	if (id[65] & (1 << 2))
		printf("%s RX_LOS implemented, inverted\n", pfx);
	if (id[65] & (1 << 3))
		printf("%s TX_FAULT implemented\n", pfx);
	if (id[65] & (1 << 4))
		printf("%s TX_DISABLE implemented\n", pfx);
	if (id[65] & (1 << 5))
		printf("%s RATE_SELECT implemented\n", pfx);
	if (id[65] & (1 << 6))
		printf("%s Tunable transmitter technology\n", pfx);
	if (id[65] & (1 << 7))
		printf("%s Receiver decision threshold implemented\n", pfx);
	if (id[64] & (1 << 0))
		printf("%s Linear receiver output implemented\n", pfx);
	if (id[64] & (1 << 1))
		printf("%s Power level 2 requirement\n", pfx);
	if (id[64] & (1 << 2))
		printf("%s Cooled transceiver implemented\n", pfx);
	if (id[64] & (1 << 3))
		printf("%s Retimer or CDR implemented\n", pfx);
	if (id[64] & (1 << 4))
		printf("%s Paging implemented\n", pfx);
	if (id[64] & (1 << 5))
		printf("%s Power level 3 requirement\n", pfx);
}

void sff8079_show_all(const __u8 *id)
{
	sff8079_show_identifier(id);
	if (((id[0] == 0x02) || (id[0] == 0x03)) && (id[1] == 0x04)) {
		sff8079_show_ext_identifier(id);
		sff8079_show_connector(id);
		sff8079_show_transceiver(id);
		sff8079_show_encoding(id);
		sff8079_show_value_with_unit(id, 12, "BR, Nominal", 100, "MBd");
		sff8079_show_rate_identifier(id);
		sff8079_show_value_with_unit(id, 14,
					     "Length (SMF,km)", 1, "km");
		sff8079_show_value_with_unit(id, 15, "Length (SMF)", 100, "m");
		sff8079_show_value_with_unit(id, 16, "Length (50um)", 10, "m");
		sff8079_show_value_with_unit(id, 17,
					     "Length (62.5um)", 10, "m");
		sff8079_show_value_with_unit(id, 18, "Length (Copper)", 1, "m");
		sff8079_show_value_with_unit(id, 19, "Length (OM3)", 10, "m");
		sff8079_show_wavelength_or_copper_compliance(id);
		sff8079_show_ascii(id, 20, 35, "Vendor name");
		sff8079_show_oui(id);
		sff8079_show_ascii(id, 40, 55, "Vendor PN");
		sff8079_show_ascii(id, 56, 59, "Vendor rev");
		sff8079_show_options(id);
		sff8079_show_value_with_unit(id, 66, "BR margin, max", 1, "%");
		sff8079_show_value_with_unit(id, 67, "BR margin, min", 1, "%");
		sff8079_show_ascii(id, 68, 83, "Vendor SN");
		sff8079_show_ascii(id, 84, 91, "Date code");
	}
}
