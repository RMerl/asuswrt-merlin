/*
 * hwmon-vid.c - VID/VRM/VRD voltage conversions
 *
 * Copyright (c) 2004 Rudolf Marek <r.marek@assembler.cz>
 *
 * Partly imported from i2c-vid.h of the lm_sensors project
 * Copyright (c) 2002 Mark D. Studebaker <mdsxyz123@yahoo.com>
 * With assistance from Trent Piepho <xyzzy@speakeasy.org>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 */

#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/hwmon-vid.h>

/*
 * Common code for decoding VID pins.
 *
 * References:
 *
 * For VRM 8.4 to 9.1, "VRM x.y DC-DC Converter Design Guidelines",
 * available at http://developer.intel.com/.
 *
 * For VRD 10.0 and up, "VRD x.y Design Guide",
 * available at http://developer.intel.com/.
 *
 * AMD Athlon 64 and AMD Opteron Processors, AMD Publication 26094,
 * http://www.amd.com/us-en/assets/content_type/white_papers_and_tech_docs/26094.PDF
 * Table 74. VID Code Voltages
 * This corresponds to an arbitrary VRM code of 24 in the functions below.
 * These CPU models (K8 revision <= E) have 5 VID pins. See also:
 * Revision Guide for AMD Athlon 64 and AMD Opteron Processors, AMD Publication 25759,
 * http://www.amd.com/us-en/assets/content_type/white_papers_and_tech_docs/25759.pdf
 *
 * AMD NPT Family 0Fh Processors, AMD Publication 32559,
 * http://www.amd.com/us-en/assets/content_type/white_papers_and_tech_docs/32559.pdf
 * Table 71. VID Code Voltages
 * This corresponds to an arbitrary VRM code of 25 in the functions below.
 * These CPU models (K8 revision >= F) have 6 VID pins. See also:
 * Revision Guide for AMD NPT Family 0Fh Processors, AMD Publication 33610,
 * http://www.amd.com/us-en/assets/content_type/white_papers_and_tech_docs/33610.pdf
 *
 * The 17 specification is in fact Intel Mobile Voltage Positioning -
 * (IMVP-II). You can find more information in the datasheet of Max1718
 * http://www.maxim-ic.com/quick_view2.cfm/qv_pk/2452
 *
 * The 13 specification corresponds to the Intel Pentium M series. There
 * doesn't seem to be any named specification for these. The conversion
 * tables are detailed directly in the various Pentium M datasheets:
 * http://www.intel.com/design/intarch/pentiumm/docs_pentiumm.htm
 *
 * The 14 specification corresponds to Intel Core series. There
 * doesn't seem to be any named specification for these. The conversion
 * tables are detailed directly in the various Pentium Core datasheets:
 * http://www.intel.com/design/mobile/datashts/309221.htm
 *
 * The 110 (VRM 11) specification corresponds to Intel Conroe based series.
 * http://www.intel.com/design/processor/applnots/313214.htm
 */

/*
 * vrm is the VRM/VRD document version multiplied by 10.
 * val is the 4-bit or more VID code.
 * Returned value is in mV to avoid floating point in the kernel.
 * Some VID have some bits in uV scale, this is rounded to mV.
 */
int vid_from_reg(int val, u8 vrm)
{
	int vid;

	switch(vrm) {

	case 100:               /* VRD 10.0 */
		/* compute in uV, round to mV */
		val &= 0x3f;
		if((val & 0x1f) == 0x1f)
			return 0;
		if((val & 0x1f) <= 0x09 || val == 0x0a)
			vid = 1087500 - (val & 0x1f) * 25000;
		else
			vid = 1862500 - (val & 0x1f) * 25000;
		if(val & 0x20)
			vid -= 12500;
		return((vid + 500) / 1000);

	case 110:		/* Intel Conroe */
				/* compute in uV, round to mV */
		val &= 0xff;
		if (val < 0x02 || val > 0xb2)
			return 0;
		return((1600000 - (val - 2) * 6250 + 500) / 1000);

	case 24:		/* Athlon64 & Opteron */
		val &= 0x1f;
		if (val == 0x1f)
			return 0;
				/* fall through */
	case 25:		/* AMD NPT 0Fh */
		val &= 0x3f;
		return (val < 32) ? 1550 - 25 * val
			: 775 - (25 * (val - 31)) / 2;

	case 91:		/* VRM 9.1 */
	case 90:		/* VRM 9.0 */
		val &= 0x1f;
		return(val == 0x1f ? 0 :
		                       1850 - val * 25);

	case 85:		/* VRM 8.5 */
		val &= 0x1f;
		return((val & 0x10  ? 25 : 0) +
		       ((val & 0x0f) > 0x04 ? 2050 : 1250) -
		       ((val & 0x0f) * 50));

	case 84:		/* VRM 8.4 */
		val &= 0x0f;
				/* fall through */
	case 82:		/* VRM 8.2 */
		val &= 0x1f;
		return(val == 0x1f ? 0 :
		       val & 0x10  ? 5100 - (val) * 100 :
		                     2050 - (val) * 50);
	case 17:		/* Intel IMVP-II */
		val &= 0x1f;
		return(val & 0x10 ? 975 - (val & 0xF) * 25 :
				    1750 - val * 50);
	case 13:
		val &= 0x3f;
		return(1708 - val * 16);
	case 14:		/* Intel Core */
				/* compute in uV, round to mV */
		val &= 0x7f;
		return(val > 0x77 ? 0 : (1500000 - (val * 12500) + 500) / 1000);
	default:		/* report 0 for unknown */
		if (vrm)
			printk(KERN_WARNING "hwmon-vid: Requested unsupported "
			       "VRM version (%u)\n", (unsigned int)vrm);
		return 0;
	}
}


/*
 * After this point is the code to automatically determine which
 * VRM/VRD specification should be used depending on the CPU.
 */

struct vrm_model {
	u8 vendor;
	u8 eff_family;
	u8 eff_model;
	u8 eff_stepping;
	u8 vrm_type;
};

#define ANY 0xFF

#ifdef CONFIG_X86

/*
 * The stepping parameter is highest acceptable stepping for current line.
 * The model match must be exact for 4-bit values. For model values 0x10
 * and above (extended model), all models below the parameter will match.
 */

static struct vrm_model vrm_models[] = {
	{X86_VENDOR_AMD, 0x6, ANY, ANY, 90},		/* Athlon Duron etc */
	{X86_VENDOR_AMD, 0xF, 0x3F, ANY, 24},		/* Athlon 64, Opteron */
	/* In theory, all NPT family 0Fh processors have 6 VID pins and should
	   thus use vrm 25, however in practice not all mainboards route the
	   6th VID pin because it is never needed. So we use the 5 VID pin
	   variant (vrm 24) for the models which exist today. */
	{X86_VENDOR_AMD, 0xF, 0x7F, ANY, 24},		/* NPT family 0Fh */
	{X86_VENDOR_AMD, 0xF, ANY, ANY, 25},		/* future fam. 0Fh */
	{X86_VENDOR_AMD, 0x10, ANY, ANY, 25},		/* NPT family 10h */

	{X86_VENDOR_INTEL, 0x6, 0x9, ANY, 13},		/* Pentium M (130 nm) */
	{X86_VENDOR_INTEL, 0x6, 0xB, ANY, 85},		/* Tualatin */
	{X86_VENDOR_INTEL, 0x6, 0xD, ANY, 13},		/* Pentium M (90 nm) */
	{X86_VENDOR_INTEL, 0x6, 0xE, ANY, 14},		/* Intel Core (65 nm) */
	{X86_VENDOR_INTEL, 0x6, 0xF, ANY, 110},		/* Intel Conroe */
	{X86_VENDOR_INTEL, 0x6, ANY, ANY, 82},		/* any P6 */
	{X86_VENDOR_INTEL, 0xF, 0x0, ANY, 90},		/* P4 */
	{X86_VENDOR_INTEL, 0xF, 0x1, ANY, 90},		/* P4 Willamette */
	{X86_VENDOR_INTEL, 0xF, 0x2, ANY, 90},		/* P4 Northwood */
	{X86_VENDOR_INTEL, 0xF, ANY, ANY, 100},		/* Prescott and above assume VRD 10 */

	{X86_VENDOR_CENTAUR, 0x6, 0x7, ANY, 85},	/* Eden ESP/Ezra */
	{X86_VENDOR_CENTAUR, 0x6, 0x8, 0x7, 85},	/* Ezra T */
	{X86_VENDOR_CENTAUR, 0x6, 0x9, 0x7, 85},	/* Nemiah */
	{X86_VENDOR_CENTAUR, 0x6, 0x9, ANY, 17},	/* C3-M, Eden-N */
	{X86_VENDOR_CENTAUR, 0x6, 0xA, 0x7, 0},		/* No information */
	{X86_VENDOR_CENTAUR, 0x6, 0xA, ANY, 13},	/* C7, Esther */

	{X86_VENDOR_UNKNOWN, ANY, ANY, ANY, 0}		/* stop here */
};

static u8 find_vrm(u8 eff_family, u8 eff_model, u8 eff_stepping, u8 vendor)
{
	int i = 0;

	while (vrm_models[i].vendor!=X86_VENDOR_UNKNOWN) {
		if (vrm_models[i].vendor==vendor)
			if ((vrm_models[i].eff_family==eff_family)
			 && ((vrm_models[i].eff_model==eff_model) ||
			     (vrm_models[i].eff_model >= 0x10 &&
			      eff_model <= vrm_models[i].eff_model) ||
			     (vrm_models[i].eff_model==ANY)) &&
			     (eff_stepping <= vrm_models[i].eff_stepping))
				return vrm_models[i].vrm_type;
		i++;
	}

	return 0;
}

u8 vid_which_vrm(void)
{
	struct cpuinfo_x86 *c = &cpu_data(0);
	u32 eax;
	u8 eff_family, eff_model, eff_stepping, vrm_ret;

	if (c->x86 < 6)		/* Any CPU with family lower than 6 */
		return 0;	/* doesn't have VID and/or CPUID */

	eax = cpuid_eax(1);
	eff_family = ((eax & 0x00000F00)>>8);
	eff_model  = ((eax & 0x000000F0)>>4);
	eff_stepping = eax & 0xF;
	if (eff_family == 0xF) {	/* use extended model & family */
		eff_family += ((eax & 0x00F00000)>>20);
		eff_model += ((eax & 0x000F0000)>>16)<<4;
	}
	vrm_ret = find_vrm(eff_family, eff_model, eff_stepping, c->x86_vendor);
	if (vrm_ret == 0)
		printk(KERN_INFO "hwmon-vid: Unknown VRM version of your "
		       "x86 CPU\n");
	return vrm_ret;
}

/* and now for something completely different for the non-x86 world */
#else
u8 vid_which_vrm(void)
{
	printk(KERN_INFO "hwmon-vid: Unknown VRM version of your CPU\n");
	return 0;
}
#endif

EXPORT_SYMBOL(vid_from_reg);
EXPORT_SYMBOL(vid_which_vrm);

MODULE_AUTHOR("Rudolf Marek <r.marek@assembler.cz>");

MODULE_DESCRIPTION("hwmon-vid driver");
MODULE_LICENSE("GPL");
