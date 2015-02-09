/* 
 *    PDC Console support - ie use firmware to dump text via boot console
 *
 *    Copyright (C) 1999-2003 Matthew Wilcox <willy at parisc-linux.org>
 *    Copyright (C) 2000 Martin K Petersen <mkp at mkp.net>
 *    Copyright (C) 2000 John Marvin <jsm at parisc-linux.org>
 *    Copyright (C) 2000-2003 Paul Bame <bame at parisc-linux.org>
 *    Copyright (C) 2000 Philipp Rumpf <prumpf with tux.org>
 *    Copyright (C) 2000 Michael Ang <mang with subcarrier.org>
 *    Copyright (C) 2000 Grant Grundler <grundler with parisc-linux.org>
 *    Copyright (C) 2001-2002 Ryan Bradetich <rbrad at parisc-linux.org>
 *    Copyright (C) 2001 Helge Deller <deller at parisc-linux.org>
 *    Copyright (C) 2001 Thomas Bogendoerfer <tsbogend at parisc-linux.org>
 *    Copyright (C) 2002 Randolph Chung <tausq with parisc-linux.org>
 *
 *
 *    This program is free software; you can redistribute it and/or modify
 *    it under the terms of the GNU General Public License as published by
 *    the Free Software Foundation; either version 2 of the License, or
 *    (at your option) any later version.
 *
 *    This program is distributed in the hope that it will be useful,
 *    but WITHOUT ANY WARRANTY; without even the implied warranty of
 *    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *    GNU General Public License for more details.
 *
 *    You should have received a copy of the GNU General Public License
 *    along with this program; if not, write to the Free Software
 *    Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */

/*
 *  The PDC console is a simple console, which can be used for debugging 
 *  boot related problems on HP PA-RISC machines.
 *
 *  This code uses the ROM (=PDC) based functions to read and write characters
 *  from and to PDC's boot path.
 *  Since all character read from that path must be polled, this code never
 *  can or will be a fully functional linux console.
 */

/* Define EARLY_BOOTUP_DEBUG to debug kernel related boot problems. 
 * On production kernels EARLY_BOOTUP_DEBUG should be undefined. */
#define EARLY_BOOTUP_DEBUG


#include <linux/kernel.h>
#include <linux/console.h>
#include <linux/string.h>
#include <linux/init.h>
#include <linux/major.h>
#include <linux/tty.h>
#include <asm/pdc.h>		/* for iodc_call() proto and friends */

static DEFINE_SPINLOCK(pdc_console_lock);

static void pdc_console_write(struct console *co, const char *s, unsigned count)
{
	int i = 0;
	unsigned long flags;

	spin_lock_irqsave(&pdc_console_lock, flags);
	do {
		i += pdc_iodc_print(s + i, count - i);
	} while (i < count);
	spin_unlock_irqrestore(&pdc_console_lock, flags);
}

int pdc_console_poll_key(struct console *co)
{
	int c;
	unsigned long flags;

	spin_lock_irqsave(&pdc_console_lock, flags);
	c = pdc_iodc_getc();
	spin_unlock_irqrestore(&pdc_console_lock, flags);

	return c;
}

static int pdc_console_setup(struct console *co, char *options)
{
	return 0;
}

#if defined(CONFIG_PDC_CONSOLE)
#include <linux/vt_kern.h>

static struct tty_driver * pdc_console_device (struct console *c, int *index)
{
	extern struct tty_driver console_driver;
	*index = c->index ? c->index-1 : fg_console;
	return &console_driver;
}
#else
#define pdc_console_device NULL
#endif

static struct console pdc_cons = {
	.name =		"ttyB",
	.write =	pdc_console_write,
	.device =	pdc_console_device,
	.setup =	pdc_console_setup,
	.flags =	CON_BOOT | CON_PRINTBUFFER | CON_ENABLED,
	.index =	-1,
};

static int pdc_console_initialized;

static void pdc_console_init_force(void)
{
	if (pdc_console_initialized)
		return;
	++pdc_console_initialized;
	
	/* If the console is duplex then copy the COUT parameters to CIN. */
	if (PAGE0->mem_cons.cl_class == CL_DUPLEX)
		memcpy(&PAGE0->mem_kbd, &PAGE0->mem_cons, sizeof(PAGE0->mem_cons));

	/* register the pdc console */
	register_console(&pdc_cons);
}

void __init pdc_console_init(void)
{
#if defined(EARLY_BOOTUP_DEBUG) || defined(CONFIG_PDC_CONSOLE)
	pdc_console_init_force();
#endif
#ifdef EARLY_BOOTUP_DEBUG
	printk(KERN_INFO "Initialized PDC Console for debugging.\n");
#endif
}


/*
 * Used for emergencies. Currently only used if an HPMC occurs. If an
 * HPMC occurs, it is possible that the current console may not be
 * properly initialised after the PDC IO reset. This routine unregisters
 * all of the current consoles, reinitializes the pdc console and
 * registers it.
 */

void pdc_console_restart(void)
{
	struct console *console;

	if (pdc_console_initialized)
		return;

	/* If we've already seen the output, don't bother to print it again */
	if (console_drivers != NULL)
		pdc_cons.flags &= ~CON_PRINTBUFFER;

	while ((console = console_drivers) != NULL)
		unregister_console(console_drivers);

	/* force registering the pdc console */
	pdc_console_init_force();
}
