/*
 * Copyright (C) 2000,2001,2002,2003,2004 Broadcom Corporation
 *
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

/*
 * Driver support for the on-chip sb1250 dual-channel serial port,
 * running in asynchronous mode.  Also, support for doing a serial console
 * on one of those ports
 */
#include <linux/types.h>
#include <linux/kernel.h>
#include <linux/serial.h>
#include <linux/interrupt.h>
#include <linux/module.h>
#include <linux/console.h>
#include <linux/kdev_t.h>
#include <linux/major.h>
#include <linux/termios.h>
#include <linux/spinlock.h>
#include <linux/irq.h>
#include <linux/errno.h>
#include <linux/tty.h>
#include <linux/sched.h>
#include <linux/tty_flip.h>
#include <linux/timer.h>
#include <linux/init.h>
#include <linux/mm.h>
#include <asm/delay.h>
#include <asm/io.h>
#include <asm/uaccess.h>
#include <asm/sibyte/swarm.h>
#include <asm/sibyte/sb1250.h>
#if defined(CONFIG_SIBYTE_BCM1x55) || defined(CONFIG_SIBYTE_BCM1x80)
#include <asm/sibyte/bcm1480_regs.h>
#include <asm/sibyte/bcm1480_int.h>
#elif defined(CONFIG_SIBYTE_SB1250) || defined(CONFIG_SIBYTE_BCM112X)
#include <asm/sibyte/sb1250_regs.h>
#include <asm/sibyte/sb1250_int.h>
#else
#error invalid SiByte UART configuation
#endif
#include <asm/sibyte/sb1250_uart.h>
#include <asm/war.h>

#if defined(CONFIG_SIBYTE_BCM1x55) || defined(CONFIG_SIBYTE_BCM1x80)
#define UNIT_CHANREG(n,reg)	A_BCM1480_DUART_CHANREG((n),(reg))
#define UNIT_IMRREG(n)		A_BCM1480_DUART_IMRREG(n)
#define UNIT_INT(n)		(K_BCM1480_INT_UART_0 + (n))
#elif defined(CONFIG_SIBYTE_SB1250) || defined(CONFIG_SIBYTE_BCM112X)
#define UNIT_CHANREG(n,reg)	A_DUART_CHANREG((n),(reg))
#define UNIT_IMRREG(n)		A_DUART_IMRREG(n)
#define UNIT_INT(n)		(K_INT_UART_0 + (n))
#else
#error invalid SiByte UART configuation
#endif

/* Toggle spewing of debugging output */
#undef DEBUG

#define DEFAULT_CFLAGS          (CS8 | B115200)

#define TX_INTEN          1
#define DUART_INITIALIZED 2

#define DUART_MAX_LINE 4
char sb1250_duart_present[DUART_MAX_LINE];
EXPORT_SYMBOL(sb1250_duart_present);

/*
 * In bug 1956, we get glitches that can mess up uart registers.  This
 * "read-mode-reg after any register access" is an accepted workaround.
 */
#if SIBYTE_1956_WAR
# define SB1_SER1956_WAR {							\
	u32 ignore;										\
	ignore = csr_in32(uart_states[line].mode_1);	\
	ignore = csr_in32(uart_states[line].mode_2);	\
	}
#else
# define SB1_SER1956_WAR
#endif

/*
 * Still not sure what the termios structures set up here are for,
 *  but we have to supply pointers to them to register the tty driver
 */
static struct tty_driver *sb1250_duart_driver; //, sb1250_duart_callout_driver;

/*
 * This lock protects both the open flags for all the uart states as
 * well as the reference count for the module
 */
static DEFINE_SPINLOCK(open_lock);

typedef struct {
	unsigned char		outp_buf[SERIAL_XMIT_SIZE];
	unsigned int		outp_head;
	unsigned int		outp_tail;
	unsigned int		outp_count;
	spinlock_t		outp_lock;
	unsigned int		open;
	unsigned int		line;
	unsigned int		last_cflags;
	unsigned long		flags;
	struct tty_struct	*tty;

	/* CSR addresses */
	unsigned int		*status;
	unsigned int		*imr;
	unsigned int		*tx_hold;
	unsigned int		*rx_hold;
	unsigned int		*mode_1;
	unsigned int		*mode_2;
	unsigned int		*clk_sel;
	unsigned int		*cmd;
} uart_state_t;

static uart_state_t uart_states[DUART_MAX_LINE];

/*
 * Inline functions local to this module
 */

static inline u32 READ_SERCSR(volatile u32 *addr, int line)
{
	u32 val = csr_in32(addr);
	SB1_SER1956_WAR;
	return val;
}

static inline void WRITE_SERCSR(u32 val, volatile u32 *addr, int line)
{
	csr_out32(val, addr);
	SB1_SER1956_WAR;
}

static void init_duart_port(uart_state_t *port, int line)
{
	if (!(port->flags & DUART_INITIALIZED)) {
		port->line = line;
		port->status = IOADDR(UNIT_CHANREG(line, R_DUART_STATUS));
		port->imr = IOADDR(UNIT_IMRREG(line));
		port->tx_hold = IOADDR(UNIT_CHANREG(line, R_DUART_TX_HOLD));
		port->rx_hold = IOADDR(UNIT_CHANREG(line, R_DUART_RX_HOLD));
		port->mode_1 = IOADDR(UNIT_CHANREG(line, R_DUART_MODE_REG_1));
		port->mode_2 = IOADDR(UNIT_CHANREG(line, R_DUART_MODE_REG_2));
		port->clk_sel = IOADDR(UNIT_CHANREG(line, R_DUART_CLK_SEL));
		port->cmd = IOADDR(UNIT_CHANREG(line, R_DUART_CMD));
		port->last_cflags = DEFAULT_CFLAGS;
		spin_lock_init(&port->outp_lock);
		port->flags |= DUART_INITIALIZED;
	}
}

/*
 * Mask out the passed interrupt lines at the duart level.  This should be
 * called while holding the associated outp_lock.
 */
static inline void duart_mask_ints(unsigned int line, unsigned int mask)
{
	uart_state_t *port = uart_states + line;
	u64 tmp = READ_SERCSR(port->imr, line);
	WRITE_SERCSR(tmp & ~mask, port->imr, line);
}


/* Unmask the passed interrupt lines at the duart level */
static inline void duart_unmask_ints(unsigned int line, unsigned int mask)
{
	uart_state_t *port = uart_states + line;
	u64 tmp = READ_SERCSR(port->imr, line);
	WRITE_SERCSR(tmp | mask, port->imr, line);
}

static inline void transmit_char_pio(uart_state_t *us)
{
	struct tty_struct *tty = us->tty;
	int blocked = 0;

	if (spin_trylock(&us->outp_lock)) {
		for (;;) {
			if (!(READ_SERCSR(us->status, us->line) & M_DUART_TX_RDY))
				break;
			if (us->outp_count <= 0 || tty->stopped || tty->hw_stopped) {
				break;
			} else {
				WRITE_SERCSR(us->outp_buf[us->outp_head],
					     us->tx_hold, us->line);
				us->outp_head = (us->outp_head + 1) & (SERIAL_XMIT_SIZE-1);
				if (--us->outp_count <= 0)
					break;
			}
			udelay(10);
		}
		spin_unlock(&us->outp_lock);
	} else {
		blocked = 1;
	}

	if (!us->outp_count || tty->stopped ||
	    tty->hw_stopped || blocked) {
		us->flags &= ~TX_INTEN;
		duart_mask_ints(us->line, M_DUART_IMR_TX);
	}

	if (us->open &&
	    (us->outp_count < (SERIAL_XMIT_SIZE/2))) {
		/*
		 * We told the discipline at one point that we had no
		 * space, so it went to sleep.  Wake it up when we hit
		 * half empty
		 */
		if ((tty->flags & (1 << TTY_DO_WRITE_WAKEUP)) &&
		    tty->ldisc.write_wakeup)
			tty->ldisc.write_wakeup(tty);
		wake_up_interruptible(&tty->write_wait);
	}
}

/*
 * Generic interrupt handler for both channels.  dev_id is a pointer
 * to the proper uart_states structure, so from that we can derive
 * which port interrupted
 */

static irqreturn_t duart_int(int irq, void *dev_id)
{
	uart_state_t *us = (uart_state_t *)dev_id;
	struct tty_struct *tty = us->tty;
	unsigned int status = READ_SERCSR(us->status, us->line);

	pr_debug("DUART INT\n");

	if (status & M_DUART_RX_RDY) {
		int counter = 2048;
		unsigned int ch;

		if (status & M_DUART_OVRUN_ERR)
			tty_insert_flip_char(tty, 0, TTY_OVERRUN);
		if (status & M_DUART_PARITY_ERR) {
			printk("Parity error!\n");
		} else if (status & M_DUART_FRM_ERR) {
			printk("Frame error!\n");
		}

		while (counter > 0) {
			if (!(READ_SERCSR(us->status, us->line) & M_DUART_RX_RDY))
				break;
			ch = READ_SERCSR(us->rx_hold, us->line);
			tty_insert_flip_char(tty, ch, 0);
			udelay(1);
			counter--;
		}
		tty_flip_buffer_push(tty);
	}

	if (status & M_DUART_TX_RDY) {
		transmit_char_pio(us);
	}

	return IRQ_HANDLED;
}

/*
 *  Actual driver functions
 */

/* Return the number of characters we can accomodate in a write at this instant */
static int duart_write_room(struct tty_struct *tty)
{
	uart_state_t *us = (uart_state_t *) tty->driver_data;
	int retval;

	retval = SERIAL_XMIT_SIZE - us->outp_count;

	pr_debug("duart_write_room called, returning %i\n", retval);

	return retval;
}

/* memcpy the data from src to destination, but take extra care if the
   data is coming from user space */
static inline int copy_buf(char *dest, const char *src, int size, int from_user)
{
	if (from_user) {
		(void) copy_from_user(dest, src, size);
	} else {
		memcpy(dest, src, size);
	}
	return size;
}

/*
 * Buffer up to count characters from buf to be written.  If we don't have
 * other characters buffered, enable the tx interrupt to start sending
 */
static int duart_write(struct tty_struct *tty, const unsigned char *buf,
		       int count)
{
	uart_state_t *us;
	int c, t, total = 0;
	unsigned long flags;

	if (!tty) return 0;

	us = tty->driver_data;
	if (!us) return 0;

	pr_debug("duart_write called for %i chars by %i (%s)\n", count,
			current->pid, current->comm);

	spin_lock_irqsave(&us->outp_lock, flags);

	for (;;) {
		c = count;

		t = SERIAL_XMIT_SIZE - us->outp_tail;
		if (t < c) c = t;

		t = SERIAL_XMIT_SIZE - 1 - us->outp_count;
		if (t < c) c = t;

		if (c <= 0) break;

		memcpy(us->outp_buf + us->outp_tail, buf, c);

		us->outp_count += c;
		us->outp_tail = (us->outp_tail + c) & (SERIAL_XMIT_SIZE - 1);
		buf += c;
		count -= c;
		total += c;
	}

	spin_unlock_irqrestore(&us->outp_lock, flags);

	if (us->outp_count && !tty->stopped &&
	    !tty->hw_stopped && !(us->flags & TX_INTEN)) {
		us->flags |= TX_INTEN;
		duart_unmask_ints(us->line, M_DUART_IMR_TX);
	}

	return total;
}


/* Buffer one character to be written.  If there's not room for it, just drop
   it on the floor.  This is used for echo, among other things */
static void duart_put_char(struct tty_struct *tty, u_char ch)
{
	uart_state_t *us = (uart_state_t *) tty->driver_data;
	unsigned long flags;

	pr_debug("duart_put_char called.  Char is %x (%c)\n", (int)ch, ch);

	spin_lock_irqsave(&us->outp_lock, flags);

	if (us->outp_count == SERIAL_XMIT_SIZE) {
		spin_unlock_irqrestore(&us->outp_lock, flags);
		return;
	}

	us->outp_buf[us->outp_tail] = ch;
	us->outp_tail = (us->outp_tail + 1) &(SERIAL_XMIT_SIZE-1);
	us->outp_count++;

	spin_unlock_irqrestore(&us->outp_lock, flags);
}

static void duart_flush_chars(struct tty_struct * tty)
{
	uart_state_t *port;

	if (!tty)
		return;

	port = tty->driver_data;

	if (!port)
		return;

	if (port->outp_count <= 0 || tty->stopped || tty->hw_stopped) {
		return;
	}

	port->flags |= TX_INTEN;
	duart_unmask_ints(port->line, M_DUART_IMR_TX);
}

/* Return the number of characters in the output buffer that have yet to be
   written */
static int duart_chars_in_buffer(struct tty_struct *tty)
{
	uart_state_t *us = (uart_state_t *) tty->driver_data;
	int retval;

	retval = us->outp_count;

	pr_debug("duart_chars_in_buffer returning %i\n", retval);

	return retval;
}

/* Kill everything we haven't yet shoved into the FIFO.  Turn off the
   transmit interrupt since we've nothing more to transmit */
static void duart_flush_buffer(struct tty_struct *tty)
{
	uart_state_t *us = (uart_state_t *) tty->driver_data;
	unsigned long flags;

	pr_debug("duart_flush_buffer called\n");
	spin_lock_irqsave(&us->outp_lock, flags);
	us->outp_head = us->outp_tail = us->outp_count = 0;
	spin_unlock_irqrestore(&us->outp_lock, flags);

	wake_up_interruptible(&us->tty->write_wait);
	if ((tty->flags & (1 << TTY_DO_WRITE_WAKEUP)) &&
	    tty->ldisc.write_wakeup)
		tty->ldisc.write_wakeup(tty);
}


/* See sb1250 user manual for details on these registers */
static inline void duart_set_cflag(unsigned int line, unsigned int cflag)
{
	unsigned int mode_reg1 = 0, mode_reg2 = 0;
	unsigned int clk_divisor;
	uart_state_t *port = uart_states + line;

	switch (cflag & CSIZE) {
	case CS7:
		mode_reg1 |= V_DUART_BITS_PER_CHAR_7;

	default:
		/* We don't handle CS5 or CS6...is there a way we're supposed to
		 * flag this?  right now we just force them to CS8 */
		mode_reg1 |= 0x0;
		break;
	}
	if (cflag & CSTOPB) {
	        mode_reg2 |= M_DUART_STOP_BIT_LEN_2;
	}
	if (!(cflag & PARENB)) {
	        mode_reg1 |= V_DUART_PARITY_MODE_NONE;
	}
	if (cflag & PARODD) {
		mode_reg1 |= M_DUART_PARITY_TYPE_ODD;
	}

	/* Formula for this is (5000000/baud)-1, but we saturate
	   at 12 bits, which means we can't actually do anything less
	   that 1200 baud */
	switch (cflag & CBAUD) {
	case B200:
	case B300:
	case B1200:	clk_divisor = 4095;		break;
	case B1800:	clk_divisor = 2776;		break;
	case B2400:	clk_divisor = 2082;		break;
	case B4800:	clk_divisor = 1040;		break;
	case B9600:	clk_divisor = 519;		break;
	case B19200:	clk_divisor = 259;		break;
	case B38400:	clk_divisor = 129;		break;
	default:
	case B57600:	clk_divisor = 85;		break;
	case B115200:	clk_divisor = 42;		break;
	}
	WRITE_SERCSR(mode_reg1, port->mode_1, port->line);
	WRITE_SERCSR(mode_reg2, port->mode_2, port->line);
	WRITE_SERCSR(clk_divisor, port->clk_sel, port->line);
	port->last_cflags = cflag;
}


/* Handle notification of a termios change.  */
static void duart_set_termios(struct tty_struct *tty, struct ktermios *old)
{
	uart_state_t *us = (uart_state_t *) tty->driver_data;

	pr_debug("duart_set_termios called by %i (%s)\n", current->pid,
		current->comm);
	if (old && tty->termios->c_cflag == old->c_cflag)
		return;
	duart_set_cflag(us->line, tty->termios->c_cflag);
}

static int get_serial_info(uart_state_t *us, struct serial_struct * retinfo)
{
	struct serial_struct tmp;

	memset(&tmp, 0, sizeof(tmp));

	tmp.type = PORT_SB1250;
	tmp.line = us->line;
	tmp.port = UNIT_CHANREG(tmp.line,0);
	tmp.irq = UNIT_INT(tmp.line);
	tmp.xmit_fifo_size = 16; /* fixed by hw */
	tmp.baud_base = 5000000;
	tmp.io_type = SERIAL_IO_MEM;

	if (copy_to_user(retinfo,&tmp,sizeof(*retinfo)))
		return -EFAULT;

	return 0;
}

static int duart_ioctl(struct tty_struct *tty, struct file * file,
		       unsigned int cmd, unsigned long arg)
{
	uart_state_t *us = (uart_state_t *) tty->driver_data;

/*	if (serial_paranoia_check(info, tty->device, "rs_ioctl"))
	return -ENODEV;*/
	switch (cmd) {
	case TIOCMGET:
		printk("Ignoring TIOCMGET\n");
		break;
	case TIOCMBIS:
		printk("Ignoring TIOCMBIS\n");
		break;
	case TIOCMBIC:
		printk("Ignoring TIOCMBIC\n");
		break;
	case TIOCMSET:
		printk("Ignoring TIOCMSET\n");
		break;
	case TIOCGSERIAL:
		return get_serial_info(us,(struct serial_struct *) arg);
	case TIOCSSERIAL:
		printk("Ignoring TIOCSSERIAL\n");
		break;
	case TIOCSERCONFIG:
		printk("Ignoring TIOCSERCONFIG\n");
		break;
	case TIOCSERGETLSR: /* Get line status register */
		printk("Ignoring TIOCSERGETLSR\n");
		break;
	case TIOCSERGSTRUCT:
		printk("Ignoring TIOCSERGSTRUCT\n");
		break;
	case TIOCMIWAIT:
		printk("Ignoring TIOCMIWAIT\n");
		break;
	case TIOCGICOUNT:
		printk("Ignoring TIOCGICOUNT\n");
		break;
	case TIOCSERGWILD:
		printk("Ignoring TIOCSERGWILD\n");
		break;
	case TIOCSERSWILD:
		printk("Ignoring TIOCSERSWILD\n");
		break;
	default:
		break;
	}
//	printk("Ignoring IOCTL %x from pid %i (%s)\n", cmd, current->pid, current->comm);
	return -ENOIOCTLCMD;
}

/* XXXKW locking? */
static void duart_start(struct tty_struct *tty)
{
	uart_state_t *us = (uart_state_t *) tty->driver_data;

	pr_debug("duart_start called\n");

	if (us->outp_count && !(us->flags & TX_INTEN)) {
		us->flags |= TX_INTEN;
		duart_unmask_ints(us->line, M_DUART_IMR_TX);
	}
}

/* XXXKW locking? */
static void duart_stop(struct tty_struct *tty)
{
	uart_state_t *us = (uart_state_t *) tty->driver_data;

	pr_debug("duart_stop called\n");

	if (us->outp_count && (us->flags & TX_INTEN)) {
		us->flags &= ~TX_INTEN;
		duart_mask_ints(us->line, M_DUART_IMR_TX);
	}
}

/* Not sure on the semantics of this; are we supposed to wait until the stuff
 * already in the hardware FIFO drains, or are we supposed to wait until
 * we've drained the output buffer, too?  I'm assuming the former, 'cause thats
 * what the other drivers seem to assume
 */

static void duart_wait_until_sent(struct tty_struct *tty, int timeout)
{
	uart_state_t *us = (uart_state_t *) tty->driver_data;
	unsigned long orig_jiffies;

	orig_jiffies = jiffies;
	pr_debug("duart_wait_until_sent(%d)+\n", timeout);
	while (!(READ_SERCSR(us->status, us->line) & M_DUART_TX_EMT)) {
		set_current_state(TASK_INTERRUPTIBLE);
		schedule_timeout(1);
		if (signal_pending(current))
			break;
		if (timeout && time_after(jiffies, orig_jiffies + timeout))
			break;
	}
	pr_debug("duart_wait_until_sent()-\n");
}

/*
 * duart_hangup() --- called by tty_hangup() when a hangup is signaled.
 */
static void duart_hangup(struct tty_struct *tty)
{
	uart_state_t *us = (uart_state_t *) tty->driver_data;

	duart_flush_buffer(tty);
	us->open = 0;
	us->tty = 0;
}

/*
 * Open a tty line.  Note that this can be called multiple times, so ->open can
 * be >1.  Only set up the tty struct if this is a "new" open, e.g. ->open was
 * zero
 */
static int duart_open(struct tty_struct *tty, struct file *filp)
{
	uart_state_t *us;
	unsigned int line = tty->index;
	unsigned long flags;

	if ((line >= tty->driver->num) || !sb1250_duart_present[line])
		return -ENODEV;

	pr_debug("duart_open called by %i (%s), tty is %p, rw is %p, ww is %p\n",
	       current->pid, current->comm, tty, (void *)&tty->read_wait,
	       (void *)&tty->write_wait);

	us = uart_states + line;
	tty->driver_data = us;

	spin_lock_irqsave(&open_lock, flags);
	if (!us->open) {
		us->tty = tty;
		us->tty->termios->c_cflag = us->last_cflags;
	}
	us->open++;
	us->flags &= ~TX_INTEN;
	duart_unmask_ints(line, M_DUART_IMR_RX);
	spin_unlock_irqrestore(&open_lock, flags);

	return 0;
}


/*
 * Close a reference count out.  If reference count hits zero, null the
 * tty, kill the interrupts.  The tty_io driver is responsible for making
 * sure we've cleared out our internal buffers before calling close()
 */
static void duart_close(struct tty_struct *tty, struct file *filp)
{
	uart_state_t *us = (uart_state_t *) tty->driver_data;
	unsigned long flags;

	pr_debug("duart_close called by %i (%s)\n", current->pid, current->comm);

	if (!us || !us->open)
		return;

	spin_lock_irqsave(&open_lock, flags);
	if (tty_hung_up_p(filp)) {
		spin_unlock_irqrestore(&open_lock, flags);
		return;
	}

	if (--us->open < 0) {
		us->open = 0;
		printk(KERN_ERR "duart: bad open count: %d\n", us->open);
	}
	if (us->open) {
		spin_unlock_irqrestore(&open_lock, flags);
		return;
	}

	spin_unlock_irqrestore(&open_lock, flags);

	tty->closing = 1;

	/* Stop accepting input */
	duart_mask_ints(us->line, M_DUART_IMR_RX);
	/* Wait for FIFO to drain */
	while (!(READ_SERCSR(us->status, us->line) & M_DUART_TX_EMT))
		;

	if (tty->driver->flush_buffer)
		tty->driver->flush_buffer(tty);
	if (tty->ldisc.flush_buffer)
		tty->ldisc.flush_buffer(tty);
	tty->closing = 0;
}


static struct tty_operations duart_ops = {
        .open   = duart_open,
        .close = duart_close,
        .write = duart_write,
        .put_char = duart_put_char,
        .flush_chars = duart_flush_chars,
        .write_room = duart_write_room,
        .chars_in_buffer = duart_chars_in_buffer,
        .flush_buffer = duart_flush_buffer,
        .ioctl = duart_ioctl,
//        .throttle = duart_throttle,
//        .unthrottle = duart_unthrottle,
        .set_termios = duart_set_termios,
        .stop = duart_stop,
        .start = duart_start,
        .hangup = duart_hangup,
	.wait_until_sent = duart_wait_until_sent,
};

/* Initialize the sb1250_duart_present array based on SOC type.  */
static void __init sb1250_duart_init_present_lines(void)
{
	int i, max_lines;

	/* Set the number of available units based on the SOC type.  */
	switch (soc_type) {
	case K_SYS_SOC_TYPE_BCM1x55:
	case K_SYS_SOC_TYPE_BCM1x80:
		max_lines = 4;
		break;
	default:
		/* Assume at least two serial ports at the normal address.  */
		max_lines = 2;
		break;
	}
	if (max_lines > DUART_MAX_LINE)
		max_lines = DUART_MAX_LINE;

	for (i = 0; i < max_lines; i++)
		sb1250_duart_present[i] = 1;
}

/* Set up the driver and register it, register the UART interrupts.  This
   is called from tty_init, or as a part of the module init */
static int __init sb1250_duart_init(void)
{
	int i;

	sb1250_duart_init_present_lines();

	sb1250_duart_driver = alloc_tty_driver(DUART_MAX_LINE);
	if (!sb1250_duart_driver)
		return -ENOMEM;

	sb1250_duart_driver->owner = THIS_MODULE;
	sb1250_duart_driver->name = "duart";
	sb1250_duart_driver->major = TTY_MAJOR;
	sb1250_duart_driver->minor_start = SB1250_DUART_MINOR_BASE;
	sb1250_duart_driver->type            = TTY_DRIVER_TYPE_SERIAL;
	sb1250_duart_driver->subtype         = SERIAL_TYPE_NORMAL;
	sb1250_duart_driver->init_termios    = tty_std_termios;
	sb1250_duart_driver->flags           = TTY_DRIVER_REAL_RAW;
	tty_set_operations(sb1250_duart_driver, &duart_ops);

	for (i = 0; i < DUART_MAX_LINE; i++) {
		uart_state_t *port = uart_states + i;

		if (!sb1250_duart_present[i])
			continue;

		init_duart_port(port, i);
		duart_mask_ints(i, M_DUART_IMR_ALL);
		if (request_irq(UNIT_INT(i), duart_int, 0, "uart", port)) {
			panic("Couldn't get uart0 interrupt line");
		}
		/*
		 * this generic write to a register does not implement the 1956
		 * WAR and sometimes output gets corrupted afterwards,
		 * especially if the port was in use as a console.
		 */
		__raw_writel(M_DUART_RX_EN|M_DUART_TX_EN, port->cmd);

		/*
		 * we should really check to see if it's registered as a console
		 * before trashing those settings
		 */
		duart_set_cflag(i, port->last_cflags);
	}

	/* Interrupts are now active, our ISR can be called. */

	if (tty_register_driver(sb1250_duart_driver)) {
		printk(KERN_ERR "Couldn't register sb1250 duart serial driver\n");
		put_tty_driver(sb1250_duart_driver);
		return 1;
	}
	return 0;
}

/* Unload the driver.  Unregister stuff, get ready to go away */
static void __exit sb1250_duart_fini(void)
{
	unsigned long flags;
	int i;

	local_irq_save(flags);
	tty_unregister_driver(sb1250_duart_driver);
	put_tty_driver(sb1250_duart_driver);

	for (i = 0; i < DUART_MAX_LINE; i++) {
		if (!sb1250_duart_present[i])
			continue;
		free_irq(UNIT_INT(i), &uart_states[i]);
		disable_irq(UNIT_INT(i));
	}
	local_irq_restore(flags);
}

module_init(sb1250_duart_init);
module_exit(sb1250_duart_fini);
MODULE_DESCRIPTION("SB1250 Duart serial driver");
MODULE_AUTHOR("Broadcom Corp.");

#ifdef CONFIG_SIBYTE_SB1250_DUART_CONSOLE

/*
 * Serial console stuff.  Very basic, polling driver for doing serial
 * console output.  The console_sem is held by the caller, so we
 * shouldn't be interrupted for more console activity.
 * XXXKW What about getting interrupted by uart driver activity?
 */

void serial_outc(unsigned char c, int line)
{
	uart_state_t *port = uart_states + line;
	while (!(READ_SERCSR(port->status, line) & M_DUART_TX_RDY)) ;
	WRITE_SERCSR(c, port->tx_hold, line);
	while (!(READ_SERCSR(port->status, port->line) & M_DUART_TX_EMT)) ;
}

static void ser_console_write(struct console *cons, const char *s,
	unsigned int count)
{
	int line = cons->index;
	uart_state_t *port = uart_states + line;
	u32 imr;

	imr = READ_SERCSR(port->imr, line);
	WRITE_SERCSR(0, port->imr, line);
	while (count--) {
		if (*s == '\n')
			serial_outc('\r', line);
		serial_outc(*s++, line);
	}
	WRITE_SERCSR(imr, port->imr, line);
}

static struct tty_driver *ser_console_device(struct console *c, int *index)
{
	*index = c->index;
	return sb1250_duart_driver;
}

static int __init ser_console_setup(struct console *cons, char *str)
{
	int i;

	sb1250_duart_init_present_lines();

	for (i = 0; i < DUART_MAX_LINE; i++) {
		uart_state_t *port = uart_states + i;
		u32 cflags = DEFAULT_CFLAGS;

		if (!sb1250_duart_present[i])
			continue;

		init_duart_port(port, i);
		if (str) {
			int speed;
			char par = 'n';
			int cbits = 8;

			cflags = 0;

			/*
			 * format is in Documentation/serial_console.txt
			 */
			sscanf(str, "%d%c%d", &speed, &par, &cbits);

			switch (speed) {
			case 200:
			case 300:
			case 1200:
				cflags |= B1200;
				break;
			case 1800:
				cflags |= B1800;
				break;
			case 2400:
				cflags |= B2400;
				break;
			case 4800:
				cflags |= B4800;
				break;
			default:
			case 9600:
				cflags |= B9600;
				break;
			case 19200:
				cflags |= B19200;
				break;
			case 38400:
				cflags |= B38400;
				break;
			case 57600:
				cflags |= B57600;
				break;
			case 115200:
				cflags |= B115200;
				break;
			}
			switch (par) {
			case 'o':
				cflags |= PARODD;
			case 'e':
				cflags |= PARENB;
			}
			switch (cbits) {
			default:	// we only do 7 or 8
			case 8:
				cflags |= CS8;
				break;
			case 7:
				cflags |= CS7;
				break;
			}
		}
		duart_set_cflag(i, cflags);
		WRITE_SERCSR(M_DUART_RX_EN | M_DUART_TX_EN, port->cmd, i);
	}

	return 0;
}

static struct console sb1250_serial_console = {
	.name		= "duart",
	.write		= ser_console_write,
	.device		= ser_console_device,
	.setup		= ser_console_setup,
	.flags		= CON_PRINTBUFFER,
	.index		= -1,
};

static int __init sb1250_serial_console_init(void)
{
	//add_preferred_console("duart", 0, "57600n8");
	register_console(&sb1250_serial_console);
	return 0;
}

console_initcall(sb1250_serial_console_init);

#endif /* CONFIG_SIBYTE_SB1250_DUART_CONSOLE */
