/*
 * This file is subject to the terms and conditions of the GNU General Public
 * License.  See the file "COPYING" in the main directory of this archive
 * for more details.
 *
 * Copyright (C) 2002, 2003, 06, 07 Ralf Baechle (ralf@linux-mips.org)
 * Copyright (C) 2007 MIPS Technologies, Inc.
 *   written by Ralf Baechle (ralf@linux-mips.org)
 */
#include <linux/console.h>
#include <linux/init.h>

#include <asm/setup.h>

#ifdef CONFIG_MIPS_SEAD3
#include <linux/string.h>
#include <asm/mips-boards/prom.h>

extern void prom_putchar(char, char);

static void __init
early_console_write(struct console *con, const char *s, unsigned n)
{
	while (n-- && *s) {
		if (*s == '\n')
			prom_putchar('\r', con->index);
		prom_putchar(*s, con->index);
		s++;
	}
}
#else
extern void prom_putchar(char);

static void __init
early_console_write(struct console *con, const char *s, unsigned n)
{
	while (n-- && *s) {
		if (*s == '\n')
			prom_putchar('\r');
		prom_putchar(*s);
		s++;
	}
}
#endif

static struct console early_console __initdata = {
	.name	= "early",
	.write	= early_console_write,
	.flags	= CON_PRINTBUFFER | CON_BOOT,
	.index	= -1
};

static int early_console_initialized __initdata;

void __init setup_early_printk(void)
{
	if (early_console_initialized)
		return;
	early_console_initialized = 1;

#ifdef CONFIG_MIPS_SEAD3
	if ((strstr(prom_getcmdline(), "console=ttyS0")) != NULL)
		early_console.index = 0;
	else if ((strstr(prom_getcmdline(), "console=ttyS1")) != NULL)
		early_console.index = 1;
#endif

	register_console(&early_console);
}
