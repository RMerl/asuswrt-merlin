/*
 * sff-common.c: Implements SFF-8024 Rev 4.0 i.e. Specifcation
 * of pluggable I/O configuration
 *
 * Common utilities across SFF-8436/8636 and SFF-8472/8079
 * are defined in this file
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
 *  Vidya Sagar Ravipati <vidya@cumulusnetworks.com>
 *   This implementation is loosely based on current SFP parser
 *   and SFF-8024 Rev 4.0 spec (ftp://ftp.seagate.com/pub/sff/SFF-8024.PDF)
 *   by SFF Committee.
 */

#include <stdio.h>
#include <math.h>
#include "sff-common.h"

double convert_mw_to_dbm(double mw)
{
	return (10. * log10(mw / 1000.)) + 30.;
}

void sff_show_value_with_unit(const __u8 *id, unsigned int reg,
			      const char *name, unsigned int mult,
			      const char *unit)
{
	unsigned int val = id[reg];

	printf("\t%-41s : %u%s\n", name, val * mult, unit);
}

void sff_show_ascii(const __u8 *id, unsigned int first_reg,
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

void sff8024_show_oui(const __u8 *id, int id_offset)
{
	printf("\t%-41s : %02x:%02x:%02x\n", "Vendor OUI",
		      id[id_offset], id[(id_offset) + 1],
		      id[(id_offset) + 2]);
}

void sff8024_show_identifier(const __u8 *id, int id_offset)
{
	printf("\t%-41s : 0x%02x", "Identifier", id[id_offset]);
	switch (id[id_offset]) {
	case SFF8024_ID_UNKNOWN:
		printf(" (no module present, unknown, or unspecified)\n");
		break;
	case SFF8024_ID_GBIC:
		printf(" (GBIC)\n");
		break;
	case SFF8024_ID_SOLDERED_MODULE:
		printf(" (module soldered to motherboard)\n");
		break;
	case SFF8024_ID_SFP:
		printf(" (SFP)\n");
		break;
	case SFF8024_ID_300_PIN_XBI:
		printf(" (300 pin XBI)\n");
		break;
	case SFF8024_ID_XENPAK:
		printf(" (XENPAK)\n");
		break;
	case SFF8024_ID_XFP:
		printf(" (XFP)\n");
		break;
	case SFF8024_ID_XFF:
		printf(" (XFF)\n");
		break;
	case SFF8024_ID_XFP_E:
		printf(" (XFP-E)\n");
		break;
	case SFF8024_ID_XPAK:
		printf(" (XPAK)\n");
		break;
	case SFF8024_ID_X2:
		printf(" (X2)\n");
		break;
	case SFF8024_ID_DWDM_SFP:
		printf(" (DWDM-SFP)\n");
		break;
	case SFF8024_ID_QSFP:
		printf(" (QSFP)\n");
		break;
	case SFF8024_ID_QSFP_PLUS:
		printf(" (QSFP+)\n");
		break;
	case SFF8024_ID_CXP:
		printf(" (CXP)\n");
		break;
	case SFF8024_ID_HD4X:
		printf(" (Shielded Mini Multilane HD 4X)\n");
		break;
	case SFF8024_ID_HD8X:
		printf(" (Shielded Mini Multilane HD 8X)\n");
		break;
	case SFF8024_ID_QSFP28:
		printf(" (QSFP28)\n");
		break;
	case SFF8024_ID_CXP2:
		printf(" (CXP2/CXP28)\n");
		break;
	case SFF8024_ID_CDFP:
		printf(" (CDFP Style 1/Style 2)\n");
		break;
	case SFF8024_ID_HD4X_FANOUT:
		printf(" (Shielded Mini Multilane HD 4X Fanout Cable)\n");
		break;
	case SFF8024_ID_HD8X_FANOUT:
		printf(" (Shielded Mini Multilane HD 8X Fanout Cable)\n");
		break;
	case SFF8024_ID_CDFP_S3:
		printf(" (CDFP Style 3)\n");
		break;
	case SFF8024_ID_MICRO_QSFP:
		printf(" (microQSFP)\n");
		break;
	default:
		printf(" (reserved or unknown)\n");
		break;
	}
}

void sff8024_show_connector(const __u8 *id, int ctor_offset)
{
	printf("\t%-41s : 0x%02x", "Connector", id[ctor_offset]);
	switch (id[ctor_offset]) {
	case  SFF8024_CTOR_UNKNOWN:
		printf(" (unknown or unspecified)\n");
		break;
	case SFF8024_CTOR_SC:
		printf(" (SC)\n");
		break;
	case SFF8024_CTOR_FC_STYLE_1:
		printf(" (Fibre Channel Style 1 copper)\n");
		break;
	case SFF8024_CTOR_FC_STYLE_2:
		printf(" (Fibre Channel Style 2 copper)\n");
		break;
	case SFF8024_CTOR_BNC_TNC:
		printf(" (BNC/TNC)\n");
		break;
	case SFF8024_CTOR_FC_COAX:
		printf(" (Fibre Channel coaxial headers)\n");
		break;
	case SFF8024_CTOR_FIBER_JACK:
		printf(" (FibreJack)\n");
		break;
	case SFF8024_CTOR_LC:
		printf(" (LC)\n");
		break;
	case SFF8024_CTOR_MT_RJ:
		printf(" (MT-RJ)\n");
		break;
	case SFF8024_CTOR_MU:
		printf(" (MU)\n");
		break;
	case SFF8024_CTOR_SG:
		printf(" (SG)\n");
		break;
	case SFF8024_CTOR_OPT_PT:
		printf(" (Optical pigtail)\n");
		break;
	case SFF8024_CTOR_MPO:
		printf(" (MPO Parallel Optic)\n");
		break;
	case SFF8024_CTOR_MPO_2:
		printf(" (MPO Parallel Optic - 2x16)\n");
		break;
	case SFF8024_CTOR_HSDC_II:
		printf(" (HSSDC II)\n");
		break;
	case SFF8024_CTOR_COPPER_PT:
		printf(" (Copper pigtail)\n");
		break;
	case SFF8024_CTOR_RJ45:
		printf(" (RJ45)\n");
		break;
	case SFF8024_CTOR_NO_SEPARABLE:
		printf(" (No separable connector)\n");
		break;
	case SFF8024_CTOR_MXC_2x16:
		printf(" (MXC 2x16)\n");
		break;
	default:
		printf(" (reserved or unknown)\n");
		break;
	}
}

void sff8024_show_encoding(const __u8 *id, int encoding_offset, int sff_type)
{
	printf("\t%-41s : 0x%02x", "Encoding", id[encoding_offset]);
	switch (id[encoding_offset]) {
	case SFF8024_ENCODING_UNSPEC:
		printf(" (unspecified)\n");
		break;
	case SFF8024_ENCODING_8B10B:
		printf(" (8B/10B)\n");
		break;
	case SFF8024_ENCODING_4B5B:
		printf(" (4B/5B)\n");
		break;
	case SFF8024_ENCODING_NRZ:
		printf(" (NRZ)\n");
		break;
	case SFF8024_ENCODING_4h:
		if (sff_type == ETH_MODULE_SFF_8472)
			printf(" (Manchester)\n");
		else if (sff_type == ETH_MODULE_SFF_8636)
			printf(" (SONET Scrambled)\n");
		break;
	case SFF8024_ENCODING_5h:
		if (sff_type == ETH_MODULE_SFF_8472)
			printf(" (SONET Scrambled)\n");
		else if (sff_type == ETH_MODULE_SFF_8636)
			printf(" (64B/66B)\n");
		break;
	case SFF8024_ENCODING_6h:
		if (sff_type == ETH_MODULE_SFF_8472)
			printf(" (64B/66B)\n");
		else if (sff_type == ETH_MODULE_SFF_8636)
			printf(" (Manchester)\n");
		break;
	case SFF8024_ENCODING_256B:
		printf(" ((256B/257B (transcoded FEC-enabled data))\n");
		break;
	case SFF8024_ENCODING_PAM4:
		printf(" (PAM4)\n");
		break;
	default:
		printf(" (reserved or unknown)\n");
		break;
	}
}

void sff_show_thresholds(struct sff_diags sd)
{
	PRINT_BIAS("Laser bias current high alarm threshold",
		   sd.bias_cur[HALRM]);
	PRINT_BIAS("Laser bias current low alarm threshold",
		   sd.bias_cur[LALRM]);
	PRINT_BIAS("Laser bias current high warning threshold",
		   sd.bias_cur[HWARN]);
	PRINT_BIAS("Laser bias current low warning threshold",
		   sd.bias_cur[LWARN]);

	PRINT_xX_PWR("Laser output power high alarm threshold",
		     sd.tx_power[HALRM]);
	PRINT_xX_PWR("Laser output power low alarm threshold",
		     sd.tx_power[LALRM]);
	PRINT_xX_PWR("Laser output power high warning threshold",
		     sd.tx_power[HWARN]);
	PRINT_xX_PWR("Laser output power low warning threshold",
		     sd.tx_power[LWARN]);

	PRINT_TEMP("Module temperature high alarm threshold",
		   sd.sfp_temp[HALRM]);
	PRINT_TEMP("Module temperature low alarm threshold",
		   sd.sfp_temp[LALRM]);
	PRINT_TEMP("Module temperature high warning threshold",
		   sd.sfp_temp[HWARN]);
	PRINT_TEMP("Module temperature low warning threshold",
		   sd.sfp_temp[LWARN]);

	PRINT_VCC("Module voltage high alarm threshold",
		  sd.sfp_voltage[HALRM]);
	PRINT_VCC("Module voltage low alarm threshold",
		  sd.sfp_voltage[LALRM]);
	PRINT_VCC("Module voltage high warning threshold",
		  sd.sfp_voltage[HWARN]);
	PRINT_VCC("Module voltage low warning threshold",
		  sd.sfp_voltage[LWARN]);

	PRINT_xX_PWR("Laser rx power high alarm threshold",
		     sd.rx_power[HALRM]);
	PRINT_xX_PWR("Laser rx power low alarm threshold",
		     sd.rx_power[LALRM]);
	PRINT_xX_PWR("Laser rx power high warning threshold",
		     sd.rx_power[HWARN]);
	PRINT_xX_PWR("Laser rx power low warning threshold",
		     sd.rx_power[LWARN]);
}
