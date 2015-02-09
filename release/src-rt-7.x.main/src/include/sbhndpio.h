/*
 * Generic Broadcom Home Networking Division (HND) PIO engine HW interface
 * This supports the following chips: BCM42xx, 44xx, 47xx .
 *
 * Copyright (C) 2014, Broadcom Corporation. All Rights Reserved.
 * 
 * Permission to use, copy, modify, and/or distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 * 
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY
 * SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN ACTION
 * OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF OR IN
 * CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 * $Id: sbhndpio.h 241182 2011-02-17 21:50:03Z $
 */

#ifndef	_sbhndpio_h_
#define	_sbhndpio_h_

/* PIO structure,
 *  support two PIO format: 2 bytes access and 4 bytes access
 *  basic FIFO register set is per channel(transmit or receive)
 *  a pair of channels is defined for convenience
 */

/* 2byte-wide pio register set per channel(xmt or rcv) */
typedef volatile struct {
	uint16	fifocontrol;
	uint16	fifodata;
	uint16	fifofree;	/* only valid in xmt channel, not in rcv channel */
	uint16	PAD;
} pio2regs_t;

/* a pair of pio channels(tx and rx) */
typedef volatile struct {
	pio2regs_t	tx;
	pio2regs_t	rx;
} pio2regp_t;

/* 4byte-wide pio register set per channel(xmt or rcv) */
typedef volatile struct {
	uint32	fifocontrol;
	uint32	fifodata;
} pio4regs_t;

/* a pair of pio channels(tx and rx) */
typedef volatile struct {
	pio4regs_t	tx;
	pio4regs_t	rx;
} pio4regp_t;

#endif /* _sbhndpio_h_ */
