/*
 * Carsten Langgaard, carstenl@mips.com
 * Copyright (C) 1999,2000 MIPS Technologies, Inc.  All rights reserved.
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
 */

#include <linux/compiler.h>
#include <linux/timer.h>
#include <asm/io.h>
#include <asm/mips-boards/generic.h>
#include <asm/mips-boards/prom.h>

extern const char display_string[];
static unsigned int display_count;
static unsigned int max_display_count;

#define DISPLAY_LCDINSTRUCTION         (0*2)
#define DISPLAY_LCDDATA                (1*2)
#define DISPLAY_CPLDSTATUS             (2*2)
#define DISPLAY_CPLDDATA               (3*2)
#define LCD_SETDDRAM                   0x80
#define LCD_IR_BF                      0x80

static void lcd_wait(unsigned int __iomem *display)
{
	/* wait for CPLD state machine to become idle */
	do {
	} while (__raw_readl(display + DISPLAY_CPLDSTATUS) & 1);

	do {
		__raw_readl(display + DISPLAY_LCDINSTRUCTION);

		/* wait for CPLD state machine to become idle */
		do {
		} while (__raw_readl(display + DISPLAY_CPLDSTATUS) & 1);
	} while (__raw_readl(display + DISPLAY_CPLDDATA) & LCD_IR_BF);
}

void mips_display_message(const char *str)
{
	static unsigned int __iomem *display;  /* static => auto initialized to NULL */
	int i;
	char ch;

	if (unlikely(display == NULL))
		display = ioremap_nocache(LCD_DISPLAY_POS_BASE, 4*2*sizeof(int));

	for (i = 0; i < 16; i++) {
		if (*str)
			ch = *str++;
		else
			ch = ' ';
		lcd_wait(display);
		__raw_writel(LCD_SETDDRAM | i, display + DISPLAY_LCDINSTRUCTION);
		lcd_wait(display);
		__raw_writel(ch, display + DISPLAY_LCDDATA);
	}
}

static void scroll_display_message(unsigned long data);
static DEFINE_TIMER(mips_scroll_timer, scroll_display_message, HZ, 0);

static void scroll_display_message(unsigned long data)
{
	mips_display_message(&display_string[display_count++]);
	if (display_count == max_display_count)
		display_count = 0;

	mod_timer(&mips_scroll_timer, jiffies + HZ);
}

void mips_scroll_message(void)
{
	del_timer_sync(&mips_scroll_timer);
	max_display_count = strlen(display_string) + 1 - 16;
	mod_timer(&mips_scroll_timer, jiffies + 1);
}
