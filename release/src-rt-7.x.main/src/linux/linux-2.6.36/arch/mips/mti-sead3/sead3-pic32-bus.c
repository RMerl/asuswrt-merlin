/*
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.
 */

#include <linux/delay.h>
#include <linux/kernel.h>
#include <linux/spinlock.h>
#include <linux/init.h>
#include <linux/io.h>
#include <linux/errno.h>

#define PIC32_NULL	0x00
#define PIC32_RD	0x01
#define PIC32_SYSRD	0x02
#define PIC32_WR	0x10
#define PIC32_SYSWR	0x20
#define PIC32_IRQ_CLR   0x40
#define PIC32_STATUS	0x80

#define DELAY()	udelay(100)

/* spinlock to ensure atomic access to PIC32 */
static DEFINE_SPINLOCK(pic32_bus_lock);

static void __iomem *bus_xfer   = (void __iomem *)0xbf000600;
static void __iomem *bus_status = (void __iomem *)0xbf000060;

static inline unsigned int ioready(void)
{
	return readl(bus_status) & 1;
}

static inline void wait_ioready(void)
{
	do { } while (!ioready());
}

static inline void wait_ioclear(void)
{
	do { } while (ioready());
}

static inline void check_ioclear(void)
{
	if (ioready()) {
		pr_debug("ioclear: initially busy\n");
		do {
			(void) readl(bus_xfer);
			DELAY();
		} while (ioready());
		pr_debug("ioclear: cleared busy\n");
	}
}

u32 pic32_bus_readl(u32 reg)
{
	unsigned long flags;
	u32 status, val;

	spin_lock_irqsave(&pic32_bus_lock, flags);

	check_ioclear();

	writel((PIC32_RD << 24) | (reg & 0x00ffffff), bus_xfer);
	DELAY();
	wait_ioready();
	status = readl(bus_xfer);
	DELAY();
	val = readl(bus_xfer);
	wait_ioclear();

	pr_debug("pic32_bus_readl: *%x -> %x (status=%x)\n", reg, val, status);

	spin_unlock_irqrestore(&pic32_bus_lock, flags);

	return val;
}

void pic32_bus_writel(u32 val, u32 reg)
{
	unsigned long flags;
	u32 status;

	spin_lock_irqsave(&pic32_bus_lock, flags);

	check_ioclear();

	writel((PIC32_WR << 24) | (reg & 0x00ffffff), bus_xfer);
	DELAY();
	writel(val, bus_xfer);
	DELAY();
	wait_ioready();
	status = readl(bus_xfer);
	wait_ioclear();

	pr_debug("pic32_bus_writel: *%x <- %x (status=%x)\n", reg, val, status);

	spin_unlock_irqrestore(&pic32_bus_lock, flags);
}
