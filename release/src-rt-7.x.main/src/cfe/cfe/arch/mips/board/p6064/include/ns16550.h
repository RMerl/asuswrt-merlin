/*
 * ns16550.h: NS16550/450 & XR16850 UART registers
 *
 * Copyright (c) 1998-1999, Algorithmics Ltd.  All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify 
 * it under the terms of the "Free MIPS" License Agreement, a copy of 
 * which is available at:
 *
 *  http://www.algor.co.uk/ftp/pub/doc/freemips-license.txt
 *
 * You may not, however, modify or remove any part of this copyright 
 * message if this program is redistributed or reused in whole or in
 * part.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * "Free MIPS" License for more details.  
 */

#ifndef NS16550_HZ
#define NS16550_HZ	1843200
#endif

#define NS16550_TICK	(NS16550_HZ / 16)

/* 16 bit baud rate divisor (lower byte in dll, upper in dlm) */
#define	BRTC(x)		(NS16550_TICK / (x))

/* permissible speed variance in thousandths; real == desired +- 3.0% */
#define SPEED_TOLERANCE	30


#ifndef __ASSEMBLER__

#ifndef nsreg
#if #endian(big)
#define nsreg(x)	unsigned int x:24; unsigned char x
#else
#define nsreg(x)	unsigned char x; unsigned :24
#endif
#endif

#ifndef nslayout
#define nslayout(r0,r1,r2,r3) nsreg(r0); nsreg(r1); nsreg(r2); nsreg(r3)
#endif

typedef struct {
    nslayout (data, ier, iir, cfcr);
	/* data */	/* data register (R/W) */
#define	dll	data	/* 16550 fifo control (W) */
	/* ier */	/* interrupt enable (W) */
#define	dlm	ier	/* 16550 fifo control (W) */
	/* iir */	/* interrupt identification (R) */
#define	fifo	iir	/* 16550 fifo control (W) */
	/* cfcr) */	/* line control register (R/W) */
    nslayout(mcr,lsr,msr,scr);
	/* mcr */	/* modem control register (R/W) */
        /* lsr */	/* line status register (R/W) */
	/* msr */	/* modem status register (R/W) */
	/* scr */	/* scratch register (R/W) */
} ns16550dev;

typedef struct {
    nslayout(fifo2,fctr,efr,cfcr);
	/* fifo2 */	/* fifo trigger register (R/W) */
	/* fctr */	/* feature control (W) */
	/* efr */	/* enhanced feature register (R/W) */
	/* cfcr */	/* same as above */
    nslayout(xon1, xon2, xoff1, xoff2);
	/* xon1 */	/* xon-1 word (R/W) */
	/* xon2 */	/* xon-2 word (R/W) */
	/* xoff1 */	/* xoff-2 word (R/W) */
	/* xoff2 */	/* xoff-2 word (R) */
} xr16850efr;

#else

#ifndef NSREG
#if #endian(big)
#define NSREG(x)	((x)*4+3)
#else
#define NSREG(x)	((x)*4+0)
#endif
#endif

#define DATA	NSREG(0)
#define	DLL	DATA
#define IER	NSREG(1)
#define	DLM	IER
#define IIR	NSREG(2)
#define	FIFO	IIR
#define CFCR	NSREG(3)
#define MCR	NSREG(4)
#define LSR	NSREG(5)
#define MSR	NSREG(6)
#define SCR	NSREG(7)

#endif /* __ASSEMBLER__ */

/* interrupt enable register */
#define	IER_ERXRDY	0x01	/* int on rx ready */
#define	IER_ETXRDY	0x02	/* int on tx ready */
#define	IER_ERLS	0x04	/* int on line status change */
#define	IER_EMSC	0x08	/* int on modem status change */
#define IER_SLEEP	0x10	/* (16850) sleep mode */
#define IER_XOFF	0x20	/* (16850) int on receiving xoff */
#define IER_RTS		0x40	/* (16850) int on rts */
#define IER_CTS		0x80	/* (16850) int on cts */

/* interrupt identification register */
#define	IIR_IMASK	0xf	/* mask */
#define	IIR_RXTOUT	0xc	/* receive timeout */
#define	IIR_RLS		0x6	/* receive line status */
#define	IIR_RXRDY	0x4	/* receive ready */
#define	IIR_TXRDY	0x2	/* transmit ready */
#define	IIR_NOPEND	0x1	/* nothing */
#define	IIR_MLSC	0x0	/* modem status */
#define IIR_XOFF	0x10	/* (16850) xoff interrupt */
#define IIR_RTSCTS	0x20	/* (16850) rts/cts interrupt */
#define	IIR_FIFO_MASK	0xc0	/* set if FIFOs are enabled */

/* fifo control register */
#define	FIFO_ENABLE	0x01	/* enable fifo */
#define	FIFO_RCV_RST	0x02	/* reset receive fifo */
#define	FIFO_XMT_RST	0x04	/* reset transmit fifo */
#define	FIFO_DMA_MODE	0x08	/* enable dma mode */
#define	FIFO_TRIGGER_1	0x00	/* trigger at 1 char */
#define	FIFO_TRIGGER_4	0x40	/* trigger at 4 chars */
#define	FIFO_TRIGGER_8	0x80	/* trigger at 8 chars */
#define	FIFO_TRIGGER_14	0xc0	/* trigger at 14 chars */
#define FIFO_RXTRIG_a	0x00	/* (16850) Rx fifo trigger */
#define FIFO_RXTRIG_b	0x40	/* (16850) Rx fifo trigger */
#define FIFO_RXTRIG_c	0x80	/* (16850) Rx fifo trigger */
#define FIFO_RXTRIG_d	0xc0	/* (16850) Rx fifo trigger */
#define FIFO_TXTRIG_a	0x00	/* (16850) Tx fifo trigger */
#define FIFO_TXTRIG_b	0x10	/* (16850) Tx fifo trigger */
#define FIFO_TXTRIG_c	0x20	/* (16850) Tx fifo trigger */
#define FIFO_TXTRIG_d	0x30	/* (16850) Tx fifo trigger */

/* character format control register */
#define	CFCR_DLAB	0x80	/* divisor latch */
#define	CFCR_SBREAK	0x40	/* send break */
#define	CFCR_PZERO	0x30	/* zero parity */
#define	CFCR_PONE	0x20	/* one parity */
#define	CFCR_PEVEN	0x10	/* even parity */
#define	CFCR_PODD	0x00	/* odd parity */
#define	CFCR_PENAB	0x08	/* parity enable */
#define	CFCR_STOPB	0x04	/* 2 stop bits */
#define CFCR_CSIZE	0x03
#define	CFCR_8BITS	0x03	/* 8 data bits */
#define	CFCR_7BITS	0x02	/* 7 data bits */
#define	CFCR_6BITS	0x01	/* 6 data bits */
#define	CFCR_5BITS	0x00	/* 5 data bits */
#define CFCR_EFR	0xbf	/* access 16850 extended registers */

/* modem control register */
#define MCR_CLKDIV4	0x80	/* (16850) divide baud rate by 4 */
#define MCR_IRRT	0x40	/* (16850) IrDA connection */
#define MCR_XON_ANY	0x20	/* (16850) XON any?? */
#define	MCR_LOOPBACK	0x10	/* loopback */
#define	MCR_IENABLE	0x08	/* output 2 = int enable */
#define	MCR_DRS		0x04
#define	MCR_RTS		0x02	/* enable RTS */
#define	MCR_DTR		0x01	/* enable DTR */

/* line status register */
#define	LSR_RCV_FIFO	0x80	/* error in receive fifo */
#define	LSR_TSRE	0x40	/* transmitter empty */
#define	LSR_TXRDY	0x20	/* transmitter ready */
#define	LSR_BI		0x10	/* break detected */
#define	LSR_FE		0x08	/* framing error */
#define	LSR_PE		0x04	/* parity error */
#define	LSR_OE		0x02	/* overrun error */
#define	LSR_RXRDY	0x01	/* receiver ready */
#define	LSR_RCV_MASK	0x1f

/* modem status register */
#define	MSR_DCD		0x80	/* DCD active */
#define	MSR_RI		0x40	/* RI  active */
#define	MSR_DSR		0x20	/* DSR active */
#define	MSR_CTS		0x10	/* CTS active */
#define	MSR_DDCD	0x08    /* DCD changed */
#define	MSR_TERI	0x04    /* RI  changed */
#define	MSR_DDSR	0x02    /* DSR changed */
#define	MSR_DCTS	0x01    /* CTS changed */
 
/* 16850 EFR */
#define EFR_AUTO_CTS	0x80	/* auto CTS output flow control */
#define EFR_AUTO_RTS	0x40	/* auto RTS input flow control */
#define EFR_SPECIAL	0x20	/* special character detect */
#define EFR_ENABLE	0x10	/* enable 16850 extensions */
#define EFR_IFLOW_NONE	0x00	/* no input "software" flow control */
#define EFR_IFLOW_1	0x08	/* transmit Xon/Xoff-1 */
#define EFR_IFLOW_2	0x04	/* transmit Xon/Xoff-2 */
#define EFR_OFLOW_NONE	0x00	/* no output "software" flow control */
#define EFR_OFLOW_1	0x02	/* check for Xon/Xoff-1 */
#define EFR_OFLOW_2	0x01	/* check for Xon/Xoff-2 */

/* 16850 FCTR */
#define FCTR_TX_LEVEL	0x80	/* Tx programmable trigger level selected */
#define FCTR_RX_LEVEL	0x00	/* Rx programmable trigger level selected */
#define FCTR_SCPAD_SWAP	0x40	/* scratchpad becomes fifo count / emsr */
#define FCTR_TTABLE_A	0x00	/* select trigger table A */
#define FCTR_TTABLE_B	0x10	/* select trigger table B */
#define FCTR_TTABLE_C	0x20	/* select trigger table C */
#define FCTR_TTABLE_D	0x30	/* select trigger table D */
#define FCTR_RS485	0x08	/* auto RS485 direction control */
#define FCTR_IRRX_INV	0x04	/* invert IrDA Rx data */
#define FCTR_RTSDELAY_0	0x00	/* RTS delay timer */
#define FCTR_RTSDELAY_4	0x01
#define FCTR_RTSDELAY_6	0x02
#define FCTR_RTSDELAY_8	0x03

/* These next two appear in place of the scratchpad register if 
   FCTR_SCPAD_SWAP is set */

/* 16850 EMSR (write only) */
#define EMSR_ALT_FCNT	0x02	/* alternate Tx/Rx fifo count */
#define EMSR_FCNT_TX	0x01	/* select Tx fifo count */
#define EMSR_FCNT_RX	0x00	/* select Rx fifo count */

/* 16850 FIFO count (read only) */
#define FCNT_TX		0x80	/* fifo count is Tx fifo */
#define FCNT_RX		0x00	/* fifo count is Rx fifo */
#define FCNT_MASK	0x7f	/* fifo count */
