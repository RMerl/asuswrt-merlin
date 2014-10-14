/*
 * Copyright (C) 2000 RidgeRun, Inc.
 * Author: RidgeRun, Inc.
 *   glonnon@ridgerun.com, skranz@ridgerun.com, stevej@ridgerun.com
 *
 * Copyright 2001 MontaVista Software Inc.
 * Author: Jun Sun, jsun@mvista.com or jsun@junsun.net
 * Copyright (C) 2000, 2001, 2003 Ralf Baechle (ralf@gnu.org)
 *
 *  This program is free software; you can redistribute  it and/or modify it
 *  under  the terms of  the GNU General  Public License as published by the
 *  Free Software Foundation;  either version 2 of the  License, or (at your
 *  option) any later version.
 *
 *  THIS  SOFTWARE  IS PROVIDED   ``AS  IS'' AND   ANY  EXPRESS OR IMPLIED
 *  WARRANTIES,   INCLUDING, BUT NOT  LIMITED  TO, THE IMPLIED WARRANTIES OF
 *  MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.  IN
 *  NO  EVENT  SHALL   THE AUTHOR  BE    LIABLE FOR ANY   DIRECT, INDIRECT,
 *  INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 *  NOT LIMITED   TO, PROCUREMENT OF  SUBSTITUTE GOODS  OR SERVICES; LOSS OF
 *  USE, DATA,  OR PROFITS; OR  BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON
 *  ANY THEORY OF LIABILITY, WHETHER IN  CONTRACT, STRICT LIABILITY, OR TORT
 *  (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 *  THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 *  You should have received a copy of the  GNU General Public License along
 *  with this program; if not, write  to the Free Software Foundation, Inc.,
 *  675 Mass Ave, Cambridge, MA 02139, USA.
 *
 */
#include <linux/errno.h>
#include <linux/init.h>
#include <linux/kernel_stat.h>
#include <linux/module.h>
#include <linux/signal.h>
#include <linux/sched.h>
#include <linux/types.h>
#include <linux/interrupt.h>
#include <linux/ioport.h>
#include <linux/timex.h>
#include <linux/slab.h>
#include <linux/random.h>
#include <linux/bitops.h>
#include <asm/bootinfo.h>
#include <asm/io.h>
#include <asm/irq.h>
#include <asm/irq_cpu.h>
#include <asm/mipsregs.h>
#include <asm/system.h>

asmlinkage void plat_irq_dispatch(void)
{
	unsigned int pending = read_c0_status() & read_c0_cause();

	if (pending & STATUSF_IP2)		/* int0 hardware line */
		do_IRQ(2);
	else if (pending & STATUSF_IP3)		/* int1 hardware line */
		do_IRQ(3);
	else if (pending & STATUSF_IP4)		/* int2 hardware line */
		do_IRQ(4);
	else if (pending & STATUSF_IP5)		/* int3 hardware line */
		do_IRQ(5);
	else if (pending & STATUSF_IP6)		/* int4 hardware line */
		do_IRQ(6);
	else if (pending & STATUSF_IP7)		/* cpu timer */
		do_IRQ(7);
	else {
		/*
		 * Now look at the extended interrupts
		 */
		pending = (read_c0_cause() & (read_c0_intcontrol() << 8)) >> 16;

		if (pending & STATUSF_IP8)		/* int6 hardware line */
			do_IRQ(8);
		else if (pending & STATUSF_IP9)		/* int7 hardware line */
			do_IRQ(9);
		else if (pending & STATUSF_IP10)	/* int8 hardware line */
			do_IRQ(10);
		else if (pending & STATUSF_IP11)	/* int9 hardware line */
			do_IRQ(11);
	}
}

void __init arch_init_irq(void)
{
	/*
	 * Clear all of the interrupts while we change the able around a bit.
	 * int-handler is not on bootstrap
	 */
	clear_c0_status(ST0_IM);
	local_irq_disable();

	mips_cpu_irq_init();
	rm7k_cpu_irq_init();
}
