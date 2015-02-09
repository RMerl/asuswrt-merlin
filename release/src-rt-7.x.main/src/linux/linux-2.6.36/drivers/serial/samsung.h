/* linux/drivers/serial/samsung.h
 *
 * Driver for Samsung SoC onboard UARTs.
 *
 * Ben Dooks, Copyright (c) 2003-2008 Simtec Electronics
 *	http://armlinux.simtec.co.uk/
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
*/

struct s3c24xx_uart_info {
	char			*name;
	unsigned int		type;
	unsigned int		fifosize;
	unsigned long		rx_fifomask;
	unsigned long		rx_fifoshift;
	unsigned long		rx_fifofull;
	unsigned long		tx_fifomask;
	unsigned long		tx_fifoshift;
	unsigned long		tx_fifofull;

	/* uart port features */

	unsigned int		has_divslot:1;

	/* clock source control */

	int (*get_clksrc)(struct uart_port *, struct s3c24xx_uart_clksrc *clk);
	int (*set_clksrc)(struct uart_port *, struct s3c24xx_uart_clksrc *clk);

	/* uart controls */
	int (*reset_port)(struct uart_port *, struct s3c2410_uartcfg *);
};

struct s3c24xx_uart_port {
	unsigned char			rx_claimed;
	unsigned char			tx_claimed;
	unsigned int			pm_level;
	unsigned long			baudclk_rate;

	unsigned int			rx_irq;
	unsigned int			tx_irq;

	struct s3c24xx_uart_info	*info;
	struct s3c24xx_uart_clksrc	*clksrc;
	struct clk			*clk;
	struct clk			*baudclk;
	struct uart_port		port;

#ifdef CONFIG_CPU_FREQ
	struct notifier_block		freq_transition;
#endif
};

/* conversion functions */

#define s3c24xx_dev_to_port(__dev) (struct uart_port *)dev_get_drvdata(__dev)
#define s3c24xx_dev_to_cfg(__dev) (struct s3c2410_uartcfg *)((__dev)->platform_data)

/* register access controls */

#define portaddr(port, reg) ((port)->membase + (reg))

#define rd_regb(port, reg) (__raw_readb(portaddr(port, reg)))
#define rd_regl(port, reg) (__raw_readl(portaddr(port, reg)))

#define wr_regb(port, reg, val) __raw_writeb(val, portaddr(port, reg))
#define wr_regl(port, reg, val) __raw_writel(val, portaddr(port, reg))

extern int s3c24xx_serial_probe(struct platform_device *dev,
				struct s3c24xx_uart_info *uart);

extern int __devexit s3c24xx_serial_remove(struct platform_device *dev);

extern int s3c24xx_serial_initconsole(struct platform_driver *drv,
				      struct s3c24xx_uart_info **uart);

extern int s3c24xx_serial_init(struct platform_driver *drv,
			       struct s3c24xx_uart_info *info);

#ifdef CONFIG_SERIAL_SAMSUNG_CONSOLE

#define s3c24xx_console_init(__drv, __inf)				\
static int __init s3c_serial_console_init(void)				\
{									\
	struct s3c24xx_uart_info *uinfo[CONFIG_SERIAL_SAMSUNG_UARTS];	\
	int i;								\
									\
	for (i = 0; i < CONFIG_SERIAL_SAMSUNG_UARTS; i++)		\
		uinfo[i] = __inf;					\
	return s3c24xx_serial_initconsole(__drv, uinfo);		\
}									\
									\
console_initcall(s3c_serial_console_init)

#else
#define s3c24xx_console_init(drv, inf) extern void no_console(void)
#endif

#ifdef CONFIG_SERIAL_SAMSUNG_DEBUG

extern void printascii(const char *);

static void dbg(const char *fmt, ...)
{
	va_list va;
	char buff[256];

	va_start(va, fmt);
	vsprintf(buff, fmt, va);
	va_end(va);

	printascii(buff);
}

#else
#define dbg(x...) do { } while (0)
#endif
