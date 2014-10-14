/* Copyright (C) 2005-2010 Freescale Semiconductor, Inc.
 * Copyright (c) 2005 MontaVista Software
 *
 * This program is free software; you can redistribute  it and/or modify it
 * under  the terms of  the GNU General  Public License as published by the
 * Free Software Foundation;  either version 2 of the  License, or (at your
 * option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the  GNU General Public License along
 * with this program; if not, write  to the Free Software Foundation, Inc.,
 * 675 Mass Ave, Cambridge, MA 02139, USA.
 */
#ifndef _EHCI_FSL_H
#define _EHCI_FSL_H

/* offsets for the non-ehci registers in the FSL SOC USB controller */
#define FSL_SOC_USB_ULPIVP	0x170
#define FSL_SOC_USB_PORTSC1	0x184
#define PORT_PTS_MSK		(3<<30)
#define PORT_PTS_UTMI		(0<<30)
#define PORT_PTS_ULPI		(2<<30)
#define	PORT_PTS_SERIAL		(3<<30)
#define PORT_PTS_PTW		(1<<28)
#define FSL_SOC_USB_PORTSC2	0x188

#define FSL_SOC_USB_USBGENCTRL	0x200
#define USBGENCTRL_PPP		(1 << 3)
#define USBGENCTRL_PFP		(1 << 2)
#define FSL_SOC_USB_ISIPHYCTRL	0x204
#define ISIPHYCTRL_PXE		(1)
#define ISIPHYCTRL_PHYE		(1 << 4)

#define FSL_SOC_USB_SNOOP1	0x400	/* NOTE: big-endian */
#define FSL_SOC_USB_SNOOP2	0x404	/* NOTE: big-endian */
#define FSL_SOC_USB_AGECNTTHRSH	0x408	/* NOTE: big-endian */
#define FSL_SOC_USB_PRICTRL	0x40c	/* NOTE: big-endian */
#define FSL_SOC_USB_SICTRL	0x410	/* NOTE: big-endian */
#define FSL_SOC_USB_CTRL	0x500	/* NOTE: big-endian */
#define SNOOP_SIZE_2GB		0x1e
#endif				/* _EHCI_FSL_H */
