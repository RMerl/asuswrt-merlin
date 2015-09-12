/*
 * HND SiliconBackplane Gigabit Ethernet core registers
 *
 * Copyright (C) 2015, Broadcom Corporation. All Rights Reserved.
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
 *
 * $Id: sbgige.h 241182 2011-02-17 21:50:03Z $
 */

#ifndef	_sbgige_h_
#define	_sbgige_h_

#include <typedefs.h>
#include <sbconfig.h>
#include <pcicfg.h>

/* cpp contortions to concatenate w/arg prescan */
#ifndef PAD
#define	_PADLINE(line)	pad ## line
#define	_XSTR(line)	_PADLINE(line)
#define	PAD		_XSTR(__LINE__)
#endif	/* PAD */

/* PCI to OCP shim registers */
typedef volatile struct {
	uint32 FlushStatusControl;
	uint32 FlushReadAddr;
	uint32 FlushTimeoutCntr;
	uint32 BarrierReg;
	uint32 MaocpSIControl;
	uint32 SiocpMaControl;
	uint8 PAD[0x02E8];
} sbgige_pcishim_t;

/* SB core registers */
typedef volatile struct {
	/* PCI I/O Read/Write registers */
	uint8 pciio[0x0400];

	/* Reserved */
	uint8 reserved[0x0400];

	/* PCI configuration registers */
	pci_config_regs pcicfg;
	uint8 PAD[0x0300];

	/* PCI to OCP shim registers */
	sbgige_pcishim_t pcishim;

	/* Sonics SiliconBackplane registers */
	sbconfig_t sbconfig;
} sbgige_t;

#endif	/* _sbgige_h_ */
