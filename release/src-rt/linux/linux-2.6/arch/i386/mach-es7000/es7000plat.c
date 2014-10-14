/*
 * Written by: Garry Forsgren, Unisys Corporation
 *             Natalie Protasevich, Unisys Corporation
 * This file contains the code to configure and interface
 * with Unisys ES7000 series hardware system manager.
 *
 * Copyright (c) 2003 Unisys Corporation.  All Rights Reserved.
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of version 2 of the GNU General Public License as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it would be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program; if not, write the Free Software Foundation, Inc., 59
 * Temple Place - Suite 330, Boston MA 02111-1307, USA.
 *
 * Contact information: Unisys Corporation, Township Line & Union Meeting
 * Roads-A, Unisys Way, Blue Bell, Pennsylvania, 19424, or:
 *
 * http://www.unisys.com
 */

#include <linux/module.h>
#include <linux/types.h>
#include <linux/kernel.h>
#include <linux/smp.h>
#include <linux/string.h>
#include <linux/spinlock.h>
#include <linux/errno.h>
#include <linux/notifier.h>
#include <linux/reboot.h>
#include <linux/init.h>
#include <linux/acpi.h>
#include <asm/io.h>
#include <asm/nmi.h>
#include <asm/smp.h>
#include <asm/apicdef.h>
#include "es7000.h"
#include <mach_mpparse.h>

/*
 * ES7000 Globals
 */

volatile unsigned long	*psai = NULL;
struct mip_reg		*mip_reg;
struct mip_reg		*host_reg;
int 			mip_port;
unsigned long		mip_addr, host_addr;

/*
 * GSI override for ES7000 platforms.
 */

static unsigned int base;

static int
es7000_rename_gsi(int ioapic, int gsi)
{
	if (es7000_plat == ES7000_ZORRO)
		return gsi;

	if (!base) {
		int i;
		for (i = 0; i < nr_ioapics; i++)
			base += nr_ioapic_registers[i];
	}

	if (!ioapic && (gsi < 16)) 
		gsi += base;
	return gsi;
}

void __init
setup_unisys(void)
{
	/*
	 * Determine the generation of the ES7000 currently running.
	 *
	 * es7000_plat = 1 if the machine is a 5xx ES7000 box
	 * es7000_plat = 2 if the machine is a x86_64 ES7000 box
	 *
	 */
	if (!(boot_cpu_data.x86 <= 15 && boot_cpu_data.x86_model <= 2))
		es7000_plat = ES7000_ZORRO;
	else
		es7000_plat = ES7000_CLASSIC;
	ioapic_renumber_irq = es7000_rename_gsi;
}

/*
 * Parse the OEM Table
 */

int __init
parse_unisys_oem (char *oemptr)
{
	int                     i;
	int 			success = 0;
	unsigned char           type, size;
	unsigned long           val;
	char                    *tp = NULL;
	struct psai             *psaip = NULL;
	struct mip_reg_info 	*mi;
	struct mip_reg		*host, *mip;

	tp = oemptr;

	tp += 8;

	for (i=0; i <= 6; i++) {
		type = *tp++;
		size = *tp++;
		tp -= 2;
		switch (type) {
		case MIP_REG:
			mi = (struct mip_reg_info *)tp;
			val = MIP_RD_LO(mi->host_reg);
			host_addr = val;
			host = (struct mip_reg *)val;
			host_reg = __va(host);
			val = MIP_RD_LO(mi->mip_reg);
			mip_port = MIP_PORT(mi->mip_info);
			mip_addr = val;
			mip = (struct mip_reg *)val;
			mip_reg = __va(mip);
			Dprintk("es7000_mipcfg: host_reg = 0x%lx \n",
				(unsigned long)host_reg);
			Dprintk("es7000_mipcfg: mip_reg = 0x%lx \n",
				(unsigned long)mip_reg);
			success++;
			break;
		case MIP_PSAI_REG:
			psaip = (struct psai *)tp;
			if (tp != NULL) {
				if (psaip->addr)
					psai = __va(psaip->addr);
				else
					psai = NULL;
				success++;
			}
			break;
		default:
			break;
		}
		tp += size;
	}

	if (success < 2) {
		es7000_plat = NON_UNISYS;
	} else
		setup_unisys();
	return es7000_plat;
}

#ifdef CONFIG_ACPI
int __init
find_unisys_acpi_oem_table(unsigned long *oem_addr)
{
	struct acpi_table_header *header = NULL;
	int i = 0;
	while (ACPI_SUCCESS(acpi_get_table("OEM1", i++, &header))) {
		if (!memcmp((char *) &header->oem_id, "UNISYS", 6)) {
			struct oem_table *t = (struct oem_table *)header;
			*oem_addr = (unsigned long)__acpi_map_table(t->OEMTableAddr,
								    t->OEMTableSize);
			return 0;
		}
	}
	return -1;
}
#endif

/*
 * This file also gets compiled if CONFIG_X86_GENERICARCH is set. Generic
 * arch already has got following function definitions (asm-generic/es7000.c)
 * hence no need to define these for that case.
 */
#ifndef CONFIG_X86_GENERICARCH
void es7000_sw_apic(void);
void __init enable_apic_mode(void)
{
	es7000_sw_apic();
	return;
}

__init int mps_oem_check(struct mp_config_table *mpc, char *oem,
		char *productid)
{
	if (mpc->mpc_oemptr) {
		struct mp_config_oemtable *oem_table =
			(struct mp_config_oemtable *)mpc->mpc_oemptr;
		if (!strncmp(oem, "UNISYS", 6))
			return parse_unisys_oem((char *)oem_table);
	}
	return 0;
}
#ifdef CONFIG_ACPI
/* Hook from generic ACPI tables.c */
int __init acpi_madt_oem_check(char *oem_id, char *oem_table_id)
{
	unsigned long oem_addr;
	if (!find_unisys_acpi_oem_table(&oem_addr)) {
		if (es7000_check_dsdt())
			return parse_unisys_oem((char *)oem_addr);
		else {
			setup_unisys();
			return 1;
		}
	}
	return 0;
}
#else
int __init acpi_madt_oem_check(char *oem_id, char *oem_table_id)
{
	return 0;
}
#endif
#endif /* COFIG_X86_GENERICARCH */

static void
es7000_spin(int n)
{
	int i = 0;

	while (i++ < n)
		rep_nop();
}

static int __init
es7000_mip_write(struct mip_reg *mip_reg)
{
	int			status = 0;
	int			spin;

	spin = MIP_SPIN;
	while (((unsigned long long)host_reg->off_38 &
		(unsigned long long)MIP_VALID) != 0) {
			if (--spin <= 0) {
				printk("es7000_mip_write: Timeout waiting for Host Valid Flag");
				return -1;
			}
		es7000_spin(MIP_SPIN);
	}

	memcpy(host_reg, mip_reg, sizeof(struct mip_reg));
	outb(1, mip_port);

	spin = MIP_SPIN;

	while (((unsigned long long)mip_reg->off_38 &
		(unsigned long long)MIP_VALID) == 0) {
		if (--spin <= 0) {
			printk("es7000_mip_write: Timeout waiting for MIP Valid Flag");
			return -1;
		}
		es7000_spin(MIP_SPIN);
	}

	status = ((unsigned long long)mip_reg->off_0 &
		(unsigned long long)0xffff0000000000ULL) >> 48;
	mip_reg->off_38 = ((unsigned long long)mip_reg->off_38 &
		(unsigned long long)~MIP_VALID);
	return status;
}

int
es7000_start_cpu(int cpu, unsigned long eip)
{
	unsigned long vect = 0, psaival = 0;

	if (psai == NULL)
		return -1;

	vect = ((unsigned long)__pa(eip)/0x1000) << 16;
	psaival = (0x1000000 | vect | cpu);

	while (*psai & 0x1000000)
                ;

	*psai = psaival;

	return 0;

}

int
es7000_stop_cpu(int cpu)
{
	int startup;

	if (psai == NULL)
		return -1;

	startup= (0x1000000 | cpu);

	while ((*psai & 0xff00ffff) != startup)
		;

	startup = (*psai & 0xff0000) >> 16;
	*psai &= 0xffffff;

	return 0;

}

void __init
es7000_sw_apic()
{
	if (es7000_plat) {
		int mip_status;
		struct mip_reg es7000_mip_reg;

		printk("ES7000: Enabling APIC mode.\n");
        	memset(&es7000_mip_reg, 0, sizeof(struct mip_reg));
        	es7000_mip_reg.off_0 = MIP_SW_APIC;
        	es7000_mip_reg.off_38 = (MIP_VALID);
        	while ((mip_status = es7000_mip_write(&es7000_mip_reg)) != 0)
              		printk("es7000_sw_apic: command failed, status = %x\n",
				mip_status);
		return;
	}
}
