/*
 * Copyright 2008-2009 Analog Devices Inc.
 *
 * Licensed under the GPL-2 or later
 */

#include <linux/serial.h>
#include <asm/dma.h>
#include <asm/portmux.h>

#define UART_GET_CHAR(uart)     bfin_read16(((uart)->port.membase + OFFSET_RBR))
#define UART_GET_DLL(uart)	bfin_read16(((uart)->port.membase + OFFSET_DLL))
#define UART_GET_IER(uart)      bfin_read16(((uart)->port.membase + OFFSET_IER))
#define UART_GET_DLH(uart)	bfin_read16(((uart)->port.membase + OFFSET_DLH))
#define UART_GET_IIR(uart)      bfin_read16(((uart)->port.membase + OFFSET_IIR))
#define UART_GET_LCR(uart)      bfin_read16(((uart)->port.membase + OFFSET_LCR))
#define UART_GET_GCTL(uart)     bfin_read16(((uart)->port.membase + OFFSET_GCTL))

#define UART_PUT_CHAR(uart, v)   bfin_write16(((uart)->port.membase + OFFSET_THR), v)
#define UART_PUT_DLL(uart, v)    bfin_write16(((uart)->port.membase + OFFSET_DLL), v)
#define UART_PUT_IER(uart, v)    bfin_write16(((uart)->port.membase + OFFSET_IER), v)
#define UART_SET_IER(uart, v)    UART_PUT_IER(uart, UART_GET_IER(uart) | (v))
#define UART_CLEAR_IER(uart, v)  UART_PUT_IER(uart, UART_GET_IER(uart) & ~(v))
#define UART_PUT_DLH(uart, v)    bfin_write16(((uart)->port.membase + OFFSET_DLH), v)
#define UART_PUT_LCR(uart, v)    bfin_write16(((uart)->port.membase + OFFSET_LCR), v)
#define UART_PUT_GCTL(uart, v)   bfin_write16(((uart)->port.membase + OFFSET_GCTL), v)

#define UART_SET_DLAB(uart)     do { UART_PUT_LCR(uart, UART_GET_LCR(uart) | DLAB); SSYNC(); } while (0)
#define UART_CLEAR_DLAB(uart)   do { UART_PUT_LCR(uart, UART_GET_LCR(uart) & ~DLAB); SSYNC(); } while (0)

#define UART_GET_CTS(x) gpio_get_value(x->cts_pin)
#define UART_DISABLE_RTS(x) gpio_set_value(x->rts_pin, 1)
#define UART_ENABLE_RTS(x) gpio_set_value(x->rts_pin, 0)
#define UART_ENABLE_INTS(x, v) UART_PUT_IER(x, v)
#define UART_DISABLE_INTS(x) UART_PUT_IER(x, 0)

#if defined(CONFIG_BFIN_UART0_CTSRTS) || defined(CONFIG_BFIN_UART1_CTSRTS)
# define CONFIG_SERIAL_BFIN_CTSRTS

# ifndef CONFIG_UART0_CTS_PIN
#  define CONFIG_UART0_CTS_PIN -1
# endif

# ifndef CONFIG_UART0_RTS_PIN
#  define CONFIG_UART0_RTS_PIN -1
# endif

# ifndef CONFIG_UART1_CTS_PIN
#  define CONFIG_UART1_CTS_PIN -1
# endif

# ifndef CONFIG_UART1_RTS_PIN
#  define CONFIG_UART1_RTS_PIN -1
# endif
#endif

#define BFIN_UART_TX_FIFO_SIZE	2

/*
 * The pin configuration is different from schematic
 */
struct bfin_serial_port {
	struct uart_port port;
	unsigned int old_status;
	int status_irq;
	unsigned int lsr;
#ifdef CONFIG_SERIAL_BFIN_DMA
	int tx_done;
	int tx_count;
	struct circ_buf rx_dma_buf;
	struct timer_list rx_dma_timer;
	int rx_dma_nrows;
	unsigned int tx_dma_channel;
	unsigned int rx_dma_channel;
	struct work_struct tx_dma_workqueue;
#endif
#ifdef CONFIG_SERIAL_BFIN_CTSRTS
	struct timer_list cts_timer;
	int cts_pin;
	int rts_pin;
#endif
};

/* The hardware clears the LSR bits upon read, so we need to cache
 * some of the more fun bits in software so they don't get lost
 * when checking the LSR in other code paths (TX).
 */
static inline unsigned int UART_GET_LSR(struct bfin_serial_port *uart)
{
	unsigned int lsr = bfin_read16(uart->port.membase + OFFSET_LSR);
	uart->lsr |= (lsr & (BI|FE|PE|OE));
	return lsr | uart->lsr;
}

static inline void UART_CLEAR_LSR(struct bfin_serial_port *uart)
{
	uart->lsr = 0;
	bfin_write16(uart->port.membase + OFFSET_LSR, -1);
}

struct bfin_serial_res {
	unsigned long uart_base_addr;
	int uart_irq;
	int uart_status_irq;
#ifdef CONFIG_SERIAL_BFIN_DMA
	unsigned int uart_tx_dma_channel;
	unsigned int uart_rx_dma_channel;
#endif
#ifdef CONFIG_SERIAL_BFIN_CTSRTS
	int uart_cts_pin;
	int uart_rts_pin;
#endif
};

struct bfin_serial_res bfin_serial_resource[] = {
#ifdef CONFIG_SERIAL_BFIN_UART0
	{
	 0xFFC00400,
	 IRQ_UART0_RX,
	 IRQ_UART0_ERROR,
#ifdef CONFIG_SERIAL_BFIN_DMA
	 CH_UART0_TX,
	 CH_UART0_RX,
#endif
#ifdef CONFIG_SERIAL_BFIN_CTSRTS
	 CONFIG_UART0_CTS_PIN,
	 CONFIG_UART0_RTS_PIN,
#endif
	 },
#endif
#ifdef CONFIG_SERIAL_BFIN_UART1
	{
	 0xFFC02000,
	 IRQ_UART1_RX,
	 IRQ_UART1_ERROR,
#ifdef CONFIG_SERIAL_BFIN_DMA
	 CH_UART1_TX,
	 CH_UART1_RX,
#endif
#ifdef CONFIG_SERIAL_BFIN_CTSRTS
	 CONFIG_UART1_CTS_PIN,
	 CONFIG_UART1_RTS_PIN,
#endif
	 },
#endif
};

#define DRIVER_NAME "bfin-uart"
