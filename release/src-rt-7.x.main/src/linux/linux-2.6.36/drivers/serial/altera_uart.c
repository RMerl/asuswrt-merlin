/*
 * altera_uart.c -- Altera UART driver
 *
 * Based on mcf.c -- Freescale ColdFire UART driver
 *
 * (C) Copyright 2003-2007, Greg Ungerer <gerg@snapgear.com>
 * (C) Copyright 2008, Thomas Chou <thomas@wytron.com.tw>
 * (C) Copyright 2010, Tobias Klauser <tklauser@distanz.ch>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 */

#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/interrupt.h>
#include <linux/module.h>
#include <linux/console.h>
#include <linux/tty.h>
#include <linux/tty_flip.h>
#include <linux/serial.h>
#include <linux/serial_core.h>
#include <linux/platform_device.h>
#include <linux/io.h>
#include <linux/altera_uart.h>

#define DRV_NAME "altera_uart"

/*
 * Altera UART register definitions according to the Nios UART datasheet:
 * http://www.altera.com/literature/ds/ds_nios_uart.pdf
 */

#define ALTERA_UART_SIZE		32

#define ALTERA_UART_RXDATA_REG		0
#define ALTERA_UART_TXDATA_REG		4
#define ALTERA_UART_STATUS_REG		8
#define ALTERA_UART_CONTROL_REG		12
#define ALTERA_UART_DIVISOR_REG		16
#define ALTERA_UART_EOP_REG		20

#define ALTERA_UART_STATUS_PE_MSK	0x0001	/* parity error */
#define ALTERA_UART_STATUS_FE_MSK	0x0002	/* framing error */
#define ALTERA_UART_STATUS_BRK_MSK	0x0004	/* break */
#define ALTERA_UART_STATUS_ROE_MSK	0x0008	/* RX overrun error */
#define ALTERA_UART_STATUS_TOE_MSK	0x0010	/* TX overrun error */
#define ALTERA_UART_STATUS_TMT_MSK	0x0020	/* TX shift register state */
#define ALTERA_UART_STATUS_TRDY_MSK	0x0040	/* TX ready */
#define ALTERA_UART_STATUS_RRDY_MSK	0x0080	/* RX ready */
#define ALTERA_UART_STATUS_E_MSK	0x0100	/* exception condition */
#define ALTERA_UART_STATUS_DCTS_MSK	0x0400	/* CTS logic-level change */
#define ALTERA_UART_STATUS_CTS_MSK	0x0800	/* CTS logic state */
#define ALTERA_UART_STATUS_EOP_MSK	0x1000	/* EOP written/read */

						/* Enable interrupt on... */
#define ALTERA_UART_CONTROL_PE_MSK	0x0001	/* ...parity error */
#define ALTERA_UART_CONTROL_FE_MSK	0x0002	/* ...framing error */
#define ALTERA_UART_CONTROL_BRK_MSK	0x0004	/* ...break */
#define ALTERA_UART_CONTROL_ROE_MSK	0x0008	/* ...RX overrun */
#define ALTERA_UART_CONTROL_TOE_MSK	0x0010	/* ...TX overrun */
#define ALTERA_UART_CONTROL_TMT_MSK	0x0020	/* ...TX shift register empty */
#define ALTERA_UART_CONTROL_TRDY_MSK	0x0040	/* ...TX ready */
#define ALTERA_UART_CONTROL_RRDY_MSK	0x0080	/* ...RX ready */
#define ALTERA_UART_CONTROL_E_MSK	0x0100	/* ...exception*/

#define ALTERA_UART_CONTROL_TRBK_MSK	0x0200	/* TX break */
#define ALTERA_UART_CONTROL_DCTS_MSK	0x0400	/* Interrupt on CTS change */
#define ALTERA_UART_CONTROL_RTS_MSK	0x0800	/* RTS signal */
#define ALTERA_UART_CONTROL_EOP_MSK	0x1000	/* Interrupt on EOP */

/*
 * Local per-uart structure.
 */
struct altera_uart {
	struct uart_port port;
	unsigned int sigs;	/* Local copy of line sigs */
	unsigned short imr;	/* Local IMR mirror */
};

static unsigned int altera_uart_tx_empty(struct uart_port *port)
{
	return (readl(port->membase + ALTERA_UART_STATUS_REG) &
		ALTERA_UART_STATUS_TMT_MSK) ? TIOCSER_TEMT : 0;
}

static unsigned int altera_uart_get_mctrl(struct uart_port *port)
{
	struct altera_uart *pp = container_of(port, struct altera_uart, port);
	unsigned int sigs;

	sigs =
	    (readl(port->membase + ALTERA_UART_STATUS_REG) &
	     ALTERA_UART_STATUS_CTS_MSK) ? TIOCM_CTS : 0;
	sigs |= (pp->sigs & TIOCM_RTS);

	return sigs;
}

static void altera_uart_set_mctrl(struct uart_port *port, unsigned int sigs)
{
	struct altera_uart *pp = container_of(port, struct altera_uart, port);

	pp->sigs = sigs;
	if (sigs & TIOCM_RTS)
		pp->imr |= ALTERA_UART_CONTROL_RTS_MSK;
	else
		pp->imr &= ~ALTERA_UART_CONTROL_RTS_MSK;
	writel(pp->imr, port->membase + ALTERA_UART_CONTROL_REG);
}

static void altera_uart_start_tx(struct uart_port *port)
{
	struct altera_uart *pp = container_of(port, struct altera_uart, port);

	pp->imr |= ALTERA_UART_CONTROL_TRDY_MSK;
	writel(pp->imr, port->membase + ALTERA_UART_CONTROL_REG);
}

static void altera_uart_stop_tx(struct uart_port *port)
{
	struct altera_uart *pp = container_of(port, struct altera_uart, port);

	pp->imr &= ~ALTERA_UART_CONTROL_TRDY_MSK;
	writel(pp->imr, port->membase + ALTERA_UART_CONTROL_REG);
}

static void altera_uart_stop_rx(struct uart_port *port)
{
	struct altera_uart *pp = container_of(port, struct altera_uart, port);

	pp->imr &= ~ALTERA_UART_CONTROL_RRDY_MSK;
	writel(pp->imr, port->membase + ALTERA_UART_CONTROL_REG);
}

static void altera_uart_break_ctl(struct uart_port *port, int break_state)
{
	struct altera_uart *pp = container_of(port, struct altera_uart, port);
	unsigned long flags;

	spin_lock_irqsave(&port->lock, flags);
	if (break_state == -1)
		pp->imr |= ALTERA_UART_CONTROL_TRBK_MSK;
	else
		pp->imr &= ~ALTERA_UART_CONTROL_TRBK_MSK;
	writel(pp->imr, port->membase + ALTERA_UART_CONTROL_REG);
	spin_unlock_irqrestore(&port->lock, flags);
}

static void altera_uart_enable_ms(struct uart_port *port)
{
}

static void altera_uart_set_termios(struct uart_port *port,
				    struct ktermios *termios,
				    struct ktermios *old)
{
	unsigned long flags;
	unsigned int baud, baudclk;

	baud = uart_get_baud_rate(port, termios, old, 0, 4000000);
	baudclk = port->uartclk / baud;

	if (old)
		tty_termios_copy_hw(termios, old);
	tty_termios_encode_baud_rate(termios, baud, baud);

	spin_lock_irqsave(&port->lock, flags);
	writel(baudclk, port->membase + ALTERA_UART_DIVISOR_REG);
	spin_unlock_irqrestore(&port->lock, flags);
}

static void altera_uart_rx_chars(struct altera_uart *pp)
{
	struct uart_port *port = &pp->port;
	unsigned char ch, flag;
	unsigned short status;

	while ((status = readl(port->membase + ALTERA_UART_STATUS_REG)) &
	       ALTERA_UART_STATUS_RRDY_MSK) {
		ch = readl(port->membase + ALTERA_UART_RXDATA_REG);
		flag = TTY_NORMAL;
		port->icount.rx++;

		if (status & ALTERA_UART_STATUS_E_MSK) {
			writel(status, port->membase + ALTERA_UART_STATUS_REG);

			if (status & ALTERA_UART_STATUS_BRK_MSK) {
				port->icount.brk++;
				if (uart_handle_break(port))
					continue;
			} else if (status & ALTERA_UART_STATUS_PE_MSK) {
				port->icount.parity++;
			} else if (status & ALTERA_UART_STATUS_ROE_MSK) {
				port->icount.overrun++;
			} else if (status & ALTERA_UART_STATUS_FE_MSK) {
				port->icount.frame++;
			}

			status &= port->read_status_mask;

			if (status & ALTERA_UART_STATUS_BRK_MSK)
				flag = TTY_BREAK;
			else if (status & ALTERA_UART_STATUS_PE_MSK)
				flag = TTY_PARITY;
			else if (status & ALTERA_UART_STATUS_FE_MSK)
				flag = TTY_FRAME;
		}

		if (uart_handle_sysrq_char(port, ch))
			continue;
		uart_insert_char(port, status, ALTERA_UART_STATUS_ROE_MSK, ch,
				 flag);
	}

	tty_flip_buffer_push(port->state->port.tty);
}

static void altera_uart_tx_chars(struct altera_uart *pp)
{
	struct uart_port *port = &pp->port;
	struct circ_buf *xmit = &port->state->xmit;

	if (port->x_char) {
		/* Send special char - probably flow control */
		writel(port->x_char, port->membase + ALTERA_UART_TXDATA_REG);
		port->x_char = 0;
		port->icount.tx++;
		return;
	}

	while (readl(port->membase + ALTERA_UART_STATUS_REG) &
	       ALTERA_UART_STATUS_TRDY_MSK) {
		if (xmit->head == xmit->tail)
			break;
		writel(xmit->buf[xmit->tail],
		       port->membase + ALTERA_UART_TXDATA_REG);
		xmit->tail = (xmit->tail + 1) & (UART_XMIT_SIZE - 1);
		port->icount.tx++;
	}

	if (uart_circ_chars_pending(xmit) < WAKEUP_CHARS)
		uart_write_wakeup(port);

	if (xmit->head == xmit->tail) {
		pp->imr &= ~ALTERA_UART_CONTROL_TRDY_MSK;
		writel(pp->imr, port->membase + ALTERA_UART_CONTROL_REG);
	}
}

static irqreturn_t altera_uart_interrupt(int irq, void *data)
{
	struct uart_port *port = data;
	struct altera_uart *pp = container_of(port, struct altera_uart, port);
	unsigned int isr;

	isr = readl(port->membase + ALTERA_UART_STATUS_REG) & pp->imr;

	spin_lock(&port->lock);
	if (isr & ALTERA_UART_STATUS_RRDY_MSK)
		altera_uart_rx_chars(pp);
	if (isr & ALTERA_UART_STATUS_TRDY_MSK)
		altera_uart_tx_chars(pp);
	spin_unlock(&port->lock);

	return IRQ_RETVAL(isr);
}

static void altera_uart_config_port(struct uart_port *port, int flags)
{
	port->type = PORT_ALTERA_UART;

	/* Clear mask, so no surprise interrupts. */
	writel(0, port->membase + ALTERA_UART_CONTROL_REG);
	/* Clear status register */
	writel(0, port->membase + ALTERA_UART_STATUS_REG);
}

static int altera_uart_startup(struct uart_port *port)
{
	struct altera_uart *pp = container_of(port, struct altera_uart, port);
	unsigned long flags;
	int ret;

	ret = request_irq(port->irq, altera_uart_interrupt, IRQF_DISABLED,
			DRV_NAME, port);
	if (ret) {
		pr_err(DRV_NAME ": unable to attach Altera UART %d "
		       "interrupt vector=%d\n", port->line, port->irq);
		return ret;
	}

	spin_lock_irqsave(&port->lock, flags);

	/* Enable RX interrupts now */
	pp->imr = ALTERA_UART_CONTROL_RRDY_MSK;
	writel(pp->imr, port->membase + ALTERA_UART_CONTROL_REG);

	spin_unlock_irqrestore(&port->lock, flags);

	return 0;
}

static void altera_uart_shutdown(struct uart_port *port)
{
	struct altera_uart *pp = container_of(port, struct altera_uart, port);
	unsigned long flags;

	spin_lock_irqsave(&port->lock, flags);

	/* Disable all interrupts now */
	pp->imr = 0;
	writel(pp->imr, port->membase + ALTERA_UART_CONTROL_REG);

	spin_unlock_irqrestore(&port->lock, flags);

	free_irq(port->irq, port);
}

static const char *altera_uart_type(struct uart_port *port)
{
	return (port->type == PORT_ALTERA_UART) ? "Altera UART" : NULL;
}

static int altera_uart_request_port(struct uart_port *port)
{
	/* UARTs always present */
	return 0;
}

static void altera_uart_release_port(struct uart_port *port)
{
	/* Nothing to release... */
}

static int altera_uart_verify_port(struct uart_port *port,
				   struct serial_struct *ser)
{
	if ((ser->type != PORT_UNKNOWN) && (ser->type != PORT_ALTERA_UART))
		return -EINVAL;
	return 0;
}

/*
 *	Define the basic serial functions we support.
 */
static struct uart_ops altera_uart_ops = {
	.tx_empty	= altera_uart_tx_empty,
	.get_mctrl	= altera_uart_get_mctrl,
	.set_mctrl	= altera_uart_set_mctrl,
	.start_tx	= altera_uart_start_tx,
	.stop_tx	= altera_uart_stop_tx,
	.stop_rx	= altera_uart_stop_rx,
	.enable_ms	= altera_uart_enable_ms,
	.break_ctl	= altera_uart_break_ctl,
	.startup	= altera_uart_startup,
	.shutdown	= altera_uart_shutdown,
	.set_termios	= altera_uart_set_termios,
	.type		= altera_uart_type,
	.request_port	= altera_uart_request_port,
	.release_port	= altera_uart_release_port,
	.config_port	= altera_uart_config_port,
	.verify_port	= altera_uart_verify_port,
};

static struct altera_uart altera_uart_ports[CONFIG_SERIAL_ALTERA_UART_MAXPORTS];

#if defined(CONFIG_SERIAL_ALTERA_UART_CONSOLE)

int __init early_altera_uart_setup(struct altera_uart_platform_uart *platp)
{
	struct uart_port *port;
	int i;

	for (i = 0; i < CONFIG_SERIAL_ALTERA_UART_MAXPORTS && platp[i].mapbase; i++) {
		port = &altera_uart_ports[i].port;

		port->line = i;
		port->type = PORT_ALTERA_UART;
		port->mapbase = platp[i].mapbase;
		port->membase = ioremap(port->mapbase, ALTERA_UART_SIZE);
		port->iotype = SERIAL_IO_MEM;
		port->irq = platp[i].irq;
		port->uartclk = platp[i].uartclk;
		port->flags = ASYNC_BOOT_AUTOCONF;
		port->ops = &altera_uart_ops;
	}

	return 0;
}

static void altera_uart_console_putc(struct uart_port *port, const char c)
{
	while (!(readl(port->membase + ALTERA_UART_STATUS_REG) &
		 ALTERA_UART_STATUS_TRDY_MSK))
		cpu_relax();

	writel(c, port->membase + ALTERA_UART_TXDATA_REG);
}

static void altera_uart_console_write(struct console *co, const char *s,
				      unsigned int count)
{
	struct uart_port *port = &(altera_uart_ports + co->index)->port;

	for (; count; count--, s++) {
		altera_uart_console_putc(port, *s);
		if (*s == '\n')
			altera_uart_console_putc(port, '\r');
	}
}

static int __init altera_uart_console_setup(struct console *co, char *options)
{
	struct uart_port *port;
	int baud = CONFIG_SERIAL_ALTERA_UART_BAUDRATE;
	int bits = 8;
	int parity = 'n';
	int flow = 'n';

	if (co->index < 0 || co->index >= CONFIG_SERIAL_ALTERA_UART_MAXPORTS)
		return -EINVAL;
	port = &altera_uart_ports[co->index].port;
	if (port->membase == 0)
		return -ENODEV;

	if (options)
		uart_parse_options(options, &baud, &parity, &bits, &flow);

	return uart_set_options(port, co, baud, parity, bits, flow);
}

static struct uart_driver altera_uart_driver;

static struct console altera_uart_console = {
	.name	= "ttyS",
	.write	= altera_uart_console_write,
	.device	= uart_console_device,
	.setup	= altera_uart_console_setup,
	.flags	= CON_PRINTBUFFER,
	.index	= -1,
	.data	= &altera_uart_driver,
};

static int __init altera_uart_console_init(void)
{
	register_console(&altera_uart_console);
	return 0;
}

console_initcall(altera_uart_console_init);

#define	ALTERA_UART_CONSOLE	(&altera_uart_console)

#else

#define	ALTERA_UART_CONSOLE	NULL

#endif /* CONFIG_ALTERA_UART_CONSOLE */

/*
 *	Define the altera_uart UART driver structure.
 */
static struct uart_driver altera_uart_driver = {
	.owner		= THIS_MODULE,
	.driver_name	= DRV_NAME,
	.dev_name	= "ttyS",
	.major		= TTY_MAJOR,
	.minor		= 64,
	.nr		= CONFIG_SERIAL_ALTERA_UART_MAXPORTS,
	.cons		= ALTERA_UART_CONSOLE,
};

static int __devinit altera_uart_probe(struct platform_device *pdev)
{
	struct altera_uart_platform_uart *platp = pdev->dev.platform_data;
	struct uart_port *port;
	int i;

	for (i = 0; i < CONFIG_SERIAL_ALTERA_UART_MAXPORTS && platp[i].mapbase; i++) {
		port = &altera_uart_ports[i].port;

		port->line = i;
		port->type = PORT_ALTERA_UART;
		port->mapbase = platp[i].mapbase;
		port->membase = ioremap(port->mapbase, ALTERA_UART_SIZE);
		port->iotype = SERIAL_IO_MEM;
		port->irq = platp[i].irq;
		port->uartclk = platp[i].uartclk;
		port->ops = &altera_uart_ops;
		port->flags = ASYNC_BOOT_AUTOCONF;

		uart_add_one_port(&altera_uart_driver, port);
	}

	return 0;
}

static int __devexit altera_uart_remove(struct platform_device *pdev)
{
	struct uart_port *port;
	int i;

	for (i = 0; i < CONFIG_SERIAL_ALTERA_UART_MAXPORTS; i++) {
		port = &altera_uart_ports[i].port;
		if (port)
			uart_remove_one_port(&altera_uart_driver, port);
	}

	return 0;
}

static struct platform_driver altera_uart_platform_driver = {
	.probe	= altera_uart_probe,
	.remove	= __devexit_p(altera_uart_remove),
	.driver	= {
		.name	= DRV_NAME,
		.owner	= THIS_MODULE,
		.pm	= NULL,
	},
};

static int __init altera_uart_init(void)
{
	int rc;

	rc = uart_register_driver(&altera_uart_driver);
	if (rc)
		return rc;
	rc = platform_driver_register(&altera_uart_platform_driver);
	if (rc) {
		uart_unregister_driver(&altera_uart_driver);
		return rc;
	}
	return 0;
}

static void __exit altera_uart_exit(void)
{
	platform_driver_unregister(&altera_uart_platform_driver);
	uart_unregister_driver(&altera_uart_driver);
}

module_init(altera_uart_init);
module_exit(altera_uart_exit);

MODULE_DESCRIPTION("Altera UART driver");
MODULE_AUTHOR("Thomas Chou <thomas@wytron.com.tw>");
MODULE_LICENSE("GPL");
MODULE_ALIAS("platform:" DRV_NAME);
