/*
 * Copyright (C) 1999, 2000, 2004, 2005  MIPS Technologies, Inc.
 *	All rights reserved.
 *	Authors: Carsten Langgaard <carstenl@mips.com>
 *		 Maciej W. Rozycki <macro@mips.com>
 *
 *  This program is free software; you can distribute it and/or modify it
 *  under the terms of the GNU General Public License (Version 2) as
 *  published by the Free Software Foundation.
 *
 *  This program is distributed in the hope it will be useful, but WITHOUT
 *  ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 *  FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License
 *  for more details.
 *
 *  You should have received a copy of the GNU General Public License along
 *  with this program; if not, write to the Free Software Foundation, Inc.,
 *  59 Temple Place - Suite 330, Boston MA 02111-1307, USA.
 *
 * PROM library initialisation code.
 */
#include <linux/init.h>
#include <linux/string.h>
#include <linux/kernel.h>

#include <asm/bootinfo.h>
#include <asm/io.h>
#include <asm/system.h>
#include <asm/cacheflush.h>
#include <asm/traps.h>

#include <asm/mips-boards/prom.h>
#include <asm/mips-boards/generic.h>

int prom_argc;
int *_prom_argv, *_prom_envp;

/*
 * YAMON (32-bit PROM) pass arguments and environment as 32-bit pointer.
 * This macro take care of sign extension, if running in 64-bit mode.
 */
#define prom_envp(index) ((char *)(long)_prom_envp[(index)])

int init_debug;  /* global var => auto initialized to 0 */

int mips_revision_corid;
int mips_revision_sconid;

char *prom_getenv(char *envname)
{
	/*
	 * Return a pointer to the given environment variable.
	 * In 64-bit mode: we're using 64-bit pointers, but all pointers
	 * in the PROM structures are only 32-bit, so we need some
	 * workarounds, if we are running in 64-bit mode.
	 */
	int i, index = 0;

	i = strlen(envname);

	while (prom_envp(index)) {
		if (strncmp(envname, prom_envp(index), i) == 0)
			return prom_envp(index+1);
		index += 2;
	}

	return NULL;
}

static inline unsigned char str2hexnum(unsigned char c)
{
	if (c >= '0' && c <= '9')
		return c - '0';
	if (c >= 'a' && c <= 'f')
		return c - 'a' + 10;
	return 0; /* foo */
}

static inline void str2eaddr(unsigned char *ea, unsigned char *str)
{
	int i;

	for (i = 0; i < 6; i++) {
		unsigned char num;

		if ((*str == '.') || (*str == ':'))
			str++;
		num = str2hexnum(*str++) << 4;
		num |= (str2hexnum(*str++));
		ea[i] = num;
	}
}

int get_ethernet_addr(char *ethernet_addr)
{
	char *ethaddr_str;

	ethaddr_str = prom_getenv("ethaddr");
	if (!ethaddr_str) {
		printk("ethaddr not set in boot prom\n");
		return -1;
	}
	str2eaddr(ethernet_addr, ethaddr_str);

	if (init_debug > 1) {
		int i;
		printk("get_ethernet_addr: ");
		for (i = 0; i < 5; i++)
			printk("%02x:", (unsigned char)*(ethernet_addr+i));
		printk("%02x\n", *(ethernet_addr+i));
	}

	return 0;
}

#ifdef CONFIG_SERIAL_8250_CONSOLE
static void __init console_config(void)
{
	char console_string[40];
	int baud = 0;
	char parity = '\0', bits = '\0', flow = '\0';
	char *s;

	if ((strstr(prom_getcmdline(), "console=")) == NULL) {
		s = prom_getenv("modetty0");
		if (s) {
			while (*s >= '0' && *s <= '9')
				baud = baud*10 + *s++ - '0';
			if (*s == ',')
				s++;
			if (*s)
				parity = *s++;
			if (*s == ',')
				s++;
			if (*s)
				bits = *s++;
			if (*s == ',')
				s++;
			if (*s == 'h')
				flow = 'r';
		}
		if (baud == 0)
			baud = 38400;
		if (parity != 'n' && parity != 'o' && parity != 'e')
			parity = 'n';
		if (bits != '7' && bits != '8')
			bits = '8';
		if (flow == '\0')
			flow = 'r';
		sprintf(console_string, " console=ttyS0,%d%c%c%c", baud, parity, bits, flow);
		strcat(prom_getcmdline(), console_string);
		pr_info("Config serial console:%s\n", console_string);
	}
}
#endif

static void __init mips_nmi_setup(void)
{
	void *base;
	extern char except_vec_nmi;

	base = cpu_has_veic ?
		(void *)(CAC_BASE + 0xa80) :
		(void *)(CAC_BASE + 0x380);
#ifdef CONFIG_CPU_MICROMIPS
	/*
	 * Decrement the exception vector address by one for MicroMIPS.
	 */
	memcpy(base, (&except_vec_nmi - 1), 0x80);

	/*
	 * This is a hack. We do not know if the boot loader was built with
	 * MicroMIPS instructions or not. If it was not, the NMI exception
	 * code at 0x80000a80 will be taken in MIPS32 mode. The hand coded
	 * assembly below forces us into MicroMIPS mode if we are a pure
	 * MicroMIPS kernel. The assembly instructions are:
	 *
	 *  3C1A8000   lui       k0,0x8000
	 *  375A0381   ori       k0,k0,0x381
	 *  03400008   jr        k0
	 *  00000000   nop
	 *
	 * The mode switch occurs by jumping to the unaligned exception
	 * vector address at 0x80000381 which would have been 0x80000380
	 * in MIPS32 mode. The jump to the unaligned address transitions
	 * us into MicroMIPS mode.
	 */
	if (!cpu_has_veic) {
		void *base2 = (void *)(CAC_BASE + 0xa80);
		*((unsigned int *)base2) = 0x3c1a8000;
		*((unsigned int *)base2 + 1) = 0x375a0381;
		*((unsigned int *)base2 + 2) = 0x03400008;
		*((unsigned int *)base2 + 3) = 0x00000000;
		flush_icache_range((unsigned long)base2, (unsigned long)base2 + 0x10);
	}
#else
	memcpy(base, &except_vec_nmi, 0x80);
#endif
	flush_icache_range((unsigned long)base, (unsigned long)base + 0x80);
}

static void __init mips_ejtag_setup(void)
{
	void *base;
	extern char except_vec_ejtag_debug;

	base = cpu_has_veic ?
		(void *)(CAC_BASE + 0xa00) :
		(void *)(CAC_BASE + 0x300);
#ifdef CONFIG_CPU_MICROMIPS
	/* Deja vu... */
	memcpy(base, (&except_vec_ejtag_debug - 1), 0x80);
	if (!cpu_has_veic) {
		void *base2 = (void *)(CAC_BASE + 0xa00);
		*((unsigned int *)base2) = 0x3c1a8000;
		*((unsigned int *)base2 + 1) = 0x375a0301;
		*((unsigned int *)base2 + 2) = 0x03400008;
		*((unsigned int *)base2 + 3) = 0x00000000;
		flush_icache_range((unsigned long)base2, (unsigned long)base2 + 0x10);
	}
#else
	memcpy(base, &except_vec_ejtag_debug, 0x80);
#endif
	flush_icache_range((unsigned long)base, (unsigned long)base + 0x80);
}

extern struct plat_smp_ops msmtc_smp_ops;

void __init prom_init(void)
{
	prom_argc = fw_arg0;
	_prom_argv = (int *) fw_arg1;
	_prom_envp = (int *) fw_arg2;

	mips_display_message("LINUX");

	mips_revision_corid = MIPS_REVISION_CORID;
	mips_revision_sconid = MIPS_REVISION_SCONID;
	if (mips_revision_sconid == MIPS_REVISION_SCON_OTHER) {
		switch (mips_revision_corid) {
		case MIPS_REVISION_CORID_QED_RM5261:
		case MIPS_REVISION_CORID_CORE_LV:
		case MIPS_REVISION_CORID_CORE_FPGA:
		case MIPS_REVISION_CORID_CORE_FPGAR2:
			mips_revision_sconid = MIPS_REVISION_SCON_GT64120;
			break;
		case MIPS_REVISION_CORID_CORE_EMUL_BON:
		case MIPS_REVISION_CORID_BONITO64:
		case MIPS_REVISION_CORID_CORE_20K:
			mips_revision_sconid = MIPS_REVISION_SCON_BONITO;
			break;
		case MIPS_REVISION_CORID_CORE_MSC:
		case MIPS_REVISION_CORID_CORE_FPGA2:
		case MIPS_REVISION_CORID_CORE_24K:
			/*
			 * SOCit/ROCit support is essentially identical
			 * but make an attempt to distinguish them
			 */
			mips_revision_sconid = MIPS_REVISION_SCON_SOCIT;
			break;
		case MIPS_REVISION_CORID_CORE_FPGA3:
		case MIPS_REVISION_CORID_CORE_FPGA4:
		case MIPS_REVISION_CORID_CORE_FPGA5:
		case MIPS_REVISION_CORID_CORE_EMUL_MSC:
		default:
			/* See above */
			mips_revision_sconid = MIPS_REVISION_SCON_ROCIT;
			break;
		}
	}

	board_nmi_handler_setup = mips_nmi_setup;
	board_ejtag_handler_setup = mips_ejtag_setup;

	pr_info("\nLINUX started...\n");
	prom_init_cmdline();
	prom_meminit();
#ifdef CONFIG_SERIAL_8250_CONSOLE
	console_config();
#endif
#ifdef CONFIG_MIPS_CMP
	register_smp_ops(&cmp_smp_ops);
#endif
#ifdef CONFIG_MIPS_MT_SMP
	register_smp_ops(&vsmp_smp_ops);
#endif
#ifdef CONFIG_MIPS_MT_SMTC
	register_smp_ops(&msmtc_smp_ops);
#endif
}
